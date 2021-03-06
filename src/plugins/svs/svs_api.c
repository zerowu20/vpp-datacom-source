/*
 * Copyright (c) 2018 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <svs/svs.h>
#include <vnet/fib/fib_api.h>
#include <vnet/ip/ip_types_api.h>

#include <vpp/app/version.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

/* define message IDs */
#include <vnet/format_fns.h>
#include <svs/svs.api_enum.h>
#include <svs/svs.api_types.h>

/**
 * Base message ID fot the plugin
 */
static u32 svs_base_msg_id;
#define REPLY_MSG_ID_BASE (svs_base_msg_id)
#include <vlibapi/api_helper_macros.h>

static void
vl_api_svs_plugin_get_version_t_handler (vl_api_svs_plugin_get_version_t * mp)
{
  vl_api_svs_plugin_get_version_reply_t *rmp;
  int msg_size = sizeof (*rmp);
  vl_api_registration_t *rp;

  rp = vl_api_client_index_to_registration (mp->client_index);
  if (rp == 0)
    return;

  rmp = vl_msg_api_alloc (msg_size);
  clib_memset (rmp, 0, msg_size);
  rmp->_vl_msg_id =
    ntohs (VL_API_SVS_PLUGIN_GET_VERSION_REPLY + svs_base_msg_id);
  rmp->context = mp->context;
  rmp->major = htonl (SVS_PLUGIN_VERSION_MAJOR);
  rmp->minor = htonl (SVS_PLUGIN_VERSION_MINOR);

  vl_api_send_msg (rp, (u8 *) rmp);
}

static void
vl_api_svs_table_add_del_t_handler (vl_api_svs_table_add_del_t * mp)
{
  vl_api_svs_table_add_del_reply_t *rmp;
  fib_protocol_t fproto;
  int rv = 0;

  rv = fib_proto_from_api_address_family (mp->af, &fproto);
  if (rv < 0)
    goto error;

  if (mp->is_add)
    {
      rv = svs_table_add (fproto, ntohl (mp->table_id));
    }
  else
    {
      rv = svs_table_delete (fproto, ntohl (mp->table_id));
    }

error:
  REPLY_MACRO (VL_API_SVS_TABLE_ADD_DEL_REPLY);
}

static void
vl_api_svs_route_add_del_t_handler (vl_api_svs_route_add_del_t * mp)
{
  vl_api_svs_route_add_del_reply_t *rmp;
  fib_prefix_t pfx;
  int rv = 0;

  ip_prefix_decode (&mp->prefix, &pfx);

  if (mp->is_add)
    {
      rv = svs_route_add (ntohl (mp->table_id), &pfx,
			  ntohl (mp->source_table_id));
    }
  else
    {
      rv = svs_route_delete (ntohl (mp->table_id), &pfx);
    }

  REPLY_MACRO (VL_API_SVS_ROUTE_ADD_DEL_REPLY);
}

static void
vl_api_svs_enable_disable_t_handler (vl_api_svs_enable_disable_t * mp)
{
  vl_api_svs_enable_disable_reply_t *rmp;
  fib_protocol_t fproto;
  int rv = 0;

  VALIDATE_SW_IF_INDEX (mp);

  rv = fib_proto_from_api_address_family (mp->af, &fproto);
  if (rv < 0)
    goto error;

  if (mp->is_enable)
    {
      rv = svs_enable (fproto, ntohl (mp->table_id), ntohl (mp->sw_if_index));
    }
  else
    {
      rv =
	svs_disable (fproto, ntohl (mp->table_id), ntohl (mp->sw_if_index));
    }

  BAD_SW_IF_INDEX_LABEL;
error:
  REPLY_MACRO (VL_API_SVS_ENABLE_DISABLE_REPLY);
}

typedef struct svs_dump_walk_ctx_t_
{
  vl_api_registration_t *rp;
  u32 context;
} svs_dump_walk_ctx_t;


static walk_rc_t
svs_send_details (fib_protocol_t fproto,
		  u32 table_id, u32 sw_if_index, void *args)
{
  vl_api_svs_details_t *mp;
  svs_dump_walk_ctx_t *ctx;

  ctx = args;

  mp = vl_msg_api_alloc (sizeof (*mp));
  mp->_vl_msg_id = ntohs (VL_API_SVS_DETAILS + svs_base_msg_id);

  mp->context = ctx->context;
  mp->sw_if_index = htonl (sw_if_index);
  mp->table_id = htonl (table_id);
  mp->af = fib_proto_to_api_address_family (fproto);

  vl_api_send_msg (ctx->rp, (u8 *) mp);

  return (WALK_CONTINUE);
}

static void
vl_api_svs_dump_t_handler (vl_api_svs_dump_t * mp)
{
  vl_api_registration_t *rp;

  rp = vl_api_client_index_to_registration (mp->client_index);
  if (rp == 0)
    return;

  svs_dump_walk_ctx_t ctx = {
    .rp = rp,
    .context = mp->context,
  };

  svs_walk (svs_send_details, &ctx);
}

#include <svs/svs.api.c>
static clib_error_t *
svs_api_init (vlib_main_t * vm)
{
  /* Ask for a correctly-sized block of API message decode slots */
  svs_base_msg_id = setup_message_id_table ();

  return 0;
}

VLIB_INIT_FUNCTION (svs_api_init);

VLIB_PLUGIN_REGISTER () = {
  .version = VPP_BUILD_VER,
  .description = "Source Virtual Routing and Forwarding (VRF) Select",
};

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

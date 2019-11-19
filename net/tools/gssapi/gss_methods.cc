// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "net/tools/gssapi/gss_types.h"

// Only the GSSAPI exports used by //net are defined here and in
// gss_import_name.cc.

extern "C" GSS_EXPORT OM_uint32 gss_release_buffer(OM_uint32* minor_status,
                                                   gss_buffer_t buffer) {
  *minor_status = 0;
  return 0;
}

extern "C" GSS_EXPORT OM_uint32
gss_display_name(OM_uint32* minor_status,
                 const gss_name_t input_name,
                 gss_buffer_t output_name_buffer,
                 gss_OID* output_name_type) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32 gss_display_status(OM_uint32* minor_status,
                                                   OM_uint32 status_value,
                                                   int status_type,
                                                   const gss_OID mech_type,
                                                   OM_uint32* message_contex,
                                                   gss_buffer_t status_string) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32
gss_init_sec_context(OM_uint32* minor_status,
                     const gss_cred_id_t initiator_cred_handle,
                     gss_ctx_id_t* context_handle,
                     const gss_name_t target_name,
                     const gss_OID mech_type,
                     OM_uint32 req_flags,
                     OM_uint32 time_req,
                     const gss_channel_bindings_t input_chan_bindings,
                     const gss_buffer_t input_token,
                     gss_OID* actual_mech_type,
                     gss_buffer_t output_token,
                     OM_uint32* ret_flags,
                     OM_uint32* time_rec) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32
gss_wrap_size_limit(OM_uint32* minor_status,
                    const gss_ctx_id_t context_handle,
                    int conf_req_flag,
                    gss_qop_t qop_req,
                    OM_uint32 req_output_size,
                    OM_uint32* max_input_size) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32
gss_delete_sec_context(OM_uint32* minor_status,
                       gss_ctx_id_t* context_handle,
                       gss_buffer_t output_token) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32
gss_inquire_context(OM_uint32* minor_status,
                    const gss_ctx_id_t context_handle,
                    gss_name_t* src_name,
                    gss_name_t* targ_name,
                    OM_uint32* lifetime_rec,
                    gss_OID* mech_type,
                    OM_uint32* ctx_flags,
                    int* locally_initiated,
                    int* open) {
  return 0;
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_vpn_provider.idl modified Fri May  6 20:38:30 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_vpn_provider_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_VpnProvider::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVpnProvider(instance);
}

PP_Bool IsVpnProvider(PP_Resource resource) {
  VLOG(4) << "PPB_VpnProvider::IsVpnProvider()";
  EnterResource<PPB_VpnProvider_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Bind(PP_Resource vpn_provider,
             struct PP_Var configuration_id,
             struct PP_Var configuration_name,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VpnProvider::Bind()";
  EnterResource<PPB_VpnProvider_API> enter(vpn_provider, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Bind(
      configuration_id, configuration_name, enter.callback()));
}

int32_t SendPacket(PP_Resource vpn_provider,
                   struct PP_Var packet,
                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VpnProvider::SendPacket()";
  EnterResource<PPB_VpnProvider_API> enter(vpn_provider, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SendPacket(packet, enter.callback()));
}

int32_t ReceivePacket(PP_Resource vpn_provider,
                      struct PP_Var* packet,
                      struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VpnProvider::ReceivePacket()";
  EnterResource<PPB_VpnProvider_API> enter(vpn_provider, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->ReceivePacket(packet, enter.callback()));
}

const PPB_VpnProvider_0_1 g_ppb_vpnprovider_thunk_0_1 = {
    &Create, &IsVpnProvider, &Bind, &SendPacket, &ReceivePacket};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_VpnProvider_0_1* GetPPB_VpnProvider_0_1_Thunk() {
  return &g_ppb_vpnprovider_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi

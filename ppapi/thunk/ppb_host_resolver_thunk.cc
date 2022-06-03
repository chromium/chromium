// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_host_resolver.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_host_resolver_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_HostResolver::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateHostResolver(instance);
}

PP_Bool IsHostResolver(PP_Resource resource) {
  VLOG(4) << "PPB_HostResolver::IsHostResolver()";
  EnterResource<PPB_HostResolver_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Resolve(PP_Resource host_resolver,
                const char* host,
                uint16_t port,
                const struct PP_HostResolver_Hint* hint,
                struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_HostResolver::Resolve()";
  EnterResource<PPB_HostResolver_API> enter(host_resolver, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Resolve(host, port, hint, enter.callback()));
}

struct PP_Var GetCanonicalName(PP_Resource host_resolver) {
  VLOG(4) << "PPB_HostResolver::GetCanonicalName()";
  EnterResource<PPB_HostResolver_API> enter(host_resolver, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetCanonicalName();
}

uint32_t GetNetAddressCount(PP_Resource host_resolver) {
  VLOG(4) << "PPB_HostResolver::GetNetAddressCount()";
  EnterResource<PPB_HostResolver_API> enter(host_resolver, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNetAddressCount();
}

PP_Resource GetNetAddress(PP_Resource host_resolver, uint32_t index) {
  VLOG(4) << "PPB_HostResolver::GetNetAddress()";
  EnterResource<PPB_HostResolver_API> enter(host_resolver, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNetAddress(index);
}

const PPB_HostResolver_1_0 g_ppb_hostresolver_thunk_1_0 = {
    &Create,           &IsHostResolver,     &Resolve,
    &GetCanonicalName, &GetNetAddressCount, &GetNetAddress};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_HostResolver_1_0* GetPPB_HostResolver_1_0_Thunk() {
  return &g_ppb_hostresolver_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi

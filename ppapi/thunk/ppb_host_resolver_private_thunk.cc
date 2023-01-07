// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_host_resolver_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_HostResolver_Private_API> EnterHostResolver;

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateHostResolverPrivate(instance);
}

PP_Bool IsHostResolver(PP_Resource resource) {
  EnterHostResolver enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Resolve(PP_Resource host_resolver,
                const char* host,
                uint16_t port,
                const PP_HostResolver_Private_Hint* hint,
                PP_CompletionCallback callback) {
  EnterHostResolver enter(host_resolver, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Resolve(host, port, hint,
                                                 enter.callback()));
}

PP_Var GetCanonicalName(PP_Resource host_resolver) {
  EnterHostResolver enter(host_resolver, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetCanonicalName();
}

uint32_t GetSize(PP_Resource host_resolver) {
  EnterHostResolver enter(host_resolver, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetSize();
}

PP_Bool GetNetAddress(PP_Resource host_resolver,
                      uint32_t index,
                      PP_NetAddress_Private* addr) {
  EnterHostResolver enter(host_resolver, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->GetNetAddress(index, addr));
}

const PPB_HostResolver_Private g_ppb_host_resolver_thunk = {
  &Create,
  &IsHostResolver,
  &Resolve,
  &GetCanonicalName,
  &GetSize,
  &GetNetAddress
};

}  // namespace

const PPB_HostResolver_Private_0_1* GetPPB_HostResolver_Private_0_1_Thunk() {
  return &g_ppb_host_resolver_thunk;
}

}  // namespace thunk
}  // namespace ppapi

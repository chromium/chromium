// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/host_resolver_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/pass_ref.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_HostResolver_Private_0_1>() {
  return PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1;
}

}  // namespace

HostResolverPrivate::HostResolverPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_HostResolver_Private_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_HostResolver_Private_0_1>()->Create(
        instance.pp_instance()));
  }
}

// static
bool HostResolverPrivate::IsAvailable() {
  return has_interface<PPB_HostResolver_Private_0_1>();
}

int32_t HostResolverPrivate::Resolve(const std::string& host,
                                     uint16_t port,
                                     const PP_HostResolver_Private_Hint& hint,
                                     const CompletionCallback& callback) {
  if (!has_interface<PPB_HostResolver_Private_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_HostResolver_Private_0_1>()->Resolve(
      pp_resource(),
      host.c_str(),
      port,
      &hint,
      callback.pp_completion_callback());
}

Var HostResolverPrivate::GetCanonicalName() {
  if (!has_interface<PPB_HostResolver_Private_0_1>())
    return Var(Var::Null());

  PP_Var pp_canonical_name =
      get_interface<PPB_HostResolver_Private_0_1>()->GetCanonicalName(
          pp_resource());
  return Var(PASS_REF, pp_canonical_name);
}

uint32_t HostResolverPrivate::GetSize() {
  if (!has_interface<PPB_HostResolver_Private_0_1>())
    return 0;
  return get_interface<PPB_HostResolver_Private_0_1>()->GetSize(pp_resource());
}

bool HostResolverPrivate::GetNetAddress(uint32_t index,
                                        PP_NetAddress_Private* address) {
  if (!has_interface<PPB_HostResolver_Private_0_1>())
    return false;
  PP_Bool result = get_interface<PPB_HostResolver_Private_0_1>()->GetNetAddress(
      pp_resource(), index, address);
  return PP_ToBool(result);
}

}  // namespace pp

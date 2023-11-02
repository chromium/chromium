// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/host_resolver.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_HostResolver_1_0>() {
  return PPB_HOSTRESOLVER_INTERFACE_1_0;
}

}  // namespace

HostResolver::HostResolver() {
}

HostResolver::HostResolver(const InstanceHandle& instance) {
  if (has_interface<PPB_HostResolver_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_HostResolver_1_0>()->Create(
        instance.pp_instance()));
  }
}

HostResolver::HostResolver(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

HostResolver::HostResolver(const HostResolver& other) : Resource(other) {
}

HostResolver::~HostResolver() {
}

HostResolver& HostResolver::operator=(const HostResolver& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool HostResolver::IsAvailable() {
  return has_interface<PPB_HostResolver_1_0>();
}

int32_t HostResolver::Resolve(const char* host,
                              uint16_t port,
                              const PP_HostResolver_Hint& hint,
                              const CompletionCallback& callback) {
  if (has_interface<PPB_HostResolver_1_0>()) {
    return get_interface<PPB_HostResolver_1_0>()->Resolve(
        pp_resource(), host, port, &hint, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

Var HostResolver::GetCanonicalName() const {
  if (has_interface<PPB_HostResolver_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_HostResolver_1_0>()->GetCanonicalName(
                   pp_resource()));
  }

  return Var();
}

uint32_t HostResolver::GetNetAddressCount() const {
  if (has_interface<PPB_HostResolver_1_0>()) {
    return get_interface<PPB_HostResolver_1_0>()->GetNetAddressCount(
        pp_resource());
  }

  return 0;
}

NetAddress HostResolver::GetNetAddress(uint32_t index) const {
  if (has_interface<PPB_HostResolver_1_0>()) {
    return NetAddress(PASS_REF,
                      get_interface<PPB_HostResolver_1_0>()->GetNetAddress(
                          pp_resource(), index));
  }

  return NetAddress();
}

}  // namespace pp

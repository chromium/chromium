// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/vpn_provider.h"

#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var_array.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_VpnProvider_0_1>() {
  return PPB_VPNPROVIDER_INTERFACE_0_1;
}
}  // namespace

VpnProvider::VpnProvider(const InstanceHandle& instance)
    : associated_instance_(instance) {
  if (has_interface<PPB_VpnProvider_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_VpnProvider_0_1>()->Create(
        associated_instance_.pp_instance()));
  }
}

VpnProvider::~VpnProvider() {}

// static
bool VpnProvider::IsAvailable() {
  return has_interface<PPB_VpnProvider_0_1>();
}

int32_t VpnProvider::Bind(const Var& configuration_id,
                          const Var& configuration_name,
                          const CompletionCallback& callback) {
  if (has_interface<PPB_VpnProvider_0_1>()) {
    return get_interface<PPB_VpnProvider_0_1>()->Bind(
        pp_resource(), configuration_id.pp_var(), configuration_name.pp_var(),
        callback.pp_completion_callback());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VpnProvider::SendPacket(const Var& packet,
                                const CompletionCallback& callback) {
  if (has_interface<PPB_VpnProvider_0_1>()) {
    return get_interface<PPB_VpnProvider_0_1>()->SendPacket(
        pp_resource(), packet.pp_var(), callback.pp_completion_callback());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t VpnProvider::ReceivePacket(
    const CompletionCallbackWithOutput<Var>& callback) {
  if (has_interface<PPB_VpnProvider_0_1>()) {
    return get_interface<PPB_VpnProvider_0_1>()->ReceivePacket(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return PP_ERROR_NOINTERFACE;
}

}  // namespace pp

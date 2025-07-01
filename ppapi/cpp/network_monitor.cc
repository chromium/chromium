// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/network_monitor.h"

#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/network_list.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkMonitor_1_0>() {
  return PPB_NETWORKMONITOR_INTERFACE_1_0;
}

}  // namespace

NetworkMonitor::NetworkMonitor(const InstanceHandle& instance) {
  if (has_interface<PPB_NetworkMonitor_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_NetworkMonitor_1_0>()->Create(
        instance.pp_instance()));
  }
}

int32_t NetworkMonitor::UpdateNetworkList(
    const CompletionCallbackWithOutput<NetworkList>& callback) {
  if (has_interface<PPB_NetworkMonitor_1_0>()) {
    return get_interface<PPB_NetworkMonitor_1_0>()->UpdateNetworkList(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

// static
bool NetworkMonitor::IsAvailable() {
  return has_interface<PPB_NetworkMonitor_1_0>();
}

}  // namespace pp

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/component_context.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_interfaces_fuchsia.h"

namespace net {
namespace {

using ConnectionType = NetworkChangeNotifier::ConnectionType;

// Adapts a base::RepeatingCallback to a std::function object.
// Useful when binding callbacks to asynchronous FIDL calls, because
// it allows the caller to reference in-scope move-only objects as well as use
// Chromium's ownership signifiers such as base::Passed, base::Unretained, etc.
//
// Note that the function takes a RepeatingCallback because it is copyable, but
// in practice the callback will only be executed once by the FIDL system.
template <typename R, typename... Args>
std::function<R(Args...)> WrapCallbackAsFunction(
    base::RepeatingCallback<R(Args...)> callback) {
  return
      [callback](Args... args) { callback.Run(std::forward<Args>(args)...); };
}

}  // namespace

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia()
    : NetworkChangeNotifierFuchsia(
          base::fuchsia::ComponentContext::GetDefault()
              ->ConnectToService<fuchsia::netstack::Netstack>()) {}

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(
    fuchsia::netstack::NetstackPtr netstack)
    : netstack_(std::move(netstack)) {
  DCHECK(netstack_);

  netstack_.set_error_handler(
      []() { LOG(ERROR) << "Lost connection to netstack."; });
  netstack_.events().OnInterfacesChanged =
      [this](fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces) {
        ProcessInterfaceList(base::OnceClosure(), std::move(interfaces));
      };

  // Fetch the interface list synchronously, so that an initial ConnectionType
  // is available before we return.
  base::RunLoop wait_for_interfaces;
  netstack_->GetInterfaces([
    this, quit_closure = wait_for_interfaces.QuitClosure()
  ](fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces) {
    ProcessInterfaceList(quit_closure, std::move(interfaces));
  });
  wait_for_interfaces.Run();
}

NetworkChangeNotifierFuchsia::~NetworkChangeNotifierFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierFuchsia::GetCurrentConnectionType() const {
  ConnectionType type = static_cast<ConnectionType>(
      base::subtle::Acquire_Load(&cached_connection_type_));
  return type;
}

void NetworkChangeNotifierFuchsia::ProcessInterfaceList(
    base::OnceClosure on_initialized_cb,
    fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces) {
  netstack_->GetRouteTable(WrapCallbackAsFunction(base::BindRepeating(
      &NetworkChangeNotifierFuchsia::OnRouteTableReceived,
      base::Unretained(this), base::Passed(std::move(on_initialized_cb)),
      base::Passed(std::move(interfaces)))));
}

void NetworkChangeNotifierFuchsia::OnRouteTableReceived(
    base::OnceClosure on_initialized_cb,
    fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces,
    fidl::VectorPtr<fuchsia::netstack::RouteTableEntry> route_table) {
  // Find the default interface in the routing table.
  auto default_route_interface = std::find_if(
      route_table->begin(), route_table->end(),
      [](const fuchsia::netstack::RouteTableEntry& rt) {
        return MaskPrefixLength(internal::NetAddressToIPAddress(rt.netmask)) ==
               0;
      });

  // Find the default interface in the NetInterface list.
  const fuchsia::netstack::NetInterface* default_interface = nullptr;
  if (default_route_interface != route_table->end()) {
    for (const auto& cur_interface : *interfaces) {
      if (cur_interface.id == default_route_interface->nicid) {
        default_interface = &cur_interface;
      }
    }
  }

  base::flat_set<IPAddress> addresses;
  std::string ssid;
  ConnectionType connection_type = CONNECTION_NONE;
  if (default_interface) {
    std::vector<NetworkInterface> flattened_interfaces =
        internal::NetInterfaceToNetworkInterfaces(*default_interface);
    std::transform(
        flattened_interfaces.begin(), flattened_interfaces.end(),
        std::inserter(addresses, addresses.begin()),
        [](const NetworkInterface& interface) { return interface.address; });
    if (!flattened_interfaces.empty()) {
      connection_type = flattened_interfaces.front().type;
    }

    // TODO(https://crbug.com/848355): Treat SSID changes as IP address changes.
  }

  bool connection_type_changed = false;
  if (connection_type != cached_connection_type_) {
    base::subtle::Release_Store(&cached_connection_type_, connection_type);
    connection_type_changed = true;
  }

  if (addresses != cached_addresses_) {
    std::swap(cached_addresses_, addresses);
    if (on_initialized_cb.is_null()) {
      NotifyObserversOfIPAddressChange();
    }
    connection_type_changed = true;
  }

  if (on_initialized_cb.is_null() && connection_type_changed) {
    NotifyObserversOfConnectionTypeChange();
  }

  if (!on_initialized_cb.is_null()) {
    std::move(on_initialized_cb).Run();
  }
}

}  // namespace net

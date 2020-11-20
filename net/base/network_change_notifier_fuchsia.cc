// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <lib/async-loop/cpp/loop.h>
#include <lib/sys/cpp/component_context.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_interfaces_fuchsia.h"

namespace net {

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(
    fuchsia::hardware::ethernet::Features required_features)
    : NetworkChangeNotifierFuchsia(base::ComponentContextForProcess()
                                       ->svc()
                                       ->Connect<fuchsia::netstack::Netstack>(),
                                   required_features) {}

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(
    fidl::InterfaceHandle<fuchsia::netstack::Netstack> netstack,
    fuchsia::hardware::ethernet::Features required_features,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                            system_dns_config_notifier),
      required_features_(required_features) {
  DCHECK(netstack);

  netstack_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "Lost connection to netstack.";
  });

  netstack_.events().OnInterfacesChanged = fit::bind_member(
      this, &NetworkChangeNotifierFuchsia::ProcessInterfaceList);

  // Temporarily bind to a local dispatcher so we can synchronously wait for the
  // synthetic event to populate the initial state.
  async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);
  zx_status_t status = netstack_.Bind(std::move(netstack), loop.dispatcher());
  ZX_CHECK(status == ZX_OK, status) << "Bind()";
  on_initial_interfaces_received_ =
      base::BindOnce(&async::Loop::Quit, base::Unretained(&loop));
  status = loop.Run();
  ZX_CHECK(status == ZX_ERR_CANCELED, status) << "loop.Run()";

  // Bind to the dispatcher for the thread's MessagePump.
  //
  // Note this must be done before |loop| is destroyed, since that would close
  // the interface handle underlying |netstack_|.
  status = netstack_.Bind(netstack_.Unbind());
  ZX_CHECK(status == ZX_OK, status) << "Bind()";
}

NetworkChangeNotifierFuchsia::~NetworkChangeNotifierFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ClearGlobalPointer();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierFuchsia::GetCurrentConnectionType() const {
  ConnectionType type = static_cast<ConnectionType>(
      base::subtle::Acquire_Load(&cached_connection_type_));
  return type;
}

void NetworkChangeNotifierFuchsia::ProcessInterfaceList(
    std::vector<fuchsia::netstack::NetInterface> interfaces) {
  netstack_->GetRouteTable(
      [this, interfaces = std::move(interfaces)](
          std::vector<fuchsia::netstack::RouteTableEntry> route_table) mutable {
        OnRouteTableReceived(std::move(interfaces), std::move(route_table));
      });
}

void NetworkChangeNotifierFuchsia::OnRouteTableReceived(
    std::vector<fuchsia::netstack::NetInterface> interfaces,
    std::vector<fuchsia::netstack::RouteTableEntry> route_table) {
  // Create a set of NICs that have default routes (ie 0.0.0.0).
  base::flat_set<uint32_t> default_route_ids;
  for (const auto& route : route_table) {
    if (MaskPrefixLength(
            internal::FuchsiaIpAddressToIPAddress(route.netmask)) == 0) {
      default_route_ids.insert(route.nicid);
    }
  }

  ConnectionType connection_type = CONNECTION_NONE;
  base::flat_set<IPAddress> addresses;
  for (auto& interface : interfaces) {
    // Filter out loopback and invalid connection types.
    if ((internal::ConvertConnectionType(interface) ==
         NetworkChangeNotifier::CONNECTION_NONE) ||
        (interface.features &
         fuchsia::hardware::ethernet::Features::LOOPBACK) ==
            fuchsia::hardware::ethernet::Features::LOOPBACK) {
      continue;
    }

    // Filter out interfaces that do not meet the |required_features_|.
    if ((interface.features & required_features_) != required_features_) {
      continue;
    }

    // Filter out interfaces with non-default routes.
    if (!default_route_ids.contains(interface.id)) {
      continue;
    }

    std::vector<NetworkInterface> flattened_interfaces =
        internal::NetInterfaceToNetworkInterfaces(interface);
    if (flattened_interfaces.empty()) {
      continue;
    }

    // Add the addresses from this interface to the list of all addresses.
    std::transform(
        flattened_interfaces.begin(), flattened_interfaces.end(),
        std::inserter(addresses, addresses.begin()),
        [](const NetworkInterface& interface) { return interface.address; });

    // Set the default connection to the first interface connection found.
    if (connection_type == CONNECTION_NONE) {
      connection_type = flattened_interfaces.front().type;
    }
  }

  if (addresses != cached_addresses_) {
    std::swap(cached_addresses_, addresses);
    NotifyObserversOfIPAddressChange();
  }

  if (connection_type != cached_connection_type_) {
    base::subtle::Release_Store(&cached_connection_type_, connection_type);
    NotifyObserversOfConnectionTypeChange();
  }

  if (on_initial_interfaces_received_) {
    std::move(on_initial_interfaces_received_).Run();
  }
}

}  // namespace net

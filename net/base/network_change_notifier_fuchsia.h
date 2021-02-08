// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

#include <fuchsia/netstack/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <vector>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"

namespace net {

class NET_EXPORT_PRIVATE NetworkChangeNotifierFuchsia
    : public NetworkChangeNotifier {
 public:
  // Registers for asynchronous notifications of changes to network interfaces.
  // Interfaces are filtered by |required_features|.
  explicit NetworkChangeNotifierFuchsia(
      fuchsia::hardware::ethernet::Features required_features);
  NetworkChangeNotifierFuchsia(const NetworkChangeNotifierFuchsia&) = delete;
  NetworkChangeNotifierFuchsia& operator=(const NetworkChangeNotifierFuchsia&) =
      delete;
  ~NetworkChangeNotifierFuchsia() override;

  // NetworkChangeNotifier implementation.
  ConnectionType GetCurrentConnectionType() const override;

 private:
  friend class NetworkChangeNotifierFuchsiaTest;

  // For testing purposes. Receives a |netstack| pointer for easy mocking.
  // Interfaces are filtered by |required_features|.
  NetworkChangeNotifierFuchsia(
      fidl::InterfaceHandle<fuchsia::netstack::Netstack> netstack,
      fuchsia::hardware::ethernet::Features required_features,
      SystemDnsConfigChangeNotifier* system_dns_config_notifier = nullptr);

  // Forwards the network interface list along with the result of
  // GetRouteTable() to OnRouteTableReceived().
  void ProcessInterfaceList(
      std::vector<fuchsia::netstack::NetInterface> interfaces);

  // Computes network change notification state change from the list of
  // interfaces and routing table data, sending observer events if IP or
  // connection type changes are detected.
  void OnRouteTableReceived(
      std::vector<fuchsia::netstack::NetInterface> interfaces,
      std::vector<fuchsia::netstack::RouteTableEntry> table);

  // Required features for an interface to be taken into account.
  const fuchsia::hardware::ethernet::Features required_features_;

  fuchsia::netstack::NetstackPtr netstack_;

  // Used to allow the constructor to block until the initial state is received
  // from |netstack_|.
  int pending_route_table_requests_ = 0;
  base::OnceClosure on_initial_interfaces_received_;

  // The ConnectionType of the default network interface, stored as an atomic
  // 32-bit int for safe concurrent access.
  base::subtle::Atomic32 cached_connection_type_ = CONNECTION_UNKNOWN;

  // Set of addresses from the previous query/update for the default interface.
  base::flat_set<IPAddress> cached_addresses_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

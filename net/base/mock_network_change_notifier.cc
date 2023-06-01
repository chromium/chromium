// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_network_change_notifier.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"

namespace net::test {

// static
std::unique_ptr<MockNetworkChangeNotifier> MockNetworkChangeNotifier::Create() {
  // Use an empty noop SystemDnsConfigChangeNotifier to disable actual system
  // DNS configuration notifications.
  return base::WrapUnique(new MockNetworkChangeNotifier(
      std::make_unique<SystemDnsConfigChangeNotifier>(
          nullptr /* task_runner */, nullptr /* dns_config_service */)));
}

MockNetworkChangeNotifier::~MockNetworkChangeNotifier() {
  StopSystemDnsConfigNotifier();
}

MockNetworkChangeNotifier::ConnectionType
MockNetworkChangeNotifier::GetCurrentConnectionType() const {
  return connection_type_;
}

void MockNetworkChangeNotifier::ForceNetworkHandlesSupported() {
  force_network_handles_supported_ = true;
}

bool MockNetworkChangeNotifier::AreNetworkHandlesCurrentlySupported() const {
  return force_network_handles_supported_;
}

void MockNetworkChangeNotifier::SetConnectedNetworksList(
    const NetworkList& network_list) {
  connected_networks_ = network_list;
}

void MockNetworkChangeNotifier::GetCurrentConnectedNetworks(
    NetworkList* network_list) const {
  network_list->clear();
  *network_list = connected_networks_;
}

void MockNetworkChangeNotifier::NotifyNetworkMadeDefault(
    handles::NetworkHandle network) {
  QueueNetworkMadeDefault(network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

void MockNetworkChangeNotifier::QueueNetworkMadeDefault(
    handles::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::NetworkChangeType::kMadeDefault, network);
}

void MockNetworkChangeNotifier::NotifyNetworkDisconnected(
    handles::NetworkHandle network) {
  QueueNetworkDisconnected(network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

void MockNetworkChangeNotifier::QueueNetworkDisconnected(
    handles::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::NetworkChangeType::kDisconnected, network);
}

void MockNetworkChangeNotifier::NotifyNetworkConnected(
    handles::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::NetworkChangeType::kConnected, network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

bool MockNetworkChangeNotifier::IsDefaultNetworkActiveInternal() {
  return is_default_network_active_;
}

void MockNetworkChangeNotifier::SetConnectionTypeAndNotifyObservers(
    ConnectionType connection_type) {
  SetConnectionType(connection_type);
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

MockNetworkChangeNotifier::ConnectionCost
MockNetworkChangeNotifier::GetCurrentConnectionCost() {
  if (use_default_connection_cost_implementation_)
    return NetworkChangeNotifier::GetCurrentConnectionCost();
  return connection_cost_;
}

#if BUILDFLAG(IS_LINUX)
AddressMapOwnerLinux* MockNetworkChangeNotifier::GetAddressMapOwnerInternal() {
  return address_map_owner_;
}
#endif  // BUILDFLAG(IS_LINUX)

MockNetworkChangeNotifier::MockNetworkChangeNotifier(
    std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                            dns_config_notifier.get()),
      dns_config_notifier_(std::move(dns_config_notifier)) {}

ScopedMockNetworkChangeNotifier::ScopedMockNetworkChangeNotifier()
    : disable_network_change_notifier_for_tests_(
          std::make_unique<NetworkChangeNotifier::DisableForTest>()),
      mock_network_change_notifier_(MockNetworkChangeNotifier::Create()) {}

ScopedMockNetworkChangeNotifier::~ScopedMockNetworkChangeNotifier() = default;

MockNetworkChangeNotifier*
ScopedMockNetworkChangeNotifier::mock_network_change_notifier() {
  return mock_network_change_notifier_.get();
}

}  // namespace net::test

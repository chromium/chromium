// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_network_change_notifier.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"

namespace net {
namespace test {

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
    NetworkChangeNotifier::NetworkHandle network) {
  QueueNetworkMadeDefault(network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

void MockNetworkChangeNotifier::QueueNetworkMadeDefault(
    NetworkChangeNotifier::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::MADE_DEFAULT, network);
}

void MockNetworkChangeNotifier::NotifyNetworkDisconnected(
    NetworkChangeNotifier::NetworkHandle network) {
  QueueNetworkDisconnected(network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

void MockNetworkChangeNotifier::QueueNetworkDisconnected(
    NetworkChangeNotifier::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::DISCONNECTED, network);
}

void MockNetworkChangeNotifier::NotifyNetworkConnected(
    NetworkChangeNotifier::NetworkHandle network) {
  NetworkChangeNotifier::NotifyObserversOfSpecificNetworkChange(
      NetworkChangeNotifier::CONNECTED, network);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
}

MockNetworkChangeNotifier::MockNetworkChangeNotifier(
    std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                            dns_config_notifier.get()),
      force_network_handles_supported_(false),
      connection_type_(CONNECTION_UNKNOWN),
      dns_config_notifier_(std::move(dns_config_notifier)) {}

ScopedMockNetworkChangeNotifier::ScopedMockNetworkChangeNotifier()
    : disable_network_change_notifier_for_tests_(
          new NetworkChangeNotifier::DisableForTest()),
      mock_network_change_notifier_(MockNetworkChangeNotifier::Create()) {}

ScopedMockNetworkChangeNotifier::~ScopedMockNetworkChangeNotifier() = default;

MockNetworkChangeNotifier*
ScopedMockNetworkChangeNotifier::mock_network_change_notifier() {
  return mock_network_change_notifier_.get();
}

}  // namespace test
}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_

#include <memory>

#include "net/base/network_change_notifier.h"

namespace net {

class SystemDnsConfigChangeNotifier;

namespace test {

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  static std::unique_ptr<MockNetworkChangeNotifier> Create();
  ~MockNetworkChangeNotifier() override;

  ConnectionType GetCurrentConnectionType() const override;

  void ForceNetworkHandlesSupported();

  bool AreNetworkHandlesCurrentlySupported() const override;

  void SetConnectionType(ConnectionType connection_type) {
    connection_type_ = connection_type;
  }

  void SetConnectedNetworksList(const NetworkList& network_list);

  void GetCurrentConnectedNetworks(NetworkList* network_list) const override;

  // Delivers a MADE_DEFAULT notification to observers.
  void NotifyNetworkMadeDefault(NetworkChangeNotifier::NetworkHandle network);

  // Queues a MADE_DEFAULT notification to be delivered to observers
  // but does not spin the message loop to actually deliver it.
  void QueueNetworkMadeDefault(NetworkChangeNotifier::NetworkHandle network);

  // Delivers a DISCONNECTED notification to observers.
  void NotifyNetworkDisconnected(NetworkChangeNotifier::NetworkHandle network);

  // Queues a DISCONNECTED notification to be delivered to observers
  // but does not spin the message loop to actually deliver it.
  void QueueNetworkDisconnected(NetworkChangeNotifier::NetworkHandle network);

  // Delivers a CONNECTED notification to observers.
  void NotifyNetworkConnected(NetworkChangeNotifier::NetworkHandle network);

 private:
  // Create using MockNetworkChangeNotifier::Create().
  MockNetworkChangeNotifier(
      std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier);

  bool force_network_handles_supported_;
  ConnectionType connection_type_;
  NetworkChangeNotifier::NetworkList connected_networks_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
};

// Class to replace existing NetworkChangeNotifier singleton with a
// MockNetworkChangeNotifier for a test. To use, simply create a
// ScopedMockNetworkChangeNotifier object in the test.
class ScopedMockNetworkChangeNotifier {
 public:
  ScopedMockNetworkChangeNotifier();
  ~ScopedMockNetworkChangeNotifier();

  MockNetworkChangeNotifier* mock_network_change_notifier();

 private:
  std::unique_ptr<NetworkChangeNotifier::DisableForTest>
      disable_network_change_notifier_for_tests_;
  std::unique_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
};

}  // namespace test
}  // namespace net

#endif  // NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_

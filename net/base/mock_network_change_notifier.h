// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_

#include <memory>

#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"

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
  void NotifyNetworkMadeDefault(handles::NetworkHandle network);

  // Queues a MADE_DEFAULT notification to be delivered to observers
  // but does not spin the message loop to actually deliver it.
  void QueueNetworkMadeDefault(handles::NetworkHandle network);

  // Delivers a DISCONNECTED notification to observers.
  void NotifyNetworkDisconnected(handles::NetworkHandle network);

  // Queues a DISCONNECTED notification to be delivered to observers
  // but does not spin the message loop to actually deliver it.
  void QueueNetworkDisconnected(handles::NetworkHandle network);

  // Delivers a CONNECTED notification to observers.
  void NotifyNetworkConnected(handles::NetworkHandle network);

  void SetConnectionTypeAndNotifyObservers(ConnectionType connection_type);

  // Sets the cached value of the connection cost. If
  // use_default_connection_cost_implementation is set to true, this value gets
  // ignored.
  void SetConnectionCost(ConnectionCost connection_cost) {
    connection_cost_ = connection_cost;
  }

  bool IsDefaultNetworkActiveInternal() override;

  void SetIsDefaultNetworkActiveInternalForTesting(bool is_active) {
    is_default_network_active_ = is_active;
  }

  // Tells this class to ignore its cached connection cost value and instead
  // call the base class's implementation. This is intended to allow tests to
  // mock the product code's fallback to the default implementation in certain
  // situations. For example, the APIs to support this functionality exist on
  // Win10 only so it falls back to the default on Win7, so this function allows
  // tests to validate the default implementation's behavior on Win10 machines.
  void SetUseDefaultConnectionCostImplementation(
      bool use_default_connection_cost_implementation) {
    use_default_connection_cost_implementation_ =
        use_default_connection_cost_implementation;
  }

  // Returns either the cached connection cost value or the default
  // implementation's result, depending on whether
  // use_default_connection_cost_implementation is set to true.
  ConnectionCost GetCurrentConnectionCost() override;

#if BUILDFLAG(IS_LINUX)
  void SetAddressMapOwnerLinux(AddressMapOwnerLinux* address_map_owner) {
    address_map_owner_ = address_map_owner;
  }

  AddressMapOwnerLinux* GetAddressMapOwnerInternal() override;
#endif

 private:
  // Create using MockNetworkChangeNotifier::Create().
  explicit MockNetworkChangeNotifier(
      std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier);

  bool force_network_handles_supported_ = false;
  bool is_default_network_active_ = true;
  ConnectionType connection_type_ = CONNECTION_UNKNOWN;
  ConnectionCost connection_cost_ = CONNECTION_COST_UNKNOWN;
  bool use_default_connection_cost_implementation_ = false;
  NetworkChangeNotifier::NetworkList connected_networks_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
#if BUILDFLAG(IS_LINUX)
  raw_ptr<AddressMapOwnerLinux> address_map_owner_ = nullptr;
#endif
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

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See network_change_notifier_android.h for design explanations.

#include "net/android/network_change_notifier_android.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Types of network changes. See similarly named functions in
// NetworkChangeNotifier::NetworkObserver for descriptions.
enum ChangeType {
  NONE,
  CONNECTED,
  SOON_TO_DISCONNECT,
  DISCONNECTED,
  MADE_DEFAULT,
};

class NetworkChangeNotifierDelegateAndroidObserver
    : public NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  typedef NetworkChangeNotifier::ConnectionCost ConnectionCost;
  typedef NetworkChangeNotifier::ConnectionType ConnectionType;
  typedef NetworkChangeNotifier::NetworkList NetworkList;

  NetworkChangeNotifierDelegateAndroidObserver() = default;

  // NetworkChangeNotifierDelegateAndroid::Observer:
  void OnConnectionTypeChanged() override { type_notifications_count_++; }

  void OnConnectionCostChanged() override { cost_notifications_count_++; }

  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      net::NetworkChangeNotifier::ConnectionType type) override {
    max_bandwidth_notifications_count_++;
  }

  void OnNetworkConnected(handles::NetworkHandle network) override {}

  void OnNetworkSoonToDisconnect(handles::NetworkHandle network) override {}

  void OnNetworkDisconnected(handles::NetworkHandle network) override {}

  void OnNetworkMadeDefault(handles::NetworkHandle network) override {}

  void OnDefaultNetworkActive() override {
    default_network_active_notifications_count_++;
  }

  int type_notifications_count() const { return type_notifications_count_; }
  int cost_notifications_count() const { return cost_notifications_count_; }
  int bandwidth_notifications_count() const {
    return max_bandwidth_notifications_count_;
  }
  int default_network_active_notifications_count() const {
    return default_network_active_notifications_count_;
  }

 private:
  int type_notifications_count_ = 0;
  int cost_notifications_count_ = 0;
  int max_bandwidth_notifications_count_ = 0;
  int default_network_active_notifications_count_ = 0;
};

class NetworkChangeNotifierObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  NetworkChangeNotifierObserver() = default;

  // NetworkChangeNotifier::ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType connection_type) override {
    notifications_count_++;
  }

  int notifications_count() const {
    return notifications_count_;
  }

 private:
  int notifications_count_ = 0;
};

class NetworkChangeNotifierConnectionCostObserver
    : public NetworkChangeNotifier::ConnectionCostObserver {
 public:
  // NetworkChangeNotifier::ConnectionCostObserver:
  void OnConnectionCostChanged(
      NetworkChangeNotifier::ConnectionCost cost) override {
    notifications_count_++;
  }

  int notifications_count() const { return notifications_count_; }

 private:
  int notifications_count_ = 0;
};

class NetworkChangeNotifierMaxBandwidthObserver
    : public NetworkChangeNotifier::MaxBandwidthObserver {
 public:
  // NetworkChangeNotifier::MaxBandwidthObserver:
  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      NetworkChangeNotifier::ConnectionType type) override {
    notifications_count_++;
  }

  int notifications_count() const { return notifications_count_; }

 private:
  int notifications_count_ = 0;
};

// A NetworkObserver used for verifying correct notifications are sent.
class TestNetworkObserver : public NetworkChangeNotifier::NetworkObserver {
 public:
  TestNetworkObserver() { Clear(); }

  void ExpectChange(ChangeType change, handles::NetworkHandle network) {
    EXPECT_EQ(last_change_type_, change);
    EXPECT_EQ(last_network_changed_, network);
    Clear();
  }

 private:
  void Clear() {
    last_change_type_ = NONE;
    last_network_changed_ = handles::kInvalidNetworkHandle;
  }

  // NetworkChangeNotifier::NetworkObserver implementation:
  void OnNetworkConnected(handles::NetworkHandle network) override {
    ExpectChange(NONE, handles::kInvalidNetworkHandle);
    last_change_type_ = CONNECTED;
    last_network_changed_ = network;
  }
  void OnNetworkSoonToDisconnect(handles::NetworkHandle network) override {
    ExpectChange(NONE, handles::kInvalidNetworkHandle);
    last_change_type_ = SOON_TO_DISCONNECT;
    last_network_changed_ = network;
  }
  void OnNetworkDisconnected(handles::NetworkHandle network) override {
    ExpectChange(NONE, handles::kInvalidNetworkHandle);
    last_change_type_ = DISCONNECTED;
    last_network_changed_ = network;
  }
  void OnNetworkMadeDefault(handles::NetworkHandle network) override {
    // Cannot test for Clear()ed state as we receive CONNECTED immediately prior
    // to MADE_DEFAULT.
    last_change_type_ = MADE_DEFAULT;
    last_network_changed_ = network;
  }

  ChangeType last_change_type_;
  handles::NetworkHandle last_network_changed_;
};

}  // namespace

class BaseNetworkChangeNotifierAndroidTest : public TestWithTaskEnvironment {
 protected:
  typedef NetworkChangeNotifier::ConnectionType ConnectionType;
  typedef NetworkChangeNotifier::ConnectionCost ConnectionCost;
  typedef NetworkChangeNotifier::ConnectionSubtype ConnectionSubtype;

  ~BaseNetworkChangeNotifierAndroidTest() override = default;

  void RunTest(
      const base::RepeatingCallback<int(void)>& notifications_count_getter,
      const base::RepeatingCallback<ConnectionType(void)>&
          connection_type_getter) {
    EXPECT_EQ(0, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());

    // Changing from online to offline should trigger a notification.
    SetOffline();
    EXPECT_EQ(1, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    // No notification should be triggered when the offline state hasn't
    // changed.
    SetOffline();
    EXPECT_EQ(1, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    // Going from offline to online should trigger a notification.
    SetOnline();
    EXPECT_EQ(2, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());
  }

  void SetOnline(bool drain_run_loop = true) {
    delegate_.SetOnline();
    if (drain_run_loop) {
      // Note that this is needed because base::ObserverListThreadSafe uses
      // PostTask().
      base::RunLoop().RunUntilIdle();
    }
  }

  void SetOffline(bool drain_run_loop = true) {
    delegate_.SetOffline();
    if (drain_run_loop) {
      // See comment above.
      base::RunLoop().RunUntilIdle();
    }
  }

  void FakeConnectionCostChange(ConnectionCost cost) {
    delegate_.FakeConnectionCostChanged(cost);
    base::RunLoop().RunUntilIdle();
  }

  void FakeConnectionSubtypeChange(ConnectionSubtype subtype) {
    delegate_.FakeConnectionSubtypeChanged(subtype);
    base::RunLoop().RunUntilIdle();
  }

  void FakeNetworkChange(ChangeType change,
                         handles::NetworkHandle network,
                         ConnectionType type) {
    switch (change) {
      case CONNECTED:
        delegate_.FakeNetworkConnected(network, type);
        break;
      case SOON_TO_DISCONNECT:
        delegate_.FakeNetworkSoonToBeDisconnected(network);
        break;
      case DISCONNECTED:
        delegate_.FakeNetworkDisconnected(network);
        break;
      case MADE_DEFAULT:
        delegate_.FakeDefaultNetwork(network, type);
        break;
      case NONE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    // See comment above.
    base::RunLoop().RunUntilIdle();
  }

  void FakeDefaultNetworkActive() {
    delegate_.FakeDefaultNetworkActive();
    // See comment above.
    base::RunLoop().RunUntilIdle();
  }

  void FakePurgeActiveNetworkList(NetworkChangeNotifier::NetworkList networks) {
    delegate_.FakePurgeActiveNetworkList(networks);
    // See comment above.
    base::RunLoop().RunUntilIdle();
  }

  NetworkChangeNotifierDelegateAndroid delegate_;
};

// Tests that NetworkChangeNotifierDelegateAndroid is initialized with the
// actual connection type rather than a hardcoded one (e.g.
// CONNECTION_UNKNOWN). Initializing the connection type to CONNECTION_UNKNOWN
// and relying on the first network change notification to set it correctly can
// be problematic in case there is a long delay between the delegate's
// construction and the notification.
TEST_F(BaseNetworkChangeNotifierAndroidTest,
       DelegateIsInitializedWithCurrentConnectionType) {
  SetOffline();
  ASSERT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            delegate_.GetCurrentConnectionType());
  // Instantiate another delegate to validate that it uses the actual
  // connection type at construction.
  auto other_delegate =
      std::make_unique<NetworkChangeNotifierDelegateAndroid>();
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            other_delegate->GetCurrentConnectionType());

  // Toggle the global connectivity state and instantiate another delegate
  // again.
  SetOnline();
  ASSERT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            delegate_.GetCurrentConnectionType());
  other_delegate = std::make_unique<NetworkChangeNotifierDelegateAndroid>();
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            other_delegate->GetCurrentConnectionType());
}

class NetworkChangeNotifierAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  NetworkChangeNotifierAndroidTest() : notifier_(&delegate_) {
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &connection_type_observer_);
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &other_connection_type_observer_);
    NetworkChangeNotifier::AddConnectionCostObserver(
        &connection_cost_observer_);
    NetworkChangeNotifier::AddMaxBandwidthObserver(&max_bandwidth_observer_);
  }

  void ForceNetworkHandlesSupportedForTesting() {
    notifier_.ForceNetworkHandlesSupportedForTesting();
  }

  NetworkChangeNotifierObserver connection_type_observer_;
  NetworkChangeNotifierConnectionCostObserver connection_cost_observer_;
  NetworkChangeNotifierMaxBandwidthObserver max_bandwidth_observer_;
  NetworkChangeNotifierObserver other_connection_type_observer_;
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  NetworkChangeNotifierAndroid notifier_;
};

class NetworkChangeNotifierDelegateAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  NetworkChangeNotifierDelegateAndroidTest() {
    delegate_.RegisterObserver(&delegate_observer_);
  }

  ~NetworkChangeNotifierDelegateAndroidTest() override {
    delegate_.UnregisterObserver(&delegate_observer_);
  }

  NetworkChangeNotifierDelegateAndroidObserver delegate_observer_;
};

// Tests that the NetworkChangeNotifierDelegateAndroid's observer is notified.
// A testing-only observer is used here for testing. In production the
// delegate's observers are instances of NetworkChangeNotifierAndroid.
TEST_F(NetworkChangeNotifierDelegateAndroidTest, DelegateObserverNotified) {
  RunTest(base::BindRepeating(&NetworkChangeNotifierDelegateAndroidObserver::
                                  type_notifications_count,
                              base::Unretained(&delegate_observer_)),
          base::BindRepeating(
              &NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionType,
              base::Unretained(&delegate_)));
}

// When a NetworkChangeNotifierAndroid is observing a
// NetworkChangeNotifierDelegateAndroid for network state changes, and the
// NetworkChangeNotifierDelegateAndroid's connectivity state changes, the
// NetworkChangeNotifierAndroid should reflect that state.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToNetworkChangeNotifierAndroid) {
  RunTest(
      base::BindRepeating(&NetworkChangeNotifierObserver::notifications_count,
                          base::Unretained(&connection_type_observer_)),
      base::BindRepeating(
          &NetworkChangeNotifierAndroid::GetCurrentConnectionType,
          base::Unretained(&notifier_)));
}

// When a NetworkChangeNotifierAndroid's connection state changes, it should
// notify all of its observers.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToClientsOfNetworkChangeNotifier) {
  RunTest(
      base::BindRepeating(&NetworkChangeNotifierObserver::notifications_count,
                          base::Unretained(&connection_type_observer_)),
      base::BindRepeating(&NetworkChangeNotifier::GetConnectionType));
  // Check that *all* the observers are notified.
  EXPECT_EQ(connection_type_observer_.notifications_count(),
            other_connection_type_observer_.notifications_count());
}

TEST_F(NetworkChangeNotifierAndroidTest, ConnectionCost) {
  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_UNMETERED);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_COST_UNMETERED,
            notifier_.GetConnectionCost());
  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_METERED);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_COST_METERED,
            notifier_.GetConnectionCost());
}

TEST_F(NetworkChangeNotifierAndroidTest, ConnectionCostCallbackNotifier) {
  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_UNMETERED);
  EXPECT_EQ(1, connection_cost_observer_.notifications_count());

  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_METERED);
  EXPECT_EQ(2, connection_cost_observer_.notifications_count());
}

TEST_F(NetworkChangeNotifierDelegateAndroidTest,
       ConnectionCostCallbackNotifier) {
  EXPECT_EQ(0, delegate_observer_.cost_notifications_count());

  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_UNMETERED);
  EXPECT_EQ(1, delegate_observer_.cost_notifications_count());

  FakeConnectionCostChange(ConnectionCost::CONNECTION_COST_METERED);
  EXPECT_EQ(2, delegate_observer_.cost_notifications_count());
}

TEST_F(NetworkChangeNotifierAndroidTest, MaxBandwidth) {
  SetOnline();
  double max_bandwidth_mbps = 0.0;
  NetworkChangeNotifier::ConnectionType connection_type =
      NetworkChangeNotifier::CONNECTION_NONE;
  notifier_.GetMaxBandwidthAndConnectionType(&max_bandwidth_mbps,
                                             &connection_type);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN, connection_type);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), max_bandwidth_mbps);
  SetOffline();
  notifier_.GetMaxBandwidthAndConnectionType(&max_bandwidth_mbps,
                                             &connection_type);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE, connection_type);
  EXPECT_EQ(0.0, max_bandwidth_mbps);
}

TEST_F(NetworkChangeNotifierAndroidTest, MaxBandwidthCallbackNotifier) {
  // The bandwidth notification should always be forwarded, even if the value
  // doesn't change (because the type might have changed).
  FakeConnectionSubtypeChange(ConnectionSubtype::SUBTYPE_CDMA);
  EXPECT_EQ(1, max_bandwidth_observer_.notifications_count());

  FakeConnectionSubtypeChange(ConnectionSubtype::SUBTYPE_CDMA);
  EXPECT_EQ(2, max_bandwidth_observer_.notifications_count());

  FakeConnectionSubtypeChange(ConnectionSubtype::SUBTYPE_LTE);
  EXPECT_EQ(3, max_bandwidth_observer_.notifications_count());
}

TEST_F(NetworkChangeNotifierDelegateAndroidTest,
       MaxBandwidthNotifiedOnConnectionChange) {
  EXPECT_EQ(0, delegate_observer_.bandwidth_notifications_count());
  SetOffline();
  EXPECT_EQ(1, delegate_observer_.bandwidth_notifications_count());
  SetOnline();
  EXPECT_EQ(2, delegate_observer_.bandwidth_notifications_count());
  SetOnline();
  EXPECT_EQ(2, delegate_observer_.bandwidth_notifications_count());
}

TEST_F(NetworkChangeNotifierAndroidTest, NetworkCallbacks) {
  ForceNetworkHandlesSupportedForTesting();

  TestNetworkObserver network_observer;
  NetworkChangeNotifier::AddNetworkObserver(&network_observer);

  // Test empty values
  EXPECT_EQ(handles::kInvalidNetworkHandle,
            NetworkChangeNotifier::GetDefaultNetwork());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            NetworkChangeNotifier::GetNetworkConnectionType(100));
  NetworkChangeNotifier::NetworkList network_list;
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(0u, network_list.size());
  // Test connecting network
  FakeNetworkChange(CONNECTED, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(CONNECTED, 100);
  EXPECT_EQ(handles::kInvalidNetworkHandle,
            NetworkChangeNotifier::GetDefaultNetwork());
  // Test GetConnectedNetworks()
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(1u, network_list.size());
  EXPECT_EQ(100, network_list[0]);
  // Test GetNetworkConnectionType()
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_WIFI,
            NetworkChangeNotifier::GetNetworkConnectionType(100));
  // Test deduplication of connecting signal
  FakeNetworkChange(CONNECTED, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(NONE, handles::kInvalidNetworkHandle);
  // Test connecting another network
  FakeNetworkChange(CONNECTED, 101, NetworkChangeNotifier::CONNECTION_3G);
  network_observer.ExpectChange(CONNECTED, 101);
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(2u, network_list.size());
  EXPECT_EQ(100, network_list[0]);
  EXPECT_EQ(101, network_list[1]);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_WIFI,
            NetworkChangeNotifier::GetNetworkConnectionType(100));
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_3G,
            NetworkChangeNotifier::GetNetworkConnectionType(101));
  // Test lingering network
  FakeNetworkChange(SOON_TO_DISCONNECT, 100,
                    NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(SOON_TO_DISCONNECT, 100);
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(2u, network_list.size());
  EXPECT_EQ(100, network_list[0]);
  EXPECT_EQ(101, network_list[1]);
  // Test disconnecting network
  FakeNetworkChange(DISCONNECTED, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(DISCONNECTED, 100);
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(1u, network_list.size());
  EXPECT_EQ(101, network_list[0]);
  // Test deduplication of disconnecting signal
  FakeNetworkChange(DISCONNECTED, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(NONE, handles::kInvalidNetworkHandle);
  // Test delay of default network signal until connect signal
  FakeNetworkChange(MADE_DEFAULT, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(NONE, handles::kInvalidNetworkHandle);
  FakeNetworkChange(CONNECTED, 100, NetworkChangeNotifier::CONNECTION_WIFI);
  network_observer.ExpectChange(MADE_DEFAULT, 100);
  EXPECT_EQ(100, NetworkChangeNotifier::GetDefaultNetwork());
  // Test change of default
  FakeNetworkChange(MADE_DEFAULT, 101, NetworkChangeNotifier::CONNECTION_3G);
  network_observer.ExpectChange(MADE_DEFAULT, 101);
  EXPECT_EQ(101, NetworkChangeNotifier::GetDefaultNetwork());
  // Test deduplication default signal
  FakeNetworkChange(MADE_DEFAULT, 101, NetworkChangeNotifier::CONNECTION_3G);
  network_observer.ExpectChange(NONE, handles::kInvalidNetworkHandle);
  // Test that networks can change type
  FakeNetworkChange(CONNECTED, 101, NetworkChangeNotifier::CONNECTION_4G);
  network_observer.ExpectChange(NONE, handles::kInvalidNetworkHandle);
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_4G,
            NetworkChangeNotifier::GetNetworkConnectionType(101));
  // Test purging the network list
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(2u, network_list.size());
  EXPECT_EQ(100, network_list[0]);
  EXPECT_EQ(101, network_list[1]);
  network_list.erase(network_list.begin() + 1);  // Remove network 101
  FakePurgeActiveNetworkList(network_list);
  network_observer.ExpectChange(DISCONNECTED, 101);
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  EXPECT_EQ(1u, network_list.size());
  EXPECT_EQ(100, network_list[0]);
  EXPECT_EQ(handles::kInvalidNetworkHandle,
            NetworkChangeNotifier::GetDefaultNetwork());

  NetworkChangeNotifier::RemoveNetworkObserver(&network_observer);
}

// Tests that network type changes happen synchronously. Otherwise the type
// "change" at browser startup leaves tasks on the queue that will later
// invalidate any network requests that have been started.
TEST_F(NetworkChangeNotifierDelegateAndroidTest, TypeChangeIsSynchronous) {
  const int initial_value = delegate_observer_.type_notifications_count();
  SetOffline(/*drain_run_loop=*/false);
  // Note that there's no call to |base::RunLoop::RunUntilIdle| here. The
  // update must happen synchronously.
  EXPECT_EQ(initial_value + 1, delegate_observer_.type_notifications_count());
}

TEST_F(NetworkChangeNotifierDelegateAndroidTest, DefaultNetworkActive) {
  // No notifications should be received when there are no observers.
  EXPECT_EQ(0, delegate_observer_.default_network_active_notifications_count());
  FakeDefaultNetworkActive();
  EXPECT_EQ(0, delegate_observer_.default_network_active_notifications_count());

  // Simulate calls to NetworkChangeNotifier::AddDefaultNetworkObserver().
  // Notifications should be received now.
  delegate_.DefaultNetworkActiveObserverAdded();
  FakeDefaultNetworkActive();
  EXPECT_EQ(1, delegate_observer_.default_network_active_notifications_count());
  delegate_.DefaultNetworkActiveObserverAdded();
  FakeDefaultNetworkActive();
  EXPECT_EQ(2, delegate_observer_.default_network_active_notifications_count());

  // Simulate call to NetworkChangeNotifier::AddDefaultNetworkObserver().
  // Notifications should be received until the last observer has been
  // removed.
  delegate_.DefaultNetworkActiveObserverRemoved();
  FakeDefaultNetworkActive();
  EXPECT_EQ(3, delegate_observer_.default_network_active_notifications_count());
  delegate_.DefaultNetworkActiveObserverRemoved();
  FakeDefaultNetworkActive();
  EXPECT_EQ(3, delegate_observer_.default_network_active_notifications_count());

  // Double check that things keep working as expected after re-adding an
  // observer.
  delegate_.DefaultNetworkActiveObserverAdded();
  FakeDefaultNetworkActive();
  EXPECT_EQ(4, delegate_observer_.default_network_active_notifications_count());

  // Cleanup: delegate destructor DCHECKS that all observers have been
  // removed.
  delegate_.DefaultNetworkActiveObserverRemoved();
}

}  // namespace net

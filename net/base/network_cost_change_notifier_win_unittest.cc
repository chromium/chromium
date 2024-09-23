// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_cost_change_notifier_win.h"

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/scoped_os_info_override_win.h"
#include "base/win/windows_version.h"
#include "net/base/network_change_notifier.h"
#include "net/test/test_connection_cost_observer.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/win/fake_network_cost_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class NetworkCostChangeNotifierWinTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override {
    if (base::win::GetVersion() <
        NetworkCostChangeNotifierWin::kSupportedOsVersion) {
      GTEST_SKIP();
    }
  }

 protected:
  FakeNetworkCostManagerEnvironment fake_network_cost_manager_environment_;
};

TEST_F(NetworkCostChangeNotifierWinTest, InitialCostUnknown) {
  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN);

  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Wait for `NetworkCostChangeNotifierWin` to finish initializing.
  cost_change_observer.WaitForConnectionCostChanged();

  // `NetworkCostChangeNotifierWin` must report an unknown cost after
  // initializing.
  EXPECT_EQ(cost_change_observer.cost_changed_calls(), 1u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN);
}

TEST_F(NetworkCostChangeNotifierWinTest, InitialCostKnown) {
  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Initializing changes the cost from unknown to unmetered.
  cost_change_observer.WaitForConnectionCostChanged();

  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 1u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);
}

TEST_F(NetworkCostChangeNotifierWinTest, MultipleCostChangedEvents) {
  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Initializing changes the cost from unknown to unmetered.
  cost_change_observer.WaitForConnectionCostChanged();

  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 1u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  // The simulated event changes the cost from unmetered to metered.
  cost_change_observer.WaitForConnectionCostChanged();

  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 2u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN);

  // The simulated event changes the cost from metered to unknown.
  cost_change_observer.WaitForConnectionCostChanged();

  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 3u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN);
}

TEST_F(NetworkCostChangeNotifierWinTest, DuplicateEvents) {
  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Initializing changes the cost from unknown to unmetered.
  cost_change_observer.WaitForConnectionCostChanged();
  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 1u);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  cost_change_observer.WaitForConnectionCostChanged();

  // Changing from unmetered to unmetered must dispatch a cost changed event.
  ASSERT_EQ(cost_change_observer.cost_changed_calls(), 2u);
  EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);
}

TEST_F(NetworkCostChangeNotifierWinTest, ShutdownImmediately) {
  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Shutting down immediately must not crash.
  cost_change_notifier.Reset();

  // Wait for `NetworkCostChangeNotifierWin` to finish initializing and shutting
  // down.
  RunUntilIdle();

  // `NetworkCostChangeNotifierWin` reports a connection change after
  // initializing.
  EXPECT_EQ(cost_change_observer.cost_changed_calls(), 1u);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  // Wait for `NetworkCostChangeNotifierWin` to handle the cost changed event.
  RunUntilIdle();

  // After shutdown, cost changed events must have no effect.
  EXPECT_EQ(cost_change_observer.cost_changed_calls(), 1u);
}

TEST_F(NetworkCostChangeNotifierWinTest, ErrorHandling) {
  // Simulate the failure of each OS API while initializing
  // `NetworkCostChangeNotifierWin`.
  constexpr const NetworkCostManagerStatus kErrorList[] = {
      NetworkCostManagerStatus::kErrorCoCreateInstanceFailed,
      NetworkCostManagerStatus::kErrorQueryInterfaceFailed,
      NetworkCostManagerStatus::kErrorFindConnectionPointFailed,
      NetworkCostManagerStatus::kErrorAdviseFailed,
      NetworkCostManagerStatus::kErrorGetCostFailed,
  };
  for (auto error : kErrorList) {
    fake_network_cost_manager_environment_.SetCost(
        NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

    fake_network_cost_manager_environment_.SimulateError(error);

    TestConnectionCostObserver cost_change_observer;
    auto cost_change_callback = base::BindRepeating(
        &TestConnectionCostObserver::OnConnectionCostChanged,
        base::Unretained(&cost_change_observer));

    base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
        NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

    if (error == NetworkCostManagerStatus::kErrorGetCostFailed) {
      // `NetworkCostChangeNotifierWin` must report an unknown cost after
      // `INetworkCostManager::GetCost()` fails.
      cost_change_observer.WaitForConnectionCostChanged();

      EXPECT_EQ(cost_change_observer.cost_changed_calls(), 1u);
      EXPECT_EQ(cost_change_observer.last_cost_changed_input(),
                NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNKNOWN);
    } else {
      // Wait for `NetworkCostChangeNotifierWin` to finish initializing.
      RunUntilIdle();

      // `NetworkCostChangeNotifierWin` must NOT report a changed cost after
      // failing to initialize.
      EXPECT_EQ(cost_change_observer.cost_changed_calls(), 0u);
    }
  }
}

TEST_F(NetworkCostChangeNotifierWinTest, UnsupportedOS) {
  base::test::ScopedOSInfoOverride os_override(
      base::test::ScopedOSInfoOverride::Type::kWinServer2016);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  TestConnectionCostObserver cost_change_observer;
  auto cost_change_callback =
      base::BindRepeating(&TestConnectionCostObserver::OnConnectionCostChanged,
                          base::Unretained(&cost_change_observer));

  base::SequenceBound<NetworkCostChangeNotifierWin> cost_change_notifier =
      NetworkCostChangeNotifierWin::CreateInstance(cost_change_callback);

  // Wait for `NetworkCostChangeNotifierWin` to finish initializing.
  RunUntilIdle();

  // `NetworkCostChangeNotifierWin` must NOT report a changed cost for
  // unsupported OSes.
  EXPECT_EQ(cost_change_observer.cost_changed_calls(), 0u);
}

}  // namespace net

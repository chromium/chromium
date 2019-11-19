// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace net {

// Subclass of NetworkChangeNotifierWin that overrides functions so that no
// Windows API networking function results effect tests.
class TestNetworkChangeNotifierWin : public NetworkChangeNotifierWin {
 public:
  TestNetworkChangeNotifierWin() {
    last_computed_connection_type_ = NetworkChangeNotifier::CONNECTION_UNKNOWN;
    last_announced_offline_ = false;
  }

  ~TestNetworkChangeNotifierWin() override {
    // This is needed so we don't try to stop watching for IP address changes,
    // as we never actually started.
    set_is_watching(false);
  }

  // From NetworkChangeNotifierWin.
  void RecomputeCurrentConnectionTypeOnBlockingSequence(
      base::OnceCallback<void(ConnectionType)> reply_callback) const override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply_callback),
                                  NetworkChangeNotifier::CONNECTION_UNKNOWN));
  }

  // From NetworkChangeNotifierWin.
  MOCK_METHOD0(WatchForAddressChangeInternal, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifierWin);
};

class TestIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  TestIPAddressObserver() {
    NetworkChangeNotifier::AddIPAddressObserver(this);
  }

  ~TestIPAddressObserver() {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  MOCK_METHOD0(OnIPAddressChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestIPAddressObserver);
};

bool ExitMessageLoopAndReturnFalse() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
  return false;
}

class NetworkChangeNotifierWinTest : public TestWithTaskEnvironment {
 public:
  // Calls WatchForAddressChange, and simulates a WatchForAddressChangeInternal
  // success.  Expects that |network_change_notifier_| has just been created, so
  // it's not watching anything yet, and there have been no previous
  // WatchForAddressChangeInternal failures.
  void StartWatchingAndSucceed() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged()).Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .Times(1)
        .WillOnce(Return(true));

    network_change_notifier_.WatchForAddressChange();

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run to trigger a failure.
    base::RunLoop().RunUntilIdle();
  }

  // Calls WatchForAddressChange, and simulates a WatchForAddressChangeInternal
  // failure.
  void StartWatchingAndFail() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged()).Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));

    network_change_notifier_.WatchForAddressChange();

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run.
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change event, resulting in a call to OnObjectSignaled.
  // The resulting call to WatchForAddressChangeInternal then succeeds.
  void SignalAndSucceed() {
    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged()).Times(1);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .Times(1)
        .WillOnce(Return(true));

    network_change_notifier_.OnObjectSignaled(INVALID_HANDLE_VALUE);

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    // Run the task to notify observers of the IP address change event.
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change event, resulting in a call to OnObjectSignaled.
  // The resulting call to WatchForAddressChangeInternal then fails.
  void SignalAndFail() {
    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged()).Times(1);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));

    network_change_notifier_.OnObjectSignaled(INVALID_HANDLE_VALUE);

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    // Run the task to notify observers of the IP address change event.
    base::RunLoop().RunUntilIdle();
  }

  // Runs the message loop until WatchForAddressChange is called again, as a
  // result of the already posted task after a WatchForAddressChangeInternal
  // failure.  Simulates a success on the resulting call to
  // WatchForAddressChangeInternal.
  void RetryAndSucceed() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    base::RunLoop run_loop;

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged())
        .Times(1)
        .WillOnce(Invoke(&run_loop, &base::RunLoop::QuitWhenIdle));
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .Times(1).WillOnce(Return(true));

    run_loop.Run();

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());
  }

  // Runs the message loop until WatchForAddressChange is called again, as a
  // result of the already posted task after a WatchForAddressChangeInternal
  // failure.  Simulates a failure on the resulting call to
  // WatchForAddressChangeInternal.
  void RetryAndFail() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    int initial_sequential_failures =
        network_change_notifier_.sequential_failures();

    EXPECT_CALL(test_ip_address_observer_, OnIPAddressChanged()).Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(ExitMessageLoopAndReturnFalse));

    base::RunLoop().Run();

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(initial_sequential_failures,
              network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run.
    base::RunLoop().RunUntilIdle();
  }

 private:
  // Note that the order of declaration here is important.

  // Allows creating a new NetworkChangeNotifier.  Must be created before
  // |network_change_notifier_| and destroyed after it to avoid DCHECK failures.
  NetworkChangeNotifier::DisableForTest disable_for_test_;

  StrictMock<TestNetworkChangeNotifierWin> network_change_notifier_;

  // Must be created after |network_change_notifier_|, so it can add itself as
  // an IPAddressObserver.
  StrictMock<TestIPAddressObserver> test_ip_address_observer_;
};

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinBasic) {
  StartWatchingAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStart) {
  StartWatchingAndFail();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStartOnce) {
  StartWatchingAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStartTwice) {
  StartWatchingAndFail();
  RetryAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinSignal) {
  StartWatchingAndSucceed();
  SignalAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailSignalOnce) {
  StartWatchingAndSucceed();
  SignalAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailSignalTwice) {
  StartWatchingAndSucceed();
  SignalAndFail();
  RetryAndFail();
  RetryAndSucceed();
}

}  // namespace net

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

class FakeWaiter : public WebSocketEndpointLockManager::Waiter {
 public:
  FakeWaiter() = default;

  void GotEndpointLock() override {
    CHECK(!called_);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_ = false;
};

class BlockingWaiter : public FakeWaiter {
 public:
  void WaitForLock() {
    while (!called()) {
      run_loop_.Run();
    }
  }

  void GotEndpointLock() override {
    FakeWaiter::GotEndpointLock();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

class WebSocketEndpointLockManagerTest : public TestWithTaskEnvironment {
 protected:
  WebSocketEndpointLockManagerTest() {
    websocket_endpoint_lock_manager_.SetUnlockDelayForTesting(
        base::TimeDelta());
  }

  ~WebSocketEndpointLockManagerTest() override {
    // Permit any pending asynchronous unlock operations to complete.
    RunUntilIdle();
    // If this check fails then subsequent tests may fail.
    CHECK(websocket_endpoint_lock_manager_.IsEmpty());
  }

  IPEndPoint DummyEndpoint() {
    return IPEndPoint(IPAddress::IPv4Localhost(), 80);
  }

  void UnlockDummyEndpoint(int times) {
    for (int i = 0; i < times; ++i) {
      websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
      RunUntilIdle();
    }
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;
};

TEST_F(WebSocketEndpointLockManagerTest, LockEndpointReturnsOkOnce) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledOnOk) {
  FakeWaiter waiter;
  EXPECT_THAT(
      websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(), &waiter),
      IsOk());
  RunUntilIdle();
  EXPECT_FALSE(waiter.called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledImmediately) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));
  RunUntilIdle();
  EXPECT_FALSE(waiters[1].called());

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockCalledWhenUnlocked) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest,
       EndpointUnlockedIfWaiterAlreadyDeleted) {
  FakeWaiter first_lock_holder;
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &first_lock_holder),
              IsOk());

  {
    FakeWaiter short_lived_waiter;
    EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                  DummyEndpoint(), &short_lived_waiter));
  }

  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();

  FakeWaiter second_lock_holder;
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(
                  DummyEndpoint(), &second_lock_holder),
              IsOk());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, LockReleaserWorks) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));

  {
    WebSocketEndpointLockManager::LockReleaser releaser(
        &websocket_endpoint_lock_manager_, DummyEndpoint());
  }
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

// UnlockEndpoint() should cause any LockReleasers for this endpoint to be
// unregistered.
TEST_F(WebSocketEndpointLockManagerTest, LockReleaserForgottenOnUnlock) {
  FakeWaiter waiter;

  EXPECT_THAT(
      websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(), &waiter),
      IsOk());
  WebSocketEndpointLockManager::LockReleaser releaser(
      &websocket_endpoint_lock_manager_, DummyEndpoint());
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(websocket_endpoint_lock_manager_.IsEmpty());
}

// When ownership of the endpoint is passed to a new waiter, the new waiter can
// construct another LockReleaser.
TEST_F(WebSocketEndpointLockManagerTest, NextWaiterCanCreateLockReleaserAgain) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));

  WebSocketEndpointLockManager::LockReleaser releaser1(
      &websocket_endpoint_lock_manager_, DummyEndpoint());
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());
  WebSocketEndpointLockManager::LockReleaser releaser2(
      &websocket_endpoint_lock_manager_, DummyEndpoint());

  UnlockDummyEndpoint(1);
}

// Destroying LockReleaser after UnlockEndpoint() does nothing.
TEST_F(WebSocketEndpointLockManagerTest,
       DestroyLockReleaserAfterUnlockEndpointDoesNothing) {
  FakeWaiter waiters[3];

  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[2]));
  {
    WebSocketEndpointLockManager::LockReleaser releaser(
        &websocket_endpoint_lock_manager_, DummyEndpoint());
    websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  }
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());
  EXPECT_FALSE(waiters[2].called());

  UnlockDummyEndpoint(2);
}

// UnlockEndpoint() should always be asynchronous.
TEST_F(WebSocketEndpointLockManagerTest, UnlockEndpointIsAsynchronous) {
  FakeWaiter waiters[2];
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &waiters[0]),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &waiters[1]));

  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  EXPECT_FALSE(waiters[1].called());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

// UnlockEndpoint() should normally have a delay.
TEST_F(WebSocketEndpointLockManagerTest, UnlockEndpointIsDelayed) {
  using base::TimeTicks;

  // This 1ms delay is too short for very slow environments (usually those
  // running memory checkers). In those environments, the code takes >1ms to run
  // and no delay is needed. Rather than increase the delay and slow down the
  // test everywhere, the test doesn't explicitly verify that a delay has been
  // applied. Instead it just verifies that the whole thing took >=1ms. 1ms is
  // easily enough for normal compiles even on Android, so the fact that there
  // is a delay is still checked on every platform.
  const base::TimeDelta unlock_delay = base::Milliseconds(1);
  websocket_endpoint_lock_manager_.SetUnlockDelayForTesting(unlock_delay);
  FakeWaiter fake_waiter;
  BlockingWaiter blocking_waiter;
  EXPECT_THAT(websocket_endpoint_lock_manager_.LockEndpoint(DummyEndpoint(),
                                                            &fake_waiter),
              IsOk());
  EXPECT_EQ(ERR_IO_PENDING, websocket_endpoint_lock_manager_.LockEndpoint(
                                DummyEndpoint(), &blocking_waiter));

  TimeTicks before_unlock = TimeTicks::Now();
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  blocking_waiter.WaitForLock();
  TimeTicks after_unlock = TimeTicks::Now();
  EXPECT_GE(after_unlock - before_unlock, unlock_delay);
  websocket_endpoint_lock_manager_.SetUnlockDelayForTesting(base::TimeDelta());
  UnlockDummyEndpoint(1);
}

}  // namespace

}  // namespace net

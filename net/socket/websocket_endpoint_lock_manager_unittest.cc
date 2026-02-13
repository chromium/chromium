// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include <array>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
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

namespace net {

namespace {

using test::IsError;
using test::IsOk;
using EndpointLock = WebSocketEndpointLockManager::EndpointLock;

class FakeWaiter {
 public:
  FakeWaiter() = default;

  void WaitForLock() { run_loop_.Run(); }

  void GotEndpointLock() {
    CHECK(!called_);
    called_ = true;
    run_loop_.Quit();
  }

  bool called() const { return called_; }

  base::OnceClosure GetCallback() {
    return base::BindOnce(&FakeWaiter::GotEndpointLock, base::Unretained(this));
  }

 private:
  bool called_ = false;
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
  std::array<FakeWaiter, 2> waiters;
  EndpointLock endpoint_lock1(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock1.LockEndpoint(waiters[0].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledOnOk) {
  FakeWaiter waiter;
  EndpointLock endpoint_lock(&websocket_endpoint_lock_manager_,
                             DummyEndpoint());
  EXPECT_THAT(endpoint_lock.LockEndpoint(waiter.GetCallback()), IsOk());
  RunUntilIdle();
  EXPECT_FALSE(waiter.called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledImmediately) {
  std::array<FakeWaiter, 2> waiters;
  EndpointLock endpoint_lock1(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock1.LockEndpoint(waiters[0].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(waiters[1].called());

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockCalledWhenUnlocked) {
  std::array<FakeWaiter, 2> waiters;
  EndpointLock endpoint_lock1(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock1.LockEndpoint(waiters[0].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest,
       EndpointUnlockedIfWaiterAlreadyDeleted) {
  FakeWaiter first_lock_holder;
  EndpointLock first_endpoint_lock(&websocket_endpoint_lock_manager_,
                                   DummyEndpoint());
  EXPECT_THAT(first_endpoint_lock.LockEndpoint(first_lock_holder.GetCallback()),
              IsOk());

  {
    FakeWaiter short_lived_waiter;
    EndpointLock short_lived_endpoint_lock(&websocket_endpoint_lock_manager_,
                                           DummyEndpoint());
    EXPECT_THAT(short_lived_endpoint_lock.LockEndpoint(
                    short_lived_waiter.GetCallback()),
                IsError(ERR_IO_PENDING));
  }

  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();

  FakeWaiter second_lock_holder;
  EndpointLock second_endpoint_lock(&websocket_endpoint_lock_manager_,
                                    DummyEndpoint());
  EXPECT_THAT(
      second_endpoint_lock.LockEndpoint(second_lock_holder.GetCallback()),
      IsOk());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, DeletingEndpointLockPassesOwnership) {
  std::array<FakeWaiter, 2> waiters;
  auto endpoint_lock1 = std::make_unique<EndpointLock>(
      &websocket_endpoint_lock_manager_, DummyEndpoint());
  EXPECT_THAT(endpoint_lock1->LockEndpoint(waiters[0].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));

  endpoint_lock1.reset();
  waiters[1].WaitForLock();
  EXPECT_FALSE(websocket_endpoint_lock_manager_.IsEmpty());

  UnlockDummyEndpoint(1);
  EXPECT_TRUE(websocket_endpoint_lock_manager_.IsEmpty());
}

// UnlockEndpoint() should cause any EndpointLock holding the lock for this
// endpoint to be unregistered.
TEST_F(WebSocketEndpointLockManagerTest, EndpointLockForgottenOnUnlock) {
  FakeWaiter waiter;

  EndpointLock endpoint_lock(&websocket_endpoint_lock_manager_,
                             DummyEndpoint());
  EXPECT_THAT(endpoint_lock.LockEndpoint(waiter.GetCallback()), IsOk());
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(websocket_endpoint_lock_manager_.IsEmpty());
}

// Destroying EndpointLock after UnlockEndpoint() does nothing.
TEST_F(WebSocketEndpointLockManagerTest,
       DestroyEndpointLockAfterUnlockEndpointDoesNothing) {
  std::array<FakeWaiter, 3> waiters;

  auto endpoint_lock1 = std::make_unique<EndpointLock>(
      &websocket_endpoint_lock_manager_, DummyEndpoint());
  EXPECT_THAT(endpoint_lock1->LockEndpoint(waiters[0].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));
  EndpointLock endpoint_lock3(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock3.LockEndpoint(waiters[2].GetCallback()),
              IsError(ERR_IO_PENDING));

  endpoint_lock1.reset();
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());
  EXPECT_FALSE(waiters[2].called());

  UnlockDummyEndpoint(2);
}

// UnlockEndpoint() should always be asynchronous.
TEST_F(WebSocketEndpointLockManagerTest, UnlockEndpointIsAsynchronous) {
  std::array<FakeWaiter, 2> waiters;
  EndpointLock endpoint_lock1(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock1.LockEndpoint(waiters[1].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));

  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  EXPECT_FALSE(waiters[1].called());
  waiters[1].WaitForLock();

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
  std::array<FakeWaiter, 2> waiters;
  EndpointLock endpoint_lock1(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock1.LockEndpoint(waiters[1].GetCallback()), IsOk());
  EndpointLock endpoint_lock2(&websocket_endpoint_lock_manager_,
                              DummyEndpoint());
  EXPECT_THAT(endpoint_lock2.LockEndpoint(waiters[1].GetCallback()),
              IsError(ERR_IO_PENDING));

  TimeTicks before_unlock = TimeTicks::Now();
  websocket_endpoint_lock_manager_.UnlockEndpoint(DummyEndpoint());
  waiters[1].WaitForLock();
  TimeTicks after_unlock = TimeTicks::Now();
  EXPECT_GE(after_unlock - before_unlock, unlock_delay);
  websocket_endpoint_lock_manager_.SetUnlockDelayForTesting(base::TimeDelta());
  UnlockDummyEndpoint(1);
}

}  // namespace

}  // namespace net

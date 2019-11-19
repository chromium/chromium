// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/connection_time_observer.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace test {

class ConnectionTimeObserverTest : public ::testing::Test {
 public:
  ConnectionTimeObserverTest();
  ~ConnectionTimeObserverTest() override;

 protected:
  // Test interface.
  void SetUp() override;

  // A map of fake times to calculate TimeDelta between two states.
  std::map<protocol::ConnectionToHost::State, base::TimeTicks> test_map_;

  // Observes and saves the times when the chromoting connection state changes.
  std::unique_ptr<ConnectionTimeObserver> connection_time_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionTimeObserverTest);
};

ConnectionTimeObserverTest::ConnectionTimeObserverTest() = default;

ConnectionTimeObserverTest::~ConnectionTimeObserverTest() = default;

void ConnectionTimeObserverTest::SetUp() {
  connection_time_observer_.reset(new ConnectionTimeObserver());

  base::TimeTicks now = base::TimeTicks::Now();
  test_map_.insert(std::make_pair(
          protocol::ConnectionToHost::State::INITIALIZING,
          now + base::TimeDelta::FromMilliseconds(10)));
  test_map_.insert(std::make_pair(
          protocol::ConnectionToHost::State::CONNECTING,
          now + base::TimeDelta::FromMilliseconds(20)));
  test_map_.insert(std::make_pair(
          protocol::ConnectionToHost::State::AUTHENTICATED,
          now + base::TimeDelta::FromMilliseconds(30)));
  test_map_.insert(std::make_pair(
          protocol::ConnectionToHost::State::CONNECTED,
          now + base::TimeDelta::FromMilliseconds(40)));
  test_map_.insert(std::make_pair(
          protocol::ConnectionToHost::State::CLOSED,
          now + base::TimeDelta::FromMilliseconds(50)));

  connection_time_observer_->SetTransitionTimesMapForTest(test_map_);
}

TEST_F(ConnectionTimeObserverTest, ChromotingConnectionSuccess) {
  EXPECT_EQ(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::CONNECTING).InMilliseconds(), 10);
  EXPECT_EQ(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::CONNECTING,
      protocol::ConnectionToHost::State::AUTHENTICATED).InMilliseconds(), 10);
  EXPECT_EQ(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::AUTHENTICATED,
      protocol::ConnectionToHost::State::CONNECTED).InMilliseconds(), 10);
  EXPECT_EQ(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::CONNECTED,
      protocol::ConnectionToHost::State::CLOSED).InMilliseconds(), 10);
}

TEST_F(ConnectionTimeObserverTest, StartStateNotFound) {
  EXPECT_TRUE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::FAILED,
          protocol::ConnectionToHost::State::CLOSED).is_max());
}

TEST_F(ConnectionTimeObserverTest, EndStateNotFound) {
  EXPECT_TRUE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::FAILED).is_max());
}

TEST_F(ConnectionTimeObserverTest, NegativeTransitionDelay) {
  EXPECT_TRUE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::CLOSED,
          protocol::ConnectionToHost::State::INITIALIZING).is_max());
}

TEST_F(ConnectionTimeObserverTest, TestOnConnectionStateChangedWithTestMap) {
  // Should fail to find FAILED.
  EXPECT_TRUE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::FAILED).is_max());

  // Registers the time at which FAILED state occurred into the map.
  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::FAILED,
      protocol::ErrorCode::PEER_IS_OFFLINE);

  EXPECT_FALSE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::FAILED).is_zero());
}

TEST_F(ConnectionTimeObserverTest, TestOnConnectionStateChangedWithoutTestMap) {
  connection_time_observer_->SetTransitionTimesMapForTest(
      std::map<protocol::ConnectionToHost::State, base::TimeTicks>());

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::RunLoop run_loop;

  // Should fail to find INITIALIZING in an empty map.
  EXPECT_TRUE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::FAILED).is_max());

  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ErrorCode::OK);
  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::CONNECTING,
      protocol::ErrorCode::OK);
  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::AUTHENTICATED,
      protocol::ErrorCode::OK);

  // Wait for 10 milliseconds for CONNECTED state.
  // Note: This test can only guarantee a positive TimeDelta between a previous
  // state and the CONNECTED state. Prior states have non-deterministic times
  // between each other.
  base::OneShotTimer timer;
  timer.Start(FROM_HERE,
              base::TimeDelta::FromMilliseconds(10),
              run_loop.QuitClosure());
  run_loop.Run();

  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::CONNECTED,
      protocol::ErrorCode::OK);

  // Verify that the time waited is positive and at least 10 milliseconds.
  EXPECT_GE(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::AUTHENTICATED,
          protocol::ConnectionToHost::State::CONNECTED).InMilliseconds(), 10);
}

TEST_F(ConnectionTimeObserverTest, TestOnConnectionStateDataDoesNotChange) {
  base::TimeDelta first_time_delta =
      connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::CONNECTED);

  // This check makes sure that the two states are actually found.
  EXPECT_FALSE(first_time_delta.is_zero());

  // The initial data should not change and should log an error.
  connection_time_observer_->ConnectionStateChanged(
      protocol::ConnectionToHost::State::CONNECTED, protocol::ErrorCode::OK);

  EXPECT_EQ(connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::INITIALIZING,
          protocol::ConnectionToHost::State::CONNECTED), first_time_delta);
}

}  // namespace test
}  // namespace remoting

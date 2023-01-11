// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/zombie_host_detector.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Extra time to fast forward to make sure the task gets run.
static constexpr base::TimeDelta kFastForwardDelta = base::Seconds(1);

}  // namespace

namespace remoting {

class ZombieHostDetectorTest : public testing::Test {
 public:
  ZombieHostDetectorTest() {
    network_change_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_ETHERNET);

    zombie_host_detector_.Start();
  }

 protected:
  void FastForwardUntilZombieStateDetected(
      base::TimeDelta fast_forward_interval,
      const base::RepeatingClosure& on_fast_forward) {
    bool zombie_state_detected = false;
    EXPECT_CALL(mock_on_zombie_state_detected_, Run()).WillOnce([&]() {
      zombie_state_detected = true;
    });
    while (!zombie_state_detected) {
      task_environment_.FastForwardBy(fast_forward_interval);
      on_fast_forward.Run();
    }
  }

  base::TimeDelta GetNextDetectionDurationSinceNow() const {
    return zombie_host_detector_.GetNextDetectionTime() -
           base::TimeTicks::Now();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_{net::test::MockNetworkChangeNotifier::Create()};
  base::MockCallback<base::OnceClosure> mock_on_zombie_state_detected_;
  ZombieHostDetector zombie_host_detector_{
      mock_on_zombie_state_detected_.Get()};
};

TEST_F(ZombieHostDetectorTest, NoEvent_Noop) {
  task_environment_.FastForwardBy(base::Hours(1));
}

TEST_F(ZombieHostDetectorTest, AllEventsAreCurrent_Noop) {
  // Fast forward to 5s before detection.
  task_environment_.FastForwardBy(GetNextDetectionDurationSinceNow() -
                                  base::Seconds(5));

  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  task_environment_.FastForwardBy(base::Seconds(6));
}

TEST_F(ZombieHostDetectorTest, HeartbeatNotCurrent_CallbackCalled) {
  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  // Only report signaling active but not heartbeat.
  FastForwardUntilZombieStateDetected(
      ZombieHostDetector::kMaxSignalingActiveInterval - kFastForwardDelta,
      base::BindLambdaForTesting(
          [&]() { zombie_host_detector_.OnSignalingActive(); }));
}

TEST_F(ZombieHostDetectorTest, SignalingNotCurrent_CallbackCalled) {
  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  // Only report heartbeat but not signaling active.
  FastForwardUntilZombieStateDetected(
      ZombieHostDetector::kMaxHeartbeatInterval - kFastForwardDelta,
      base::BindLambdaForTesting(
          [&]() { zombie_host_detector_.OnHeartbeatSent(); }));
}

TEST_F(ZombieHostDetectorTest, NeitherIsCurrent_CallbackCalled) {
  EXPECT_CALL(mock_on_zombie_state_detected_, Run()).Times(1);

  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  task_environment_.FastForwardBy(base::Hours(1));
}

TEST_F(ZombieHostDetectorTest, NeitherIsCurrentWhileNoConnection_Noop) {
  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  task_environment_.FastForwardBy(base::Hours(1));
}

TEST_F(ZombieHostDetectorTest, NoEventAfterComingBackOnline_Noop) {
  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  task_environment_.FastForwardBy(base::Hours(1));

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  task_environment_.FastForwardBy(GetNextDetectionDurationSinceNow() +
                                  kFastForwardDelta);

  task_environment_.FastForwardBy(base::Hours(1));
}

TEST_F(ZombieHostDetectorTest, NeitherIsCurrentWhenJustComeBackOnline_Noop) {
  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  task_environment_.FastForwardBy(base::Hours(1));

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  task_environment_.FastForwardBy(GetNextDetectionDurationSinceNow() +
                                  kFastForwardDelta);
}

TEST_F(ZombieHostDetectorTest, NotCurrentAfterComingBackOnline_CallbackCalled) {
  zombie_host_detector_.OnHeartbeatSent();
  zombie_host_detector_.OnSignalingActive();

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  task_environment_.FastForwardBy(base::Hours(1));

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  task_environment_.FastForwardBy(GetNextDetectionDurationSinceNow() +
                                  kFastForwardDelta);

  FastForwardUntilZombieStateDetected(
      ZombieHostDetector::kMaxHeartbeatInterval - kFastForwardDelta,
      base::DoNothing());  // Not reporting any event.
}

}  // namespace remoting

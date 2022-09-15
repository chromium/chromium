// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_transport_throttle_context.h"

#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ThrottleResult = WebTransportThrottleContext::ThrottleResult;
using Tracker = WebTransportThrottleContext::Tracker;

class WebTransportThrottleContextTest : public testing::Test {
 public:
  WebTransportThrottleContext& context() { return context_; }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void CreatePending(int count) {
    for (int i = 0; i < count; ++i) {
      base::RunLoop run_loop;
      auto result = context().PerformThrottle(base::BindLambdaForTesting(
          [&run_loop, this](std::unique_ptr<Tracker> tracker) {
            trackers_.push(std::move(tracker));
            run_loop.Quit();
          }));
      EXPECT_EQ(ThrottleResult::kOk, result);
      run_loop.Run();
    }
  }

  // Causes the first `count` pending handshakes to be signalled established.
  void EstablishPending(int count) {
    DCHECK_LE(count, static_cast<int>(trackers_.size()));
    for (int i = 0; i < count; ++i) {
      trackers_.front()->OnHandshakeEstablished();
      trackers_.pop();
    }
  }

  // Causes the first `count` pending handshakes to be signalled failed.
  void FailPending(int count) {
    DCHECK_LE(count, static_cast<int>(trackers_.size()));
    for (int i = 0; i < count; ++i) {
      trackers_.front()->OnHandshakeFailed();
      trackers_.pop();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  WebTransportThrottleContext context_;

  base::queue<std::unique_ptr<Tracker>> trackers_;
};

class CallTrackingCallback {
 public:
  WebTransportThrottleContext::ThrottleDoneCallback Callback() {
    // This use of base::Unretained is safe because the
    // WebTransportThrottleContext is always destroyed at the end of the test
    // before it gets a change to call any callbacks.
    return base::BindOnce(&CallTrackingCallback::OnCall,
                          base::Unretained(this));
  }

  bool called() const { return called_; }

 private:
  void OnCall(std::unique_ptr<Tracker> tracker) { called_ = true; }

  bool called_ = false;
};

TEST_F(WebTransportThrottleContextTest, PerformThrottleSyncWithNonePending) {
  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, PerformThrottleNotSyncWithOnePending) {
  CreatePending(1);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);
  EXPECT_FALSE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, Max64Connections) {
  CreatePending(64);

  CallTrackingCallback callback;
  EXPECT_EQ(ThrottleResult::kTooManyPendingSessions,
            context().PerformThrottle(callback.Callback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, DelayWithOnePending) {
  CreatePending(1);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  // The delay should be at least 5ms.
  FastForwardBy(base::Milliseconds(4));
  EXPECT_FALSE(callback.called());

  // The delay should be less than 16ms.
  FastForwardBy(base::Milliseconds(12));
  EXPECT_TRUE(callback.called());
}

// The reason for testing with 3 pending connections is that the possible range
// of delays doesn't overlap with 1 pending connection.
TEST_F(WebTransportThrottleContextTest, DelayWithThreePending) {
  CreatePending(3);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  // The delay should be at least 20ms.
  FastForwardBy(base::Milliseconds(19));
  EXPECT_FALSE(callback.called());

  // The delay should be less than 61ms.
  FastForwardBy(base::Milliseconds(42));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, DelayIsTruncated) {
  CreatePending(63);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  // The delay should be no less than 30s.
  FastForwardBy(base::Seconds(29));
  EXPECT_FALSE(callback.called());

  // The delay should be less than 91s.
  FastForwardBy(base::Seconds(62));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, EstablishedRemainsPendingFor10ms) {
  CreatePending(1);

  EstablishPending(1);

  // The delay should be more than 9ms.
  FastForwardBy(base::Milliseconds(9));

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);
  EXPECT_FALSE(callback.called());

  // The delay should be less than 11ms.
  FastForwardBy(base::Milliseconds(2));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, FailedRemainsPendingFor3m) {
  CreatePending(1);

  FailPending(1);

  // The delay should be more than 299 seconds.
  FastForwardBy(base::Seconds(299));
  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);
  EXPECT_FALSE(callback.called());

  // The delay should be less than 301 seconds.
  FastForwardBy(base::Seconds(301));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, HandshakeCompletionTriggersPending) {
  CreatePending(3);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  EstablishPending(3);

  // After 10ms the handshakes should no longer be pending and the pending
  // throttle should fire.
  FastForwardBy(base::Milliseconds(10));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest, HandshakeCompletionResetsTimer) {
  CreatePending(5);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  EstablishPending(2);

  // After 10ms the handshakes should no longer be pending and the timer for the
  // pending throttle should be reset.
  FastForwardBy(base::Milliseconds(10));

  // The 10ms should be counted towards the throttling time.
  // There should be more than 9ms remaining.
  FastForwardBy(base::Milliseconds(9));
  EXPECT_FALSE(callback.called());

  // There should be less than 51 ms remaining.
  FastForwardBy(base::Milliseconds(42));
  EXPECT_TRUE(callback.called());
}

TEST_F(WebTransportThrottleContextTest,
       ManyHandshakeCompletionsTriggerPending) {
  CreatePending(63);

  CallTrackingCallback callback;
  const auto result = context().PerformThrottle(callback.Callback());
  EXPECT_EQ(result, ThrottleResult::kOk);

  // Move time forward so that the maximum delay for a handshake with one
  // pending has passed.
  FastForwardBy(base::Milliseconds(15));

  // Leave one pending or the pending handshake will be triggered without
  // recalculating the delay.
  EstablishPending(62);

  // After 10ms the handshakes should no longer be pending and the pending
  // connection throttle timer should have fired.
  FastForwardBy(base::Milliseconds(10));
  EXPECT_TRUE(callback.called());
}

}  // namespace

}  // namespace content

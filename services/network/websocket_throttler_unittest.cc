// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_throttler.h"

#include <optional>
#include <vector>

#include "base/test/task_environment.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class WebSocketThrottlerTest : public ::testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST(WebSocketPerProcessThrottlerTest, InitialState) {
  WebSocketPerProcessThrottler throttler;

  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, Pending) {
  WebSocketPerProcessThrottler throttler;

  auto tracker = throttler.IssuePendingConnectionTracker();

  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
  EXPECT_EQ(1, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, Complete) {
  WebSocketPerProcessThrottler throttler;

  {
    auto tracker = throttler.IssuePendingConnectionTracker();

    EXPECT_FALSE(throttler.HasTooManyPendingConnections());
    EXPECT_EQ(1, throttler.num_pending_connections());
    EXPECT_EQ(0, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());
    EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());

    tracker.OnCompleteHandshake();

    EXPECT_FALSE(throttler.HasTooManyPendingConnections());
    EXPECT_EQ(0, throttler.num_pending_connections());
    EXPECT_EQ(1, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());
    EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());

    // Destruct |tracker|.
  }

  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(1, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, Failed) {
  WebSocketPerProcessThrottler throttler;

  {
    auto tracker = throttler.IssuePendingConnectionTracker();

    EXPECT_FALSE(throttler.HasTooManyPendingConnections());
    EXPECT_EQ(1, throttler.num_pending_connections());
    EXPECT_EQ(0, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());
    EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());

    // Destruct |tracker|.
  }

  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(1, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, TooManyPendingConnections) {
  constexpr int limit = 255;
  WebSocketPerProcessThrottler throttler;

  std::vector<WebSocketPerProcessThrottler::PendingConnection> trackers;
  for (int i = 0; i < limit - 1; ++i) {
    ASSERT_FALSE(throttler.HasTooManyPendingConnections());
    trackers.push_back(throttler.IssuePendingConnectionTracker());
  }

  ASSERT_FALSE(throttler.HasTooManyPendingConnections());
  trackers.push_back(throttler.IssuePendingConnectionTracker());
  EXPECT_TRUE(throttler.HasTooManyPendingConnections());
}

TEST(WebSocketPerProcessThrottlerTest, CompletedConnectionsDontCount) {
  constexpr int limit = 255;
  WebSocketPerProcessThrottler throttler;

  for (int i = 0; i < limit * 3; ++i) {
    ASSERT_FALSE(throttler.HasTooManyPendingConnections());
    auto tracker = throttler.IssuePendingConnectionTracker();
    tracker.OnCompleteHandshake();
  }
  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
}

TEST(WebSocketPerProcessThrottlerTest, FailedConnectionsDontCount) {
  constexpr int limit = 255;
  WebSocketPerProcessThrottler throttler;

  for (int i = 0; i < limit * 3; ++i) {
    ASSERT_FALSE(throttler.HasTooManyPendingConnections());
    auto tracker = throttler.IssuePendingConnectionTracker();
  }
  EXPECT_FALSE(throttler.HasTooManyPendingConnections());
}

TEST(WebSocketPerProcessThrottlerTest, Roll) {
  WebSocketPerProcessThrottler throttler;
  for (int i = 0; i < 2; ++i)
    throttler.IssuePendingConnectionTracker().OnCompleteHandshake();
  for (int i = 0; i < 3; ++i)
    throttler.IssuePendingConnectionTracker();

  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(2, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(3, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());

  throttler.Roll();
  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(2, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(3, throttler.num_previous_failed_connections());

  throttler.Roll();
  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_3Pending) {
  WebSocketPerProcessThrottler throttler;
  std::vector<WebSocketPerProcessThrottler::PendingConnection> trackers;
  for (int i = 0; i < 3; ++i)
    trackers.push_back(throttler.IssuePendingConnectionTracker());

  EXPECT_EQ(3, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_7Pending) {
  WebSocketPerProcessThrottler throttler;
  std::vector<WebSocketPerProcessThrottler::PendingConnection> trackers;
  for (int i = 0; i < 7; ++i)
    trackers.push_back(throttler.IssuePendingConnectionTracker());

  EXPECT_EQ(7, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_LT(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_16Pending) {
  WebSocketPerProcessThrottler throttler;
  std::vector<WebSocketPerProcessThrottler::PendingConnection> trackers;
  for (int i = 0; i < 16; ++i)
    trackers.push_back(throttler.IssuePendingConnectionTracker());

  EXPECT_EQ(16, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_LE(base::Milliseconds(1000), throttler.CalculateDelay());
  EXPECT_LE(throttler.CalculateDelay(), base::Milliseconds(5000));
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_3Failure) {
  WebSocketPerProcessThrottler throttler;
  for (int i = 0; i < 3; ++i)
    throttler.IssuePendingConnectionTracker();

  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(3, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_EQ(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_7Failure) {
  WebSocketPerProcessThrottler throttler;
  for (int i = 0; i < 7; ++i)
    throttler.IssuePendingConnectionTracker();

  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(7, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_LT(base::TimeDelta(), throttler.CalculateDelay());
}

TEST(WebSocketPerProcessThrottlerTest, CalculateDelay_16Failure) {
  WebSocketPerProcessThrottler throttler;
  for (int i = 0; i < 16; ++i)
    throttler.IssuePendingConnectionTracker();

  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(16, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
  EXPECT_LE(base::Milliseconds(1000), throttler.CalculateDelay());
  EXPECT_LE(throttler.CalculateDelay(), base::Milliseconds(5000));
}

TEST(WebSocketPerProcessThrottlerTest, MoveTracker) {
  WebSocketPerProcessThrottler throttler;

  std::optional<WebSocketThrottler::PendingConnection> tracker_holder;
  {
    WebSocketThrottler::PendingConnection tracker =
        throttler.IssuePendingConnectionTracker();

    EXPECT_EQ(1, throttler.num_pending_connections());
    EXPECT_EQ(0, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());

    WebSocketThrottler::PendingConnection tracker2 = std::move(tracker);
    WebSocketThrottler::PendingConnection tracker3 = std::move(tracker2);

    EXPECT_EQ(1, throttler.num_pending_connections());
    EXPECT_EQ(0, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());

    tracker_holder.emplace(std::move(tracker3));

    EXPECT_EQ(1, throttler.num_pending_connections());
    EXPECT_EQ(0, throttler.num_current_succeeded_connections());
    EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
    EXPECT_EQ(0, throttler.num_current_failed_connections());
    EXPECT_EQ(0, throttler.num_previous_failed_connections());
  }

  EXPECT_EQ(1, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(0, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());

  tracker_holder = std::nullopt;

  EXPECT_EQ(0, throttler.num_pending_connections());
  EXPECT_EQ(0, throttler.num_current_succeeded_connections());
  EXPECT_EQ(0, throttler.num_previous_succeeded_connections());
  EXPECT_EQ(1, throttler.num_current_failed_connections());
  EXPECT_EQ(0, throttler.num_previous_failed_connections());
}

TEST_F(WebSocketThrottlerTest, InitialState) {
  WebSocketThrottler throttler;
  EXPECT_EQ(0u, throttler.GetSizeForTesting());
}

TEST_F(WebSocketThrottlerTest, TooManyPendingConnections) {
  constexpr int process1 = 1;
  constexpr int process2 = 2;
  constexpr int limit = 255;
  WebSocketThrottler throttler;

  std::vector<WebSocketThrottler::PendingConnection> trackers;
  for (int i = 0; i < limit - 1; ++i) {
    ASSERT_FALSE(throttler.HasTooManyPendingConnections(process1));
    ASSERT_FALSE(throttler.HasTooManyPendingConnections(process2));
    trackers.push_back(
        std::move(throttler.IssuePendingConnectionTracker(process1).value()));
    trackers.push_back(
        std::move(throttler.IssuePendingConnectionTracker(process2).value()));
  }

  EXPECT_EQ(2u, throttler.GetSizeForTesting());
  ASSERT_FALSE(throttler.HasTooManyPendingConnections(process1));
  ASSERT_FALSE(throttler.HasTooManyPendingConnections(process2));
  trackers.push_back(
      std::move(throttler.IssuePendingConnectionTracker(process1).value()));

  ASSERT_TRUE(throttler.HasTooManyPendingConnections(process1));
  ASSERT_FALSE(throttler.HasTooManyPendingConnections(process2));
  trackers.push_back(
      std::move(throttler.IssuePendingConnectionTracker(process2).value()));

  ASSERT_TRUE(throttler.HasTooManyPendingConnections(process1));
  ASSERT_TRUE(throttler.HasTooManyPendingConnections(process2));
}

TEST_F(WebSocketThrottlerTest, BrowserProcessNotThrottled) {
  WebSocketThrottler throttler;
  ASSERT_FALSE(
      throttler.HasTooManyPendingConnections(mojom::kBrowserProcessId));
  ASSERT_FALSE(throttler.IssuePendingConnectionTracker(mojom::kBrowserProcessId)
                   .has_value());
}

}  // namespace

}  // namespace network

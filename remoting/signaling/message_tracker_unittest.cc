// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/message_tracker.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class MessageTrackerTest : public testing::Test {
 public:
 protected:
  static constexpr base::TimeDelta GetCleanupInterval();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MessageTracker message_tracker_;
};

// static
constexpr base::TimeDelta MessageTrackerTest::GetCleanupInterval() {
  return MessageTracker::kCleanupInterval;
}

TEST_F(MessageTrackerTest, TestIsIdTracked_UntrackedIdReturnsTrue) {
  ASSERT_FALSE(message_tracker_.IsIdTracked("1"));
}

TEST_F(MessageTrackerTest, TrackIdOnce_IdIsTracked) {
  message_tracker_.TrackId("1");
  ASSERT_TRUE(message_tracker_.IsIdTracked("1"));
}

TEST_F(MessageTrackerTest, TrackIdAndAdvanceTimer_ExpiredIdNotRejected) {
  message_tracker_.TrackId("1");
  ASSERT_TRUE(message_tracker_.IsIdTracked("1"));
  task_environment_.FastForwardBy(GetCleanupInterval() * 2);
  ASSERT_FALSE(message_tracker_.IsIdTracked("1"));
}

TEST_F(MessageTrackerTest, TrackIdAndAdvanceTimer_NotExpiredIdRejected) {
  message_tracker_.TrackId("1");
  ASSERT_TRUE(message_tracker_.IsIdTracked("1"));
  task_environment_.FastForwardBy(GetCleanupInterval() / 2);
  ASSERT_TRUE(message_tracker_.IsIdTracked("1"));
}

}  // namespace remoting

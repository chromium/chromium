// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class PendingUserInputMonitorTest : public testing::Test {
 public:
  PendingUserInput::Monitor monitor_;
};

// Sanity check for discrete/continuous queues.
TEST_F(PendingUserInputMonitorTest, QueuingSimple) {
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseDown, {});
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseMove, {});
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseUp, {});
  monitor_.OnDequeue(WebInputEvent::Type::kMouseDown, {});
  monitor_.OnDequeue(WebInputEvent::Type::kMouseMove, {});
  monitor_.OnDequeue(WebInputEvent::Type::kMouseUp, {});
}

// Basic test of continuous and discrete event detection.
TEST_F(PendingUserInputMonitorTest, EventDetection) {
  WebInputEventAttribution focus(WebInputEventAttribution::kFocusedFrame);
  WebInputEventAttribution frame(WebInputEventAttribution::kTargetedFrame,
                                 cc::ElementId(0xDEADBEEF));

  EXPECT_EQ(monitor_.Info(false).size(), 0U);
  EXPECT_EQ(monitor_.Info(true).size(), 0U);

  // Verify that an event with invalid attribution is ignored.
  monitor_.OnEnqueue(WebInputEvent::Type::kKeyDown, {});
  EXPECT_EQ(monitor_.Info(false).size(), 0U);
  EXPECT_EQ(monitor_.Info(true).size(), 0U);

  // Discrete events with a unique attribution should increment the attribution
  // count.
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseDown, focus);
  EXPECT_EQ(monitor_.Info(false).size(), 1U);
  EXPECT_EQ(monitor_.Info(true).size(), 1U);

  // Multiple enqueued events with the same attribution target should not
  // return the attribution twice.
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseUp, focus);
  EXPECT_EQ(monitor_.Info(false).size(), 1U);
  EXPECT_EQ(monitor_.Info(true).size(), 1U);

  // Events with new attribution information should return a new attribution
  // (in this case, continuous).
  monitor_.OnEnqueue(WebInputEvent::Type::kMouseMove, frame);
  EXPECT_EQ(monitor_.Info(false).size(), 1U);
  EXPECT_EQ(monitor_.Info(true).size(), 2U);

  monitor_.OnEnqueue(WebInputEvent::Type::kKeyDown, frame);
  EXPECT_EQ(monitor_.Info(false).size(), 2U);
  EXPECT_EQ(monitor_.Info(true).size(), 2U);

  monitor_.OnDequeue(WebInputEvent::Type::kKeyDown, {});
  EXPECT_EQ(monitor_.Info(false).size(), 2U);
  EXPECT_EQ(monitor_.Info(true).size(), 2U);

  monitor_.OnDequeue(WebInputEvent::Type::kMouseDown, focus);
  EXPECT_EQ(monitor_.Info(false).size(), 2U);
  EXPECT_EQ(monitor_.Info(true).size(), 2U);

  monitor_.OnDequeue(WebInputEvent::Type::kMouseUp, focus);
  EXPECT_EQ(monitor_.Info(false).size(), 1U);
  EXPECT_EQ(monitor_.Info(true).size(), 1U);

  monitor_.OnDequeue(WebInputEvent::Type::kMouseMove, frame);
  EXPECT_EQ(monitor_.Info(false).size(), 1U);
  EXPECT_EQ(monitor_.Info(true).size(), 1U);

  monitor_.OnDequeue(WebInputEvent::Type::kKeyDown, frame);
  EXPECT_EQ(monitor_.Info(false).size(), 0U);
  EXPECT_EQ(monitor_.Info(true).size(), 0U);
}

}  // namespace scheduler
}  // namespace blink

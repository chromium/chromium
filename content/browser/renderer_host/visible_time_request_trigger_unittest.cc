// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <vector>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

namespace content {

namespace {

using ::testing::Optional;

class VisibleTimeRequestTriggerTest : public ::testing::Test {
 protected:
  // Converts a base::TimeDelta into a base::TimeTicks value suitable for
  // storing in the `event_start_time` field of a
  // RecordContentToVisibleTimeRequest.
  static base::TimeTicks StartTimeFromDelta(base::TimeDelta delta) {
    return base::TimeTicks() + delta;
  }
};

TEST_F(VisibleTimeRequestTriggerTest, UpdateAndTakeRequest) {
  VisibleTimeRequestTrigger trigger;
  EXPECT_EQ(trigger.TakeRequest(), std::nullopt);

  // Calling Update then Take should clear the stored request.
  {
    const auto event = blink::VisibleTimeEvent{
        .event_start_time = StartTimeFromDelta(base::Seconds(1)),
        .reason = blink::VisibleTimeEvent::TabSwitchReason{
            .destination_is_loaded = false}};
    const auto expected =
        blink::RecordContentToVisibleTimeRequest{.events = {event}};
    trigger.UpdateRequest(event);
    EXPECT_THAT(trigger.TakeRequest(), Optional(expected));
    EXPECT_EQ(trigger.TakeRequest(), std::nullopt);
  }

  // Calling Update multiple times should return all the requests.
  {
    const auto event1 = blink::VisibleTimeEvent{
        .event_start_time = StartTimeFromDelta(base::Seconds(3)),
        .reason = blink::VisibleTimeEvent::TabSwitchReason{
            .destination_is_loaded = true}};
    const auto event2 = blink::VisibleTimeEvent{
        .event_start_time = StartTimeFromDelta(base::Seconds(2)),
        .reason = blink::VisibleTimeEvent::BFCacheRestoreReason{}};
    const auto expected =
        blink::RecordContentToVisibleTimeRequest{.events = {event1, event2}};
    trigger.UpdateRequest(event1);
    trigger.UpdateRequest(event2);
    EXPECT_THAT(trigger.TakeRequest(), Optional(expected));
    EXPECT_EQ(trigger.TakeRequest(), std::nullopt);
  }
}

}  // namespace

}  // namespace content

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_Empty) {
  RecordContentToVisibleTimeRequest request{.events = {}};
  EXPECT_EQ(request.ExtractTabSwitchEvents(), std::nullopt);
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_NoTabSwitches) {
  auto event =
      VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                       .reason = VisibleTimeEvent::BFCacheRestoreReason{}};
  RecordContentToVisibleTimeRequest request{.events = {event}};
  EXPECT_EQ(request.ExtractTabSwitchEvents(), std::nullopt);
  EXPECT_THAT(request.events, ElementsAre(event));
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_AllTabSwitches) {
  auto event1 = VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                                 .reason = VisibleTimeEvent::TabSwitchReason{
                                     .destination_is_loaded = true}};
  auto event2 = VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                                 .reason = VisibleTimeEvent::TabSwitchReason{
                                     .destination_is_loaded = false}};
  RecordContentToVisibleTimeRequest request{.events = {event1, event2}};
  EXPECT_THAT(
      request.ExtractTabSwitchEvents(),
      Optional(RecordContentToVisibleTimeRequest{.events = {event1, event2}}));
  EXPECT_THAT(request.events, IsEmpty());
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_Mixed) {
  auto event1 = VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                                 .reason = VisibleTimeEvent::TabSwitchReason{
                                     .destination_is_loaded = true}};
  auto event2 =
      VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                       .reason = VisibleTimeEvent::BFCacheRestoreReason{}};
  auto event3 = VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                                 .reason = VisibleTimeEvent::TabSwitchReason{
                                     .destination_is_loaded = false}};
  RecordContentToVisibleTimeRequest request{.events = {event1, event2, event3}};
  EXPECT_THAT(
      request.ExtractTabSwitchEvents(),
      Optional(RecordContentToVisibleTimeRequest{.events = {event1, event3}}));
  EXPECT_THAT(request.events, ElementsAre(event2));
}

}  // namespace blink

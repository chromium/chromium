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

using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_Empty) {
  RecordContentToVisibleTimeRequest request{.events = {}};
  EXPECT_EQ(request.ExtractTabSwitchEventsWithSavedFrame(), std::nullopt);
  EXPECT_FALSE(request.AllEventsAreTabSwitchesWithSavedFrame());
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_NoTabSwitches) {
  auto event1 =
      VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                       .reason = VisibleTimeEvent::BFCacheRestoreReason{}};
  auto event2 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = false, .had_saved_frame_at_start = false}};
  RecordContentToVisibleTimeRequest request{.events = {event1, event2}};
  EXPECT_EQ(request.ExtractTabSwitchEventsWithSavedFrame(), std::nullopt);
  EXPECT_THAT(request.events, UnorderedElementsAre(event1, event2));
  EXPECT_FALSE(request.AllEventsAreTabSwitchesWithSavedFrame());
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_AllTabSwitches) {
  auto event1 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = true, .had_saved_frame_at_start = true}};
  auto event2 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = false, .had_saved_frame_at_start = true}};
  RecordContentToVisibleTimeRequest request{.events = {event1, event2}};
  EXPECT_TRUE(request.AllEventsAreTabSwitchesWithSavedFrame());

  std::optional<RecordContentToVisibleTimeRequest> tab_switch_events =
      request.ExtractTabSwitchEventsWithSavedFrame();
  EXPECT_THAT(
      tab_switch_events,
      Optional(Field("events", &RecordContentToVisibleTimeRequest::events,
                     UnorderedElementsAre(event1, event2))));
  ASSERT_TRUE(tab_switch_events.has_value());
  EXPECT_TRUE(tab_switch_events->AllEventsAreTabSwitchesWithSavedFrame());
  EXPECT_THAT(request.events, IsEmpty());
  EXPECT_FALSE(request.AllEventsAreTabSwitchesWithSavedFrame());
}

TEST(ContentToVisibleTimeRequestTest, ExtractTabSwitchEvents_Mixed) {
  auto event1 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = true, .had_saved_frame_at_start = true}};
  auto event2 =
      VisibleTimeEvent{.event_start_time = base::TimeTicks::Now(),
                       .reason = VisibleTimeEvent::BFCacheRestoreReason{}};
  auto event3 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = false, .had_saved_frame_at_start = true}};
  auto event4 = VisibleTimeEvent{
      .event_start_time = base::TimeTicks::Now(),
      .reason = VisibleTimeEvent::TabSwitchReason{
          .destination_is_loaded = true, .had_saved_frame_at_start = false}};
  RecordContentToVisibleTimeRequest request{
      .events = {event1, event2, event3, event4}};
  EXPECT_FALSE(request.AllEventsAreTabSwitchesWithSavedFrame());

  std::optional<RecordContentToVisibleTimeRequest> tab_switch_events =
      request.ExtractTabSwitchEventsWithSavedFrame();
  EXPECT_THAT(
      tab_switch_events,
      Optional(Field("events", &RecordContentToVisibleTimeRequest::events,
                     UnorderedElementsAre(event1, event3))));
  ASSERT_TRUE(tab_switch_events.has_value());
  EXPECT_TRUE(tab_switch_events->AllEventsAreTabSwitchesWithSavedFrame());
  EXPECT_THAT(request.events, UnorderedElementsAre(event2, event4));
  EXPECT_FALSE(request.AllEventsAreTabSwitchesWithSavedFrame());
}

}  // namespace blink

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_response_types.h"

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace calendar {

TEST(CalendarAPIResponseTypesTest, ParseEventList) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/events.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ("America/Los_Angeles", event_list->time_zone());
  EXPECT_EQ("calendar#events", event_list->kind());
  EXPECT_EQ("\"p32ofplf5q6gf20g\"", event_list->etag());
  EXPECT_EQ(3U, event_list->items().size());

  const CalendarEvent& event = *event_list->items()[0];
  base::Time start_time;
  ASSERT_TRUE(base::Time::FromUTCExploded(
      {2020, 11 /* November */, 1 /* Monday */, 2, 18, 0, 0, 0}, &start_time));
  EXPECT_EQ(start_time, event.start_time().date_time());

  base::Time end_time;
  ASSERT_TRUE(base::Time::FromUTCExploded(
      {2020, 11 /* November */, 1 /* Monday */, 2, 18, 30, 0, 0}, &end_time));
  EXPECT_EQ(end_time, event.end_time().date_time());
  EXPECT_EQ(event.summary(), "Mobile weekly team meeting ");
  EXPECT_EQ(event.id(), "or8221sirt4ogftest");
  EXPECT_EQ(
      event.html_link(),
      "https://www.google.com/calendar/event?eid=b3I4MjIxc2lydDRvZ2Ztest");
  EXPECT_EQ(event.color_id(), "3");
  EXPECT_EQ(event.status(), CalendarEvent::EventStatus::kConfirmed);
  EXPECT_EQ(event.self_response_status(),
            CalendarEvent::ResponseStatus::kNeedsAction);
}

TEST(CalendarAPIResponseTypesTest, ParseConferenceDataUri) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/events.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(3U, event_list->items().size());

  const CalendarEvent& event = *event_list->items()[0];
  EXPECT_EQ(event.conference_data_uri(), "https://meet.google.com/jbe-test");
}

TEST(CalendarAPIResponseTypesTest, ParseMissingConferenceDataUri) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/event_statuses.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(4U, event_list->items().size());

  for (auto& event : event_list->items()) {
    EXPECT_TRUE(event->conference_data_uri().is_empty());
  }
}

TEST(CalendarAPIResponseTypesTest, ParseInvalidConferenceDataUri) {
  std::unique_ptr<base::Value> events = test_util::LoadJSONFile(
      "calendar/event_with_invalid_conference_data_uri.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(1U, event_list->items().size());

  EXPECT_TRUE(event_list->items()[0]->conference_data_uri().is_empty());
}

TEST(CalendarAPIResponseTypesTest, ParseMissingConferenceDataEntryPointType) {
  std::unique_ptr<base::Value> events = test_util::LoadJSONFile(
      "calendar/event_with_missing_entry_point_type.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(1U, event_list->items().size());

  EXPECT_TRUE(event_list->items()[0]->conference_data_uri().is_empty());
}

TEST(CalendarAPIResponseTypesTest, ParseEventListWithCorrectEventStatuses) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/event_statuses.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(4U, event_list->items().size());

  EXPECT_EQ(event_list->items()[0]->status(),
            CalendarEvent::EventStatus::kConfirmed);
  EXPECT_EQ(event_list->items()[1]->status(),
            CalendarEvent::EventStatus::kCancelled);
  EXPECT_EQ(event_list->items()[2]->status(),
            CalendarEvent::EventStatus::kTentative);
  EXPECT_EQ(event_list->items()[3]->status(),
            CalendarEvent::EventStatus::kUnknown);
}

TEST(CalendarAPIResponseTypesTest,
     ParseEventListWithCorrectSelfResponseStatus) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/event_self_response_statuses.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);

  EXPECT_EQ(8U, event_list->items().size());

  EXPECT_EQ(event_list->items()[0]->self_response_status(),
            CalendarEvent::ResponseStatus::kUnknown);
  EXPECT_EQ(event_list->items()[1]->self_response_status(),
            CalendarEvent::ResponseStatus::kAccepted);
  EXPECT_EQ(event_list->items()[2]->self_response_status(),
            CalendarEvent::ResponseStatus::kDeclined);
  EXPECT_EQ(event_list->items()[3]->self_response_status(),
            CalendarEvent::ResponseStatus::kNeedsAction);
  EXPECT_EQ(event_list->items()[4]->self_response_status(),
            CalendarEvent::ResponseStatus::kTentative);
  EXPECT_EQ(event_list->items()[5]->self_response_status(),
            CalendarEvent::ResponseStatus::kAccepted);
  EXPECT_EQ(event_list->items()[6]->self_response_status(),
            CalendarEvent::ResponseStatus::kUnknown);
  EXPECT_EQ(event_list->items()[7]->self_response_status(),
            CalendarEvent::ResponseStatus::kUnknown);
}

TEST(CalendarAPIResponseTypesTest, ParseFailed) {
  std::unique_ptr<base::Value> events =
      test_util::LoadJSONFile("calendar/invalid_events.json");
  ASSERT_TRUE(events.get());

  ASSERT_EQ(base::Value::Type::DICT, events->type());
  auto event_list = EventList::CreateFrom(*events);
  ASSERT_EQ(event_list, nullptr);
}
}  // namespace calendar
}  // namespace google_apis

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "google_apis/common/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {
namespace {

using ::base::JSONReader;

constexpr int64_t kNanosInSecond = 1000000000;
constexpr int64_t kNanosInMinute = kNanosInSecond * 60;
constexpr int64_t kNanosInHour = kNanosInMinute * 60;

}  // namespace

TEST(ClassroomApiCourseWorkResponseTypesTest, ConvertsEmptyResponse) {
  auto raw_course_work = JSONReader::Read("{}");
  ASSERT_TRUE(raw_course_work);

  auto course_work = CourseWork::CreateFrom(raw_course_work.value());
  ASSERT_TRUE(course_work);
  EXPECT_TRUE(course_work->items().empty());
  EXPECT_TRUE(course_work->next_page_token().empty());
}

TEST(ClassroomApiCourseWorkResponseTypesTest, ConvertsCourseWork) {
  const auto raw_course_work = JSONReader::Read(R"(
      {
        "courseWork": [
          {
            "id": "course-work-item-1",
            "title": "Math assignment",
            "state": "PUBLISHED",
            "alternateLink": "https://classroom.google.com/c/abc/a/def/details",
            "creationTime": "2023-07-03T06:55:54.456Z",
            "updateTime": "2023-07-09T06:55:54.456Z"
          },
          {
            "id": "course-work-item-2",
            "title": "Math multiple choice question",
            "state": "DRAFT",
            "alternateLink": "https://classroom.google.com/c/ghi/a/jkl/details",
            "creationTime": "2023-04-03T00:10:55.000Z",
            "updateTime": "2023-04-04T00:10:55.000Z"
          }
        ]
      })");
  ASSERT_TRUE(raw_course_work);

  const auto course_work = CourseWork::CreateFrom(raw_course_work.value());
  ASSERT_TRUE(course_work);
  EXPECT_EQ(course_work->items().size(), 2u);
  EXPECT_TRUE(course_work->next_page_token().empty());

  EXPECT_EQ(course_work->items().at(0)->id(), "course-work-item-1");
  EXPECT_EQ(course_work->items().at(0)->title(), "Math assignment");
  EXPECT_EQ(course_work->items().at(0)->state(),
            CourseWorkItem::State::kPublished);
  EXPECT_EQ(course_work->items().at(0)->alternate_link(),
            "https://classroom.google.com/c/abc/a/def/details");
  EXPECT_FALSE(course_work->items().at(0)->due_date_time());
  EXPECT_FALSE(course_work->items().at(0)->due_date_time());
  EXPECT_EQ(
      util::FormatTimeAsString(course_work->items().at(0)->creation_time()),
      "2023-07-03T06:55:54.456Z");
  EXPECT_EQ(util::FormatTimeAsString(course_work->items().at(0)->last_update()),
            "2023-07-09T06:55:54.456Z");

  EXPECT_EQ(course_work->items().at(1)->id(), "course-work-item-2");
  EXPECT_EQ(course_work->items().at(1)->title(),
            "Math multiple choice question");
  EXPECT_EQ(course_work->items().at(1)->state(), CourseWorkItem::State::kOther);
  EXPECT_EQ(course_work->items().at(1)->alternate_link(),
            "https://classroom.google.com/c/ghi/a/jkl/details");
  EXPECT_FALSE(course_work->items().at(1)->due_date_time());
  EXPECT_FALSE(course_work->items().at(1)->due_date_time());
  EXPECT_EQ(
      util::FormatTimeAsString(course_work->items().at(1)->creation_time()),
      "2023-04-03T00:10:55.000Z");
  EXPECT_EQ(util::FormatTimeAsString(course_work->items().at(1)->last_update()),
            "2023-04-04T00:10:55.000Z");
}

TEST(ClassroomApiCourseWorkResponseTypesTest, ConvertsNextPageToken) {
  const auto raw_course_work = JSONReader::Read(R"(
      {
        "courseWork": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_course_work);

  const auto course_work = CourseWork::CreateFrom(raw_course_work.value());
  ASSERT_TRUE(course_work);
  EXPECT_EQ(course_work->next_page_token(), "qwerty");
}

TEST(ClassroomApiCourseWorkResponseTypesTest,
     ConvertsCourseWorkItemDueDateTime) {
  const auto raw_course_work = JSONReader::Read(R"(
      {
        "courseWork": [
          {
            "id": "date-and-time",
            "dueDate": {"year": 2023, "month": 4, "day": 25},
            "dueTime": {
              "hours": 15,
              "minutes": 9,
              "seconds": 25,
              "nanos": 250000000
            }
          },
          {
            "id": "date-only-with-zeroes",
            "dueDate": {"year": 0, "month": 0, "day": 0}
          },
          {
            "id": "date-and-time-with-partially-missing-components",
            "dueDate": {"year": 2023},
            "dueTime": {"hours": 15}
          }
        ]
      })");
  ASSERT_TRUE(raw_course_work);

  const auto course_work = CourseWork::CreateFrom(raw_course_work.value());
  ASSERT_TRUE(course_work);
  EXPECT_EQ(course_work->items().size(), 3u);

  EXPECT_EQ(course_work->items().at(0)->id(), "date-and-time");
  EXPECT_EQ(course_work->items().at(0)->due_date_time()->year, 2023);
  EXPECT_EQ(course_work->items().at(0)->due_date_time()->month, 4);
  EXPECT_EQ(course_work->items().at(0)->due_date_time()->day, 25);
  EXPECT_EQ(
      course_work->items().at(0)->due_date_time()->time_of_day.InNanoseconds(),
      15 * kNanosInHour + 9 * kNanosInMinute + 25 * kNanosInSecond + 250000000);

  EXPECT_EQ(course_work->items().at(1)->id(), "date-only-with-zeroes");
  EXPECT_EQ(course_work->items().at(1)->due_date_time()->year, 0);
  EXPECT_EQ(course_work->items().at(1)->due_date_time()->month, 0);
  EXPECT_EQ(course_work->items().at(1)->due_date_time()->day, 0);
  EXPECT_TRUE(
      course_work->items().at(1)->due_date_time()->time_of_day.is_zero());

  EXPECT_EQ(course_work->items().at(2)->id(),
            "date-and-time-with-partially-missing-components");
  EXPECT_EQ(course_work->items().at(2)->due_date_time()->year, 2023);
  EXPECT_EQ(course_work->items().at(2)->due_date_time()->month, 0);
  EXPECT_EQ(course_work->items().at(2)->due_date_time()->day, 0);
  EXPECT_EQ(
      course_work->items().at(2)->due_date_time()->time_of_day.InNanoseconds(),
      15 * kNanosInHour);
}

TEST(ClassroomApiCourseWorkResponseTypesTest,
     DoesNotCrashOnUnexpectedResponse) {
  const auto raw_course_work = JSONReader::Read(R"(
      {
        "courseWork": [{"id": []}],
        "nextPageToken": true
      })");
  ASSERT_TRUE(raw_course_work);

  const auto course_work = CourseWork::CreateFrom(raw_course_work.value());
  ASSERT_FALSE(course_work);
}

}  // namespace google_apis::classroom

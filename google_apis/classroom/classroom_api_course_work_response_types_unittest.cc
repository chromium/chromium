// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {

using ::base::JSONReader;

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
            "alternateLink": "https://classroom.google.com/c/abc/a/def/details"
          },
          {
            "id": "course-work-item-2",
            "title": "Math multiple choice question",
            "state": "DRAFT",
            "alternateLink": "https://classroom.google.com/c/ghi/a/jkl/details"
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

  EXPECT_EQ(course_work->items().at(1)->id(), "course-work-item-2");
  EXPECT_EQ(course_work->items().at(1)->title(),
            "Math multiple choice question");
  EXPECT_EQ(course_work->items().at(1)->state(), CourseWorkItem::State::kOther);
  EXPECT_EQ(course_work->items().at(1)->alternate_link(),
            "https://classroom.google.com/c/ghi/a/jkl/details");
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

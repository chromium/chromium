// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_courses_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {

using ::base::JSONReader;

TEST(ClassroomApiCoursesResponseTypesTest, ConvertsEmptyResponse) {
  auto raw_courses = JSONReader::Read("{}");
  ASSERT_TRUE(raw_courses);

  auto courses = Courses::CreateFrom(raw_courses.value());
  ASSERT_TRUE(courses);
  EXPECT_TRUE(courses->items().empty());
  EXPECT_TRUE(courses->next_page_token().empty());
}

TEST(ClassroomApiCoursesResponseTypesTest, ConvertsCourses) {
  const auto raw_courses = JSONReader::Read(R"(
      {
        "courses": [
          {"id": "course-1", "name": "Course Name 1", "courseState": "ACTIVE"},
          {"id": "course-2", "name": "Course Name 2", "courseState": "ARCHIVED"}
        ]
      })");
  ASSERT_TRUE(raw_courses);

  const auto courses = Courses::CreateFrom(raw_courses.value());
  ASSERT_TRUE(courses);
  EXPECT_EQ(courses->items().size(), 2u);
  EXPECT_TRUE(courses->next_page_token().empty());

  EXPECT_EQ(courses->items().at(0)->id(), "course-1");
  EXPECT_EQ(courses->items().at(0)->name(), "Course Name 1");
  EXPECT_EQ(courses->items().at(0)->state(), Course::State::kActive);

  EXPECT_EQ(courses->items().at(1)->id(), "course-2");
  EXPECT_EQ(courses->items().at(1)->name(), "Course Name 2");
  EXPECT_EQ(courses->items().at(1)->state(), Course::State::kOther);
}

TEST(ClassroomApiCoursesResponseTypesTest, ConvertsNextPageToken) {
  const auto raw_courses = JSONReader::Read(R"(
      {
        "courses": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_courses);

  const auto courses = Courses::CreateFrom(raw_courses.value());
  ASSERT_TRUE(courses);
  EXPECT_EQ(courses->next_page_token(), "qwerty");
}

TEST(ClassroomApiCoursesResponseTypesTest, ConvertsCourseStateToString) {
  EXPECT_EQ(Course::StateToString(Course::State::kActive), "ACTIVE");
}

TEST(ClassroomApiCoursesResponseTypesTest, DoesNotCrashOnUnexpectedResponse) {
  const auto raw_courses = JSONReader::Read(R"(
      {
        "courses": [{"id": []}],
        "nextPageToken": true
      })");
  ASSERT_TRUE(raw_courses);

  const auto courses = Courses::CreateFrom(raw_courses.value());
  ASSERT_FALSE(courses);
}

}  // namespace google_apis::classroom

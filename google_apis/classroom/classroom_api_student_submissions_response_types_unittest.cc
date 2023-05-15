// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {

using ::base::JSONReader;

TEST(ClassroomApiStudentSubmissionsResponseTypesTest, ConvertsEmptyResponse) {
  auto raw_student_submissions = JSONReader::Read("{}");
  ASSERT_TRUE(raw_student_submissions);

  auto submissions =
      StudentSubmissions::CreateFrom(raw_student_submissions.value());
  ASSERT_TRUE(submissions);
  EXPECT_TRUE(submissions->items().empty());
  EXPECT_TRUE(submissions->next_page_token().empty());
}

TEST(ClassroomApiStudentSubmissionsResponseTypesTest, ConvertsSubmissions) {
  const auto raw_student_submissions = JSONReader::Read(R"(
      {
        "studentSubmissions": [
          {
            "id": "student-submission-item-1",
            "courseWorkId": "course-work-id",
            "state": "NEW"
          },
          {
            "id": "student-submission-item-2",
            "courseWorkId": "course-work-id",
            "state": "CREATED"
          },
          {
            "id": "student-submission-item-3",
            "courseWorkId": "course-work-id",
            "state": "TURNED_IN",
            "assignedGrade": 3.3
          },
          {
            "id": "student-submission-item-4",
            "courseWorkId": "course-work-id",
            "state": "RETURNED",
            "assignedGrade": 99.0
          },
          {
            "id": "student-submission-item-5",
            "courseWorkId": "course-work-id",
            "state": "RECLAIMED_BY_STUDENT",
            "assignedGrade": 99.99
          }
        ]
      })");
  ASSERT_TRUE(raw_student_submissions);

  const auto submissions =
      StudentSubmissions::CreateFrom(raw_student_submissions.value());
  ASSERT_TRUE(submissions);
  EXPECT_EQ(submissions->items().size(), 5u);
  EXPECT_TRUE(submissions->next_page_token().empty());

  EXPECT_EQ(submissions->items().at(0)->id(), "student-submission-item-1");
  EXPECT_EQ(submissions->items().at(0)->course_work_id(), "course-work-id");
  EXPECT_EQ(submissions->items().at(0)->state(),
            StudentSubmission::State::kNew);
  EXPECT_FALSE(submissions->items().at(0)->assigned_grade());

  EXPECT_EQ(submissions->items().at(1)->id(), "student-submission-item-2");
  EXPECT_EQ(submissions->items().at(1)->course_work_id(), "course-work-id");
  EXPECT_EQ(submissions->items().at(1)->state(),
            StudentSubmission::State::kCreated);
  EXPECT_FALSE(submissions->items().at(1)->assigned_grade());

  EXPECT_EQ(submissions->items().at(2)->id(), "student-submission-item-3");
  EXPECT_EQ(submissions->items().at(2)->course_work_id(), "course-work-id");
  EXPECT_EQ(submissions->items().at(2)->state(),
            StudentSubmission::State::kTurnedIn);
  EXPECT_DOUBLE_EQ(submissions->items().at(2)->assigned_grade().value(), 3.3);

  EXPECT_EQ(submissions->items().at(3)->id(), "student-submission-item-4");
  EXPECT_EQ(submissions->items().at(3)->course_work_id(), "course-work-id");
  EXPECT_EQ(submissions->items().at(3)->state(),
            StudentSubmission::State::kReturned);
  EXPECT_DOUBLE_EQ(submissions->items().at(3)->assigned_grade().value(), 99);

  EXPECT_EQ(submissions->items().at(4)->id(), "student-submission-item-5");
  EXPECT_EQ(submissions->items().at(4)->course_work_id(), "course-work-id");
  EXPECT_EQ(submissions->items().at(4)->state(),
            StudentSubmission::State::kReclaimedByStudent);
  EXPECT_DOUBLE_EQ(submissions->items().at(4)->assigned_grade().value(), 99.99);
}

TEST(ClassroomApiStudentSubmissionsResponseTypesTest, ConvertsNextPageToken) {
  const auto raw_submissions = JSONReader::Read(R"(
      {
        "studentSubmissions": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_submissions);

  const auto submissions =
      StudentSubmissions::CreateFrom(raw_submissions.value());
  ASSERT_TRUE(submissions);
  EXPECT_EQ(submissions->next_page_token(), "qwerty");
}

TEST(ClassroomApiStudentSubmissionsResponseTypesTest,
     DoesNotCrashOnUnexpectedResponse) {
  const auto raw_submissions = JSONReader::Read(R"(
      {
        "studentSubmissions": [{"id": []}],
        "nextPageToken": true
      })");
  ASSERT_TRUE(raw_submissions);

  const auto submissions =
      StudentSubmissions::CreateFrom(raw_submissions.value());
  ASSERT_FALSE(submissions);
}

}  // namespace google_apis::classroom

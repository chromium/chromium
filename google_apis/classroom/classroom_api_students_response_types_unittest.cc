// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_students_response_types.h"

#include <memory>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::classroom {

using ::base::JSONReader;

TEST(ClassroomApiStudentsResponseTypesTest, ConvertsEmptyResponse) {
  auto raw_students = JSONReader::Read("{}");
  ASSERT_TRUE(raw_students);

  auto students = Students::CreateFrom(raw_students.value());
  ASSERT_TRUE(students);
  EXPECT_TRUE(students->items().empty());
  EXPECT_TRUE(students->next_page_token().empty());
}

TEST(ClassroomApiStudentsResponseTypesTest, ConvertsStudents) {
  const auto raw_students = JSONReader::Read(R"(
      {
         "students":[
            {
               "profile":{
                  "id":"student-1",
                  "name":{
                     "fullName":"Student1 full"
                  },
                  "emailAddress":"student1@foo.com",
                  "photoUrl":"//s1"
               }
            },
            {
               "profile":{
                  "id":"student-2",
                  "name":{
                     "fullName":"Student2 full"
                  },
                  "emailAddress":"student2@foo.com",
                  "photoUrl":"//s2"
               }
            }
         ]
      })");
  ASSERT_TRUE(raw_students);

  const auto students = Students::CreateFrom(raw_students.value());
  ASSERT_TRUE(students);
  EXPECT_EQ(students->items().size(), 2u);
  EXPECT_TRUE(students->next_page_token().empty());

  EXPECT_EQ(students->items().at(0)->profile().id(), "student-1");
  EXPECT_EQ(students->items().at(0)->profile().name().full_name(),
            "Student1 full");
  EXPECT_EQ(students->items().at(0)->profile().email_address(),
            "student1@foo.com");
  EXPECT_EQ(students->items().at(0)->profile().photo_url(), GURL("https://s1"));

  EXPECT_EQ(students->items().at(1)->profile().id(), "student-2");
  EXPECT_EQ(students->items().at(1)->profile().name().full_name(),
            "Student2 full");
  EXPECT_EQ(students->items().at(1)->profile().email_address(),
            "student2@foo.com");
  EXPECT_EQ(students->items().at(1)->profile().photo_url(), GURL("https://s2"));
}

TEST(ClassroomApiStudentsResponseTypesTest, ConvertsNextPageToken) {
  const auto raw_students = JSONReader::Read(R"(
      {
        "students": [],
        "nextPageToken": "qwerty"
      })");
  ASSERT_TRUE(raw_students);

  const auto students = Students::CreateFrom(raw_students.value());
  ASSERT_TRUE(students);
  EXPECT_EQ(students->next_page_token(), "qwerty");
}

TEST(ClassroomApiStudentsResponseTypesTest, DoesNotCrashOnUnexpectedResponse) {
  const auto raw_students = JSONReader::Read(R"(
      {
        "students": [{"id": []}],
        "nextPageToken": true
      })");
  ASSERT_TRUE(raw_students);

  const auto students = Students::CreateFrom(raw_students.value());
  ASSERT_FALSE(students);
}

}  // namespace google_apis::classroom

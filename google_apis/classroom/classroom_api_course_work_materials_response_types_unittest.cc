// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_course_work_materials_response_types.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "google_apis/classroom/classroom_api_material_response_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::classroom {

using ::base::JSONReader;

TEST(ClassroomApiCourseWorkMaterialResponseTypesTest, ConvertsEmptyResponse) {
  auto raw_course_work_material =
      JSONReader::Read("{}", base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_course_work_material);

  auto materials =
      CourseWorkMaterial::CreateFrom(raw_course_work_material.value());
  ASSERT_TRUE(materials);
  EXPECT_TRUE(materials->items().empty());
  EXPECT_TRUE(materials->next_page_token().empty());
}

TEST(ClassroomApiCourseWorkMaterialResponseTypesTest,
     ConvertsCourseWorkMaterials) {
  const auto raw_course_work_material =
      JSONReader::Read(R"(
      {
        "courseWorkMaterial": [
          {
            "id": "material-1",
            "title": "Unit 1 Reading",
            "state": "PUBLISHED",
            "alternateLink": "https://classroom.google.com/c/abc/a/def/details",
            "creationTime": "2024-01-01T12:00:00Z",
            "updateTime": "2024-01-02T13:00:00Z",
            "materials": [
              {"youtubeVideo": {"title": "Test Video"}},
              {"driveFile": {"driveFile": {"title": "Test Doc"}}}
            ]
          },
          {
            "id": "material-2",
            "title": "Draft Syllabus",
            "state": "DRAFT"
          }
        ]
      })",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_course_work_material);

  const auto materials =
      CourseWorkMaterial::CreateFrom(raw_course_work_material.value());
  ASSERT_TRUE(materials);
  EXPECT_EQ(materials->items().size(), 2u);
  EXPECT_TRUE(materials->next_page_token().empty());

  const auto* item1 = materials->items().at(0).get();
  EXPECT_EQ(item1->id(), "material-1");
  EXPECT_EQ(item1->title(), "Unit 1 Reading");
  EXPECT_EQ(item1->state(), CourseWorkMaterialItem::State::kPublished);
  EXPECT_EQ(item1->alternate_link(),
            GURL("https://classroom.google.com/c/abc/a/def/details"));

  base::Time creation_time;
  ASSERT_TRUE(base::Time::FromString("2024-01-01T12:00:00Z", &creation_time));
  EXPECT_EQ(item1->creation_time(), creation_time);

  base::Time update_time;
  ASSERT_TRUE(base::Time::FromString("2024-01-02T13:00:00Z", &update_time));
  EXPECT_EQ(item1->last_update(), update_time);

  ASSERT_EQ(item1->materials().size(), 2u);
}

TEST(ClassroomApiCourseWorkMaterialResponseTypesTest, ConvertsNextPageToken) {
  const auto raw_course_work_material =
      JSONReader::Read(R"(
      {
        "courseWorkMaterial": [],
        "nextPageToken": "page-2-token"
      })",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_course_work_material);

  const auto materials =
      CourseWorkMaterial::CreateFrom(raw_course_work_material.value());
  ASSERT_TRUE(materials);
  EXPECT_TRUE(materials->items().empty());
  EXPECT_EQ(materials->next_page_token(), "page-2-token");
}

TEST(ClassroomApiCourseWorkMaterialResponseTypesTest,
     DoesNotCrashOnUnexpectedResponse) {
  const auto raw_course_work_material =
      JSONReader::Read(R"(
      {
        "courseWorkMaterial": [{"id": 12345, "title": []}],
        "nextPageToken": true
      })",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_course_work_material);

  const auto materials =
      CourseWorkMaterial::CreateFrom(raw_course_work_material.value());
  EXPECT_FALSE(materials);
}

}  // namespace google_apis::classroom

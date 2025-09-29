// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_material_response_types.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {

using ::base::JSONReader;

TEST(ClassroomApiMaterialResponseTypesTest, ConvertsAllMaterialTypes) {
  const auto raw_materials =
      JSONReader::Read(R"([
      {"youtubeVideo": {"title": "Test Video"}},
      {"driveFile": {"driveFile": {"title": "Test Doc"}}},
      {"link": {"title": "Test Link"}},
      {"form": {"title": "Test Form"}}
  ])",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_materials);
  ASSERT_TRUE(raw_materials->is_list());

  const auto& material_list = raw_materials->GetList();

  Material youtube_material;
  ASSERT_TRUE(Material::ConvertMaterial(&material_list[0], &youtube_material));
  EXPECT_EQ(youtube_material.title(), "Test Video");
  EXPECT_EQ(youtube_material.type(), Material::Type::kYoutubeVideo);

  Material drive_material;
  ASSERT_TRUE(Material::ConvertMaterial(&material_list[1], &drive_material));
  EXPECT_EQ(drive_material.title(), "Test Doc");
  EXPECT_EQ(drive_material.type(), Material::Type::kSharedDriveFile);

  Material link_material;
  ASSERT_TRUE(Material::ConvertMaterial(&material_list[2], &link_material));
  EXPECT_EQ(link_material.title(), "Test Link");
  EXPECT_EQ(link_material.type(), Material::Type::kLink);

  Material form_material;
  ASSERT_TRUE(Material::ConvertMaterial(&material_list[3], &form_material));
  EXPECT_EQ(form_material.title(), "Test Form");
  EXPECT_EQ(form_material.type(), Material::Type::kForm);
}

TEST(ClassroomApiMaterialResponseTypesTest, HandlesUnknownMaterialType) {
  const auto raw_material =
      JSONReader::Read(R"({
      "someNewApiField": {"title": "Future Feature"}
  })",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_material);

  Material material;
  ASSERT_TRUE(Material::ConvertMaterial(&raw_material.value(), &material));
  EXPECT_EQ(material.type(), Material::Type::kUnknown);
  EXPECT_TRUE(material.title().empty());
}

TEST(ClassroomApiMaterialResponseTypesTest, FailsOnMalformedDriveFile) {
  // Test that a malformed driveFile (missing the nested driveFile object)
  // correctly causes a parsing failure.
  const auto raw_material =
      JSONReader::Read(R"({
      "driveFile": {"title": "This is incorrect"}
  })",
                       base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(raw_material);

  Material material;
  EXPECT_FALSE(Material::ConvertMaterial(&raw_material.value(), &material));
}

}  // namespace google_apis::classroom

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_material_response_types.h"

#include <memory>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {

using ::base::JSONReader;

struct MaterialTestCase {
  std::string test_name;
  std::string json;
  std::string expected_title;
  Material::Type expected_type;
};

using ClassroomApiMaterialResponseTypesTest =
    testing::TestWithParam<MaterialTestCase>;

TEST_P(ClassroomApiMaterialResponseTypesTest, ConvertsMaterialType) {
  const auto& test_case = GetParam();
  Material material;
  const base::Value raw_material = base::test::ParseJson(test_case.json);
  bool result = Material::ConvertMaterial(&raw_material, &material);

  ASSERT_TRUE(result);
  EXPECT_EQ(material.title(), test_case.expected_title);
  EXPECT_EQ(material.type(), test_case.expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClassroomApiMaterialResponseTypesTest,
    testing::Values(
        MaterialTestCase{.test_name = "YoutubeVideo",
                         .json = R"({"youtubeVideo": {"title": "Test Video"}})",
                         .expected_title = "Test Video",
                         .expected_type = Material::Type::kYoutubeVideo},
        MaterialTestCase{
            .test_name = "DriveFile",
            .json = R"({"driveFile": {"driveFile": {"title": "Test Doc"}}})",
            .expected_title = "Test Doc",
            .expected_type = Material::Type::kSharedDriveFile},
        MaterialTestCase{.test_name = "Link",
                         .json = R"({"link": {"title": "Test Link"}})",
                         .expected_title = "Test Link",
                         .expected_type = Material::Type::kLink},
        MaterialTestCase{.test_name = "Form",
                         .json = R"({"form": {"title": "Test Form"}})",
                         .expected_title = "Test Form",
                         .expected_type = Material::Type::kForm},
        MaterialTestCase{
            .test_name = "Unknown",
            .json = R"({"someNewApiField": {"title": "Future Feature"}})",
            .expected_title = "",
            .expected_type = Material::Type::kUnknown},
        MaterialTestCase{
            .test_name = "GuidedLearning",
            .json = R"({"guidedLearning": {"title": "Test Guided Learning"}})",
            .expected_title = "Test Guided Learning",
            .expected_type = Material::Type::kGuidedLearning},
        MaterialTestCase{.test_name = "Notebook",
                         .json = R"({"notebook": {"title": "Test Notebook"}})",
                         .expected_title = "Test Notebook",
                         .expected_type = Material::Type::kNotebook}),
    [](const testing::TestParamInfo<
        ClassroomApiMaterialResponseTypesTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST(ClassroomApiMaterialResponseTypesTest, FailsOnMalformedDriveFile) {
  // Test that a malformed driveFile (missing the nested driveFile object)
  // correctly causes a parsing failure.
  const auto raw_material = base::test::ParseJson(R"({
      "driveFile": {"title": "This is incorrect"}
  })");

  Material material;
  EXPECT_FALSE(Material::ConvertMaterial(&raw_material, &material));
}

}  // namespace google_apis::classroom

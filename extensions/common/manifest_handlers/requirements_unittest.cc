// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using RequirementsManifestTest = ManifestTest;

TEST_F(RequirementsManifestTest, RequirementsInvalid) {
  Testcase testcases[] = {
      Testcase("requirements_invalid_requirements.json",
               "Error at key 'requirements'. Type is invalid. Expected "
               "dictionary, found boolean."),
      Testcase("requirements_invalid_3d.json",
               "Error at key 'requirements.3D'. Type is invalid. Expected "
               "dictionary, found boolean."),
      Testcase("requirements_invalid_3d_features.json",
               "Error at key 'requirements.3D.features'. Type is invalid. "
               "Expected list, found boolean."),
      Testcase("requirements_invalid_3d_features_value.json",
               "Error at key 'requirements.3D.features'. Parsing array failed "
               "at index 0: Specified value 'foo' is invalid."),
      Testcase(
          "requirements_invalid_3d_no_features.json",
          "Error at key 'requirements.3D.features'. Manifest key is required."),
  };

  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(RequirementsManifestTest, RequirementsValid) {
  // Test the defaults.
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "requirements_valid_empty.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, false);

  // Test loading all the requirements.
  extension = LoadAndExpectSuccess("requirements_valid_full.json");
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(RequirementsInfo::GetRequirements(extension.get()).webgl, true);
}

// Tests the deprecated plugin requirement.
TEST_F(RequirementsManifestTest, RequirementsPlugin) {
  RunTestcase({"requirements_invalid_plugins_value.json",
               "Error at key 'requirements.plugins.npapi'. Type is invalid. "
               "Expected boolean, found integer."},
              EXPECT_TYPE_ERROR);

  // Using the plugins requirement should cause an install warning.
  RunTestcase(
      {"requirements_npapi_false.json", errors::kPluginsRequirementDeprecated},
      EXPECT_TYPE_WARNING);

  // Explicitly requesting the npapi requirement should cause an error.
  RunTestcase(
      {"requirements_npapi_true.json", errors::kNPAPIPluginsNotSupported},
      EXPECT_TYPE_ERROR);
}

}  // namespace extensions

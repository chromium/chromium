// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ManifestTest = testing::Test;
using extensions::mojom::ManifestLocation;

namespace extensions {

TEST(ManifestTest, ValidateWarnsOnDiffFingerprintKeyUnpacked) {
  std::vector<InstallWarning> warnings;
  Manifest(ManifestLocation::kUnpacked,
           base::Value::Dict().Set(manifest_keys::kDifferentialFingerprint, ""),
           crx_file::id_util::GenerateId("extid"))
      .ValidateManifest(&warnings);
  EXPECT_EQ(1uL, warnings.size());
  EXPECT_EQ(manifest_errors::kHasDifferentialFingerprint, warnings[0].message);
}

TEST(ManifestTest, ValidateWarnsOnDiffFingerprintKeyCommandLine) {
  std::vector<InstallWarning> warnings;
  Manifest(ManifestLocation::kCommandLine,
           base::Value::Dict().Set(manifest_keys::kDifferentialFingerprint, ""),
           crx_file::id_util::GenerateId("extid"))
      .ValidateManifest(&warnings);
  EXPECT_EQ(1uL, warnings.size());
  EXPECT_EQ(manifest_errors::kHasDifferentialFingerprint, warnings[0].message);
}

TEST(ManifestTest, ValidateSilentOnDiffFingerprintKeyInternal) {
  std::vector<InstallWarning> warnings;
  Manifest(ManifestLocation::kInternal,
           base::Value::Dict().Set(manifest_keys::kDifferentialFingerprint, ""),
           crx_file::id_util::GenerateId("extid"))
      .ValidateManifest(&warnings);
  EXPECT_EQ(0uL, warnings.size());
}

TEST(ManifestTest, ValidateSilentOnNoDiffFingerprintKeyUnpacked) {
  std::vector<InstallWarning> warnings;
  Manifest(ManifestLocation::kUnpacked, base::Value::Dict(),
           crx_file::id_util::GenerateId("extid"))
      .ValidateManifest(&warnings);
  EXPECT_EQ(0uL, warnings.size());
}

TEST(ManifestTest, ValidateSilentOnNoDiffFingerprintKeyInternal) {
  std::vector<InstallWarning> warnings;
  Manifest(ManifestLocation::kInternal, base::Value::Dict(),
           crx_file::id_util::GenerateId("extid"))
      .ValidateManifest(&warnings);
  EXPECT_EQ(0uL, warnings.size());
}

// Tests `Manifest::available_values()` and whether it correctly filters
// keys not available to the manifest.
TEST(ManifestTest, AvailableValues) {
  struct {
    const char* input_manifest;
    const char* expected_available_manifest;
  } test_cases[] =
      // clang-format off
  {
    // In manifest version 2, "host_permissions" key is not available.
    // Additionally "background.service_worker" key is not available to hosted
    // apps.
    {R"(
      {
        "name": "Test Extension",
        "app": {
          "urls": ""
        },
        "background": {
          "service_worker": "service_worker.js"
        },
        "manifest_version": 2,
        "host_permissions": [],
        "nacl_modules": ""
      }
    )",
    R"(
      {
        "name": "Test Extension",
        "app": {
          "urls": ""
        },
        "background": {},
        "manifest_version": 2,
        "nacl_modules": ""
      }
    )"},
    // In manifest version 3, "nacl_modules" key is not available.
    {R"(
      {
        "name": "Test Extension",
        "manifest_version": 3,
        "host_permissions": [],
        "nacl_modules": ""
      }
    )",
      R"(
      {
        "name": "Test Extension",
        "manifest_version": 3,
        "host_permissions": []
      }
    )"}
  };
  // clang-format on

  for (const auto& test_case : test_cases) {
    std::optional<base::Value> manifest_value =
        base::JSONReader::Read(test_case.input_manifest);
    ASSERT_TRUE(manifest_value) << test_case.input_manifest;
    ASSERT_TRUE(manifest_value->is_dict()) << test_case.input_manifest;

    Manifest manifest(ManifestLocation::kInternal,
                      std::move(*manifest_value).TakeDict(),
                      crx_file::id_util::GenerateId("extid"));

    std::optional<base::Value> expected_value =
        base::JSONReader::Read(test_case.expected_available_manifest);
    ASSERT_TRUE(expected_value) << test_case.expected_available_manifest;
    ASSERT_TRUE(expected_value->is_dict());
    EXPECT_EQ(expected_value->GetDict(), manifest.available_values());
  }
}

}  // namespace extensions

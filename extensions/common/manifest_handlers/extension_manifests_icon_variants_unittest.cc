// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class IconVariantsManifestTest : public ManifestTest {
 public:
  IconVariantsManifestTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionIconVariants);
  }

 protected:
  ManifestData GetManifestData(const std::string& icon_variants,
                               int manifest_version = 3) {
    static constexpr char kManifestStub[] =
        R"({
            "name": "Test",
            "version": "0.1",
            "manifest_version": %d,
            "icon_variants": %s
        })";
    return ManifestData::FromJSON(base::StringPrintf(
        kManifestStub, manifest_version, icon_variants.c_str()));
  }

 private:
  const ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
  base::test::ScopedFeatureList feature_list_;
};

// Parse `icon_variants` in manifest.json.
TEST_F(IconVariantsManifestTest, Success) {
  static constexpr struct {
    const char* title;
    const char* icon_variants;
  } test_cases[] = {
      {"Define a `size`.",
       R"([
            {
              "128": "128.png",
            }
          ])"},
      {"Define `any`.",
       R"([
            {
              "any": "any.png",
            }
          ])"},
      {"Define `color_schemes`.",
       R"([
            {
              "16": "16.png",
              "color_schemes": ["dark"]
            }
          ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectSuccess(GetManifestData(test_case.icon_variants));
  }
}

// Cases that could generate warnings after parsing successfully.
// TODO(crbug.com/41419485): Verify optional warnings.
TEST_F(IconVariantsManifestTest, SuccessWithOptionalWarnings) {
  static constexpr struct {
    const char* title;
    const char* icon_variants;
  } test_cases[] = {
      {"An icon size is below the minimum",
       R"([
            {
              "0": "0.png",
              "16": "16.png",
            }
          ])"},
      {"An icon size is above the max",
       R"([
            {
              "2048": "2048.png",
              "2049": "2049.png",
            }
          ])"},
      {"Invalid color_scheme.",
       R"([
            {
              "16": "16.png",
              "color_schemes": ["warning"]
            }
          ])"},
      {"An empty icon variant.",
       R"([
            {
              "16": "16.png",
            },
            {}
          ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectSuccess(GetManifestData(test_case.icon_variants));
    // TODO(crbug.com/344639840): Implement and verify warnings.
  }
}

// Cases that could generate warnings after parsing successfully.
TEST_F(IconVariantsManifestTest, Errors) {
  static constexpr struct {
    const char* title;
    const char* icon_variants;
  } test_cases[] = {
      {"Empty value", "[{}]"},
      {"Empty array", "[]"},
      {"Invalid item type", R"(["error"])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectError(GetManifestData(test_case.icon_variants),
                       "Error: Invalid icon_variants.");
  }
}

using IconVariantsFeatureFreeManifestTest = ManifestTest;

// Test that icon_variants doesn't create an error, even in the event of
// warnings.
TEST_F(IconVariantsFeatureFreeManifestTest, Warnings) {
  LoadAndExpectWarnings("icon_variants.json",
                        {"'icon_variants' requires canary channel or newer, "
                         "but this is the stable channel."});
}

}  // namespace extensions

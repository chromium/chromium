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

struct WarningTestCase {
  const char* title;
  const char* warning;
  const char* icon_variants;
};

// Cases that could generate warnings after parsing successfully.
TEST_F(IconVariantsManifestTest, SuccessWithOptionalWarning) {
  WarningTestCase test_cases[] = {
      {"An icon size is below the minimum", "Icon variant 'size' is not valid.",
       R"([
            {
              "0": "0.png",
              "16": "16.png",
            }
          ])"},
      {"An icon size is above the max.", "Icon variant 'size' is not valid.",
       R"([
            {
              "2048": "2048.png",
              "2049": "2049.png",
            }
          ])"},
      {"An empty icon variant.", "Icon variant is empty.",
       R"([
            {
              "16": "16.png",
            },
            {}
          ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectWarning(GetManifestData(test_case.icon_variants),
                         test_case.warning);
  }
}

// Check cases where there could be multiple warnings.
TEST_F(IconVariantsManifestTest, SuccessWithOptionalWarnings) {
  WarningTestCase test_cases[] = {
      {"`color_schemes` should be an array of strings",
       "Unexpected 'color_schemes' type.",
       R"([
            {
              "16": "16.png",
              "color_schemes": [["type warning"]]
            }
          ])"},
      {"`color_schemes` is expected to be an array of strings.",
       "Unexpected 'color_schemes' type.",
       R"([
            {
              "16": "16.png",
              "color_schemes": "type warning"
            }
          ])"},
      {"Invalid color scheme", "Unexpected 'color_scheme'.",
       R"([
        {
          "16": "16.png",
          "color_schemes": ["warning"]
        }
      ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    std::vector<std::string> warnings({test_case.warning, "Failed to parse."});
    LoadAndExpectWarnings(GetManifestData(test_case.icon_variants), warnings);
  }
}

// Cases that cause errors and prevent the extension from loading.
TEST_F(IconVariantsManifestTest, Errors) {
  static constexpr struct {
    const char* title;
    const char* icon_variants;
  } test_cases[] = {
      {"Empty value", "[{}]"},
      {"Empty array", "[]"},
      {"Invalid item type", R"(["error"])"},
      {"Icon variants are empty (IsEmpty())", R"([
        {
          "empty": "empty.png",
        }
      ])"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    LoadAndExpectError(GetManifestData(test_case.icon_variants),
                       "Error: 'icon_variants' is not valid.");
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

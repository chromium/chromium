// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icon_variants_handler.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/warnings_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Don't enable the icon variants feature. Warn if the key is used, but don't
// create an error.
using NoIconVariantsManifestTest = ManifestTest;

TEST_F(NoIconVariantsManifestTest, Warnings) {
  // Test simple feature's AvailabilityResult::UNSUPPORTED_CHANNEL.
  LoadAndExpectWarning("icon_variants.json",
                       "'icon_variants' requires canary channel or newer, "
                       "but this is the stable channel.");
}

// Enable the icon variants feature.
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

// Cases that would otherwise cause errors and prevent the extension from
// loading. However, `icon_variants` aims to avoid causing errors preventing
// extensions from loading. That makes the key more flexible for later changes.
TEST_F(IconVariantsManifestTest, WarnOnUnrecognizedIconVariants) {
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
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(GetManifestData(test_case.icon_variants));
    ASSERT_FALSE(IconVariantsInfo::HasIconVariants(extension.get()));
    EXPECT_TRUE(warnings_test_util::HasInstallWarning(
        extension, "'icon_variants' is not valid."));
  }
}

TEST_F(IconVariantsManifestTest, PreferIconVariantsOverIcons) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icons": {
          "16": "icons.16.png"
        },
        "icon_variants": [{
          "16": "icon_variants.16.png"
        }]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());
  EXPECT_EQ("icon_variants.16.png",
            icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                      ExtensionIconSet::Match::kExactly));
}

TEST_F(IconVariantsManifestTest, GetIconMethods) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icons": {
          "16": "icons.16.png"
        },
        "icon_variants": [
          {
            "16": "icon_variants.16.png"
          },
          {
            "16": "icon_variants.16.dark.png",
            "color_schemes": ["dark"]
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));

  static constexpr struct {
    const char* title;
    const std::optional<ExtensionIconVariant::ColorScheme> color_scheme;
    const char* expected;
  } test_cases[] = {
      {
          "Light theme icons are returned by default sans color scheme.",
          std::nullopt,
          "icon_variants.16.png",
      },
      {
          "Light theme specified.",
          ExtensionIconVariant::ColorScheme::kLight,
          "icon_variants.16.png",
      },
      {
          "Dark theme specified.",
          ExtensionIconVariant::ColorScheme::kDark,
          "icon_variants.16.dark.png",
      }};

  // GetIcons.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIcons: '%s'", test_case.title));
    const ExtensionIconSet& icons =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIcons(extension.get())
            : IconsInfo::GetIcons(*extension, test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected,
              icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                        ExtensionIconSet::Match::kExactly));
  }

  // GetIconResource.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIconResource: '%s'", test_case.title));
    const ExtensionResource& resource =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIconResource(extension.get(),
                                         extension_misc::EXTENSION_ICON_BITTY,
                                         ExtensionIconSet::Match::kExactly)
            : IconsInfo::GetIconResource(extension.get(),
                                         extension_misc::EXTENSION_ICON_BITTY,
                                         ExtensionIconSet::Match::kExactly,
                                         test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected, resource.relative_path().AsUTF8Unsafe());
  }

  // GetIconURL.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIconURL: '%s'", test_case.title));
    const GURL& icon_url =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIconURL(extension.get(),
                                    extension_misc::EXTENSION_ICON_BITTY,
                                    ExtensionIconSet::Match::kExactly)
            : IconsInfo::GetIconURL(extension.get(),
                                    extension_misc::EXTENSION_ICON_BITTY,
                                    ExtensionIconSet::Match::kExactly,
                                    test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected, icon_url.path().substr(1));
  }
}

// "color_scheme" is not currently supported in singular form. It only generates
// a warning, and not an error. Because known keys are checked first, all
// subsequent keys are assumed to be integer sizes, until a warning such as this
// proves otherwise.
TEST_F(IconVariantsManifestTest, ColorSchemeKeyWarning) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "128": "128.png",
            "color_scheme": ["dark"]
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  ASSERT_TRUE(extension->install_warnings().size() == 1);
  ASSERT_EQ("Icon variant 'size' is not valid.",
            extension->install_warnings().at(0).message);
}

// "color_schemes" is represented as a plural key for manifest.json.
TEST_F(IconVariantsManifestTest, ColorSchemesKeyValid) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "Test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "128": "128.png",
            "color_schemes": ["dark"]
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  ASSERT_TRUE(extension->install_warnings().empty());
}

}  // namespace extensions

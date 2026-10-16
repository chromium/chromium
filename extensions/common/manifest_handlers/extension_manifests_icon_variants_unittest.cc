// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "extensions/buildflags/buildflags.h"
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

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Don't enable the icon variants feature. Warn if the key is used, but don't
// create an error.
using NoIconVariantsManifestTest = ManifestTest;

TEST_F(NoIconVariantsManifestTest, Warnings) {
  // Test simple feature's AvailabilityResult::kUnsupportedChannel.
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
      {"A sized icon path that isn't a string.",
       "'icon_variants' invalid file path.",
       R"([
            {
              "16": 16,
              "32": "32.png",
            }
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
      {"An \"any\" icon path that isn't a string.",
       "'icon_variants' invalid file path.",
       R"([
            {
              "any": ["any.svg"],
              "16": "16.png"
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
      {"Mismatched types", R"([
        {
          "16": "16.png",
          "32": "32.jpg"
        }
      ])"},
      {"Mismatched types with any", R"([
        {
          "any": "icon.svg",
          "16": "16.png"
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

// Tests that an extension using only `icons` still gets an empty path for
// unmatched sizes, since only `icon_variants` can set an "any" fallback.
TEST_F(IconVariantsManifestTest, IconsKeyHasNoAnyFallback) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icons": {
          "16": "icons.16.png"
        }
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());
  EXPECT_TRUE(icons.any().empty());
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kBigger));
  EXPECT_EQ("", icons.Get(8, ExtensionIconSet::Match::kSmaller));
  EXPECT_TRUE(IconsInfo::GetIconResource(extension.get(),
                                         extension_misc::EXTENSION_ICON_MEDIUM,
                                         ExtensionIconSet::Match::kExactly)
                  .empty());
  EXPECT_TRUE(IconsInfo::GetIconURL(extension.get(),
                                    extension_misc::EXTENSION_ICON_MEDIUM,
                                    ExtensionIconSet::Match::kExactly)
                  .is_empty());
}

// Tests that an "any" icon in `icon_variants` isn't returned when the feature
// flag is disabled on a channel that otherwise allows the key, so callers keep
// using `icons` and get an empty path for unmatched sizes.
TEST_F(NoIconVariantsManifestTest, AnyIgnoredWhenDisabled) {
  ScopedCurrentChannel current_channel(version_info::Channel::CANARY);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kExtensionIconVariants);
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icons": {
          "16": "icons.16.png"
        },
        "icon_variants": [{
          "any": "icon_variants.any.svg"
        }]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  EXPECT_FALSE(IconVariantsInfo::HasIconVariants(extension.get()));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());
  EXPECT_EQ("icons.16.png", icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                                      ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::Match::kExactly));
  EXPECT_FALSE(icons.ContainsPath("icon_variants.any.svg"));
}

TEST_F(IconVariantsManifestTest, MultipleDictionaries) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(GetManifestData(
      R"([
        {"16": "icon16.png", "color_schemes": ["dark"]},
        {"48": "icon48.png", "color_schemes": ["dark"]}
      ])")));

  ASSERT_TRUE(extension);
  ASSERT_TRUE(IconVariantsInfo::HasIconVariants(extension.get()));

  const IconVariantsInfo* info =
      IconVariantsInfo::GetIconVariants(*extension.get());
  ASSERT_TRUE(info);

  const ExtensionIconSet& dark_icons =
      info->Get(ExtensionIconVariant::ColorScheme::kDark);
  EXPECT_EQ("icon16.png",
            dark_icons.Get(16, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon48.png",
            dark_icons.Get(48, ExtensionIconSet::Match::kExactly));

  const ExtensionIconSet& light_icons =
      info->Get(ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_TRUE(light_icons.empty());
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
            "16": "icon_variants.16.dark.png",
            "any": "icon_variants.any.dark.png",
            "color_schemes": ["dark"]
          },
          {
            "16": "icon_variants.16.png",
            "any": "icon_variants.any.png"
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));

  static const struct {
    const char* title;
    std::optional<ExtensionIconVariant::ColorScheme> color_scheme;
    const char* expected;
    int requested_size;
  } test_cases[] = {
      {"Dark theme specified.", ExtensionIconVariant::ColorScheme::kDark,
       "icon_variants.16.dark.png", extension_misc::EXTENSION_ICON_BITTY},
      {"Light theme specified.", ExtensionIconVariant::ColorScheme::kLight,
       "icon_variants.16.png", extension_misc::EXTENSION_ICON_BITTY},
      {"No theme specified.", std::nullopt, "icon_variants.16.png",
       extension_misc::EXTENSION_ICON_BITTY},
      {"Any size fallback for dark theme.",
       ExtensionIconVariant::ColorScheme::kDark, "icon_variants.any.dark.png",
       extension_misc::EXTENSION_ICON_LARGE},
      {"Any size fallback for light theme.",
       ExtensionIconVariant::ColorScheme::kLight, "icon_variants.any.png",
       extension_misc::EXTENSION_ICON_LARGE},
  };

  // GetIcons.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIcons: '%s'", test_case.title));
    const ExtensionIconSet& icons =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIcons(extension.get())
            : IconsInfo::GetIcons(*extension, test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected, icons.Get(test_case.requested_size,
                                            ExtensionIconSet::Match::kBigger));
  }

  // GetIconResource.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIconResource: '%s'", test_case.title));
    const ExtensionResource& resource =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIconResource(extension.get(),
                                         test_case.requested_size,
                                         ExtensionIconSet::Match::kBigger)
            : IconsInfo::GetIconResource(extension.get(),
                                         test_case.requested_size,
                                         ExtensionIconSet::Match::kBigger,
                                         test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected, resource.relative_path().AsUTF8Unsafe());
  }

  // GetIconURL.
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("GetIconURL: '%s'", test_case.title));
    const GURL& icon_url =
        !test_case.color_scheme.has_value()
            ? IconsInfo::GetIconURL(extension.get(), test_case.requested_size,
                                    ExtensionIconSet::Match::kBigger)
            : IconsInfo::GetIconURL(extension.get(), test_case.requested_size,
                                    ExtensionIconSet::Match::kBigger,
                                    test_case.color_scheme.value());
    EXPECT_EQ(test_case.expected, icon_url.GetPath().substr(1));
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

TEST_F(IconVariantsManifestTest, GetIconUrlWithSpecialChars) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icons": {
          "16": "#icons.16.png"
        },
        "icon_variants": [
          {
            "16": "#icon_variants.16.dark.png",
            "color_schemes": ["dark"]
          },
          {
            "16": "#icon_variants.16.png"
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));

  const GURL& icon_url = IconsInfo::GetIconURL(
      extension.get(), extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::Match::kExactly);
  EXPECT_EQ("%23icon_variants.16.png", icon_url.GetPath().substr(1));

  const GURL& icon_url_light = IconsInfo::GetIconURL(
      extension.get(), extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::Match::kExactly,
      ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_EQ("%23icon_variants.16.png", icon_url_light.GetPath().substr(1));

  const GURL& icon_url_dark = IconsInfo::GetIconURL(
      extension.get(), extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::Match::kExactly,
      ExtensionIconVariant::ColorScheme::kDark);
  EXPECT_EQ("%23icon_variants.16.dark.png", icon_url_dark.GetPath().substr(1));
}

// Tests loading an extension with only icon_variants defined in manifest.
TEST_F(IconVariantsManifestTest, OnlyIconVariants) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [{
          "48": "icon.png"
        }]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());
  EXPECT_EQ("icon.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                  ExtensionIconSet::Match::kExactly));
}

// Tests icon_variants with "any" SVG key for dark color scheme.
TEST_F(IconVariantsManifestTest, IconVariantsAnySvg) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "128": "icons/128.png",
            "color_schemes": ["light"]
          },
          {
            "any": "icons/spool.svg",
            "color_schemes": ["dark"]
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  const ExtensionIconSet& dark_icons =
      IconsInfo::GetIcons(*extension, ExtensionIconVariant::ColorScheme::kDark);
  EXPECT_EQ("icons/spool.svg",
            dark_icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                           ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/spool.svg",
            dark_icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                           ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/spool.svg",
            dark_icons.Get(38, ExtensionIconSet::Match::kExactly));

  const ExtensionIconSet& light_icons = IconsInfo::GetIcons(
      *extension, ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_EQ("icons/128.png",
            light_icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                            ExtensionIconSet::Match::kExactly));
}

// Tests that an earlier "any" icon doesn't prevent later sized icons from
// being added, and that a "512" icon isn't treated as "any".
TEST_F(IconVariantsManifestTest, AnyDoesNotShadowSizedIcons) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "any": "icons/any.svg"
          },
          {
            "16": "icons/16.png",
            "512": "icons/512.png"
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(
      *extension, ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_EQ("icons/16.png", icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                                      ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/512.png", icons.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                                       ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/any.svg", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                       ExtensionIconSet::Match::kExactly));
}

// Tests that an explicit color scheme overrides a variant without
// `color_schemes`, and that the first explicit variant wins over later ones.
TEST_F(IconVariantsManifestTest, ExplicitColorSchemePrecedence) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "16": "icons/default16.png"
          },
          {
            "any": "icons/default_any.svg"
          },
          {
            "16": "icons/dark16_first.png",
            "color_schemes": ["dark"]
          },
          {
            "any": "icons/dark_any_first.svg",
            "color_schemes": ["dark"]
          },
          {
            "16": "icons/dark16_second.png",
            "color_schemes": ["dark"]
          },
          {
            "any": "icons/dark_any_second.svg",
            "color_schemes": ["dark"]
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));

  const ExtensionIconSet& dark_icons =
      IconsInfo::GetIcons(*extension, ExtensionIconVariant::ColorScheme::kDark);
  EXPECT_EQ("icons/dark16_first.png",
            dark_icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                           ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/dark_any_first.svg", dark_icons.any());

  const ExtensionIconSet& light_icons = IconsInfo::GetIcons(
      *extension, ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_EQ("icons/default16.png",
            light_icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                            ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/default_any.svg", light_icons.any());
}

// Tests that an explicit variant listed before a variant without
// `color_schemes` isn't overwritten by the later variant.
TEST_F(IconVariantsManifestTest, ExplicitBeforeDefaultPrecedence) {
  ManifestData manifest_data = ManifestData::FromJSON(
      R"({
        "name": "test",
        "version": "1",
        "manifest_version": 3,
        "icon_variants": [
          {
            "16": "icons/dark16.svg",
            "any": "icons/dark_any.svg",
            "color_schemes": ["dark"]
          },
          {
            "16": "icons/default16.svg",
            "any": "icons/default_any.svg"
          }
        ]
      })");
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(manifest_data));

  const ExtensionIconSet& dark_icons =
      IconsInfo::GetIcons(*extension, ExtensionIconVariant::ColorScheme::kDark);
  EXPECT_EQ("icons/dark16.svg",
            dark_icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                           ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/dark_any.svg", dark_icons.any());

  const ExtensionIconSet& light_icons = IconsInfo::GetIcons(
      *extension, ExtensionIconVariant::ColorScheme::kLight);
  EXPECT_EQ("icons/default16.svg",
            light_icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                            ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icons/default_any.svg", light_icons.any());
}

}  // namespace extensions

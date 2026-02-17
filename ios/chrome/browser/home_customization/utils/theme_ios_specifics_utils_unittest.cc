// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/home_customization/utils/theme_ios_specifics_utils.h"

#include <string>

#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Constants for mock URLs used in tests.
constexpr char kTestUrl1[] = "http://example.com/1";
constexpr char kTestUrl2[] = "http://example.com/2";

// Constants for mock colors used in tests.
constexpr uint32_t kTestColor1 = 0xff0000;
constexpr uint32_t kTestColor2 = 0x00ff00;

// Constants for mock browser color variants used in tests.
const auto kTestVariant1 =
    sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT;
const auto kTestVariant2 = sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL;

}  // namespace

// Test suite for ThemeIosSpecifics utility functions.
class ThemeIosSpecificsUtilsTest : public testing::Test {
 protected:
  // Helper to create an `NtpCustomBackground` with a specific URL.
  sync_pb::NtpCustomBackground CreateNtpBackground(const std::string& url) {
    sync_pb::NtpCustomBackground background;
    background.set_url(url);
    return background;
  }

  // Helper to create a `UserColorTheme`.
  sync_pb::UserColorTheme CreateUserColorTheme(
      uint32_t color,
      sync_pb::UserColorTheme::BrowserColorVariant variant) {
    sync_pb::UserColorTheme theme;
    theme.set_color(color);
    theme.set_browser_color_variant(variant);
    return theme;
  }
};

// Tests that `AreNtpCustomBackgroundsEquivalent` returns true when URLs match.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsTrueWhenNtpBackgroundUrlsMatch) {
  sync_pb::NtpCustomBackground background1 = CreateNtpBackground(kTestUrl1);
  sync_pb::NtpCustomBackground background2 = CreateNtpBackground(kTestUrl1);

  EXPECT_TRUE(home_customization::AreNtpCustomBackgroundsEquivalent(
      background1, background2));
}

// Tests that `AreNtpCustomBackgroundsEquivalent` returns false when URLs
// differ.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsFalseWhenNtpBackgroundUrlsDiffer) {
  sync_pb::NtpCustomBackground background1 = CreateNtpBackground(kTestUrl1);
  sync_pb::NtpCustomBackground background2 = CreateNtpBackground(kTestUrl2);

  EXPECT_FALSE(home_customization::AreNtpCustomBackgroundsEquivalent(
      background1, background2));
}

// Tests that `AreNtpCustomBackgroundsEquivalent` ignores non-URL fields.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsTrueWhenNtpBackgroundUrlsMatchButOtherFieldsDiffer) {
  sync_pb::NtpCustomBackground background1 = CreateNtpBackground(kTestUrl1);
  background1.set_attribution_line_1("Photographer A");

  sync_pb::NtpCustomBackground background2 = CreateNtpBackground(kTestUrl1);
  background2.set_attribution_line_1("Photographer B");

  EXPECT_TRUE(home_customization::AreNtpCustomBackgroundsEquivalent(
      background1, background2));
}

// Tests that `AreUserColorThemesEquivalent` returns true when color and variant
// match.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsTrueWhenUserColorThemesMatch) {
  sync_pb::UserColorTheme theme1 =
      CreateUserColorTheme(kTestColor1, kTestVariant1);
  sync_pb::UserColorTheme theme2 =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  EXPECT_TRUE(home_customization::AreUserColorThemesEquivalent(theme1, theme2));
}

// Tests that `AreUserColorThemesEquivalent` returns false when colors differ.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsFalseWhenUserThemeColorsDiffer) {
  sync_pb::UserColorTheme theme1 =
      CreateUserColorTheme(kTestColor1, kTestVariant1);
  sync_pb::UserColorTheme theme2 =
      CreateUserColorTheme(kTestColor2, kTestVariant1);

  EXPECT_FALSE(
      home_customization::AreUserColorThemesEquivalent(theme1, theme2));
}

// Tests that `AreUserColorThemesEquivalent` returns false when variants differ.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsFalseWhenUserThemeVariantsDiffer) {
  sync_pb::UserColorTheme theme1 =
      CreateUserColorTheme(kTestColor1, kTestVariant1);
  sync_pb::UserColorTheme theme2 =
      CreateUserColorTheme(kTestColor1, kTestVariant2);

  EXPECT_FALSE(
      home_customization::AreUserColorThemesEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns true for two empty
// protos.
TEST_F(ThemeIosSpecificsUtilsTest, ReturnsTrueWhenBothThemeSpecificsAreEmpty) {
  sync_pb::ThemeIosSpecifics theme1;
  sync_pb::ThemeIosSpecifics theme2;

  EXPECT_TRUE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns false if only one proto
// has an NTP background.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsFalseWhenOnlyOneThemeSpecificsHasNtpBackground) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);

  sync_pb::ThemeIosSpecifics theme2;

  EXPECT_FALSE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns true for matching NTP
// backgrounds.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsTrueWhenThemeSpecificsNtpBackgroundsMatch) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);

  EXPECT_TRUE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns false for differing NTP
// backgrounds.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsFalseWhenThemeSpecificsNtpBackgroundsDiffer) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_ntp_background() = CreateNtpBackground(kTestUrl2);

  EXPECT_FALSE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns false if only one proto
// has a user color theme.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsFalseWhenOnlyOneThemeSpecificsHasUserColorTheme) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  sync_pb::ThemeIosSpecifics theme2;

  EXPECT_FALSE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns true for matching user
// color themes.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsTrueWhenThemeSpecificsUserColorThemesMatch) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  EXPECT_TRUE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns false for differing user
// color themes.
TEST_F(ThemeIosSpecificsUtilsTest,
       ReturnsFalseWhenThemeSpecificsUserColorThemesDiffer) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor2, kTestVariant1);

  EXPECT_FALSE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` ignores user color theme
// differences if NTP backgrounds are present and match (NTP background takes
// precedence).
TEST_F(
    ThemeIosSpecificsUtilsTest,
    ReturnsTrueWhenThemeSpecificsNtpBackgroundsMatchDespiteDifferingColorThemes) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);
  *theme1.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);
  *theme2.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor2, kTestVariant2);

  EXPECT_TRUE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

// Tests that `AreThemeIosSpecificsEquivalent` returns false if NTP backgrounds
// differ, even if user color themes match.
TEST_F(
    ThemeIosSpecificsUtilsTest,
    ReturnsFalseWhenThemeSpecificsNtpBackgroundsDifferDespiteMatchingColorThemes) {
  sync_pb::ThemeIosSpecifics theme1;
  *theme1.mutable_ntp_background() = CreateNtpBackground(kTestUrl1);
  *theme1.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  sync_pb::ThemeIosSpecifics theme2;
  *theme2.mutable_ntp_background() = CreateNtpBackground(kTestUrl2);
  *theme2.mutable_user_color_theme() =
      CreateUserColorTheme(kTestColor1, kTestVariant1);

  EXPECT_FALSE(
      home_customization::AreThemeIosSpecificsEquivalent(theme1, theme2));
}

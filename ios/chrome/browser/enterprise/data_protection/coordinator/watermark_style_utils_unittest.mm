// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_style_utils.h"

#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for `GetWatermarkStyleFromPrefs` utility functions.
class WatermarkStyleUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    enterprise_connectors::RegisterProfilePrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;
};

// Tests that standard valid preference values are correctly mapped to
// WatermarkStyle.
TEST_F(WatermarkStyleUtilsTest, StandardPreferences) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, 35);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, 15);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           24);

  WatermarkStyle style = GetWatermarkStyleFromPrefs(&pref_service_);

  ASSERT_TRUE(style.fill_opacity.has_value());
  EXPECT_NEAR(style.fill_opacity.value(), 0.35f, 0.001f);

  ASSERT_TRUE(style.outline_opacity.has_value());
  EXPECT_NEAR(style.outline_opacity.value(), 0.15f, 0.001f);

  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size.value(), 24);
}

// Tests that values below the minimum allowed range are clamped to safe lower
// bounds.
TEST_F(WatermarkStyleUtilsTest, ClampingLowValues) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, -10);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, -5);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           0);

  WatermarkStyle style = GetWatermarkStyleFromPrefs(&pref_service_);

  ASSERT_TRUE(style.fill_opacity.has_value());
  EXPECT_NEAR(style.fill_opacity.value(), 0.0f, 0.001f);

  ASSERT_TRUE(style.outline_opacity.has_value());
  EXPECT_NEAR(style.outline_opacity.value(), 0.0f, 0.001f);

  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size.value(), 1);
}

// Tests that values above the maximum allowed range are clamped to safe upper
// bounds.
TEST_F(WatermarkStyleUtilsTest, ClampingHighValues) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, 150);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, 200);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           600);

  WatermarkStyle style = GetWatermarkStyleFromPrefs(&pref_service_);

  ASSERT_TRUE(style.fill_opacity.has_value());
  EXPECT_NEAR(style.fill_opacity.value(), 1.0f, 0.001f);

  ASSERT_TRUE(style.outline_opacity.has_value());
  EXPECT_NEAR(style.outline_opacity.value(), 1.0f, 0.001f);

  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size.value(), 500);
}

// Tests that a null PrefService returns a WatermarkStyle where every field is
// std::nullopt.
TEST_F(WatermarkStyleUtilsTest, NullPrefService) {
  WatermarkStyle style = GetWatermarkStyleFromPrefs(nullptr);

  EXPECT_FALSE(style.fill_opacity.has_value());
  EXPECT_FALSE(style.outline_opacity.has_value());
  EXPECT_FALSE(style.font_size.has_value());
}

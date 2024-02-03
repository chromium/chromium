// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

namespace {

class FontFeaturesTest : public testing::Test {};

static const FontOrientation orientations[] = {
    FontOrientation::kHorizontal,
    FontOrientation::kVerticalRotated,
    FontOrientation::kVerticalMixed,
    FontOrientation::kVerticalUpright,
};

class FontFeaturesByOrientationTest
    : public FontFeaturesTest,
      public testing::WithParamInterface<FontOrientation> {
 public:
  FontOrientation GetOrientation() const { return GetParam(); }
  bool IsHorizontal() const { return !IsVerticalAnyUpright(GetOrientation()); }
};

INSTANTIATE_TEST_SUITE_P(FontFeaturesTest,
                         FontFeaturesByOrientationTest,
                         testing::ValuesIn(orientations));

// Test 'chws' or 'vchw' is on by default.
TEST_P(FontFeaturesByOrientationTest, EastAsianContextualSpacingOnByDefault) {
  constexpr hb_tag_t chws = HB_TAG('c', 'h', 'w', 's');
  constexpr hb_tag_t vchw = HB_TAG('v', 'c', 'h', 'w');
  FontDescription font_description;
  font_description.SetOrientation(GetOrientation());
  FontFeatures features;
  features.Initialize(font_description);
  if (IsHorizontal()) {
    EXPECT_EQ(features.FindValueForTesting(chws), 1u);
    EXPECT_EQ(features.FindValueForTesting(vchw), std::nullopt);
  } else {
    EXPECT_EQ(features.FindValueForTesting(chws), std::nullopt);
    EXPECT_EQ(features.FindValueForTesting(vchw), 1u);
  }
}

// If author opted-in or opted-out, it should be honored.
TEST_P(FontFeaturesByOrientationTest,
       EastAsianContextualSpacingHonorsFontFeatureSettings) {
  constexpr hb_tag_t chws = HB_TAG('c', 'h', 'w', 's');
  constexpr hb_tag_t vchw = HB_TAG('v', 'c', 'h', 'w');
  for (unsigned value = 0; value <= 1; ++value) {
    scoped_refptr<FontFeatureSettings> settings = FontFeatureSettings::Create();
    settings->Append({IsHorizontal() ? chws : vchw, static_cast<int>(value)});
    FontDescription font_description;
    font_description.SetOrientation(GetOrientation());
    font_description.SetFeatureSettings(settings);
    FontFeatures features;
    features.Initialize(font_description);
    if (IsHorizontal()) {
      EXPECT_EQ(features.FindValueForTesting(chws), value);
      EXPECT_EQ(features.FindValueForTesting(vchw), std::nullopt);
    } else {
      EXPECT_EQ(features.FindValueForTesting(chws), std::nullopt);
      EXPECT_EQ(features.FindValueForTesting(vchw), value);
    }
  }
}

// Test glyph-width GPOS features that should not enable 'chws'/'vchw'.
TEST_P(FontFeaturesByOrientationTest,
       EastAsianContextualSpacingOffByFeatureSettings) {
  constexpr hb_tag_t chws = HB_TAG('c', 'h', 'w', 's');
  constexpr hb_tag_t vchw = HB_TAG('v', 'c', 'h', 'w');
  const hb_tag_t tags[] = {
      IsHorizontal() ? HB_TAG('h', 'a', 'l', 't') : HB_TAG('v', 'h', 'a', 'l'),
      IsHorizontal() ? HB_TAG('p', 'a', 'l', 't') : HB_TAG('v', 'p', 'a', 'l'),
  };
  for (const hb_tag_t tag : tags) {
    scoped_refptr<FontFeatureSettings> settings = FontFeatureSettings::Create();
    settings->Append({tag, 1});
    FontDescription font_description;
    font_description.SetOrientation(GetOrientation());
    font_description.SetFeatureSettings(settings);
    FontFeatures features;
    features.Initialize(font_description);
    EXPECT_EQ(features.FindValueForTesting(chws), std::nullopt);
    EXPECT_EQ(features.FindValueForTesting(vchw), std::nullopt);
  }
}

// Test the current behavior when multiple glyph-width GPOS features are set via
// `FontFeatureSettings`. Current |FontFeatures| does not resolve conflicts,
// just pass them all as specified to HarfBuzz.
TEST_P(FontFeaturesByOrientationTest, MultipleGlyphWidthGPOS) {
  const hb_tag_t tags[] = {
      HB_TAG('c', 'h', 'w', 's'), HB_TAG('v', 'c', 'h', 'w'),
      HB_TAG('h', 'a', 'l', 't'), HB_TAG('v', 'h', 'a', 'l'),
      HB_TAG('p', 'a', 'l', 't'), HB_TAG('v', 'p', 'a', 'l'),
  };
  scoped_refptr<FontFeatureSettings> settings = FontFeatureSettings::Create();
  for (const hb_tag_t tag : tags)
    settings->Append({tag, 1});
  FontDescription font_description;
  font_description.SetOrientation(GetOrientation());
  font_description.SetFeatureSettings(settings);
  FontFeatures features;
  features.Initialize(font_description);
  // Check all features are enabled.
  for (const hb_tag_t tag : tags)
    EXPECT_EQ(features.FindValueForTesting(tag), 1u);
}

}  // namespace

}  // namespace blink

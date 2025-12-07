// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

#include <hb.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

namespace {

//
// Tests that need `RenderingTest` such as `IsInitial()` are in
// `InlineNodeTest.FontFeature*'.
//
class FontFeaturesTest : public testing::Test {};

std::optional<uint32_t> FindValue(uint32_t tag,
                                  const FontFeatureRanges& features) {
  for (const FontFeatureRange& feature : features) {
    if (feature.tag == tag) {
      return feature.value;
    }
  }
  return std::nullopt;
}

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
  FontFeatureRanges features;
  FontFeatureRange::FromFontDescription(font_description, features);
  if (IsHorizontal()) {
    EXPECT_EQ(FindValue(chws, features), 1u);
    EXPECT_EQ(FindValue(vchw, features), std::nullopt);
  } else {
    EXPECT_EQ(FindValue(chws, features), std::nullopt);
    EXPECT_EQ(FindValue(vchw, features), 1u);
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
    FontFeatureRanges features;
    FontFeatureRange::FromFontDescription(font_description, features);
    if (IsHorizontal()) {
      EXPECT_EQ(FindValue(chws, features), value);
      EXPECT_EQ(FindValue(vchw, features), std::nullopt);
    } else {
      EXPECT_EQ(FindValue(chws, features), std::nullopt);
      EXPECT_EQ(FindValue(vchw, features), value);
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
    FontFeatureRanges features;
    FontFeatureRange::FromFontDescription(font_description, features);
    EXPECT_EQ(FindValue(chws, features), std::nullopt);
    EXPECT_EQ(FindValue(vchw, features), std::nullopt);
  }
}

// Test the current behavior when multiple glyph-width GPOS features are set via
// `FontFeatureSettings`. Current |FontFeatureRanges| does not resolve
// conflicts, just pass them all as specified to HarfBuzz.
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
  FontFeatureRanges features;
  FontFeatureRange::FromFontDescription(font_description, features);
  // Check all features are enabled.
  for (const hb_tag_t tag : tags)
    EXPECT_EQ(FindValue(tag, features), 1u);
}

}  // namespace

}  // namespace blink

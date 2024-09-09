// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

const FontSelectionValue Weights[] = {
    kThinWeightValue,   kExtraLightWeightValue, kLightWeightValue,
    kNormalWeightValue, kMediumWeightValue,     kSemiBoldWeightValue,
    kBoldWeightValue,   kExtraBoldWeightValue,  kBlackWeightValue};

class FontCacheMacTest : public testing::TestWithParam<FontSelectionValue> {
 protected:
  FontDescription CreateFontDescriptionWithFontSynthesisNone(
      FontSelectionValue weight,
      FontSelectionValue style,
      FontSelectionValue stretch) {
    FontDescription font_description;

    font_description.SetWeight(weight);
    font_description.SetStyle(style);
    font_description.SetStretch(stretch);

    font_description.SetSyntheticBold(false);
    font_description.SetSyntheticItalic(false);

    font_description.SetFontSynthesisWeight(
        FontDescription::kAutoFontSynthesisWeight);
    font_description.SetFontSynthesisStyle(
        FontDescription::kAutoFontSynthesisStyle);

    return font_description;
  }

  void TestSystemUISyntheticBold() {
    FontSelectionValue weight = GetParam();

    FontCache& font_cache = FontCache::Get();
    for (size_t size = 5; size <= 30; size++) {
      FontDescription font_description =
          CreateFontDescriptionWithFontSynthesisNone(weight, kNormalSlopeValue,
                                                     kNormalWidthValue);
      const FontPlatformData* font_platform_data =
          font_cache.CreateFontPlatformData(
              font_description,
              FontFaceCreationParams(font_family_names::kSystemUi), size);
      EXPECT_FALSE(font_platform_data->SyntheticBold());
    }
  }
};

INSTANTIATE_TEST_SUITE_P(SystemUISyntheticBold,
                         FontCacheMacTest,
                         testing::ValuesIn(Weights));

TEST_P(FontCacheMacTest, SystemUISyntheticBoldCoreText) {
  TestSystemUISyntheticBold();
}

}  // namespace blink

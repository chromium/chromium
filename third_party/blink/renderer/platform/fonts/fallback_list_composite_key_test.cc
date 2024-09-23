// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/fallback_list_composite_key.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"

namespace blink {

class FallbackListCompositeKeyTest : public ::testing::Test {};

TEST_F(FallbackListCompositeKeyTest, AllFeatures) {
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  FallbackListCompositeKey key_a = FallbackListCompositeKey(font_description);

  // Test every relevant property except font families, which are tested in
  // CompositeKeyFontFamilies. Check that the key is different from
  // a description without the property change and that it is the same upon
  // re-query (i.e. that the key is stable).
  font_description.SetComputedSize(15.0);
  FallbackListCompositeKey key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetSpecifiedSize(16.0);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetAdjustedSize(17.0);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontSizeAdjust font_size_adjust(1.2, FontSizeAdjust::Metric::kCapHeight);
  font_description.SetSizeAdjust(font_size_adjust);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value(8);
  font_description.SetStyle(font_selection_value);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value_weight(1);
  font_description.SetWeight(font_selection_value_weight);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value_stretch(1.2f);
  font_description.SetStretch(font_selection_value_stretch);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetVariantCaps(FontDescription::kPetiteCaps);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontVariantEastAsian font_variant_east_asian =
      FontVariantEastAsian::InitializeFromUnsigned(57u);
  font_description.SetVariantEastAsian(font_variant_east_asian);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontDescription::VariantLigatures variant_ligatures(
      FontDescription::kEnabledLigaturesState);
  font_description.SetVariantLigatures(variant_ligatures);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  FontVariantNumeric font_variant_numeric =
      FontVariantNumeric::InitializeFromUnsigned(171u);
  font_description.SetVariantNumeric(font_variant_numeric);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetIsAbsoluteSize(true);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetGenericFamily(FontDescription::kSerifFamily);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetKerning(FontDescription::kNormalKerning);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetKeywordSize(5);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSmoothing(kAntialiased);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontOpticalSizing(kNoneOpticalSizing);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetTextRendering(kOptimizeLegibility);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetOrientation(FontOrientation::kVerticalMixed);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetWidthVariant(kHalfWidth);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetLocale(&LayoutLocale::GetSystem());
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetSyntheticBold(true);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetSyntheticItalic(true);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisWeight(
      FontDescription::kNoneFontSynthesisWeight);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisStyle(
      FontDescription::kNoneFontSynthesisStyle);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisSmallCaps(
      FontDescription::kNoneFontSynthesisSmallCaps);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  scoped_refptr<FontFeatureSettings> font_feature_setting =
      FontFeatureSettings::Create();
  font_feature_setting->Append(FontFeature(AtomicString("1234"), 2));
  font_description.SetFeatureSettings(font_feature_setting);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  scoped_refptr<FontVariationSettings> font_variation_setting =
      FontVariationSettings::Create();
  font_variation_setting->Append(FontVariationAxis(AtomicString("1234"), 1.5f));
  font_description.SetVariationSettings(font_variation_setting);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetVariantPosition(FontDescription::kSubVariantPosition);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetWordSpacing(1.2);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetLetterSpacing(0.9);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  font_description.SetSubpixelAscentDescent(true);
  key_b = FallbackListCompositeKey(font_description);
  EXPECT_NE(key_a, key_b);
  key_a = FallbackListCompositeKey(font_description);
  EXPECT_EQ(key_a, key_b);

  // HashCategory does not matter for the key
  // FontPalette is not used in the CompositeKey
  // FontVariantAlternates is not used in the CompositeKey
}

TEST_F(FallbackListCompositeKeyTest, FontFamilies) {
  // One family in both descriptors
  FontDescription font_description_a;
  font_description_a.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  FallbackListCompositeKey key_a = FallbackListCompositeKey(font_description_a);

  FontDescription font_description_b;
  font_description_b.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  FallbackListCompositeKey key_b = FallbackListCompositeKey(font_description_b);

  EXPECT_EQ(key_a, key_b);

  // Differing family lists
  scoped_refptr<SharedFontFamily> next_family_a = SharedFontFamily::Create(
      AtomicString("CustomFont1"), FontFamily::Type::kFamilyName);
  font_description_a.SetFamily(FontFamily(font_family_names::kSerif,
                                          FontFamily::Type::kGenericFamily,
                                          next_family_a));
  key_a = FallbackListCompositeKey(font_description_a);
  EXPECT_NE(key_a, key_b);

  // Same family lists with multiple entries
  scoped_refptr<SharedFontFamily> next_family_b = SharedFontFamily::Create(
      AtomicString("CustomFont1"), FontFamily::Type::kFamilyName);
  font_description_b.SetFamily(FontFamily(font_family_names::kSerif,
                                          FontFamily::Type::kGenericFamily,
                                          next_family_b));
  key_b = FallbackListCompositeKey(font_description_b);
  EXPECT_EQ(key_a, key_b);

  // Same number of entries, different names
  next_family_a = SharedFontFamily::Create(AtomicString("CustomFont1a"),
                                           FontFamily::Type::kFamilyName);
  font_description_a.SetFamily(FontFamily(font_family_names::kSerif,
                                          FontFamily::Type::kGenericFamily,
                                          next_family_a));
  key_a = FallbackListCompositeKey(font_description_a);
  next_family_a = SharedFontFamily::Create(AtomicString("CustomFont1b"),
                                           FontFamily::Type::kFamilyName);
  font_description_b.SetFamily(FontFamily(font_family_names::kSerif,
                                          FontFamily::Type::kGenericFamily,
                                          next_family_b));
  key_b = FallbackListCompositeKey(font_description_b);
  EXPECT_NE(key_a, key_b);
}

TEST_F(FallbackListCompositeKeyTest, GenericVsFamily) {
  // Verify that we correctly distinguish between an unquoted
  // CSS generic family and a quoted family name.
  // See crbug.com/1408485
  FontDescription font_description_a;
  font_description_a.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  FallbackListCompositeKey key_a = FallbackListCompositeKey(font_description_a);

  FontDescription font_description_b;
  font_description_b.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kFamilyName));
  FallbackListCompositeKey key_b = FallbackListCompositeKey(font_description_b);

  EXPECT_NE(key_a, key_b);
}

}  // namespace blink

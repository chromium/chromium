/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/font_description.h"

#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontDescriptionTest : public FontTestBase {};

TEST_F(FontDescriptionTest, TestHashCollision) {
  FontSelectionValue weights[] = {
      FontSelectionValue(100), FontSelectionValue(200),
      FontSelectionValue(300), FontSelectionValue(400),
      FontSelectionValue(500), FontSelectionValue(600),
      FontSelectionValue(700), FontSelectionValue(800),
      FontSelectionValue(900)};
  FontSelectionValue stretches[]{
      kUltraCondensedWidthValue, kExtraCondensedWidthValue,
      kCondensedWidthValue,      kSemiCondensedWidthValue,
      kNormalWidthValue,         kSemiExpandedWidthValue,
      kExpandedWidthValue,       kExtraExpandedWidthValue,
      kUltraExpandedWidthValue};

  FontSelectionValue slopes[] = {kNormalSlopeValue, kItalicSlopeValue};

  FontDescription source;
  WTF::Vector<unsigned> hashes;
  for (size_t i = 0; i < std::size(weights); i++) {
    source.SetWeight(weights[i]);
    for (size_t j = 0; j < std::size(stretches); j++) {
      source.SetStretch(stretches[j]);
      for (size_t k = 0; k < std::size(slopes); k++) {
        source.SetStyle(slopes[k]);
        unsigned hash = source.StyleHashWithoutFamilyList();
        ASSERT_FALSE(hashes.Contains(hash));
        hashes.push_back(hash);
      }
    }
  }
}

TEST_F(FontDescriptionTest, VariationSettingsIdenticalCacheKey) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariationSettings> settings_a =
      FontVariationSettings::Create();
  settings_a->Append(FontVariationAxis(AtomicString("test"), 1));

  scoped_refptr<FontVariationSettings> settings_b =
      FontVariationSettings::Create();
  settings_b->Append(FontVariationAxis(AtomicString("test"), 1));

  ASSERT_EQ(*settings_a, *settings_b);

  a.SetVariationSettings(settings_a);
  b.SetVariationSettings(settings_b);

  ASSERT_EQ(a, b);

  FontFaceCreationParams test_creation_params;
  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false);

  ASSERT_EQ(cache_key_a, cache_key_b);
}

TEST_F(FontDescriptionTest, VariationSettingsDifferentCacheKey) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariationSettings> settings_a =
      FontVariationSettings::Create();
  settings_a->Append(FontVariationAxis(AtomicString("test"), 1));

  scoped_refptr<FontVariationSettings> settings_b =
      FontVariationSettings::Create();
  settings_b->Append(FontVariationAxis(AtomicString("0000"), 1));

  ASSERT_NE(*settings_a, *settings_b);

  a.SetVariationSettings(settings_a);
  b.SetVariationSettings(settings_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;

  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false);

  ASSERT_NE(cache_key_a, cache_key_b);

  scoped_refptr<FontVariationSettings> second_settings_a =
      FontVariationSettings::Create();
  second_settings_a->Append(FontVariationAxis(AtomicString("test"), 1));

  scoped_refptr<FontVariationSettings> second_settings_b =
      FontVariationSettings::Create();

  ASSERT_NE(*second_settings_a, *second_settings_b);

  a.SetVariationSettings(second_settings_a);
  b.SetVariationSettings(second_settings_b);

  ASSERT_NE(a, b);

  FontCacheKey second_cache_key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey second_cache_key_b = b.CacheKey(test_creation_params, false);

  ASSERT_NE(second_cache_key_a, second_cache_key_b);
}

TEST_F(FontDescriptionTest, PaletteDifferentCacheKey) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontPalette> palette_a =
      FontPalette::Create(FontPalette::kLightPalette);

  scoped_refptr<FontPalette> palette_b =
      FontPalette::Create(FontPalette::kDarkPalette);

  ASSERT_NE(*palette_a, *palette_b);

  a.SetFontPalette(palette_a);
  b.SetFontPalette(palette_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;

  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false);

  ASSERT_NE(cache_key_a, cache_key_b);
}

TEST_F(FontDescriptionTest, VariantAlternatesDifferentCacheKey) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariantAlternates> variants_a =
      FontVariantAlternates::Create();
  variants_a->SetHistoricalForms();

  scoped_refptr<FontVariantAlternates> variants_b =
      FontVariantAlternates::Create();
  variants_b->SetStyleset({AtomicString("foo"), AtomicString("bar")});

  ASSERT_NE(*variants_a, *variants_b);
  ASSERT_EQ(*variants_a, *variants_a);
  a.SetFontVariantAlternates(variants_a);
  b.SetFontVariantAlternates(variants_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;
  FontCacheKey key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey key_b = b.CacheKey(test_creation_params, false);

  ASSERT_NE(key_a, key_b);
}

TEST_F(FontDescriptionTest, VariantEmojiDifferentCacheKey) {
  FontDescription a;
  FontDescription b(a);

  FontVariantEmoji variant_emoji_a = kEmojiVariantEmoji;
  FontVariantEmoji variant_emoji_b = kUnicodeVariantEmoji;

  a.SetVariantEmoji(variant_emoji_a);
  b.SetVariantEmoji(variant_emoji_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;
  FontCacheKey key_a = a.CacheKey(test_creation_params, false);
  FontCacheKey key_b = b.CacheKey(test_creation_params, false);

  ASSERT_NE(key_a, key_b);
}

TEST_F(FontDescriptionTest, AllFeaturesHash) {
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  unsigned key_a = font_description.GetHash();

  // Test every relevant property except font families, which are tested in
  // CompositeKeyFontFamilies. Check that the key is different from
  // a description without the property change and that it is the same upon
  // re-query (i.e. that the key is stable).
  font_description.SetComputedSize(15.0);
  unsigned key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetSpecifiedSize(16.0);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetAdjustedSize(17.0);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontSizeAdjust font_size_adjust(1.2, FontSizeAdjust::Metric::kCapHeight);
  font_description.SetSizeAdjust(font_size_adjust);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value(8);
  font_description.SetStyle(font_selection_value);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value_weight(1);
  font_description.SetWeight(font_selection_value_weight);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontSelectionValue font_selection_value_stretch(1.2f);
  font_description.SetStretch(font_selection_value_stretch);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetVariantCaps(FontDescription::kPetiteCaps);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontVariantEastAsian font_variant_east_asian =
      FontVariantEastAsian::InitializeFromUnsigned(57u);
  font_description.SetVariantEastAsian(font_variant_east_asian);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontDescription::VariantLigatures variant_ligatures(
      FontDescription::kEnabledLigaturesState);
  font_description.SetVariantLigatures(variant_ligatures);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  FontVariantNumeric font_variant_numeric =
      FontVariantNumeric::InitializeFromUnsigned(171u);
  font_description.SetVariantNumeric(font_variant_numeric);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetIsAbsoluteSize(true);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetGenericFamily(FontDescription::kSerifFamily);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetKerning(FontDescription::kNormalKerning);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetKeywordSize(5);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSmoothing(kAntialiased);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontOpticalSizing(kNoneOpticalSizing);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetTextRendering(kOptimizeLegibility);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetOrientation(FontOrientation::kVerticalMixed);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetWidthVariant(kHalfWidth);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetLocale(&LayoutLocale::GetSystem());
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetSyntheticBold(true);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetSyntheticItalic(true);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisWeight(
      FontDescription::kNoneFontSynthesisWeight);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisStyle(
      FontDescription::kNoneFontSynthesisStyle);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetFontSynthesisSmallCaps(
      FontDescription::kNoneFontSynthesisSmallCaps);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  scoped_refptr<FontFeatureSettings> font_feature_setting =
      FontFeatureSettings::Create();
  font_feature_setting->Append(FontFeature(AtomicString("1234"), 2));
  font_description.SetFeatureSettings(font_feature_setting);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  scoped_refptr<FontVariationSettings> font_variation_setting =
      FontVariationSettings::Create();
  font_variation_setting->Append(FontVariationAxis(AtomicString("1234"), 1.5f));
  font_description.SetVariationSettings(font_variation_setting);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetVariantPosition(FontDescription::kSubVariantPosition);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetWordSpacing(1.2);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetLetterSpacing(0.9);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  font_description.SetSubpixelAscentDescent(true);
  key_b = font_description.GetHash();
  EXPECT_NE(key_a, key_b);
  key_a = font_description.GetHash();
  EXPECT_EQ(key_a, key_b);

  // HashCategory does not matter for the key
  // FontVariantAlternates is not used in the key
}

TEST_F(FontDescriptionTest, FontFamiliesHash) {
  // One family in both descriptors
  FontDescription a;
  FontDescription b(a);

  a.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  b.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));

  unsigned key_a = a.GetHash();
  unsigned key_b = b.GetHash();

  EXPECT_EQ(key_a, key_b);

  // Differing family lists
  scoped_refptr<SharedFontFamily> next_family_a = SharedFontFamily::Create(
      AtomicString("CustomFont1"), FontFamily::Type::kFamilyName);
  a.SetFamily(FontFamily(font_family_names::kSerif,
                         FontFamily::Type::kGenericFamily, next_family_a));
  key_a = a.GetHash();
  EXPECT_NE(key_a, key_b);

  // Same family lists with multiple entries
  scoped_refptr<SharedFontFamily> next_family_b = SharedFontFamily::Create(
      AtomicString("CustomFont1"), FontFamily::Type::kFamilyName);
  b.SetFamily(FontFamily(font_family_names::kSerif,
                         FontFamily::Type::kGenericFamily, next_family_b));
  key_b = b.GetHash();
  EXPECT_EQ(key_a, key_b);

  // Same number of entries, different names
  next_family_a = SharedFontFamily::Create(AtomicString("CustomFont1a"),
                                           FontFamily::Type::kFamilyName);
  a.SetFamily(FontFamily(font_family_names::kSerif,
                         FontFamily::Type::kGenericFamily, next_family_a));
  key_a = a.GetHash();
  next_family_b = SharedFontFamily::Create(AtomicString("CustomFont1b"),
                                           FontFamily::Type::kFamilyName);
  b.SetFamily(FontFamily(font_family_names::kSerif,
                         FontFamily::Type::kGenericFamily, next_family_b));
  key_b = b.GetHash();
  EXPECT_NE(key_a, key_b);
}

TEST_F(FontDescriptionTest, GenericFamilyDifferentHash) {
  // Verify that we correctly distinguish between an unquoted
  // CSS generic family and a quoted family name.
  // See crbug.com/1408485
  FontDescription a;
  FontDescription b(a);

  a.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kGenericFamily));
  b.SetFamily(
      FontFamily(font_family_names::kSerif, FontFamily::Type::kFamilyName));

  unsigned key_a = a.GetHash();
  unsigned key_b = b.GetHash();

  ASSERT_NE(key_a, key_b);
}

TEST_F(FontDescriptionTest, ToString) {
  FontDescription description;

  description.SetFamily(
      FontFamily(AtomicString("A"), FontFamily::Type::kFamilyName,
                 SharedFontFamily::Create(AtomicString("B"),
                                          FontFamily::Type::kFamilyName)));

  description.SetLocale(LayoutLocale::Get(AtomicString("no")));

  scoped_refptr<FontVariationSettings> variation_settings =
      FontVariationSettings::Create();
  variation_settings->Append(FontVariationAxis{AtomicString("aaaa"), 42});
  variation_settings->Append(FontVariationAxis{AtomicString("bbbb"), 8118});
  description.SetVariationSettings(variation_settings);

  scoped_refptr<FontFeatureSettings> feature_settings = FontFeatureSettings::Create();
  feature_settings->Append(FontFeature{AtomicString("cccc"), 76});
  feature_settings->Append(FontFeature{AtomicString("dddd"), 94});
  description.SetFeatureSettings(feature_settings);

  description.SetSpecifiedSize(1.1f);
  description.SetComputedSize(2.2f);
  description.SetAdjustedSize(3.3f);
  description.SetSizeAdjust(
      FontSizeAdjust(4.4f, FontSizeAdjust::Metric::kCapHeight));
  description.SetLetterSpacing(5.5f);
  description.SetWordSpacing(6.6f);

  description.SetStyle(FontSelectionValue(31.5));
  description.SetWeight(FontSelectionValue(32.6));
  description.SetStretch(FontSelectionValue(33.7));

  description.SetTextRendering(kOptimizeLegibility);

  EXPECT_EQ(
      "family_list=[A, B], feature_settings=[cccc=76,dddd=94], "
      "variation_settings=[aaaa=42,bbbb=8118], locale=no, "
      "specified_size=1.100000, computed_size=2.200000, "
      "adjusted_size=3.300000, size_adjust=cap-height 4.4, "
      "letter_spacing=5.500000, word_spacing=6.600000, "
      "font_selection_request=[weight=32.500000, width=33.500000, "
      "slope=31.500000], typesetting_features=[Kerning,Ligatures], "
      "orientation=Horizontal, width_variant=Regular, variant_caps=Normal, "
      "is_absolute_size=false, generic_family=None, kerning=Auto, "
      "variant_ligatures=[common=Normal, discretionary=Normal, "
      "historical=Normal, contextual=Normal], keyword_size=0, "
      "font_smoothing=Auto, text_rendering=OptimizeLegibility, "
      "synthetic_bold=false, synthetic_italic=false, "
      "subpixel_positioning=false, subpixel_ascent_descent=false, "
      "variant_numeric=[numeric_figure=NormalFigure, "
      "numeric_spacing=NormalSpacing, numeric_fraction=Normal, ordinal=Off, "
      "slashed_zero=Off], variant_east_asian=[form=Normal, width=Normal, "
      "ruby=false], font_optical_sizing=Auto, font_synthesis_weight=Auto, "
      "font_synthesis_style=Auto, font_synthesis_small_caps=Auto, "
      "font_variant_position=Normal, font_variant_emoji=Normal",
      description.ToString());
}

// Verifies the correctness of the default hash trait of FontDescription.
TEST_F(FontDescriptionTest, DefaultHashTrait) {
  HashMap<FontDescription, int> map;

  FontDescription description1;

  FontDescription description2;
  description1.SetWeight(FontSelectionValue(100));

  FontDescription description3;
  description3.SetFamily(
      FontFamily(AtomicString("A"), FontFamily::Type::kFamilyName,
                 SharedFontFamily::Create(AtomicString("B"),
                                          FontFamily::Type::kFamilyName)));

  EXPECT_TRUE(map.insert(description1, 1).is_new_entry);
  EXPECT_FALSE(map.insert(description1, 1).is_new_entry);
  EXPECT_EQ(1u, map.size());

  EXPECT_TRUE(map.insert(description2, 2).is_new_entry);
  EXPECT_FALSE(map.insert(description2, 2).is_new_entry);
  EXPECT_EQ(2u, map.size());

  EXPECT_TRUE(map.insert(description3, 3).is_new_entry);
  EXPECT_FALSE(map.insert(description3, 3).is_new_entry);
  EXPECT_EQ(3u, map.size());

  EXPECT_EQ(1, map.at(description1));
  EXPECT_EQ(2, map.at(description2));
  EXPECT_EQ(3, map.at(description3));

  FontDescription not_in_map;
  not_in_map.SetWeight(FontSelectionValue(200));
  EXPECT_FALSE(map.Contains(not_in_map));

  map.erase(description2);
  EXPECT_EQ(2u, map.size());
  EXPECT_TRUE(map.Contains(description1));
  EXPECT_FALSE(map.Contains(description2));
  EXPECT_TRUE(map.Contains(description3));

  map.erase(description3);
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.Contains(description1));
  EXPECT_FALSE(map.Contains(description2));
  EXPECT_FALSE(map.Contains(description3));

  map.erase(description1);
  EXPECT_EQ(0u, map.size());
  EXPECT_FALSE(map.Contains(description1));
  EXPECT_FALSE(map.Contains(description2));
  EXPECT_FALSE(map.Contains(description3));
}

// https://crbug.com/1081017
TEST_F(FontDescriptionTest, NegativeZeroEmFontSize) {
  // 'font-size: -0.0em' sets the following
  FontDescription description1;
  description1.SetSpecifiedSize(-0.0);

  FontDescription description2;
  description2.SetSpecifiedSize(0.0);

  // Equal font descriptions must have equal hash values
  EXPECT_EQ(description1, description2);
  EXPECT_EQ(description1.GetHash(), description2.GetHash());
}

}  // namespace blink

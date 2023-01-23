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
      UltraCondensedWidthValue(), ExtraCondensedWidthValue(),
      CondensedWidthValue(),      SemiCondensedWidthValue(),
      NormalWidthValue(),         SemiExpandedWidthValue(),
      ExpandedWidthValue(),       ExtraExpandedWidthValue(),
      UltraExpandedWidthValue()};

  FontSelectionValue slopes[] = {NormalSlopeValue(), ItalicSlopeValue()};

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

TEST_F(FontDescriptionTest, VariationSettingsIdentical) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariationSettings> settings_a =
      FontVariationSettings::Create();
  settings_a->Append(FontVariationAxis("test", 1));

  scoped_refptr<FontVariationSettings> settings_b =
      FontVariationSettings::Create();
  settings_b->Append(FontVariationAxis("test", 1));

  ASSERT_EQ(*settings_a, *settings_b);

  a.SetVariationSettings(settings_a);
  b.SetVariationSettings(settings_b);

  ASSERT_EQ(a, b);

  FontFaceCreationParams test_creation_params;
  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false, false);

  ASSERT_EQ(cache_key_a, cache_key_b);
}

TEST_F(FontDescriptionTest, VariationSettingsDifferent) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariationSettings> settings_a =
      FontVariationSettings::Create();
  settings_a->Append(FontVariationAxis("test", 1));

  scoped_refptr<FontVariationSettings> settings_b =
      FontVariationSettings::Create();
  settings_b->Append(FontVariationAxis("0000", 1));

  ASSERT_NE(*settings_a, *settings_b);

  a.SetVariationSettings(settings_a);
  b.SetVariationSettings(settings_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;

  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false, false);

  ASSERT_NE(cache_key_a, cache_key_b);

  scoped_refptr<FontVariationSettings> second_settings_a =
      FontVariationSettings::Create();
  second_settings_a->Append(FontVariationAxis("test", 1));

  scoped_refptr<FontVariationSettings> second_settings_b =
      FontVariationSettings::Create();

  ASSERT_NE(*second_settings_a, *second_settings_b);

  a.SetVariationSettings(second_settings_a);
  b.SetVariationSettings(second_settings_b);

  ASSERT_NE(a, b);

  FontCacheKey second_cache_key_a =
      a.CacheKey(test_creation_params, false, false);
  FontCacheKey second_cache_key_b =
      b.CacheKey(test_creation_params, false, false);

  ASSERT_NE(second_cache_key_a, second_cache_key_b);
}

TEST_F(FontDescriptionTest, PaletteDifferent) {
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

  FontCacheKey cache_key_a = a.CacheKey(test_creation_params, false, false);
  FontCacheKey cache_key_b = b.CacheKey(test_creation_params, false, false);

  ASSERT_NE(cache_key_a, cache_key_b);
}

TEST_F(FontDescriptionTest, VariantAlternatesDifferent) {
  FontDescription a;
  FontDescription b(a);

  scoped_refptr<FontVariantAlternates> variants_a =
      FontVariantAlternates::Create();
  variants_a->SetHistoricalForms();

  scoped_refptr<FontVariantAlternates> variants_b =
      FontVariantAlternates::Create();
  variants_b->SetStyleset({"foo", "bar"});

  ASSERT_NE(*variants_a, *variants_b);
  ASSERT_EQ(*variants_a, *variants_a);
  a.SetFontVariantAlternates(variants_a);
  b.SetFontVariantAlternates(variants_b);

  ASSERT_NE(a, b);

  FontFaceCreationParams test_creation_params;
  FontCacheKey key_a = a.CacheKey(test_creation_params, false, false);
  FontCacheKey key_b = b.CacheKey(test_creation_params, false, false);

  ASSERT_NE(key_a, key_b);
}

TEST_F(FontDescriptionTest, ToString) {
  FontDescription description;

  FontFamily family;
  family.SetFamily("A", FontFamily::Type::kFamilyName);
  scoped_refptr<SharedFontFamily> b_family = SharedFontFamily::Create();
  b_family->SetFamily("B", FontFamily::Type::kFamilyName);
  family.AppendFamily(b_family);
  description.SetFamily(family);

  description.SetLocale(LayoutLocale::Get("no"));

  scoped_refptr<FontVariationSettings> variation_settings =
      FontVariationSettings::Create();
  variation_settings->Append(FontVariationAxis{"aaaa", 42});
  variation_settings->Append(FontVariationAxis{"bbbb", 8118});
  description.SetVariationSettings(variation_settings);

  scoped_refptr<FontFeatureSettings> feature_settings = FontFeatureSettings::Create();
  feature_settings->Append(FontFeature{"cccc", 76});
  feature_settings->Append(FontFeature{"dddd", 94});
  description.SetFeatureSettings(feature_settings);

  description.SetSpecifiedSize(1.1f);
  description.SetComputedSize(2.2f);
  description.SetAdjustedSize(3.3f);
  description.SetSizeAdjust(4.4f);
  description.SetLetterSpacing(5.5f);
  description.SetWordSpacing(6.6f);

  description.SetStyle(FontSelectionValue(31.5));
  description.SetWeight(FontSelectionValue(32.6));
  description.SetStretch(FontSelectionValue(33.7));

  description.SetTextRendering(kOptimizeLegibility);

  EXPECT_EQ(
      "family_list=[A, B], feature_settings=[cccc=76,dddd=94], "
      "variation_settings=[aaaa=42,bbbb=8118], locale=no, "
      "specified_size=1.100000, "
      "computed_size=2.200000, adjusted_size=3.300000, size_adjust=4.400000, "
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
      "font_variant_position=Normal",
      description.ToString());
}

// Verifies the correctness of the default hash trait of FontDescription.
TEST_F(FontDescriptionTest, DefaultHashTrait) {
  HashMap<FontDescription, int> map;

  FontDescription description1;

  FontDescription description2;
  description1.SetWeight(FontSelectionValue(100));

  FontFamily family;
  family.SetFamily("A", FontFamily::Type::kFamilyName);
  scoped_refptr<SharedFontFamily> b_family = SharedFontFamily::Create();
  b_family->SetFamily("B", FontFamily::Type::kFamilyName);
  family.AppendFamily(b_family);
  FontDescription description3;
  description3.SetFamily(family);

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

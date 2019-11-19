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

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(FontDescriptionTest, TestHashCollision) {
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
  for (size_t i = 0; i < base::size(weights); i++) {
    source.SetWeight(weights[i]);
    for (size_t j = 0; j < base::size(stretches); j++) {
      source.SetStretch(stretches[j]);
      for (size_t k = 0; k < base::size(slopes); k++) {
        source.SetStyle(slopes[k]);
        unsigned hash = source.StyleHashWithoutFamilyList();
        ASSERT_FALSE(hashes.Contains(hash));
        hashes.push_back(hash);
      }
    }
  }
}

TEST(FontDescriptionTest, ToString) {
  FontDescription description;

  FontFamily family;
  family.SetFamily("A");
  scoped_refptr<SharedFontFamily> b_family = SharedFontFamily::Create();
  b_family->SetFamily("B");
  family.AppendFamily(b_family);
  description.SetFamily(family);

  description.SetLocale(LayoutLocale::Get("no"));

  scoped_refptr<FontVariationSettings> variation_settings =
      FontVariationSettings::Create();
  variation_settings->Append(FontVariationAxis{"a", 42});
  variation_settings->Append(FontVariationAxis{"b", 8118});
  description.SetVariationSettings(variation_settings);

  scoped_refptr<FontFeatureSettings> feature_settings = FontFeatureSettings::Create();
  feature_settings->Append(FontFeature{"c", 76});
  feature_settings->Append(FontFeature{"d", 94});
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
      "family_list=[A,B], feature_settings=[c=76,d=94], "
      "variation_settings=[a=42,b=8118], locale=no, specified_size=1.100000, "
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
      "ruby=false], font_optical_sizing=Auto",
      description.ToString());
}

}  // namespace blink

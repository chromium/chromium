// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class FontFaceCacheTest : public PageTestBase {
  USING_FAST_MALLOC(FontFaceCacheTest);

 protected:
  FontFaceCacheTest() = default;
  ~FontFaceCacheTest() override = default;

  void SetUp() override;

  void ClearCache();
  void AppendTestFaceForCapabilities(const CSSValue& stretch,
                                     const CSSValue& style,
                                     const CSSValue& weight);
  void AppendTestFaceForCapabilities(const CSSValue& stretch,
                                     const CSSValue& style,
                                     const CSSPrimitiveValue& start_weight,
                                     const CSSPrimitiveValue& end_weight);
  FontDescription FontDescriptionForRequest(FontSelectionValue stretch,
                                            FontSelectionValue style,
                                            FontSelectionValue weight);

  FontFaceCache cache_;

  void Trace(blink::Visitor*);

 protected:
  const AtomicString kFontNameForTesting{"Arial"};
};

void FontFaceCacheTest::SetUp() {
  PageTestBase::SetUp();
  ClearCache();
}

void FontFaceCacheTest::ClearCache() {
  cache_.ClearAll();
}

void FontFaceCacheTest::AppendTestFaceForCapabilities(const CSSValue& stretch,
                                                      const CSSValue& style,
                                                      const CSSValue& weight) {
  CSSFontFamilyValue* family_name =
      CSSFontFamilyValue::Create(kFontNameForTesting);
  CSSFontFaceSrcValue* src = CSSFontFaceSrcValue::CreateLocal(
      kFontNameForTesting, kDoNotCheckContentSecurityPolicy,
      OriginClean::kTrue);
  CSSValueList* src_value_list = CSSValueList::CreateCommaSeparated();
  src_value_list->Append(*src);
  CSSPropertyValue properties[] = {
      CSSPropertyValue(GetCSSPropertyFontFamily(), *family_name),
      CSSPropertyValue(GetCSSPropertySrc(), *src_value_list)};
  auto* font_face_descriptor = MakeGarbageCollected<MutableCSSPropertyValueSet>(
      properties, base::size(properties));

  font_face_descriptor->SetProperty(CSSPropertyID::kFontStretch, stretch);
  font_face_descriptor->SetProperty(CSSPropertyID::kFontStyle, style);
  font_face_descriptor->SetProperty(CSSPropertyID::kFontWeight, weight);

  auto* style_rule_font_face =
      MakeGarbageCollected<StyleRuleFontFace>(font_face_descriptor);
  FontFace* font_face = FontFace::Create(&GetDocument(), style_rule_font_face);
  CHECK(font_face);
  cache_.Add(style_rule_font_face, font_face);
}

void FontFaceCacheTest::AppendTestFaceForCapabilities(
    const CSSValue& stretch,
    const CSSValue& style,
    const CSSPrimitiveValue& start_weight,
    const CSSPrimitiveValue& end_weight) {
  CSSValueList* weight_list = CSSValueList::CreateSpaceSeparated();
  weight_list->Append(start_weight);
  weight_list->Append(end_weight);
  AppendTestFaceForCapabilities(stretch, style, *weight_list);
}

FontDescription FontFaceCacheTest::FontDescriptionForRequest(
    FontSelectionValue stretch,
    FontSelectionValue style,
    FontSelectionValue weight) {
  FontFamily font_family;
  font_family.SetFamily(kFontNameForTesting);
  FontDescription description;
  description.SetFamily(font_family);
  description.SetStretch(stretch);
  description.SetStyle(style);
  description.SetWeight(weight);
  return description;
}

TEST_F(FontFaceCacheTest, Instantiate) {
  CSSIdentifierValue* stretch_value_expanded =
      CSSIdentifierValue::Create(CSSValueID::kUltraExpanded);
  CSSIdentifierValue* stretch_value_condensed =
      CSSIdentifierValue::Create(CSSValueID::kCondensed);
  CSSPrimitiveValue* weight_value = CSSNumericLiteralValue::Create(
      BoldWeightValue(), CSSPrimitiveValue::UnitType::kNumber);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kItalic);

  AppendTestFaceForCapabilities(*stretch_value_expanded, *style_value,
                                *weight_value);
  AppendTestFaceForCapabilities(*stretch_value_condensed, *style_value,
                                *weight_value);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);
}

TEST_F(FontFaceCacheTest, SimpleWidthMatch) {
  CSSIdentifierValue* stretch_value_expanded =
      CSSIdentifierValue::Create(CSSValueID::kUltraExpanded);
  CSSIdentifierValue* stretch_value_condensed =
      CSSIdentifierValue::Create(CSSValueID::kCondensed);
  CSSPrimitiveValue* weight_value = CSSNumericLiteralValue::Create(
      NormalWeightValue(), CSSPrimitiveValue::UnitType::kNumber);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  AppendTestFaceForCapabilities(*stretch_value_expanded, *style_value,
                                *weight_value);
  AppendTestFaceForCapabilities(*stretch_value_condensed, *style_value,
                                *weight_value);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_condensed = FontDescriptionForRequest(
      CondensedWidthValue(), NormalSlopeValue(), NormalWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_condensed, kFontNameForTesting);
  ASSERT_TRUE(result);

  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({CondensedWidthValue(), CondensedWidthValue()}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

TEST_F(FontFaceCacheTest, SimpleWeightMatch) {
  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSPrimitiveValue* weight_value_black =
      CSSNumericLiteralValue::Create(900, CSSPrimitiveValue::UnitType::kNumber);
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *weight_value_black);
  CSSPrimitiveValue* weight_value_thin =
      CSSNumericLiteralValue::Create(100, CSSPrimitiveValue::UnitType::kNumber);
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *weight_value_thin);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_bold = FontDescriptionForRequest(
      NormalWidthValue(), NormalSlopeValue(), BoldWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_bold, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({NormalWidthValue(), NormalWidthValue()}));
  ASSERT_EQ(
      result_capabilities.weight,
      FontSelectionRange({FontSelectionValue(900), FontSelectionValue(900)}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

// For each capability, we can either not have it at all, have two of them, or
// have only one of them.
static HeapVector<Member<CSSValue>> AvailableCapabilitiesChoices(
    size_t choice,
    CSSValue* available_values[2]) {
  HeapVector<Member<CSSValue>> available_ones;
  switch (choice) {
    case 0:
      available_ones.push_back(available_values[0]);
      available_ones.push_back(available_values[1]);
      break;
    case 1:
      available_ones.push_back(available_values[0]);
      break;
    case 2:
      available_ones.push_back(available_values[1]);
      break;
  }
  return available_ones;
}

FontSelectionRange ExpectedRangeForChoice(
    FontSelectionValue request,
    size_t choice,
    const Vector<FontSelectionValue>& choices) {
  switch (choice) {
    case 0:
      // Both are available, the request can be matched.
      return FontSelectionRange(request, request);
    case 1:
      return FontSelectionRange(choices[0], choices[0]);
    case 2:
      return FontSelectionRange(choices[1], choices[1]);
    default:
      return FontSelectionRange(FontSelectionValue(0), FontSelectionValue(0));
  }
}

// Flaky; https://crbug.com/871812
TEST_F(FontFaceCacheTest, DISABLED_MatchCombinations) {
  CSSValue* widths[] = {CSSIdentifierValue::Create(CSSValueID::kCondensed),
                        CSSIdentifierValue::Create(CSSValueID::kExpanded)};
  CSSValue* slopes[] = {CSSIdentifierValue::Create(CSSValueID::kNormal),
                        CSSIdentifierValue::Create(CSSValueID::kItalic)};
  CSSValue* weights[] = {
      CSSNumericLiteralValue::Create(100, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(900,
                                     CSSPrimitiveValue::UnitType::kNumber)};

  Vector<FontSelectionValue> width_choices = {CondensedWidthValue(),
                                              ExpandedWidthValue()};
  Vector<FontSelectionValue> slope_choices = {NormalSlopeValue(),
                                              ItalicSlopeValue()};
  Vector<FontSelectionValue> weight_choices = {FontSelectionValue(100),
                                               FontSelectionValue(900)};

  Vector<FontDescription> test_descriptions;
  for (FontSelectionValue width_choice : width_choices) {
    for (FontSelectionValue slope_choice : slope_choices) {
      for (FontSelectionValue weight_choice : weight_choices) {
        test_descriptions.push_back(FontDescriptionForRequest(
            width_choice, slope_choice, weight_choice));
      }
    }
  }

  for (size_t width_choice : {0, 1, 2}) {
    for (size_t slope_choice : {0, 1, 2}) {
      for (size_t weight_choice : {0, 1, 2}) {
        ClearCache();
        for (CSSValue* width :
             AvailableCapabilitiesChoices(width_choice, widths)) {
          for (CSSValue* slope :
               AvailableCapabilitiesChoices(slope_choice, slopes)) {
            for (CSSValue* weight :
                 AvailableCapabilitiesChoices(weight_choice, weights)) {
              AppendTestFaceForCapabilities(*width, *slope, *weight);
            }
          }
        }
        for (FontDescription& test_description : test_descriptions) {
          CSSSegmentedFontFace* result =
              cache_.Get(test_description, kFontNameForTesting);
          ASSERT_TRUE(result);
          FontSelectionCapabilities result_capabilities =
              result->GetFontSelectionCapabilities();
          ASSERT_EQ(result_capabilities.width,
                    ExpectedRangeForChoice(test_description.Stretch(),
                                           width_choice, width_choices));
          ASSERT_EQ(result_capabilities.slope,
                    ExpectedRangeForChoice(test_description.Style(),
                                           slope_choice, slope_choices));
          ASSERT_EQ(result_capabilities.weight,
                    ExpectedRangeForChoice(test_description.Weight(),
                                           weight_choice, weight_choices));
        }
      }
    }
  }
}

TEST_F(FontFaceCacheTest, WidthRangeMatching) {
  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSPrimitiveValue* weight_value_from =
      CSSNumericLiteralValue::Create(700, CSSPrimitiveValue::UnitType::kNumber);
  CSSPrimitiveValue* weight_value_to =
      CSSNumericLiteralValue::Create(800, CSSPrimitiveValue::UnitType::kNumber);
  CSSValueList* weight_list = CSSValueList::CreateSpaceSeparated();
  weight_list->Append(*weight_value_from);
  weight_list->Append(*weight_value_to);
  AppendTestFaceForCapabilities(*stretch_value, *style_value, *weight_list);

  CSSPrimitiveValue* second_weight_value_from =
      CSSNumericLiteralValue::Create(100, CSSPrimitiveValue::UnitType::kNumber);
  CSSPrimitiveValue* second_weight_value_to =
      CSSNumericLiteralValue::Create(200, CSSPrimitiveValue::UnitType::kNumber);
  CSSValueList* second_weight_list = CSSValueList::CreateSpaceSeparated();
  second_weight_list->Append(*second_weight_value_from);
  second_weight_list->Append(*second_weight_value_to);
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *second_weight_list);

  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_bold = FontDescriptionForRequest(
      NormalWidthValue(), NormalSlopeValue(), BoldWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_bold, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({NormalWidthValue(), NormalWidthValue()}));
  ASSERT_EQ(
      result_capabilities.weight,
      FontSelectionRange({FontSelectionValue(700), FontSelectionValue(800)}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

TEST_F(FontFaceCacheTest, WidthRangeMatchingBetween400500) {
  // Two font faces equally far away from a requested font weight of 450.

  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);

  CSSPrimitiveValue* weight_values_lower[] = {
      CSSNumericLiteralValue::Create(600, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(415, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(475, CSSPrimitiveValue::UnitType::kNumber),
  };

  CSSPrimitiveValue* weight_values_upper[] = {
      CSSNumericLiteralValue::Create(610, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(425, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(485, CSSPrimitiveValue::UnitType::kNumber),
  };

  // From https://drafts.csswg.org/css-fonts-4/#font-style-matching: "If the
  // desired weight is inclusively between 400 and 500, weights greater than or
  // equal to the target weight are checked in ascending order until 500 is hit
  // and checked, followed by weights less than the target weight in descending
  // order, followed by weights greater than 500, until a match is found."

  // So, the heavy font should be matched last, after the thin font, and after
  // the font that is slightly bolder than 450.
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *(weight_values_lower[0]),
                                *(weight_values_upper[0]));

  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 1ul);

  FontSelectionValue test_weight(450);

  const FontDescription& description_expanded = FontDescriptionForRequest(
      NormalWidthValue(), NormalSlopeValue(), test_weight);
  CSSSegmentedFontFace* result =
      cache_.Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetFontSelectionCapabilities().weight.minimum,
            FontSelectionValue(600));

  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *(weight_values_lower[1]),
                                *(weight_values_upper[1]));
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  result = cache_.Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetFontSelectionCapabilities().weight.minimum,
            FontSelectionValue(415));

  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *(weight_values_lower[2]),
                                *(weight_values_upper[2]));
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 3ul);

  result = cache_.Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetFontSelectionCapabilities().weight.minimum,
            FontSelectionValue(475));
}

TEST_F(FontFaceCacheTest, StretchRangeMatching) {
  CSSPrimitiveValue* stretch_value_from = CSSNumericLiteralValue::Create(
      65, CSSPrimitiveValue::UnitType::kPercentage);
  CSSPrimitiveValue* stretch_value_to = CSSNumericLiteralValue::Create(
      70, CSSPrimitiveValue::UnitType::kPercentage);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSPrimitiveValue* weight_value =
      CSSNumericLiteralValue::Create(400, CSSPrimitiveValue::UnitType::kNumber);
  CSSValueList* stretch_list = CSSValueList::CreateSpaceSeparated();
  stretch_list->Append(*stretch_value_from);
  stretch_list->Append(*stretch_value_to);
  AppendTestFaceForCapabilities(*stretch_list, *style_value, *weight_value);

  const float kStretchFrom = 110;
  const float kStretchTo = 120;
  CSSPrimitiveValue* second_stretch_value_from = CSSNumericLiteralValue::Create(
      kStretchFrom, CSSPrimitiveValue::UnitType::kPercentage);
  CSSPrimitiveValue* second_stretch_value_to = CSSNumericLiteralValue::Create(
      kStretchTo, CSSPrimitiveValue::UnitType::kPercentage);
  CSSValueList* second_stretch_list = CSSValueList::CreateSpaceSeparated();
  second_stretch_list->Append(*second_stretch_value_from);
  second_stretch_list->Append(*second_stretch_value_to);
  AppendTestFaceForCapabilities(*second_stretch_list, *style_value,
                                *weight_value);

  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_expanded = FontDescriptionForRequest(
      FontSelectionValue(105), NormalSlopeValue(), NormalWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({FontSelectionValue(kStretchFrom),
                                FontSelectionValue(kStretchTo)}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

TEST_F(FontFaceCacheTest, ObliqueRangeMatching) {
  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSPrimitiveValue* weight_value =
      CSSNumericLiteralValue::Create(400, CSSPrimitiveValue::UnitType::kNumber);

  CSSIdentifierValue* oblique_keyword_value =
      CSSIdentifierValue::Create(CSSValueID::kOblique);

  CSSValueList* oblique_range = CSSValueList::CreateCommaSeparated();
  CSSPrimitiveValue* oblique_from =
      CSSNumericLiteralValue::Create(30, CSSPrimitiveValue::UnitType::kNumber);
  CSSPrimitiveValue* oblique_to =
      CSSNumericLiteralValue::Create(35, CSSPrimitiveValue::UnitType::kNumber);
  oblique_range->Append(*oblique_from);
  oblique_range->Append(*oblique_to);
  auto* oblique_value = MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
      *oblique_keyword_value, *oblique_range);

  AppendTestFaceForCapabilities(*stretch_value, *oblique_value, *weight_value);

  CSSValueList* oblique_range_second = CSSValueList::CreateCommaSeparated();
  CSSPrimitiveValue* oblique_from_second =
      CSSNumericLiteralValue::Create(5, CSSPrimitiveValue::UnitType::kNumber);
  CSSPrimitiveValue* oblique_to_second =
      CSSNumericLiteralValue::Create(10, CSSPrimitiveValue::UnitType::kNumber);
  oblique_range_second->Append(*oblique_from_second);
  oblique_range_second->Append(*oblique_to_second);
  auto* oblique_value_second =
      MakeGarbageCollected<cssvalue::CSSFontStyleRangeValue>(
          *oblique_keyword_value, *oblique_range_second);

  AppendTestFaceForCapabilities(*stretch_value, *oblique_value_second,
                                *weight_value);

  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_italic = FontDescriptionForRequest(
      NormalWidthValue(), ItalicSlopeValue(), NormalWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_italic, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({NormalWidthValue(), NormalWidthValue()}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(
      result_capabilities.slope,
      FontSelectionRange({FontSelectionValue(30), FontSelectionValue(35)}));
}

void FontFaceCacheTest::Trace(blink::Visitor* visitor) {
  visitor->Trace(cache_);
}

}  // namespace blink

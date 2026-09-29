// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_cache.h"

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
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
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

class FontFaceCacheTest : public PageTestBase {
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

  Persistent<FontFaceCache> cache_;

 protected:
  const AtomicString kFontNameForTesting{"Arial"};
};

void FontFaceCacheTest::SetUp() {
  PageTestBase::SetUp();
  cache_ = MakeGarbageCollected<FontFaceCache>();
  ClearCache();
}

void FontFaceCacheTest::ClearCache() {
  cache_->ClearAll();
}

void FontFaceCacheTest::AppendTestFaceForCapabilities(const CSSValue& stretch,
                                                      const CSSValue& style,
                                                      const CSSValue& weight) {
  CSSFontFamilyValue* family_name =
      CSSFontFamilyValue::Create(kFontNameForTesting);
  auto* src = CSSFontFaceSrcValue::CreateLocal(kFontNameForTesting);
  CSSValueList* src_value_list = CSSValueList::CreateCommaSeparated();
  src_value_list->Append(*src);
  CSSPropertyValue properties[] = {
      CSSPropertyValue(CSSPropertyName(CSSPropertyID::kFontFamily),
                       *family_name),
      CSSPropertyValue(CSSPropertyName(CSSPropertyID::kSrc), *src_value_list)};
  auto* font_face_descriptor =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(properties);

  font_face_descriptor->SetProperty(CSSPropertyID::kFontStretch, stretch);
  font_face_descriptor->SetProperty(CSSPropertyID::kFontStyle, style);
  font_face_descriptor->SetProperty(CSSPropertyID::kFontWeight, weight);

  auto* style_rule_font_face =
      MakeGarbageCollected<StyleRuleFontFace>(font_face_descriptor);
  CascadeLayered<const StyleRuleFontFace> layered_style_rule_font_face(
      style_rule_font_face,
      /*layer=*/nullptr);
  FontFace* font_face = FontFace::Create(
      &GetDocument(), layered_style_rule_font_face, false /* is_user_style */);
  CHECK(font_face);
  cache_->Add(style_rule_font_face, font_face);
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
  FontDescription description;
  description.SetFamily(FontFamily(
      kFontNameForTesting, FontFamily::InferredTypeFor(kFontNameForTesting)));
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
      kBoldWeightValue, CSSPrimitiveValue::UnitType::kNumber);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kItalic);

  AppendTestFaceForCapabilities(*stretch_value_expanded, *style_value,
                                *weight_value);
  AppendTestFaceForCapabilities(*stretch_value_condensed, *style_value,
                                *weight_value);
  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);
}

TEST_F(FontFaceCacheTest, SimpleWidthMatch) {
  CSSIdentifierValue* stretch_value_expanded =
      CSSIdentifierValue::Create(CSSValueID::kUltraExpanded);
  CSSIdentifierValue* stretch_value_condensed =
      CSSIdentifierValue::Create(CSSValueID::kCondensed);
  CSSPrimitiveValue* weight_value = CSSNumericLiteralValue::Create(
      kNormalWeightValue, CSSPrimitiveValue::UnitType::kNumber);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  AppendTestFaceForCapabilities(*stretch_value_expanded, *style_value,
                                *weight_value);
  AppendTestFaceForCapabilities(*stretch_value_condensed, *style_value,
                                *weight_value);
  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_condensed = FontDescriptionForRequest(
      kCondensedWidthValue, kNormalSlopeValue, kNormalWeightValue);
  CSSSegmentedFontFace* result =
      cache_->Get(description_condensed, kFontNameForTesting);
  ASSERT_TRUE(result);

  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({kCondensedWidthValue, kCondensedWidthValue}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({kNormalWeightValue, kNormalWeightValue}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({kNormalSlopeValue, kNormalSlopeValue}));
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
  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_bold = FontDescriptionForRequest(
      kNormalWidthValue, kNormalSlopeValue, kBoldWeightValue);
  CSSSegmentedFontFace* result =
      cache_->Get(description_bold, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({kNormalWidthValue, kNormalWidthValue}));
  ASSERT_EQ(
      result_capabilities.weight,
      FontSelectionRange({FontSelectionValue(900), FontSelectionValue(900)}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({kNormalSlopeValue, kNormalSlopeValue}));
}

// For each capability, we can either not have it at all, have two of them, or
// have only one of them.
static HeapVector<Member<CSSValue>> AvailableCapabilitiesChoices(
    size_t choice,
    base::span<CSSValue*> available_values) {
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

  Vector<FontSelectionValue> width_choices = {kCondensedWidthValue,
                                              kExpandedWidthValue};
  Vector<FontSelectionValue> slope_choices = {kNormalSlopeValue,
                                              kItalicSlopeValue};
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
              cache_->Get(test_description, kFontNameForTesting);
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

  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_bold = FontDescriptionForRequest(
      kNormalWidthValue, kNormalSlopeValue, kBoldWeightValue);
  CSSSegmentedFontFace* result =
      cache_->Get(description_bold, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({kNormalWidthValue, kNormalWidthValue}));
  ASSERT_EQ(
      result_capabilities.weight,
      FontSelectionRange({FontSelectionValue(700), FontSelectionValue(800)}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({kNormalSlopeValue, kNormalSlopeValue}));
}

TEST_F(FontFaceCacheTest, WidthRangeMatchingBetween400500) {
  // Two font faces equally far away from a requested font weight of 450.

  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);
  CSSIdentifierValue* style_value =
      CSSIdentifierValue::Create(CSSValueID::kNormal);

  auto weight_values_lower = std::to_array<CSSPrimitiveValue*>({
      CSSNumericLiteralValue::Create(600, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(415, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(475, CSSPrimitiveValue::UnitType::kNumber),
  });

  auto weight_values_upper = std::to_array<CSSPrimitiveValue*>({
      CSSNumericLiteralValue::Create(610, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(425, CSSPrimitiveValue::UnitType::kNumber),
      CSSNumericLiteralValue::Create(485, CSSPrimitiveValue::UnitType::kNumber),
  });

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

  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 1ul);

  FontSelectionValue test_weight(450);

  const FontDescription& description_expanded = FontDescriptionForRequest(
      kNormalWidthValue, kNormalSlopeValue, test_weight);
  CSSSegmentedFontFace* result =
      cache_->Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetFontSelectionCapabilities().weight.minimum,
            FontSelectionValue(600));

  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *(weight_values_lower[1]),
                                *(weight_values_upper[1]));
  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  result = cache_->Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  ASSERT_EQ(result->GetFontSelectionCapabilities().weight.minimum,
            FontSelectionValue(415));

  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *(weight_values_lower[2]),
                                *(weight_values_upper[2]));
  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 3ul);

  result = cache_->Get(description_expanded, kFontNameForTesting);
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

  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_expanded = FontDescriptionForRequest(
      FontSelectionValue(105), kNormalSlopeValue, kNormalWeightValue);
  CSSSegmentedFontFace* result =
      cache_->Get(description_expanded, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({FontSelectionValue(kStretchFrom),
                                FontSelectionValue(kStretchTo)}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({kNormalWeightValue, kNormalWeightValue}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({kNormalSlopeValue, kNormalSlopeValue}));
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

  ASSERT_EQ(cache_->GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_italic = FontDescriptionForRequest(
      kNormalWidthValue, kItalicSlopeValue, kNormalWeightValue);
  CSSSegmentedFontFace* result =
      cache_->Get(description_italic, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({kNormalWidthValue, kNormalWidthValue}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({kNormalWeightValue, kNormalWeightValue}));
  ASSERT_EQ(
      result_capabilities.slope,
      FontSelectionRange({FontSelectionValue(30), FontSelectionValue(35)}));
}

namespace {

class FontCacheNotificationTestClient : public EmptyLocalFrameClient {
 public:
  void DidObserveSubresourceLoad(
      const SubresourceLoadMetrics& subresource_load_metrics) override {
    last_subresource_load_metrics_ = subresource_load_metrics;
    did_observe_count_++;
  }

  const std::optional<SubresourceLoadMetrics>& LastSubresourceLoadMetrics()
      const {
    return last_subresource_load_metrics_;
  }
  size_t DidObserveCount() const { return did_observe_count_; }

 private:
  std::optional<SubresourceLoadMetrics> last_subresource_load_metrics_;
  size_t did_observe_count_ = 0;
};

class FakeLocalFontFaceSource : public CSSFontFaceSource {
 public:
  explicit FakeLocalFontFaceSource(
      const AtomicString& font_name = AtomicString("TestLocalFont"))
      : font_name_(font_name) {}

  bool IsLocalFont() const override { return true; }
  const AtomicString& GetLocalFontName() const override { return font_name_; }
  bool IsLocalNonBlocking() const override { return true; }
  bool IsLocalFontAvailable(const FontDescription&) const override {
    return is_available_;
  }
  void SetAvailable(bool available) { is_available_ = available; }

  const SimpleFontData* CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override {
    return nullptr;
  }

 private:
  AtomicString font_name_;
  bool is_available_ = true;
};

class FakeRemoteFontFaceSource : public CSSFontFaceSource {
 public:
  bool IsLocalFont() const override { return false; }
  bool IsLocalNonBlocking() const override { return true; }
  bool IsLocalFontAvailable(const FontDescription&) const override {
    return true;
  }

  const SimpleFontData* CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override {
    return nullptr;
  }
};

class CSSFontFaceFontCacheNotificationTest : public PageTestBase {
 protected:
  void SetUp() override {
    client_ = MakeGarbageCollected<FontCacheNotificationTestClient>();
    PageTestBase::SetupPageWithClients(nullptr, client_);
    cache_ = MakeGarbageCollected<FontFaceCache>();
  }

  FontFace* CreateTestFontFace(bool add_to_cache = true) {
    CSSFontFamilyValue* family_name =
        CSSFontFamilyValue::Create(AtomicString("TestFamily"));
    CSSValueList* src_value_list = CSSValueList::CreateCommaSeparated();
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyName(CSSPropertyID::kFontFamily),
                         *family_name),
        CSSPropertyValue(CSSPropertyName(CSSPropertyID::kSrc),
                         *src_value_list)};
    auto* font_face_descriptor =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(properties);
    auto* style_rule_font_face =
        MakeGarbageCollected<StyleRuleFontFace>(font_face_descriptor);
    CascadeLayered<const StyleRuleFontFace> layered_style_rule_font_face(
        style_rule_font_face, nullptr);
    FontFace* font_face =
        FontFace::Create(&GetDocument(), layered_style_rule_font_face,
                         false /* is_user_style */);
    CHECK(font_face);
    if (add_to_cache) {
      cache_->Add(style_rule_font_face, font_face);
    }
    return font_face;
  }

  uint32_t GetLocalFontCacheLoadCount() const {
    return GetDocument()
        .Fetcher()
        ->GetSubresourceLoadMetricsForTesting()
        .number_of_subresource_loads_from_local_font_cache;
  }

  Persistent<FontCacheNotificationTestClient> client_;
  Persistent<FontFaceCache> cache_;
};

}  // namespace

TEST_F(CSSFontFaceFontCacheNotificationTest,
       LocalFontUpdatesSubresourceMetrics) {
  FontFace* font_face = CreateTestFontFace();
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* local_source = MakeGarbageCollected<FakeLocalFontFaceSource>();
  css_font_face->AddSource(local_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->LastSubresourceLoadMetrics(),
            SubresourceLoadMetrics{
                .number_of_subresource_loads_from_local_font_cache = 1u});
  EXPECT_EQ(client_->DidObserveCount(), 1u);
  EXPECT_EQ(font_face->LoadStatus(), FontFace::kLoaded);

  // Subsequent attempts to load when already loaded do not trigger extra
  // notifications.
  EXPECT_TRUE(css_font_face->MaybeLoadFont(font_description, "TestFamily"));
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->DidObserveCount(), 1u);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       DeduplicatesSameLocalFontNameAcrossFontFaces) {
  FontFace* font_face1 = CreateTestFontFace();
  font_face1->CssFontFace()->AddSource(
      MakeGarbageCollected<FakeLocalFontFaceSource>(
          AtomicString("Google Sans")));

  FontFace* font_face2 = CreateTestFontFace();
  font_face2->CssFontFace()->AddSource(
      MakeGarbageCollected<FakeLocalFontFaceSource>(
          AtomicString("Google Sans")));

  FontFace* font_face3 = CreateTestFontFace();
  font_face3->CssFontFace()->AddSource(
      MakeGarbageCollected<FakeLocalFontFaceSource>(AtomicString("Roboto")));

  FontDescription font_description;
  font_face1->CssFontFace()->Load(font_description);
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->DidObserveCount(), 1u);

  // Loading another @font-face rule with the same local font name ("Google
  // Sans") should be deduplicated within the document.
  font_face2->CssFontFace()->Load(font_description);
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->DidObserveCount(), 1u);

  // Loading a distinct local font name ("Roboto") increments the count to 2.
  font_face3->CssFontFace()->Load(font_description);
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 2u);
  EXPECT_EQ(client_->DidObserveCount(), 2u);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       UnattachedFontFaceUpdatesMetricsWhenLoaded) {
  // Simulate JS FontFace API (`new FontFace(...); font.load()`) before adding
  // to document.fonts (`segmented_font_faces_` is empty).
  FontFace* font_face = CreateTestFontFace(/*add_to_cache=*/false);
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* local_source = MakeGarbageCollected<FakeLocalFontFaceSource>(
      AtomicString("UnattachedLocalFont"));
  css_font_face->AddSource(local_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  EXPECT_EQ(font_face->LoadStatus(), FontFace::kLoaded);
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->DidObserveCount(), 1u);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       RemoteFontDoesNotUpdateLocalFontCacheMetrics) {
  FontFace* font_face = CreateTestFontFace();
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* remote_source = MakeGarbageCollected<FakeRemoteFontFaceSource>();
  css_font_face->AddSource(remote_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  EXPECT_EQ(GetLocalFontCacheLoadCount(), 0u);
  EXPECT_EQ(client_->DidObserveCount(), 0u);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       UnavailableLocalFontFallsBackToRemote) {
  FontFace* font_face = CreateTestFontFace();
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* local_source = MakeGarbageCollected<FakeLocalFontFaceSource>();
  local_source->SetAvailable(false);
  auto* remote_source = MakeGarbageCollected<FakeRemoteFontFaceSource>();

  css_font_face->AddSource(local_source);
  css_font_face->AddSource(remote_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  // Local font was unavailable, fell back to remote font; no font cache
  // metric should be recorded.
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 0u);
  EXPECT_EQ(client_->DidObserveCount(), 0u);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       UnavailableLocalFontFallsBackToAvailableLocalFont) {
  FontFace* font_face = CreateTestFontFace();
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* unavailable_local_source =
      MakeGarbageCollected<FakeLocalFontFaceSource>(
          AtomicString("UnavailableFont"));
  unavailable_local_source->SetAvailable(false);
  auto* available_local_source = MakeGarbageCollected<FakeLocalFontFaceSource>(
      AtomicString("AvailableFallbackFont"));
  available_local_source->SetAvailable(true);

  css_font_face->AddSource(unavailable_local_source);
  css_font_face->AddSource(available_local_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  EXPECT_EQ(GetLocalFontCacheLoadCount(), 1u);
  EXPECT_EQ(client_->DidObserveCount(), 1u);
  EXPECT_EQ(font_face->LoadStatus(), FontFace::kLoaded);
}

TEST_F(CSSFontFaceFontCacheNotificationTest,
       UnavailableLocalFontErrorDoesNotUpdateMetrics) {
  FontFace* font_face = CreateTestFontFace();
  CSSFontFace* css_font_face = font_face->CssFontFace();

  auto* unavailable_local_source =
      MakeGarbageCollected<FakeLocalFontFaceSource>();
  unavailable_local_source->SetAvailable(false);
  css_font_face->AddSource(unavailable_local_source);

  FontDescription font_description;
  css_font_face->Load(font_description);

  EXPECT_EQ(font_face->LoadStatus(), FontFace::kError);
  EXPECT_EQ(GetLocalFontCacheLoadCount(), 0u);
  EXPECT_EQ(client_->DidObserveCount(), 0u);
}

}  // namespace blink

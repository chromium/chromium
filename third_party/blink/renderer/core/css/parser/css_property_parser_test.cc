// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

static int ComputeNumberOfTracks(const CSSValueList* value_list) {
  int number_of_tracks = 0;
  for (auto& value : *value_list) {
    if (value->IsGridLineNamesValue())
      continue;
    ++number_of_tracks;
  }
  return number_of_tracks;
}

static bool IsValidPropertyValueForStyleRule(CSSPropertyID property_id,
                                             const String& value) {
  CSSTokenizer tokenizer(value);
  const auto tokens = tokenizer.TokenizeToEOF();
  const CSSParserTokenRange range(tokens);
  HeapVector<CSSPropertyValue, 256> parsed_properties;
  return CSSPropertyParser::ParseValue(
      property_id, false, range,
      StrictCSSParserContext(SecureContextMode::kSecureContext),
      parsed_properties, StyleRule::RuleType::kStyle);
}

TEST(CSSPropertyParserTest, CSSPaint_Functions) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo, func1(1px, 3px), red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo, func1(1px, 3px), red)");
}

TEST(CSSPropertyParserTest, CSSPaint_NoArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo)");
}

TEST(CSSPropertyParserTest, CSSPaint_ValidArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(bar, 10px, red)");
}

TEST(CSSPropertyParserTest, CSSPaint_InvalidFormat) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo bar)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  // Illegal format should not be parsed.
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_TrailingComma) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red,)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_PaintArgumentsDiabled) {
  ScopedCSSPaintAPIArgumentsForTest css_paint_api_arguments(false);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, GridTrackLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit9) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit10) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit11) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit12) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit13) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit14) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit15) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit16) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

static int GetGridPositionInteger(const CSSValue& value) {
  DCHECK(value.IsValueList());
  const auto& list = ToCSSValueList(value);
  DCHECK_EQ(list.length(), static_cast<size_t>(1));
  const auto& primitive_value = ToCSSPrimitiveValue(list.Item(0));
  DCHECK(primitive_value.IsNumber());
  return primitive_value.GetIntValue();
}

TEST(CSSPropertyParserTest, GridPositionLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnStart, "999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 999);
}

TEST(CSSPropertyParserTest, GridPositionLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnEnd, "1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowStart, "1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowEnd, "5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnStart, "-999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -999);
}

TEST(CSSPropertyParserTest, GridPositionLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnEnd, "-1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowStart, "-1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowEnd, "-5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, ColorFunction) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundColor, "rgba(0, 0, 0, 1)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  EXPECT_EQ(Color::kBlack, cssvalue::ToCSSColorValue(*value).Value());
}

TEST(CSSPropertyParserTest, IncompleteColor) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundColor, "rgba(123 45",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, ClipPathEllipse) {
  std::unique_ptr<DummyPageHolder> dummy_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  Document* doc = &dummy_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());
  CSSParserContext* context = CSSParserContext::Create(
      kHTMLStandardMode, SecureContextMode::kSecureContext,
      CSSParserContext::kLiveProfile, doc);

  CSSParser::ParseSingleValue(CSSPropertyClipPath,
                              "ellipse(1px 2px at invalid)", context);

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseTwoRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse(1px 2px)", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseTwoRadius));

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseOneRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse(1px)", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseOneRadius));

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseNoRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse()", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseNoRadius));
}

TEST(CSSPropertyParserTest, ScrollCustomizationPropertySingleValue) {
  RuntimeEnabledFeatures::SetScrollCustomizationEnabled(true);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-down",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  const CSSValueList* list = ToCSSValueList(value);
  EXPECT_EQ(1U, list->length());
  EXPECT_EQ(CSSValuePanDown, ToCSSIdentifierValue(list->Item(0U)).GetValueID());
}

TEST(CSSPropertyParserTest, ScrollCustomizationPropertyTwoValuesCombined) {
  RuntimeEnabledFeatures::SetScrollCustomizationEnabled(true);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-left pan-y",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  const CSSValueList* list = ToCSSValueList(value);
  EXPECT_EQ(2U, list->length());
  EXPECT_EQ(CSSValuePanLeft, ToCSSIdentifierValue(list->Item(0U)).GetValueID());
  EXPECT_EQ(CSSValuePanY, ToCSSIdentifierValue(list->Item(1U)).GetValueID());
}

TEST(CSSPropertyParserTest, ScrollCustomizationPropertyInvalidEntries) {
  // We expect exactly one property value per coordinate.
  RuntimeEnabledFeatures::SetScrollCustomizationEnabled(true);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-left pan-right",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_FALSE(value);
  value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-up pan-down",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_FALSE(value);
  value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-x pan-left",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_FALSE(value);
  value = CSSParser::ParseSingleValue(
      CSSPropertyScrollCustomization, "pan-x pan-x",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_FALSE(value);
}

TEST(CSSPropertyParserTest, GradientUseCount) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSGradient;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>* { background-image: linear-gradient(red, blue); }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST(CSSPropertyParserTest, PaintUseCount) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  document.SetSecureContextStateForTesting(SecureContextState::kSecure);
  WebFeature feature = WebFeature::kCSSPaintFunction;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>span { background-image: paint(geometry); }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST(CSSPropertyParserTest, CrossFadeUseCount) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kWebkitCrossFade;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>div { background-image: -webkit-cross-fade(url('from.png'), "
      "url('to.png'), 0.2); }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST(CSSPropertyParserTest, DropViewportDescriptor) {
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyOrientation, "portrait"));
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyOrientation, "inherit"));
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyOrientation, "var(--dummy)"));
}

TEST(CSSPropertyParserTest, DropFontfaceDescriptor) {
  EXPECT_FALSE(IsValidPropertyValueForStyleRule(CSSPropertySrc, "url(blah)"));
  EXPECT_FALSE(IsValidPropertyValueForStyleRule(CSSPropertySrc, "inherit"));
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertySrc, "var(--dummy)"));
}

class CSSPropertyUseCounterTest : public ::testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    Page::InsertOrdinaryPageForTesting(&dummy_page_holder_->GetPage());
    // Use strict mode.
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  }
  void TearDown() override { dummy_page_holder_ = nullptr; }

  void ParseProperty(CSSPropertyID property, const char* value_string) {
    const CSSValue* value =
        CSSParser::ParseSingleValue(property, String(value_string),
                                    CSSParserContext::Create(GetDocument()));
    DCHECK(value);
  }

  bool IsCounted(WebFeature feature) {
    return UseCounter::IsCounted(GetDocument(), feature);
  }

 private:
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(CSSPropertyUseCounterTest, CSSPropertyXUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyX, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyX, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyYUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyY, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyY, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyR, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyR, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRxUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyRx, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyRx, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRyUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyRy, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyRy, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyCxUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyCx, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyCx, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyCyUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyCy, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyCy, "42");
  EXPECT_TRUE(IsCounted(feature));
}

}  // namespace blink

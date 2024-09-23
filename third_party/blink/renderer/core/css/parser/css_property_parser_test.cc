// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

static int ComputeNumberOfTracks(const CSSValueList* value_list) {
  int number_of_tracks = 0;
  for (auto& value : *value_list) {
    if (value->IsGridLineNamesValue()) {
      continue;
    }
    if (auto* repeat_value =
            DynamicTo<cssvalue::CSSGridIntegerRepeatValue>(*value)) {
      number_of_tracks +=
          repeat_value->Repetitions() * ComputeNumberOfTracks(repeat_value);
      continue;
    }
    ++number_of_tracks;
  }
  return number_of_tracks;
}

static bool IsValidPropertyValueForStyleRule(CSSPropertyID property_id,
                                             const String& value) {
  CSSParserTokenStream stream(value);
  HeapVector<CSSPropertyValue, 64> parsed_properties;
  return CSSPropertyParser::ParseValue(
      property_id, /*allow_important_annotation=*/false, stream,
      StrictCSSParserContext(SecureContextMode::kSecureContext),
      parsed_properties, StyleRule::RuleType::kStyle);
}

TEST(CSSPropertyParserTest, CSSPaint_Functions) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(foo, func1(1px, 3px), red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo, func1(1px, 3px), red)");
}

TEST(CSSPropertyParserTest, CSSPaint_NoArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(foo)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo)");
}

TEST(CSSPropertyParserTest, CSSPaint_ValidArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(bar, 10px, red)");
}

TEST(CSSPropertyParserTest, CSSPaint_InvalidFormat) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(foo bar)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  // Illegal format should not be parsed.
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_TrailingComma) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(bar, 10px, red,)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_PaintArgumentsDiabled) {
  ScopedCSSPaintAPIArgumentsForTest css_paint_api_arguments(false);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, GridTrackLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000001);
}

TEST(CSSPropertyParserTest, GridTrackLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1000001);
}

TEST(CSSPropertyParserTest, GridTrackLimit9) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1200000);
}

TEST(CSSPropertyParserTest, GridTrackLimit10) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 1200000);
}

TEST(CSSPropertyParserTest, GridTrackLimit11) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 4200000);
}

TEST(CSSPropertyParserTest, GridTrackLimit12) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 4200000);
}

TEST(CSSPropertyParserTest, GridTrackLimit13) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns,
      "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 10000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit14) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows,
      "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 10000000);
}

TEST(CSSPropertyParserTest, GridTrackLimit15) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateColumns,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 9999997);
}

TEST(CSSPropertyParserTest, GridTrackLimit16) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridTemplateRows,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  EXPECT_EQ(ComputeNumberOfTracks(To<CSSValueList>(value)), 9999997);
}

static int GetGridPositionInteger(const CSSValue& value) {
  const auto& list = To<CSSValueList>(value);
  DCHECK_EQ(list.length(), static_cast<size_t>(1));
  const auto& primitive_value = To<CSSPrimitiveValue>(list.Item(0));
  DCHECK(primitive_value.IsNumber());
  return primitive_value.ComputeInteger(CSSToLengthConversionData());
}

TEST(CSSPropertyParserTest, GridPositionLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridColumnStart, "999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 999);
}

TEST(CSSPropertyParserTest, GridPositionLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridColumnEnd, "1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000000);
}

TEST(CSSPropertyParserTest, GridPositionLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridRowStart, "1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000001);
}

TEST(CSSPropertyParserTest, GridPositionLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridRowEnd, "5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 10000000);
}

TEST(CSSPropertyParserTest, GridPositionLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridColumnStart, "-999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -999);
}

TEST(CSSPropertyParserTest, GridPositionLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridColumnEnd, "-1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000000);
}

TEST(CSSPropertyParserTest, GridPositionLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridRowStart, "-1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000001);
}

TEST(CSSPropertyParserTest, GridPositionLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kGridRowEnd, "-5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -10000000);
}

TEST(CSSPropertyParserTest, ColorFunction) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundColor, "rgba(0, 0, 0, 1)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  EXPECT_EQ(Color::kBlack, To<cssvalue::CSSColor>(*value).Value());
}

TEST(CSSPropertyParserTest, IncompleteColor) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundColor, "rgba(123 45",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, ClipPathEllipse) {
  test::TaskEnvironment task_environment;
  auto dummy_holder = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  Document* doc = &dummy_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kSecureContext, doc);

  CSSParser::ParseSingleValue(CSSPropertyID::kClipPath,
                              "ellipse(1px 2px at invalid)", context);

  EXPECT_FALSE(doc->IsUseCounted(WebFeature::kBasicShapeEllipseTwoRadius));
  CSSParser::ParseSingleValue(CSSPropertyID::kClipPath, "ellipse(1px 2px)",
                              context);
  EXPECT_TRUE(doc->IsUseCounted(WebFeature::kBasicShapeEllipseTwoRadius));

  EXPECT_FALSE(doc->IsUseCounted(WebFeature::kBasicShapeEllipseNoRadius));
  CSSParser::ParseSingleValue(CSSPropertyID::kClipPath, "ellipse()", context);
  EXPECT_TRUE(doc->IsUseCounted(WebFeature::kBasicShapeEllipseNoRadius));
}

TEST(CSSPropertyParserTest, GradientUseCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSGradient;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>* { background-image: linear-gradient(red, blue); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSPropertyParserTest, PaintUseCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(
          KURL("https://example.com")),
      nullptr /* extra_data */);
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSPaintFunction;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>span { background-image: paint(geometry); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSPropertyParserTest, CrossFadeUseCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kWebkitCrossFade;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>div { background-image: -webkit-cross-fade(url('from.png'), "
      "url('to.png'), 0.2); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSPropertyParserTest, TwoValueOverflowOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow: overlay overlay;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_TRUE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, OneValueOverflowOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow: overlay;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, OverflowXOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow-x: overlay;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, OverflowYOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow-y: overlay;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, OverflowFirstValueOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow: overlay scroll;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_TRUE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, OverflowSecondValueOverlayCount) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kCSSValueOverflowOverlay;
  WebFeature feature2 = WebFeature::kTwoValuedOverflow;
  EXPECT_FALSE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(feature2));
  document.documentElement()->setInnerHTML(
      "<div style=\"height: 10px; width: 10px; overflow: scroll overlay;\">"
      "<div style=\"height: 50px; width: 50px;\"></div></div>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_TRUE(document.IsUseCounted(feature2));
}

TEST(CSSPropertyParserTest, DropFontfaceDescriptor) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyID::kSrc, "url(blah)"));
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyID::kSrc, "inherit"));
  EXPECT_FALSE(
      IsValidPropertyValueForStyleRule(CSSPropertyID::kSrc, "var(--dummy)"));
}

class CSSPropertyUseCounterTest : public ::testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    Page::InsertOrdinaryPageForTesting(&dummy_page_holder_->GetPage());
    // Use strict mode.
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  }
  void TearDown() override { dummy_page_holder_ = nullptr; }

  void ParseProperty(CSSPropertyID property, const char* value_string) {
    const CSSValue* value = CSSParser::ParseSingleValue(
        property, String(value_string),
        MakeGarbageCollected<CSSParserContext>(GetDocument()));
    DCHECK(value);
  }

  bool IsCounted(WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(CSSPropertyUseCounterTest, CSSPropertyXUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kX, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kX, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyYUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kY, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kY, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kR, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kR, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRxUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kRx, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kRx, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyRyUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kRy, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kRy, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyCxUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kCx, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kCx, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyCyUnitlessUseCount) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kCy, "0");
  // Unitless zero should not register.
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kCy, "42");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, UnitlessPresentationAttributesNotCounted) {
  WebFeature feature = WebFeature::kSVGGeometryPropertyHasNonZeroUnitlessValue;
  EXPECT_FALSE(IsCounted(feature));
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <rect x="42" y="42" rx="42" ry="42"/>
      <circle cx="42" cy="42" r="42"/>
    </svg>
  )HTML");
  EXPECT_FALSE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyContainStyleUseCount) {
  WebFeature feature = WebFeature::kCSSValueContainStyle;
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kContain, "strict");
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kContain, "content");
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kContain, "style paint");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyFontSizeWebkitXxxLargeUseCount) {
  WebFeature feature = WebFeature::kFontSizeWebkitXxxLarge;
  ParseProperty(CSSPropertyID::kFontSize, "xx-small");
  ParseProperty(CSSPropertyID::kFontSize, "larger");
  ParseProperty(CSSPropertyID::kFontSize, "smaller");
  ParseProperty(CSSPropertyID::kFontSize, "10%");
  ParseProperty(CSSPropertyID::kFontSize, "20px");
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kFontSize, "-webkit-xxx-large");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyBackgroundImageWebkitImageSet) {
  WebFeature feature = WebFeature::kWebkitImageSet;
  ParseProperty(CSSPropertyID::kBackgroundImage, "none");
  EXPECT_FALSE(IsCounted(feature));
  ParseProperty(CSSPropertyID::kBackgroundImage,
                "-webkit-image-set(url(foo) 2x)");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSPropertyBackgroundImageImageSet) {
  WebFeature feature = WebFeature::kImageSet;

  ParseProperty(CSSPropertyID::kBackgroundImage, "none");
  EXPECT_FALSE(IsCounted(feature));

  ParseProperty(CSSPropertyID::kBackgroundImage, "image-set(url(foo) 2x)");
  EXPECT_TRUE(IsCounted(feature));
}

TEST_F(CSSPropertyUseCounterTest, CSSLightDark) {
  WebFeature feature = WebFeature::kCSSLightDark;

  ParseProperty(CSSPropertyID::kBackgroundColor, "pink");
  EXPECT_FALSE(IsCounted(feature));

  ParseProperty(CSSPropertyID::kBackgroundColor, "light-dark(green, lime)");
  EXPECT_TRUE(IsCounted(feature));
}

void TestImageSetParsing(const String& testValue,
                         const String& expectedCssText) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, testValue,
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_NE(value, nullptr);

  const CSSValueList* val_list = To<CSSValueList>(value);
  ASSERT_EQ(val_list->length(), 1U);

  const CSSImageSetValue& image_set_value =
      To<CSSImageSetValue>(val_list->First());
  EXPECT_EQ(expectedCssText, image_set_value.CssText());
}

TEST(CSSPropertyParserTest, ImageSetDefaultResolution) {
  TestImageSetParsing("image-set(url(foo))", "image-set(url(\"foo\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) 3x)", "image-set(url(\"foo\") 3x)");
}

TEST(CSSPropertyParserTest, ImageSetResolutionUnitDppx) {
  TestImageSetParsing("image-set(url(foo) 3dppx)",
                      "image-set(url(\"foo\") 3dppx)");
}

TEST(CSSPropertyParserTest, ImageSetResolutionUnitDpi) {
  TestImageSetParsing("image-set(url(foo) 96dpi)",
                      "image-set(url(\"foo\") 96dpi)");
}

TEST(CSSPropertyParserTest, ImageSetResolutionUnitDpcm) {
  TestImageSetParsing("image-set(url(foo) 37dpcm)",
                      "image-set(url(\"foo\") 37dpcm)");
}

TEST(CSSPropertyParserTest, ImageSetZeroResolution) {
  TestImageSetParsing("image-set(url(foo) 0x)", "image-set(url(\"foo\") 0x)");
}

TEST(CSSPropertyParserTest, ImageSetCalcResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) calc(1x))",
                      "image-set(url(\"foo\") calc(1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcNegativerResolution) {
  TestImageSetParsing("image-set(url(foo) calc(-1x))",
                      "image-set(url(\"foo\") calc(-1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetAddCalcResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) calc(2x + 3x))",
                      "image-set(url(\"foo\") calc(5dppx))");
}

TEST(CSSPropertyParserTest, ImageSetSubCalcResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) calc(2x - 1x))",
                      "image-set(url(\"foo\") calc(1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetMultCalcResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) calc(2x * 3))",
                      "image-set(url(\"foo\") calc(6dppx))");
}

TEST(CSSPropertyParserTest, ImageSetMultCalcNegativeResolution) {
  TestImageSetParsing("image-set(url(foo) calc(1 * -1x))",
                      "image-set(url(\"foo\") calc(-1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetMultCalcNegativeNumberResolution) {
  TestImageSetParsing("image-set(url(foo) calc(-1 * 1x))",
                      "image-set(url(\"foo\") calc(-1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetDivCalcResolutionUnitX) {
  TestImageSetParsing("image-set(url(foo) calc(6x / 3))",
                      "image-set(url(\"foo\") calc(2dppx))");
}

TEST(CSSPropertyParserTest, ImageSetAddCalcResolutionUnitDpiWithX) {
  TestImageSetParsing("image-set(url(foo) calc(96dpi + 2x))",
                      "image-set(url(\"foo\") calc(3dppx))");
}

TEST(CSSPropertyParserTest, ImageSetAddCalcResolutionUnitDpiWithDpi) {
  TestImageSetParsing("image-set(url(foo) calc(96dpi + 96dpi))",
                      "image-set(url(\"foo\") calc(2dppx))");
}

TEST(CSSPropertyParserTest, ImageSetSubCalcResolutionUnitDpiFromX) {
  TestImageSetParsing("image-set(url(foo) calc(2x - 96dpi))",
                      "image-set(url(\"foo\") calc(1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcResolutionUnitDppx) {
  TestImageSetParsing("image-set(url(foo) calc(2dppx * 3))",
                      "image-set(url(\"foo\") calc(6dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcResolutionUnitDpi) {
  TestImageSetParsing("image-set(url(foo) calc(32dpi * 3))",
                      "image-set(url(\"foo\") calc(1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcResolutionUnitDpcm) {
  TestImageSetParsing("image-set(url(foo) calc(1dpcm * 37.79532))",
                      "image-set(url(\"foo\") calc(1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcMaxInf) {
  TestImageSetParsing("image-set(url(foo) calc(1 * max(INFinity * 3x, 0dpcm)))",
                      "image-set(url(\"foo\") calc(infinity * 1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcMinInf) {
  TestImageSetParsing("image-set(url(foo) calc(1 * min(inFInity * 4x, 0dpi)))",
                      "image-set(url(\"foo\") calc(0dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcMinMaxNan) {
  TestImageSetParsing("image-set(url(foo) calc(1dppx * max(0, min(10, NaN))))",
                      "image-set(url(\"foo\") calc(NaN * 1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcClamp) {
  TestImageSetParsing(
      "image-set(url(foo) calc(1dppx * clamp(-Infinity, 0, infinity)))",
      "image-set(url(\"foo\") calc(0dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcClampLeft) {
  TestImageSetParsing(
      "image-set(url(foo) calc(1dppx * clamp(0, -Infinity, infinity)))",
      "image-set(url(\"foo\") calc(0dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcClampRight) {
  TestImageSetParsing(
      "image-set(url(foo) calc(1dppx * clamp(-Infinity, infinity, 0)))",
      "image-set(url(\"foo\") calc(0dppx))");
}

TEST(CSSPropertyParserTest, ImageSetCalcClampNan) {
  TestImageSetParsing(
      "image-set(url(foo) calc(1 * clamp(-INFINITY*0dppx, 0dppx, "
      "infiniTY*0dppx)))",
      "image-set(url(\"foo\") calc(NaN * 1dppx))");
}

TEST(CSSPropertyParserTest, ImageSetUrlFunction) {
  TestImageSetParsing("image-set(url('foo') 1x)", "image-set(url(\"foo\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetUrlFunctionEmptyStrUrl) {
  TestImageSetParsing("image-set(url('') 1x)", "image-set(url(\"\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetUrlFunctionNoQuotationMarks) {
  TestImageSetParsing("image-set(url(foo) 1x)", "image-set(url(\"foo\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetNoUrlFunction) {
  TestImageSetParsing("image-set('foo' 1x)", "image-set(url(\"foo\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetEmptyStrUrl) {
  TestImageSetParsing("image-set('' 1x)", "image-set(url(\"\") 1x)");
}

TEST(CSSPropertyParserTest, ImageSetLinearGradient) {
  TestImageSetParsing("image-set(linear-gradient(red, blue) 1x)",
                      "image-set(linear-gradient(red, blue) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetRepeatingLinearGradient) {
  TestImageSetParsing("image-set(repeating-linear-gradient(red, blue 25%) 1x)",
                      "image-set(repeating-linear-gradient(red, blue 25%) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetRadialGradient) {
  TestImageSetParsing("image-set(radial-gradient(red, blue) 1x)",
                      "image-set(radial-gradient(red, blue) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetRepeatingRadialGradient) {
  TestImageSetParsing("image-set(repeating-radial-gradient(red, blue 25%) 1x)",
                      "image-set(repeating-radial-gradient(red, blue 25%) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetConicGradient) {
  TestImageSetParsing("image-set(conic-gradient(red, blue) 1x)",
                      "image-set(conic-gradient(red, blue) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetRepeatingConicGradient) {
  TestImageSetParsing("image-set(repeating-conic-gradient(red, blue 25%) 1x)",
                      "image-set(repeating-conic-gradient(red, blue 25%) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetType) {
  TestImageSetParsing("image-set(url('foo') 1x type('image/png'))",
                      "image-set(url(\"foo\") 1x type(\"image/png\"))");
}

void TestImageSetParsingFailure(const String& testValue) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage, testValue,
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_EQ(value, nullptr);
}

TEST(CSSPropertyParserTest, ImageSetEmpty) {
  TestImageSetParsingFailure("image-set()");
}

TEST(CSSPropertyParserTest, ImageSetMissingUrl) {
  TestImageSetParsingFailure("image-set(1x)");
}

TEST(CSSPropertyParserTest, ImageSetNegativeResolution) {
  TestImageSetParsingFailure("image-set(url(foo) -1x)");
}

TEST(CSSPropertyParserTest, ImageSetOnlyOneGradientColor) {
  TestImageSetParsingFailure("image-set(linear-gradient(red) 1x)");
}

TEST(CSSPropertyParserTest, ImageSetAddCalcMissingUnit1) {
  TestImageSetParsingFailure("image-set(url(foo) calc(2 + 3x))");
}

TEST(CSSPropertyParserTest, ImageSetAddCalcMissingUnit2) {
  TestImageSetParsingFailure("image-set(url(foo) calc(2x + 3))");
}

TEST(CSSPropertyParserTest, ImageSetSubCalcMissingUnit1) {
  TestImageSetParsingFailure("image-set(url(foo) calc(2 - 1x))");
}

TEST(CSSPropertyParserTest, ImageSetSubCalcMissingUnit2) {
  TestImageSetParsingFailure("image-set(url(foo) calc(2x - 1))");
}

TEST(CSSPropertyParserTest, ImageSetMultCalcDoubleX) {
  TestImageSetParsingFailure("image-set(url(foo) calc(2x * 3x))");
}

TEST(CSSPropertyParserTest, ImageSetDivCalcDoubleX) {
  TestImageSetParsingFailure("image-set(url(foo) calc(6x / 3x))");
}

TEST(CSSPropertyParserTest, LightDarkAuthor) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  ASSERT_TRUE(CSSParser::ParseSingleValue(
      CSSPropertyID::kColor, "light-dark(#000000, #ffffff)", context));
  ASSERT_TRUE(CSSParser::ParseSingleValue(CSSPropertyID::kColor,
                                          "light-dark(red, green)", context));
  // light-dark() is only valid for background-image in UA sheets.
  ASSERT_FALSE(CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundImage,
      "light-dark(url(light.png), url(dark.png))", context));
}

TEST(CSSPropertyParserTest, UALightDarkBackgroundImage) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);

  const struct {
    const char* value;
    bool valid;
  } tests[] = {
      {"light-dark()", false},
      {"light-dark(url(light.png))", false},
      {"light-dark(url(light.png) url(dark.png))", false},
      {"light-dark(url(light.png),,url(dark.png))", false},
      {"light-dark(url(light.png), url(dark.png))", true},
      {"light-dark(url(light.png), none)", true},
      {"light-dark(none, -webkit-image-set(url(dark.png) 1x))", true},
      {"light-dark(none, image-set(url(dark.png) 1x))", true},
      {"light-dark(  none  ,  none   )", true},
      {"light-dark(  url(light.png)  ,  url(dark.png)   )", true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(!!CSSParser::ParseSingleValue(CSSPropertyID::kBackgroundImage,
                                            test.value, ua_context),
              test.valid)
        << test.value;
  }
}

TEST(CSSPropertyParserTest, UAAppearanceAutoBaseSelectSerialization) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kBackgroundColor,
      "-internal-appearance-auto-base-select(red, blue)", ua_context);
  ASSERT_TRUE(value);
  EXPECT_EQ("-internal-appearance-auto-base-select(red, blue)",
            value->CssText());
}

namespace {

bool ParseCSSValue(CSSPropertyID property_id,
                   const String& value,
                   const CSSParserContext* context) {
  CSSParserTokenStream stream(value);
  HeapVector<CSSPropertyValue, 64> parsed_properties;
  return CSSPropertyParser::ParseValue(
      property_id, /*allow_important_annotation=*/false, stream, context,
      parsed_properties, StyleRule::RuleType::kStyle);
}

}  // namespace

TEST(CSSPropertyParserTest, UALightDarkBackgroundShorthand) {
  auto* ua_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);

  const struct {
    const char* value;
    bool valid;
  } tests[] = {
      {"light-dark()", false},
      {"light-dark(url(light.png))", false},
      {"light-dark(url(light.png) url(dark.png))", false},
      {"light-dark(url(light.png),,url(dark.png))", false},
      {"light-dark(url(light.png), url(dark.png))", true},
      {"light-dark(url(light.png), none)", true},
      {"light-dark(none, -webkit-image-set(url(dark.png) 1x))", true},
      {"light-dark(none, image-set(url(dark.png) 1x))", true},
      {"light-dark(  none  ,  none   )", true},
      {"light-dark(  url(light.png)  ,  url(dark.png)   )", true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(
        !!ParseCSSValue(CSSPropertyID::kBackground, test.value, ua_context),
        test.valid)
        << test.value;
  }
}

TEST(CSSPropertyParserTest, ParseRevert) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  String string = " revert";
  CSSParserTokenStream stream(string);

  const CSSValue* value = CSSPropertyParser::ParseSingleValue(
      CSSPropertyID::kMarginLeft, stream, context);
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsRevertValue());
}

TEST(CSSPropertyParserTest, ParseRevertLayer) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  String string = " revert-layer";
  CSSParserTokenStream stream(string);

  const CSSValue* value = CSSPropertyParser::ParseSingleValue(
      CSSPropertyID::kMarginLeft, stream, context);
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->IsRevertLayerValue());
}

void TestRepeatStyleParsing(const String& testValue,
                            const String& expectedCssText,
                            const CSSPropertyID& propID) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      propID, testValue,
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_NE(value, nullptr);

  const CSSValueList* val_list = To<CSSValueList>(value);
  ASSERT_EQ(val_list->length(), 1U);

  const CSSRepeatStyleValue& repeat_style_value =
      To<CSSRepeatStyleValue>(val_list->First());
  EXPECT_EQ(expectedCssText, repeat_style_value.CssText());
}

void TestRepeatStylesParsing(const String& testValue,
                             const String& expectedCssText) {
  TestRepeatStyleParsing(testValue, expectedCssText,
                         CSSPropertyID::kBackgroundRepeat);
  TestRepeatStyleParsing(testValue, expectedCssText,
                         CSSPropertyID::kMaskRepeat);
}

TEST(CSSPropertyParserTest, RepeatStyleRepeatX1) {
  TestRepeatStylesParsing("repeat-x", "repeat-x");
}

TEST(CSSPropertyParserTest, RepeatStyleRepeatX2) {
  TestRepeatStylesParsing("repeat no-repeat", "repeat-x");
}

TEST(CSSPropertyParserTest, RepeatStyleRepeatY1) {
  TestRepeatStylesParsing("repeat-y", "repeat-y");
}

TEST(CSSPropertyParserTest, RepeatStyleRepeatY2) {
  TestRepeatStylesParsing("no-repeat repeat", "repeat-y");
}

TEST(CSSPropertyParserTest, RepeatStyleRepeat1) {
  TestRepeatStylesParsing("repeat", "repeat");
}

TEST(CSSPropertyParserTest, RepeatStyleRepeat2) {
  TestRepeatStylesParsing("repeat repeat", "repeat");
}

TEST(CSSPropertyParserTest, RepeatStyleNoRepeat1) {
  TestRepeatStylesParsing("no-repeat", "no-repeat");
}

TEST(CSSPropertyParserTest, RepeatStyleNoRepeat2) {
  TestRepeatStylesParsing("no-repeat no-repeat", "no-repeat");
}

TEST(CSSPropertyParserTest, RepeatStyleSpace1) {
  TestRepeatStylesParsing("space", "space");
}

TEST(CSSPropertyParserTest, RepeatStyleSpace2) {
  TestRepeatStylesParsing("space space", "space");
}

TEST(CSSPropertyParserTest, RepeatStyleRound1) {
  TestRepeatStylesParsing("round", "round");
}

TEST(CSSPropertyParserTest, RepeatStyleRound2) {
  TestRepeatStylesParsing("round round", "round");
}

TEST(CSSPropertyParserTest, RepeatStyle2Val) {
  TestRepeatStylesParsing("round space", "round space");
}

void TestRepeatStyleViaShorthandParsing(const String& testValue,
                                        const String& expectedCssText,
                                        const CSSPropertyID& propID) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, propID, testValue, false /* important */);
  ASSERT_NE(style, nullptr);
  EXPECT_TRUE(style->AsText().Contains(expectedCssText));
}

void TestRepeatStyleViaShorthandsParsing(const String& testValue,
                                         const String& expectedCssText) {
  TestRepeatStyleViaShorthandParsing(testValue, expectedCssText,
                                     CSSPropertyID::kBackground);
  TestRepeatStyleViaShorthandParsing(testValue, expectedCssText,
                                     CSSPropertyID::kMask);
}

TEST(CSSPropertyParserTest, RepeatStyleRepeatXViaShorthand) {
  TestRepeatStyleViaShorthandsParsing("url(foo) repeat no-repeat", "repeat-x");
}

TEST(CSSPropertyParserTest, RepeatStyleRoundViaShorthand) {
  TestRepeatStyleViaShorthandsParsing("url(foo) round round", "round");
}

TEST(CSSPropertyParserTest, RepeatStyle2ValViaShorthand) {
  TestRepeatStyleViaShorthandsParsing("url(foo) space repeat", "space repeat");
}

void TestMaskPositionParsing(const String& testValue,
                             const String& expectedCssText) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kMaskPosition, testValue,
                        false /* important */);
  ASSERT_NE(style, nullptr);
  EXPECT_TRUE(style->AsText().Contains(expectedCssText));
}

TEST(CSSPropertyParserTest, MaskPositionCenter) {
  TestMaskPositionParsing("center", "center center");
}

TEST(CSSPropertyParserTest, MaskPositionTopRight) {
  TestMaskPositionParsing("top right", "right top");
}

TEST(CSSPropertyParserTest, MaskPositionBottomLeft) {
  TestMaskPositionParsing("bottom 10% left -13px", "left -13px bottom 10%");
}

void TestMaskModeParsing(const String& testValue,
                         const String& expectedCssText) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kMaskMode, testValue,
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(expectedCssText, value->CssText());
}

TEST(CSSPropertyParserTest, MaskModeAlpha) {
  TestMaskModeParsing("alpha", "alpha");
}

TEST(CSSPropertyParserTest, MaskModeLuminance) {
  TestMaskModeParsing("luminance", "luminance");
}

TEST(CSSPropertyParserTest, MaskModeMatchSource) {
  TestMaskModeParsing("match-source", "match-source");
}

TEST(CSSPropertyParserTest, MaskModeMultipleValues) {
  TestMaskModeParsing("alpha, luminance, match-source",
                      "alpha, luminance, match-source");
}

void TestMaskParsing(const String& specified_css_text,
                     const CSSPropertyID property_id,
                     const String& expected_prop_value) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  ASSERT_NE(style, nullptr);

  auto result = style->ParseAndSetProperty(
      CSSPropertyID::kMask, specified_css_text, false /* important */,
      SecureContextMode::kSecureContext, nullptr /* context_style_sheet */);
  ASSERT_NE(result, MutableCSSPropertyValueSet::kParseError);

  EXPECT_EQ(style->PropertyCount(), 9U);

  EXPECT_EQ(style->GetPropertyValue(property_id), expected_prop_value);
}

TEST(CSSPropertyParserTest, MaskRepeatFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskRepeat, "repeat");
}

TEST(CSSPropertyParserTest, MaskRepeatFromMaskNone2) {
  TestMaskParsing("none, none", CSSPropertyID::kMaskRepeat, "repeat, repeat");
}

TEST(CSSPropertyParserTest, MaskRepeatFromMaskRepeatX) {
  TestMaskParsing("repeat-x", CSSPropertyID::kMaskRepeat, "repeat-x");
}

TEST(CSSPropertyParserTest, MaskRepeatFromMaskRoundSpace) {
  TestMaskParsing("round space", CSSPropertyID::kMaskRepeat, "round space");
}

TEST(CSSPropertyParserTest, MaskClipFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskClip, "border-box");
}

TEST(CSSPropertyParserTest, MaskCompositeFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskComposite, "add");
}

TEST(CSSPropertyParserTest, MaskModeFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskMode, "match-source");
}

TEST(CSSPropertyParserTest, MaskOriginFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskOrigin, "border-box");
}

TEST(CSSPropertyParserTest, MaskPositionFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskPosition, "0% 0%");
}

TEST(CSSPropertyParserTest, MaskPositionFromMaskNone2) {
  TestMaskParsing("none, none", CSSPropertyID::kMaskPosition, "0% 0%, 0% 0%");
}

TEST(CSSPropertyParserTest, MaskPositionLayered) {
  TestMaskParsing("top right, bottom left", CSSPropertyID::kMaskPosition,
                  "right top, left bottom");
}

TEST(CSSPropertyParserTest, MaskPositionLayered2) {
  TestMaskParsing("top right, none, bottom left", CSSPropertyID::kMaskPosition,
                  "right top, 0% 0%, left bottom");
}

TEST(CSSPropertyParserTest, MaskSizeFromMaskNone) {
  TestMaskParsing("none", CSSPropertyID::kMaskSize, "auto");
}

TEST(CSSPropertyParserTest, MaskFromMaskNoneRepeatY) {
  TestMaskParsing("none repeat-y", CSSPropertyID::kMask, "repeat-y");
}

}  // namespace blink

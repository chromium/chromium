// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"  // IWYU pragma: keep (needed by GarbageCollectedIs)
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray_string.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
namespace {

using ::blink_testing::GarbageCollectedIs;
using ::blink_testing::ParseFilter;
using ::testing::Combine;
using ::testing::ElementsAreArray;
using ::testing::Matcher;
using ::testing::SizeIs;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::ValuesIn;

struct FilterTestParams {
  std::string testcase_name;
  std::string filter;
  std::vector<Matcher<FilterOperation*>> expected_ops;
};

using FilterTest = TestWithParam<FilterTestParams>;

TEST_P(FilterTest, CreatesFilterOperationsFromObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HeapVector<ScriptValue> filters = {
      CHECK_DEREF(ParseFilter(scope, GetParam().filter)).GetAsObject()};
  EXPECT_THAT(CanvasFilterOperationResolver::CreateFilterOperationsFromList(
                  filters, CHECK_DEREF(scope.GetExecutionContext()),
                  scope.GetExceptionState())
                  .Operations(),
              ElementsAreArray(GetParam().expected_ops));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

INSTANTIATE_TEST_SUITE_P(
    DropShadowTests,
    FilterTest,
    ValuesIn<FilterTestParams>({
        {.testcase_name = "DefaultParams",
         .filter = "({'name': 'dropShadow'})",
         .expected_ops = {GarbageCollectedIs<DropShadowFilterOperation>(
             ShadowData(
                 /*offset=*/{2, 2},
                 /*blur=*/{2, 2},
                 /*spread=*/0,
                 ShadowStyle::kNormal,
                 StyleColor(Color::kBlack),
                 /*opacity=*/1))}},

        {.testcase_name = "AllParamsSpecified",
         .filter = R"js(({
                     "name": "dropShadow",
                     "dx": 15,
                     "dy": 10,
                     "stdDeviation": 5,
                     "floodColor": "purple",
                     "floodOpacity": 0.7
                    }))js",
         .expected_ops = {GarbageCollectedIs<DropShadowFilterOperation>(
             ShadowData(
                 /*offset=*/{15, 10},
                 /*blur=*/{5, 5},
                 /*spread=*/0,
                 ShadowStyle::kNormal,
                 StyleColor(Color::FromRGBA(128, 0, 128, 255)),
                 /*opacity=*/0.7))}},

        {.testcase_name = "XYBlur",
         .filter = R"js(({
                     "name": "dropShadow",
                     "stdDeviation": [5, 10],
                    }))js",
         .expected_ops = {GarbageCollectedIs<DropShadowFilterOperation>(
             ShadowData(
                 /*offset=*/{2, 2},
                 /*blur=*/{5, 10},
                 /*spread=*/0,
                 ShadowStyle::kNormal,
                 StyleColor(Color::kBlack),
                 /*opacity=*/1.0))}},

        {.testcase_name = "NegativeBlur",
         .filter = R"js(({
                     "name": "dropShadow",
                     "stdDeviation": [-5, -10],
                    }))js",
         .expected_ops = {GarbageCollectedIs<DropShadowFilterOperation>(
             ShadowData(
                 /*offset=*/{2, 2},
                 /*blur=*/{0, 0},
                 /*spread=*/0,
                 ShadowStyle::kNormal,
                 StyleColor(Color::kBlack),
                 /*opacity=*/1.0))}},
    }),
    [](const TestParamInfo<FilterTestParams>& info) {
      return info.param.testcase_name;
    });

INSTANTIATE_TEST_SUITE_P(
    BlurFilterTests,
    FilterTest,
    ValuesIn<FilterTestParams>({
        {.testcase_name = "SingleValue",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": 42,
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(42))}},
        {.testcase_name = "XYValues",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": [123, 456],
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(123),
             Length::Fixed(456))}},
        {.testcase_name = "NegativeValue",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": [-1234],
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(0))}},
        {.testcase_name = "NegativeValueX",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": [-123, 456],
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(0),
             Length::Fixed(456))}},
        {.testcase_name = "NegativeValueY",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": [123, -456],
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(123),
             Length::Fixed(0))}},
        {.testcase_name = "NegativeValueXY",
         .filter = R"js(({
                     "name": "gaussianBlur",
                     "stdDeviation": [-123, -456],
                    }))js",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             Length::Fixed(0),
             Length::Fixed(0))}},
    }),
    [](const TestParamInfo<FilterTestParams>& info) {
      return info.param.testcase_name;
    });

using FilterArrayTest = TestWithParam<FilterTestParams>;

TEST_P(FilterArrayTest, CreatesFilterOperationsFromObjectArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  CHECK(scope.GetExecutionContext());
  HeapVector<ScriptValue> filters =
      CHECK_DEREF(ParseFilter(scope, GetParam().filter)).GetAsObjectArray();
  EXPECT_THAT(CanvasFilterOperationResolver::CreateFilterOperationsFromList(
                  filters, CHECK_DEREF(scope.GetExecutionContext()),
                  scope.GetExceptionState())
                  .Operations(),
              ElementsAreArray(GetParam().expected_ops));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

INSTANTIATE_TEST_SUITE_P(
    FilterArrayTests,
    FilterArrayTest,
    ValuesIn<FilterTestParams>({
        {.testcase_name = "MultipleShadows",
         .filter = R"js(([
                    {
                        "name": "dropShadow",
                        "dx": 5,
                        "dy": 5,
                        "stdDeviation": 5,
                        "floodColor": "blue",
                        "floodOpacity": 0.5
                    },
                    {
                        "name": "dropShadow",
                        "dx": 10,
                        "dy": 10,
                        "stdDeviation": 10,
                        "floodColor": "red",
                        "floodOpacity": 0.7
                    }
                    ]))js",
         .expected_ops =
             {GarbageCollectedIs<DropShadowFilterOperation>(ShadowData(
                  /*offset=*/{5, 5},
                  /*blur=*/{5, 5},
                  /*spread=*/0,
                  ShadowStyle::kNormal,
                  StyleColor(Color::FromRGBA(0, 0, 255, 255)),
                  /*opacity=*/0.5)),
              GarbageCollectedIs<DropShadowFilterOperation>(ShadowData(
                  /*offset=*/{10, 10},
                  /*blur=*/{10, 10},
                  /*spread=*/0,
                  ShadowStyle::kNormal,
                  StyleColor(Color::FromRGBA(255, 0, 0, 255)),
                  /*opacity=*/0.7))}},
        {.testcase_name = "ShadowAndBlur",
         .filter = R"js(([
                    {
                        "name": "dropShadow",
                        "dx": 5,
                        "dy": 5,
                        "stdDeviation": 5,
                        "floodColor": "blue",
                        "floodOpacity": 0.5
                    },
                    {
                        "name": "gaussianBlur",
                        "stdDeviation": 12
                    }
                    ]))js",
         .expected_ops =
             {GarbageCollectedIs<DropShadowFilterOperation>(ShadowData(
                  /*offset=*/{5, 5},
                  /*blur=*/{5, 5},
                  /*spread=*/0,
                  ShadowStyle::kNormal,
                  StyleColor(Color::FromRGBA(0, 0, 255, 255)),
                  /*opacity=*/0.5)),
              GarbageCollectedIs<BlurFilterOperation>(
                  /*std_deviation=*/Length(12.0f, Length::Type::kFixed))}},
    }),
    [](const TestParamInfo<FilterTestParams>& info) {
      return info.param.testcase_name;
    });

using CSSFilterTest = TestWithParam<FilterTestParams>;

TEST_P(CSSFilterTest, CreatesFilterOperationsFromCSSFilter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperationsFromCSSFilter(
          String(GetParam().filter), CHECK_DEREF(scope.GetExecutionContext()),
          /*style_resolution_host=*/nullptr, Font())
          .Operations(),
      ElementsAreArray(GetParam().expected_ops));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

INSTANTIATE_TEST_SUITE_P(
    CSSFilterParamTests,
    CSSFilterTest,
    ValuesIn<FilterTestParams>({
        {.testcase_name = "dropShadow",
         .filter = "drop-shadow(20px 25px 10px cyan)",
         .expected_ops = {GarbageCollectedIs<DropShadowFilterOperation>(
             ShadowData(
                 /*offset=*/{20, 25},
                 /*blur=*/{10, 10},
                 /*spread=*/0,
                 ShadowStyle::kNormal,
                 StyleColor(Color::FromRGBA(0, 255, 255, 255)),
                 /*opacity=*/1.0))}},

        {.testcase_name = "blur",
         .filter = "blur(12px)",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             /*std_deviation=*/Length(12.0f, Length::Type::kFixed))}},
    }),
    [](const TestParamInfo<FilterTestParams>& info) {
      return info.param.testcase_name;
    });

TEST(CSSResolutionTest,
     CreatesFilterOperationsFromCSSFilterWithStyleResolution) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HTMLCanvasElement* canvas =
      MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  // Pre-condition for using style resolution for fonts.
  ASSERT_NE(canvas->GetDocument().GetFrame(), nullptr);
  Font font(FontStyleResolver::ComputeFont(
      *CSSParser::ParseFont("10px sans-serif", scope.GetExecutionContext()),
      canvas->GetFontSelector()));
  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperationsFromCSSFilter(
          String("drop-shadow(1em 1em 0 black)"),
          CHECK_DEREF(scope.GetExecutionContext()), canvas, font)
          .Operations(),
      ElementsAreArray(
          {GarbageCollectedIs<DropShadowFilterOperation>(ShadowData(
              /*offset=*/{10, 10},
              /*blur=*/{0, 0},
              /*spread=*/0, ShadowStyle::kNormal, StyleColor(Color::kBlack),
              /*opacity=*/1.0))}));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(CSSResolutionTest,
     CreatesFilterOperationsFromCSSFilterWithNoStyleResolution) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperationsFromCSSFilter(
          String("drop-shadow(1em 1em 0 black)"),
          CHECK_DEREF(scope.GetExecutionContext()),
          /*style_resolution_host=*/nullptr, Font())
          .Operations(),
      // Font sized is assumed to be 16px when no style resolution is available.
      ElementsAreArray(
          {GarbageCollectedIs<DropShadowFilterOperation>(ShadowData(
              /*offset=*/{16, 16},
              /*blur=*/{0, 0},
              /*spread=*/0, ShadowStyle::kNormal, StyleColor(Color::kBlack),
              /*opacity=*/1.0))}));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

using FilterApiTest = TestWithParam<
    std::tuple<std::string, std::string, std::string, ExceptionCode>>;

TEST_P(FilterApiTest, RaisesExceptionForInvalidType) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const auto& [filter_name, param_key, param_value, expected_error] =
      GetParam();
  HeapVector<ScriptValue> filters = {
      CHECK_DEREF(
          ParseFilter(scope, base::StringPrintf(
                                 "({name: '%s', %s: %s})", filter_name.c_str(),
                                 param_key.c_str(), param_value.c_str())))
          .GetAsObject()};

  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperationsFromList(
          filters, CHECK_DEREF(scope.GetExecutionContext()),
          scope.GetExceptionState())
          .Operations(),
      SizeIs(expected_error == ToExceptionCode(DOMExceptionCode::kNoError)
                 ? 1
                 : 0));
  EXPECT_EQ(scope.GetExceptionState().Code(), expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    DropShadowValidParamTests,
    FilterApiTest,
    Combine(Values("dropShadow"),
            Values("dx", "dy", "floodOpacity"),
            Values("10",
                   "-1",
                   "0.5",
                   "null",
                   "true",
                   "false",
                   "[]",
                   "[20]",
                   "'30'"),
            Values(ToExceptionCode(DOMExceptionCode::kNoError))));

INSTANTIATE_TEST_SUITE_P(
    DropShadowInvalidParamTests,
    FilterApiTest,
    Combine(Values("dropShadow"),
            Values("dx", "dy", "floodOpacity"),
            Values("NaN",
                   "Infinity",
                   "-Infinity",
                   "undefined",
                   "'asdf'",
                   "{}",
                   "[1,2]"),
            Values(ToExceptionCode(ESErrorType::kTypeError))));

INSTANTIATE_TEST_SUITE_P(
    ValidStdDeviationTests,
    FilterApiTest,
    Combine(Values("dropShadow", "gaussianBlur"),
            Values("stdDeviation"),
            Values("10",
                   "-1",
                   "0.5",
                   "null",
                   "true",
                   "false",
                   "[]",
                   "[20]",
                   "'30'",
                   "[1,2]",
                   "[[1],'2']",
                   "[null,[]]"),
            Values(ToExceptionCode(DOMExceptionCode::kNoError))));

INSTANTIATE_TEST_SUITE_P(
    InvalidStdDeviationTests,
    FilterApiTest,
    Combine(Values("dropShadow", "gaussianBlur"),
            Values("stdDeviation"),
            Values("NaN",
                   "Infinity",
                   "-Infinity",
                   "undefined",
                   "'asdf'",
                   "{}",
                   "[1,2,3]",
                   "[1,'asdf']",
                   "[1,'undefined']"),
            Values(ToExceptionCode(ESErrorType::kTypeError))));

INSTANTIATE_TEST_SUITE_P(
    DropShadowValidFloodColorTests,
    FilterApiTest,
    Combine(Values("dropShadow"),
            Values("floodColor"),
            Values("'red'",
                   "'canvas'",
                   "'rgba(4,-3,0.5,1)'",
                   "'#aabbccdd'",
                   "'#abcd'"),
            Values(ToExceptionCode(DOMExceptionCode::kNoError))));

INSTANTIATE_TEST_SUITE_P(
    DropShadowInvalidFloodColorTests,
    FilterApiTest,
    Combine(
        Values("dropShadow"),
        Values("floodColor"),
        Values("'asdf'", "'rgba(NaN,3,2,1)'", "10", "undefined", "null", "NaN"),
        Values(ToExceptionCode(ESErrorType::kTypeError))));

}  // namespace
}  // namespace blink

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

using ::blink_testing::ParseFilter;
using ::testing::ByRef;
using ::testing::Combine;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::SizeIs;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::ValuesIn;

// Matcher testing equality between garbage collected objects.
//
// To be compatible with parameterized tests, `GarbageCollectedIs` lazy creates
// the expected garbage collected object. That's because GC objects can't be
// created in the global scope and so, we can't call `MakeGarbageCollected`
// inside of `INSTANTIATE_TEST_SUITE_P`. To circumvent this,
// `GarbageCollectedIs` delays the creation of the garbage collected objects to
// when the comparison is performed.
//
// Example use:
//  Foo* gc_object = MakeGarbageCollected<Foo>(1, 2);
//  EXPECTE_THAT(gc_object, GarbageCollectedIs<Foo>(1, 2));
MATCHER_P(GarbageCollectedIsMatcher, matcher, "") {
  return ExplainMatchResult(Eq(ByRef(*matcher.Run())), *arg, result_listener);
}

template <typename T, typename... Args>
auto GarbageCollectedIs(const Args&... args) {
  return GarbageCollectedIsMatcher(base::BindRepeating(
      [](const Args&... args) { return MakeGarbageCollected<T>(args...); },
      args...));
}

struct FilterTestParams {
  std::string testcase_name;
  std::string filter;
  std::vector<Matcher<FilterOperation*>> expected_ops;
};

using FilterTest = TestWithParam<FilterTestParams>;

TEST_P(FilterTest, CreatesFilterOperations) {
  V8TestingScope scope;
  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperations(
          CHECK_DEREF(ParseFilter(scope, GetParam().filter)),
          CHECK_DEREF(scope.GetExecutionContext()), scope.GetExceptionState())
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

using FilterApiTest = TestWithParam<
    std::tuple<std::string, std::string, std::string, ExceptionCode>>;

TEST_P(FilterApiTest, RaisesExceptionForInvalidType) {
  V8TestingScope scope;
  const auto& [filter_name, param_key, param_value, expected_error] =
      GetParam();

  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperations(
          CHECK_DEREF(ParseFilter(
              scope,
              base::StringPrintf("({name: '%s', %s: %s})", filter_name.c_str(),
                                 param_key.c_str(), param_value.c_str()))),
          CHECK_DEREF(scope.GetExecutionContext()), scope.GetExceptionState())
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
    DropShadowValidStdDeviationTests,
    FilterApiTest,
    Combine(Values("dropShadow"),
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
    DropShadowInvalidStdDeviationTests,
    FilterApiTest,
    Combine(Values("dropShadow"),
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

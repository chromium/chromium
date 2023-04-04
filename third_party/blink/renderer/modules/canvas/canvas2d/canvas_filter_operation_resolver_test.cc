// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"

#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace blink {
namespace {

using ::testing::ByRef;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::SizeIs;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;

ScriptValue ParseScriptValue(V8TestingScope& scope, const std::string& value) {
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(scope.GetIsolate(), value.c_str())
          .ToLocalChecked();
  v8::Local<v8::Script> script =
      v8::Script::Compile(scope.GetContext(), source).ToLocalChecked();
  return ScriptValue(scope.GetIsolate(),
                     script->Run(scope.GetContext()).ToLocalChecked());
}

MATCHER_P(MemberIs, matcher, "") {
  return ExplainMatchResult(Eq(ByRef(*matcher)), *arg, result_listener);
}

TEST(CreateFilterOperationsTests, DropShadowDefaults) {
  V8TestingScope scope;
  EXPECT_THAT(CanvasFilterOperationResolver::CreateFilterOperations(
                  scope.GetExecutionContext(),
                  {ParseScriptValue(scope, "({'filter': 'dropShadow'})")},
                  scope.GetExceptionState())
                  .Operations(),
              ElementsAre(MemberIs(
                  MakeGarbageCollected<DropShadowFilterOperation>(ShadowData(
                      /*location=*/{2, 2}, /*blur=*/2, /*spread=*/0,
                      ShadowStyle::kNormal, StyleColor(Color::kBlack),
                      /*opacity=*/1)))));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(CreateFilterOperationsTests, DropShadow) {
  V8TestingScope scope;
  EXPECT_THAT(CanvasFilterOperationResolver::CreateFilterOperations(
                  scope.GetExecutionContext(),
                  {ParseScriptValue(scope,
                                    R"js(({
                                       "filter": "dropShadow",
                                       "dx": 15,
                                       "dy": 10,
                                       "stdDeviation": 5,
                                       "floodColor": "purple",
                                       "floodOpacity": 0.7
                                     }))js")},
                  scope.GetExceptionState())
                  .Operations(),
              ElementsAre(MemberIs(
                  MakeGarbageCollected<DropShadowFilterOperation>(ShadowData(
                      /*location=*/{15, 10}, /*blur=*/5, /*spread=*/0,
                      ShadowStyle::kNormal,
                      StyleColor(Color::FromRGBA(128, 0, 128, 255)),
                      /*opacity=*/0.7)))));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

using FilterApiTest = TestWithParam<
    std::tuple<std::string, std::string, std::string, ExceptionCode>>;

TEST_P(FilterApiTest, RaisesExceptionForInvalidType) {
  V8TestingScope scope;
  const auto& [filter_name, param_key, param_value, expected_error] =
      GetParam();

  EXPECT_THAT(
      CanvasFilterOperationResolver::CreateFilterOperations(
          scope.GetExecutionContext(),
          {ParseScriptValue(
              scope, base::StringPrintf("({filter: '%s', %s: %s})",
                                        filter_name.c_str(), param_key.c_str(),
                                        param_value.c_str()))},
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
            Values("dx", "dy", "stdDeviation", "floodOpacity"),
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
            Values("dx", "dy", "stdDeviation", "floodOpacity"),
            Values("NaN",
                   "Infinity",
                   "-Infinity",
                   "undefined",
                   "'asdf'",
                   "{}",
                   "[1,2]"),
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

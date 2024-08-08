// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"

#include <string>
#include <vector>

#include "base/check_deref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using ::blink_testing::GarbageCollectedIs;
using ::blink_testing::ParseFilter;
using ::testing::ElementsAreArray;
using ::testing::Matcher;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

struct CanvasFilterTestParams {
  std::string testcase_name;
  std::string filter;
  std::vector<Matcher<FilterOperation*>> expected_ops;
};

using CanvasFilterTest = TestWithParam<CanvasFilterTestParams>;

TEST_P(CanvasFilterTest, CreatesFilterOperations) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_THAT(
      CanvasFilter::CreateFilterOperations(
          CHECK_DEREF(ParseFilter(scope, GetParam().filter)), Font(),
          /*style_resolution_host=*/nullptr,
          CHECK_DEREF(scope.GetExecutionContext()), scope.GetExceptionState())
          .Operations(),
      ElementsAreArray(GetParam().expected_ops));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

INSTANTIATE_TEST_SUITE_P(
    FilterInputOptionsTests,
    CanvasFilterTest,
    ValuesIn<CanvasFilterTestParams>({
        {.testcase_name = "object",
         .filter = "({'name': 'gaussianBlur', 'stdDeviation': 12})",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             /*std_deviation=*/Length(12.0f, Length::Type::kFixed))}},
        {.testcase_name = "object_array",
         .filter = "([{'name': 'gaussianBlur', 'stdDeviation': 5}, {'name': "
                   "'gaussianBlur', 'stdDeviation': 10}])",
         .expected_ops =
             {
                 GarbageCollectedIs<BlurFilterOperation>(
                     /*std_deviation=*/Length(5.0f, Length::Type::kFixed)),
                 GarbageCollectedIs<BlurFilterOperation>(
                     /*std_deviation=*/Length(10.0f, Length::Type::kFixed)),
             }},
        {.testcase_name = "css_string",
         .filter = "'blur(12px)'",
         .expected_ops = {GarbageCollectedIs<BlurFilterOperation>(
             /*std_deviation=*/Length(12.0f, Length::Type::kFixed))}},
    }),
    [](const TestParamInfo<CanvasFilterTestParams>& info) {
      return info.param.testcase_name;
    });

}  // namespace
}  // namespace blink

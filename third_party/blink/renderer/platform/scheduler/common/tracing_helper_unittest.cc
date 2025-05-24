// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "base/test/task_environment.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

namespace {

perfetto::StaticString SignOfInt(int value) {
  if (value > 0)
    return "positive";
  if (value < 0)
    return "negative";
  return nullptr;
}

}  // namespace

// TODO(kraynov): TraceableCounter tests.

// TODO(crbug.com/408328552): Re-enable this test
TEST(TracingHelperTest, DISABLED_TraceableState) {
  TraceableVariableController controller;
  TraceableState<int, "renderer.scheduler"> state(
      0, perfetto::NamedTrack("State"), &controller, SignOfInt);

  base::test::TracingEnvironment tracing_environment;
  base::test::TaskEnvironment task_environment;

  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace("renderer.scheduler");

  controller.OnTraceLogEnabled();
  state = 1;
  state = 0;
  state = -1;

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result = test_trace_processor.RunQuery(R"(
    SELECT slice.name
    FROM track left join slice on track.id = slice.track_id
    WHERE track.name = 'State'
    ORDER BY ts
  )");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"positive"},
                                     std::vector<std::string>{"negative"}));
}

TEST(TracingHelperTest, TraceableStateOperators) {
  TraceableVariableController controller;
  TraceableState<int, TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug")> x(
      -1, perfetto::NamedTrack("X"), &controller, SignOfInt);
  TraceableState<int, TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug")> y(
      1, perfetto::NamedTrack("Y"), &controller, SignOfInt);
  EXPECT_EQ(0, x + y);
  EXPECT_FALSE(x == y);
  EXPECT_TRUE(x != y);
  x = 1;
  EXPECT_EQ(0, y - x);
  EXPECT_EQ(2, x + y);
  EXPECT_EQ(x, y);
  EXPECT_FALSE(x != y);
  EXPECT_NE(x + y, 3);
  EXPECT_EQ(2 - y + 1 + x, 3);
  x = 3;
  y = 2;
  int z = x = y;
  EXPECT_EQ(2, z);
}

}  // namespace scheduler
}  // namespace blink

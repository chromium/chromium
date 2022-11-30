// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

namespace {

const char* g_last_state = nullptr;

void ExpectTraced(const char* state) {
  EXPECT_TRUE(state);
  EXPECT_TRUE(g_last_state);
  EXPECT_STREQ(state, g_last_state);
  g_last_state = nullptr;
}

void ExpectNotTraced() {
  EXPECT_FALSE(g_last_state);
}

const char* SignOfInt(int value) {
  if (value > 0)
    return "positive";
  if (value < 0)
    return "negative";
  return "zero";
}

class TraceableStateForTest
    : public TraceableState<int, TracingCategory::kDefault> {
 public:
  TraceableStateForTest(TraceableVariableController* controller)
      : TraceableState(0, "State", controller, SignOfInt) {
    // We shouldn't expect trace in constructor here because mock isn't set yet.
    mock_trace_for_test_ = &MockTrace;
  }

  TraceableStateForTest& operator=(const int& value) {
    Assign(value);
    return *this;
  }

  static void MockTrace(const char* state) {
    EXPECT_TRUE(state);
    EXPECT_FALSE(g_last_state);  // No unexpected traces.
    g_last_state = state;
  }
};

}  // namespace

// TODO(kraynov): TraceableCounter tests.

TEST(TracingHelperTest, TraceableState) {
  TraceableVariableController controller;
  TraceableStateForTest state(&controller);
  controller.OnTraceLogEnabled();
  ExpectTraced("zero");
  state = 0;
  ExpectNotTraced();
  state = 1;
  ExpectTraced("positive");
  state = -1;
  ExpectTraced("negative");
}

TEST(TracingHelperTest, TraceableStateOperators) {
  TraceableVariableController controller;
  TraceableState<int, TracingCategory::kDebug> x(-1, "X", &controller,
                                                 SignOfInt);
  TraceableState<int, TracingCategory::kDebug> y(1, "Y", &controller,
                                                 SignOfInt);
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

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(LogicalRectTest, AddOffset) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(LogicalRect(1, 2, 3, 4) + LogicalOffset(5, 6),
            LogicalRect(6, 8, 3, 4));
}

struct LogicalRectUniteTestData {
  const char* test_case;
  LogicalRect a;
  LogicalRect b;
  LogicalRect expected;
} logical_rect_unite_test_data[] = {
    {"empty", {}, {}, {}},
    {"a empty", {}, {1, 2, 3, 4}, {1, 2, 3, 4}},
    {"b empty", {1, 2, 3, 4}, {}, {1, 2, 3, 4}},
    {"a larger", {100, 50, 300, 200}, {200, 50, 200, 200}, {100, 50, 300, 200}},
    {"b larger", {200, 50, 200, 200}, {100, 50, 300, 200}, {100, 50, 300, 200}},
    {"saturated width",
     {-1000, 0, 200, 200},
     {33554402, 500, 30, 100},
     {0, 0, 99999999, 600}},
    {"saturated height",
     {0, -1000, 200, 200},
     {0, 33554402, 100, 30},
     {0, 0, 200, 99999999}},
};

std::ostream& operator<<(std::ostream& os,
                         const LogicalRectUniteTestData& data) {
  return os << "Unite " << data.test_case;
}

class LogicalRectUniteTest
    : public testing::Test,
      public testing::WithParamInterface<LogicalRectUniteTestData> {};

INSTANTIATE_TEST_SUITE_P(GeometryUnitsTest,
                         LogicalRectUniteTest,
                         testing::ValuesIn(logical_rect_unite_test_data));

TEST_P(LogicalRectUniteTest, Data) {
  const auto& data = GetParam();
  LogicalRect actual = data.a;
  actual.Unite(data.b);

  LogicalRect expected = data.expected;
  constexpr int kExtraForSaturation = 2000;
  // On arm, you cannot actually get the true saturated value just by
  // setting via LayoutUnit constructor. Instead, add to the expected
  // value to actually get a saturated expectation (which is what happens in
  // the Unite operation).
  if (data.expected.size.inline_size == GetMaxSaturatedSetResultForTesting()) {
    expected.size.inline_size += kExtraForSaturation;
  }

  if (data.expected.size.block_size == GetMaxSaturatedSetResultForTesting()) {
    expected.size.block_size += kExtraForSaturation;
  }
  EXPECT_EQ(expected, actual);
}

}  // namespace

}  // namespace blink

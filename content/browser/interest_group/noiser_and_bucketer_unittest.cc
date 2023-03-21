// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/noiser_and_bucketer.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// The function should sometimes return not equal, within the range.
template <typename Function, typename T>
void EqualAndNotEqual(T input,
                      int expected,
                      Function function,
                      int min,
                      int max) {
  bool seen_equal = false, seen_not_equal = false;
  while (!seen_equal || !seen_not_equal) {
    int actual = function(input);
    EXPECT_GE(actual, min);
    EXPECT_LE(actual, max);
    if (actual == expected) {
      seen_equal = true;
    } else {
      seen_not_equal = true;
    }
  }
}

TEST(NoiserAndBucketerTest, JoinCount) {
  constexpr int kMin = 1, kMax = 16;

  // clang-format off
  const struct {
    int input;
    int expected;
  } kTestCases[] = {
      {-2, 1},
      {-1, 1},
      {0, 1},
      {1, 1},
      {2, 2},
      {3, 3},
      {4, 4},
      {5, 5},
      {6, 6},
      {7, 7},
      {8, 8},
      {9, 9},
      {10, 10},
      {11, 11},
      {19, 11},
      {20, 11},
      {21, 12},
      {29, 12},
      {30, 12},
      {31, 13},
      {32, 13},
      {39, 13},
      {40, 13},
      {41, 14},
      {42, 14},
      {49, 14},
      {50, 14},
      {51, 15},
      {52, 15},
      {60, 15},
      {70, 15},
      {80, 15},
      {90, 15},
      {99, 15},
      {100, 15},
      {101, 16},
      {200, 16},
      {1000, 16},
  };
  // clang-format on
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_EQ(internals::BucketJoinCount(test_case.input), test_case.expected);
    EqualAndNotEqual(/*input=*/test_case.input, /*expected=*/test_case.expected,
                     /*function=*/NoiseAndBucketJoinCount,
                     /*min=*/kMin, /*max=*/kMax);
  }
}

TEST(NoiserAndBucketerTest, Recency) {
  constexpr int kMin = 0, kMax = 31;

  // clang-format off
  const struct {
    base::TimeDelta input;
    int expected;
  } kTestCases[] = {
    {base::Minutes(-2), 0},
    {base::Minutes(-1), 0},
    {base::Minutes(0), 0},
    {base::Minutes(1), 1},
    {base::Minutes(2), 2},
    {base::Minutes(3), 3},
    {base::Minutes(4), 4},
    {base::Minutes(5), 5},
    {base::Minutes(6), 6},
    {base::Minutes(7), 7},
    {base::Minutes(8), 8},
    {base::Minutes(9), 9},
    {base::Minutes(10), 10},
    {base::Minutes(11), 10},
    {base::Minutes(13), 10},
    {base::Minutes(14), 10},
    {base::Minutes(15), 11},
    {base::Minutes(16), 11},
    {base::Minutes(18), 11},
    {base::Minutes(19), 11},
    {base::Minutes(20), 12},
    {base::Minutes(21), 12},
    {base::Minutes(28), 12},
    {base::Minutes(29), 12},
    {base::Minutes(30), 13},
    {base::Minutes(31), 13},
    {base::Minutes(38), 13},
    {base::Minutes(39), 13},
    {base::Minutes(40), 14},
    {base::Minutes(41), 14},
    {base::Minutes(48), 14},
    {base::Minutes(49), 14},
    {base::Minutes(50), 15},
    {base::Minutes(51), 15},
    {base::Minutes(58), 15},
    {base::Minutes(59), 15},
    {base::Minutes(60), 16},
    {base::Minutes(61), 16},
    {base::Minutes(73), 16},
    {base::Minutes(74), 16},
    {base::Minutes(75), 17},
    {base::Minutes(76), 17},
    {base::Minutes(88), 17},
    {base::Minutes(89), 17},
    {base::Minutes(90), 18},
    {base::Minutes(91), 18},
    {base::Minutes(103), 18},
    {base::Minutes(104), 18},
    {base::Minutes(105), 19},
    {base::Minutes(106), 19},
    {base::Minutes(118), 19},
    {base::Minutes(119), 19},
    {base::Minutes(120), 20},
    {base::Minutes(121), 20},
    {base::Minutes(238), 20},
    {base::Minutes(239), 20},
    {base::Minutes(240), 21},
    {base::Minutes(241), 21},
    {base::Minutes(718), 21},
    {base::Minutes(719), 21},
    {base::Minutes(720), 22},
    {base::Minutes(721), 22},
    {base::Minutes(1438), 22},
    {base::Minutes(1439), 22},
    {base::Minutes(1440), 23},
    {base::Minutes(1441), 23},
    {base::Minutes(2158), 23},
    {base::Minutes(2159), 23},
    {base::Minutes(2160), 24},
    {base::Minutes(2161), 24},
    {base::Minutes(2878), 24},
    {base::Minutes(2879), 24},
    {base::Minutes(2880), 25},
    {base::Minutes(2881), 25},
    {base::Minutes(4318), 25},
    {base::Minutes(4319), 25},
    {base::Minutes(4320), 26},
    {base::Minutes(4321), 26},
    {base::Minutes(5758), 26},
    {base::Minutes(5759), 26},
    {base::Minutes(5760), 27},
    {base::Minutes(5761), 27},
    {base::Minutes(10078), 27},
    {base::Minutes(10079), 27},
    {base::Minutes(10080), 28},
    {base::Minutes(10081), 28},
    {base::Minutes(20158), 28},
    {base::Minutes(20159), 28},
    {base::Minutes(20160), 29},
    {base::Minutes(20161), 29},
    {base::Minutes(30238), 29},
    {base::Minutes(30239), 29},
    {base::Minutes(30240), 30},
    {base::Minutes(30241), 30},
    {base::Minutes(40318), 30},
    {base::Minutes(40319), 30},
    {base::Minutes(40320), 31},
    {base::Minutes(40321), 31},
    {base::Minutes(1000000), 31},
    {base::Minutes(1000000000), 31},
  };
  // clang-format on
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input.InMinutes());
    EXPECT_EQ(internals::BucketRecency(test_case.input), test_case.expected);
    EqualAndNotEqual(/*input=*/test_case.input, /*expected=*/test_case.expected,
                     /*function=*/NoiseAndBucketRecency,
                     /*min=*/kMin, /*max=*/kMax);
  }
}

TEST(NoiserAndBucketerTest, ModelingSignals) {
  constexpr uint16_t kMin = 0, kMax = 0x0FFF;

  for (uint16_t i = kMin; i <= kMax; i++) {
    SCOPED_TRACE(i);
    EqualAndNotEqual(/*input=*/i, /*expected=*/i,
                     /*function=*/NoiseAndMaskModelingSignals, /*min=*/kMin,
                     /*max=*/kMax);
  }
}

}  // namespace
}  // namespace content

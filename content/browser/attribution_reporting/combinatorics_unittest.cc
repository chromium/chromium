// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/combinatorics.h"

#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(CombinatoricsTest, BinomialCoefficient) {
  // Test cases generated via a python program using scipy.special.comb.
  struct {
    int n;
    int k;
    int expected;
  } kTestCases[]{
      // All cases for n and k in [0, 10).
      // clang-format off
      {0, 0, 1},  {0, 1, 0},  {0, 2, 0},  {0, 3, 0},  {0, 4, 0},   {0, 5, 0},
      {0, 6, 0},  {0, 7, 0},  {0, 8, 0},  {0, 9, 0},  {1, 0, 1},   {1, 1, 1},
      {1, 2, 0},  {1, 3, 0},  {1, 4, 0},  {1, 5, 0},  {1, 6, 0},   {1, 7, 0},
      {1, 8, 0},  {1, 9, 0},  {2, 0, 1},  {2, 1, 2},  {2, 2, 1},   {2, 3, 0},
      {2, 4, 0},  {2, 5, 0},  {2, 6, 0},  {2, 7, 0},  {2, 8, 0},   {2, 9, 0},
      {3, 0, 1},  {3, 1, 3},  {3, 2, 3},  {3, 3, 1},  {3, 4, 0},   {3, 5, 0},
      {3, 6, 0},  {3, 7, 0},  {3, 8, 0},  {3, 9, 0},  {4, 0, 1},   {4, 1, 4},
      {4, 2, 6},  {4, 3, 4},  {4, 4, 1},  {4, 5, 0},  {4, 6, 0},   {4, 7, 0},
      {4, 8, 0},  {4, 9, 0},  {5, 0, 1},  {5, 1, 5},  {5, 2, 10},  {5, 3, 10},
      {5, 4, 5},  {5, 5, 1},  {5, 6, 0},  {5, 7, 0},  {5, 8, 0},   {5, 9, 0},
      {6, 0, 1},  {6, 1, 6},  {6, 2, 15}, {6, 3, 20}, {6, 4, 15},  {6, 5, 6},
      {6, 6, 1},  {6, 7, 0},  {6, 8, 0},  {6, 9, 0},  {7, 0, 1},   {7, 1, 7},
      {7, 2, 21}, {7, 3, 35}, {7, 4, 35}, {7, 5, 21}, {7, 6, 7},   {7, 7, 1},
      {7, 8, 0},  {7, 9, 0},  {8, 0, 1},  {8, 1, 8},  {8, 2, 28},  {8, 3, 56},
      {8, 4, 70}, {8, 5, 56}, {8, 6, 28}, {8, 7, 8},  {8, 8, 1},   {8, 9, 0},
      {9, 0, 1},  {9, 1, 9},  {9, 2, 36}, {9, 3, 84}, {9, 4, 126}, {9, 5, 126},
      {9, 6, 84}, {9, 7, 36}, {9, 8, 9},  {9, 9, 1},
      // clang-format on

      // A few larger cases:
      {30, 3, 4060},
      {100, 2, 4950},
      {100, 5, 75287520},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(BinomialCoefficient(test_case.n, test_case.k), test_case.expected)
        << "n=" << test_case.n << ", k=" << test_case.k;
  }
}

TEST(CombinatoricsTest, GetKCombinationAtIndex) {
  // Test cases vetted via an equivalent calculator:
  // https://planetcalc.com/8592/
  struct {
    int index;
    int k;
    std::vector<int> expected;
  } kTestCases[]{
      {0, 0, {}},

      // clang-format off
      {0, 1, {0}},        {1, 1, {1}},        {2, 1, {2}},
      {3, 1, {3}},        {4, 1, {4}},        {5, 1, {5}},
      {6, 1, {6}},        {7, 1, {7}},        {8, 1, {8}},
      {9, 1, {9}},        {10, 1, {10}},      {11, 1, {11}},
      {12, 1, {12}},      {13, 1, {13}},      {14, 1, {14}},
      {15, 1, {15}},      {16, 1, {16}},      {17, 1, {17}},
      {18, 1, {18}},      {19, 1, {19}},

      {0, 2, {1, 0}},     {1, 2, {2, 0}},     {2, 2, {2, 1}},
      {3, 2, {3, 0}},     {4, 2, {3, 1}},     {5, 2, {3, 2}},
      {6, 2, {4, 0}},     {7, 2, {4, 1}},     {8, 2, {4, 2}},
      {9, 2, {4, 3}},     {10, 2, {5, 0}},    {11, 2, {5, 1}},
      {12, 2, {5, 2}},    {13, 2, {5, 3}},    {14, 2, {5, 4}},
      {15, 2, {6, 0}},    {16, 2, {6, 1}},    {17, 2, {6, 2}},
      {18, 2, {6, 3}},    {19, 2, {6, 4}},

      {0, 3, {2, 1, 0}},  {1, 3, {3, 1, 0}},  {2, 3, {3, 2, 0}},
      {3, 3, {3, 2, 1}},  {4, 3, {4, 1, 0}},  {5, 3, {4, 2, 0}},
      {6, 3, {4, 2, 1}},  {7, 3, {4, 3, 0}},  {8, 3, {4, 3, 1}},
      {9, 3, {4, 3, 2}},  {10, 3, {5, 1, 0}}, {11, 3, {5, 2, 0}},
      {12, 3, {5, 2, 1}}, {13, 3, {5, 3, 0}}, {14, 3, {5, 3, 1}},
      {15, 3, {5, 3, 2}}, {16, 3, {5, 4, 0}}, {17, 3, {5, 4, 1}},
      {18, 3, {5, 4, 2}}, {19, 3, {5, 4, 3}},
      // clang-format on

      {2924, 3, {26, 25, 24}},
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(GetKCombinationAtIndex(test_case.index, test_case.k),
              test_case.expected)
        << "index=" << test_case.index << ", k=" << test_case.k;
  }
}

// Simple stress test to make sure that GetKCombinationAtIndex is returning
// combinations uniquely indexed by the given index, i.e. there are never any
// repeats.
TEST(CombinatoricsTest, GetKCombination_NoRepeats) {
  for (int k = 1; k < 5; k++) {
    std::set<std::vector<int>> seen_combinations;
    for (int index = 0; index < 3000; index++) {
      EXPECT_TRUE(
          seen_combinations.insert(GetKCombinationAtIndex(index, k)).second)
          << "index=" << index << ", k=" << k;
    }
  }
}

// The k-combination at a given index is the unique set of k positive integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0 such that
// `index` = \sum_{i=1}^k {a_i}\choose{i}
TEST(CombinatoricsTest, GetKCombination_MatchesDefinition) {
  for (int k = 1; k < 5; k++) {
    for (int index = 0; index < 3000; index++) {
      std::vector<int> combination = GetKCombinationAtIndex(index, k);
      int sum = 0;
      for (int i = 0; i < k; i++) {
        sum += BinomialCoefficient(combination[i], k - i);
      }
      EXPECT_EQ(index, sum);
    }
  }
}

TEST(CombinatoricsTest, SampleStarsAndBars_MatchesExpectedDistribution) {
  const int kNumStars = 2;
  const int kNumBars = 3;
  const int kNumSamples = 100000;

  base::flat_map<std::vector<int>, int> star_bar_counts;
  for (int i = 0; i < kNumSamples; i++) {
    std::vector<int> stars_and_bars = SampleStarsAndBars(kNumStars, kNumBars);
    star_bar_counts[stars_and_bars]++;
  }

  // This is the coupon collector problem (see
  // https://en.wikipedia.org/wiki/Coupon_collector%27s_problem).
  // For n possible results:
  //
  // the expected number of trials needed to see all possible results is equal
  // to n * Sum_{i = 1,..,n} 1/i.
  //
  // The variance of the number of trials is equal to
  // Sum_{i = 1,.., n} (1 - p_i) / p_i^2,
  // where p_i = (n - i + 1) / n.
  //
  // The probability that t trials are not enough to see all possible results is
  // at most n^{-t/(n*ln(n)) + 1}.
  //
  // For the parameter setting in this test, n = 10, so the mean is ~ 29 and the
  // standard deviation is ~ 11.2.
  // Moreover, the probability that not all of the 10 states are seen after  t =
  // kNumSamples = 100000 trials is at most 10^{-4341}, which is 0 for all
  // practical purposes. So the following test should always pass.
  int expected_num_combinations =
      BinomialCoefficient(kNumStars + kNumBars, kNumStars);
  EXPECT_EQ(static_cast<int>(star_bar_counts.size()),
            expected_num_combinations);

  // For any of the n possible results, the expected number times it is seen is
  // equal to 1/n.
  // Moreover, for any possible result, the probability that it is seen more
  // than (1+alpha)*t/n times is at most
  // p_high = exp(- D(1/n + alpha/n || 1/n) * t).
  //
  // The probability that it is seen less than (1-alpha)*t/n times is at most
  // p_low = exp(-D(1/n - alpha/n || 1/n) * t,
  //
  // where D( x || y) = x * ln(x/y) + (1-x) * ln( (1-x) / (1-y) ).
  // See
  // https://en.wikipedia.org/wiki/Chernoff_bound#Additive_form_(absolute_error)
  // for details.
  //
  // Thus, the probability that the number of occurrences of one of the results
  // deviates from its expectation by alpha*t/n is at most
  // n * (p_high + p_low).
  //
  // For the parameter setting in this test, where n = 10, alpha = 0.05, and t =
  // 100000, this probability is at most 0.000019. So the following test should
  // pass with probability close to 1.
  int expected_counts =
      kNumSamples / static_cast<double>(expected_num_combinations);
  for (const auto& star_bar_count : star_bar_counts) {
    const double abs_error = expected_counts * .05;
    EXPECT_NEAR(star_bar_count.second, expected_counts, abs_error);
  }
}

}  // namespace content

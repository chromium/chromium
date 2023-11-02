// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/combinatorics.h"

#include <set>
#include <vector>

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

TEST(CombinatoricsTest, GetNumberOfStarsAndBarsSequences) {
  EXPECT_EQ(3,
            GetNumberOfStarsAndBarsSequences(/*num_stars=*/1, /*num_bars=*/2));

  EXPECT_EQ(2925,
            GetNumberOfStarsAndBarsSequences(/*num_stars=*/3, /*num_bars=*/24));
}

TEST(CombinatoricsTest, GetStarIndices) {
  const struct {
    int num_stars;
    int num_bars;
    int sequence_index;
    std::vector<int> expected;
  } kTestCases[] = {
      {1, 2, 2, {2}},
      {3, 24, 23, {6, 3, 0}},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(GetStarIndices(test_case.num_stars, test_case.num_bars,
                             test_case.sequence_index),
              test_case.expected);
  }
}

TEST(CombinatoricsTest, GetBarsPrecedingEachStar) {
  const struct {
    std::vector<int> star_indices;
    std::vector<int> expected;
  } kTestCases[] = {
      {{2}, {2}},
      {{6, 3, 0}, {4, 2, 0}},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(GetBarsPrecedingEachStar(test_case.star_indices),
              test_case.expected);
  }
}

}  // namespace content

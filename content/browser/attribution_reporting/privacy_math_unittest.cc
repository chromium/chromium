// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::mojom::SourceType;

TEST(PrivacyMathTest, BinomialCoefficient) {
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
    EXPECT_EQ(internal::BinomialCoefficient(test_case.n, test_case.k),
              test_case.expected)
        << "n=" << test_case.n << ", k=" << test_case.k;
  }
}

TEST(PrivacyMathTest, GetKCombinationAtIndex) {
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
    EXPECT_EQ(internal::GetKCombinationAtIndex(test_case.index, test_case.k),
              test_case.expected)
        << "index=" << test_case.index << ", k=" << test_case.k;
  }
}

// Simple stress test to make sure that GetKCombinationAtIndex is returning
// combinations uniquely indexed by the given index, i.e. there are never any
// repeats.
TEST(PrivacyMathTest, GetKCombination_NoRepeats) {
  for (int k = 1; k < 5; k++) {
    std::set<std::vector<int>> seen_combinations;
    for (int index = 0; index < 3000; index++) {
      EXPECT_TRUE(
          seen_combinations.insert(internal::GetKCombinationAtIndex(index, k))
              .second)
          << "index=" << index << ", k=" << k;
    }
  }
}

// The k-combination at a given index is the unique set of k positive integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0 such that
// `index` = \sum_{i=1}^k {a_i}\choose{i}
TEST(PrivacyMathTest, GetKCombination_MatchesDefinition) {
  for (int k = 1; k < 5; k++) {
    for (int index = 0; index < 3000; index++) {
      std::vector<int> combination = internal::GetKCombinationAtIndex(index, k);
      int sum = 0;
      for (int i = 0; i < k; i++) {
        sum += internal::BinomialCoefficient(combination[i], k - i);
      }
      EXPECT_EQ(index, sum);
    }
  }
}

TEST(PrivacyMathTest, GetNumberOfStarsAndBarsSequences) {
  EXPECT_EQ(3, internal::GetNumberOfStarsAndBarsSequences(/*num_stars=*/1,
                                                          /*num_bars=*/2));

  EXPECT_EQ(2925, internal::GetNumberOfStarsAndBarsSequences(/*num_stars=*/3,
                                                             /*num_bars=*/24));
}

TEST(PrivacyMathTest, GetStarIndices) {
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
    EXPECT_EQ(internal::GetStarIndices(test_case.num_stars, test_case.num_bars,
                                       test_case.sequence_index),
              test_case.expected);
  }
}

TEST(PrivacyMathTest, GetBarsPrecedingEachStar) {
  const struct {
    std::vector<int> star_indices;
    std::vector<int> expected;
  } kTestCases[] = {
      {{2}, {2}},
      {{6, 3, 0}, {4, 2, 0}},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(internal::GetBarsPrecedingEachStar(test_case.star_indices),
              test_case.expected);
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L76C1-L82C2
TEST(PrivacyMathTest, BinaryEntropy) {
  const struct {
    double x;
    double expected;
  } kTestCases[] = {
      {.x = 0, .expected = 0},
      {.x = 0.5, .expected = 1},
      {.x = 1, .expected = 0},
      {.x = 0.01, .expected = 0.08079313589591118},
      {.x = 0.99, .expected = 0.08079313589591124},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, internal::BinaryEntropy(test_case.x));
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L10-L31
TEST(PrivacyMathTest, GetRandomizedResponseRate) {
  const struct {
    int64_t num_states;
    double epsilon;
    double expected;
  } kTestCases[] = {
      {
          .num_states = 2,
          .epsilon = std::log(3),
          .expected = 0.5,
      },
      {
          .num_states = 3,
          .epsilon = std::log(3),
          .expected = 0.6,
      },
      {
          .num_states = 2925,
          .epsilon = 14,
          .expected = 0.0024263221679834087,
      },
      {
          .num_states = 3,
          .epsilon = 14,
          .expected = 0.000002494582008677539,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, GetRandomizedResponseRate(
                                      test_case.num_states, test_case.epsilon));
  }
}

// Adapted from
// https://github.com/WICG/attribution-reporting-api/blob/ab43f8c989cf881ffd7a7f71801b98d649ed164a/flexible-event/privacy.test.ts#L38-L69
TEST(PrivacyMathTest, ComputeChannelCapacity) {
  const struct {
    int64_t num_states;
    double epsilon;
    double expected;
  } kTestCases[] = {
      {
          .num_states = 2,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = 1,
      },
      {
          .num_states = 1024,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = std::log2(1024),
      },
      {
          .num_states = 3,
          .epsilon = std::numeric_limits<double>::infinity(),
          .expected = std::log2(3),
      },
      {
          .num_states = 2,
          .epsilon = std::log(3),
          .expected = 0.18872187554086717,
      },
      {
          .num_states = 2925,
          .epsilon = 14,
          .expected = 11.461727965384876,
      },
      {
          .num_states = 3,
          .epsilon = 14,
          .expected = 1.584926511508231,
      },
      {
          .num_states = 1,
          .epsilon = 14,
          .expected = 0,
      },
  };

  for (const auto& test_case : kTestCases) {
    double rate =
        GetRandomizedResponseRate(test_case.num_states, test_case.epsilon);

    EXPECT_EQ(test_case.expected,
              internal::ComputeChannelCapacity(test_case.num_states, rate));
  }
}

TEST(PrivacyMathTest, GetFakeReportsForSequenceIndex) {
  const struct {
    SourceType source_type;
    int sequence_index;
    std::vector<FakeEventLevelReport> expected;
  } kTestCases[] = {
      // Event sources only have 3 output states, so we can enumerate them:
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 1,
          .expected = {{.trigger_data = 0, .window_index = 0}},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 2,
          .expected = {{.trigger_data = 1, .window_index = 0}},
      },
      // Navigation sources have 2925 output states, so pick interesting ones:
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 20,
          .expected = {{.trigger_data = 3, .window_index = 0}},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 41,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 2, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 50,
          .expected =
              {
                  {.trigger_data = 4, .window_index = 0},
                  {.trigger_data = 4, .window_index = 0},
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 1268,
          .expected =
              {
                  {.trigger_data = 1, .window_index = 2},
                  {.trigger_data = 6, .window_index = 1},
                  {.trigger_data = 7, .window_index = 0},
              },
      },
  };

  for (const auto& test_case : kTestCases) {
    int trigger_data_cardinality =
        attribution_reporting::DefaultTriggerDataCardinality(
            test_case.source_type);
    int max_reports = test_case.source_type == SourceType::kEvent ? 1 : 3;
    EXPECT_EQ(test_case.expected,
              internal::GetFakeReportsForSequenceIndex(
                  trigger_data_cardinality,
                  *EventReportWindows::FromDefaults(base::Days(30),
                                                    test_case.source_type),
                  max_reports, test_case.sequence_index))
        << test_case.sequence_index;
  }
}

void RunRandomFakeReportsTest(const int trigger_data_cardinality,
                              const EventReportWindows& windows,
                              const int max_reports,
                              const int num_samples,
                              const double tolerance) {
  base::flat_map<std::vector<FakeEventLevelReport>, int> output_counts;
  const int64_t num_states =
      GetNumStates(trigger_data_cardinality, windows, max_reports);
  for (int i = 0; i < num_samples; i++) {
    std::vector<FakeEventLevelReport> fake_reports =
        internal::GetRandomFakeReports(trigger_data_cardinality, windows,
                                       max_reports, num_states);
    output_counts[fake_reports]++;
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
  EXPECT_EQ(static_cast<int64_t>(output_counts.size()), num_states);

  // For any of the n possible results, the expected number of times it is seen
  // is equal to 1/n. Moreover, for any possible result, the probability that it
  // is seen more than (1+alpha)*t/n times is at most p_high = exp(- D(1/n +
  // alpha/n || 1/n) * t).
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
  int expected_counts = num_samples / static_cast<double>(num_states);
  for (const auto& output_count : output_counts) {
    const double abs_error = expected_counts * tolerance;
    EXPECT_NEAR(output_count.second, expected_counts, abs_error);
  }
}

TEST(PrivacyMathTest, GetRandomFakeReports_Event_MatchesExpectedDistribution) {
  // The probability that not all of the 3 states are seen after `num_samples`
  // trials is at most ~1e-14476, which is 0 for all practical purposes, so the
  // `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most 1e-9.
  RunRandomFakeReportsTest(
      /*trigger_data_cardinality=*/2,
      *EventReportWindows::FromDefaults(base::Days(30), SourceType::kEvent),
      /*max_reports=*/1,
      /*num_samples=*/100'000,
      /*tolerance=*/0.03);
}

TEST(PrivacyMathTest,
     GetRandomFakeReports_Navigation_MatchesExpectedDistribution) {
  // The probability that not all of the 2925 states are seen after
  // `num_samples` trials is at most ~1e-19, which is 0 for all practical
  // purposes, so the `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most .0002.
  RunRandomFakeReportsTest(
      /*trigger_data_cardinality=*/8,
      *EventReportWindows::FromDefaults(base::Days(30),
                                        SourceType::kNavigation),
      /*max_reports=*/3,
      /*num_samples=*/150'000,
      /*tolerance=*/0.9);
}

}  // namespace
}  // namespace content

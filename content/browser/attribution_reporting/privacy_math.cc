// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

// The max possible number of state combinations given a valid input.
constexpr int64_t kMaxNumCombinations = 4191844505805495;

}  // namespace

RandomizedResponseData::RandomizedResponseData(double rate,
                                               double channel_capacity,
                                               RandomizedResponse response)
    : rate_(rate),
      channel_capacity_(channel_capacity),
      response_(std::move(response)) {
  DCHECK_GE(rate_, 0);
  DCHECK_LE(rate_, 1);
  DCHECK_GE(channel_capacity_, 0);
}

RandomizedResponseData::~RandomizedResponseData() = default;

RandomizedResponseData::RandomizedResponseData(const RandomizedResponseData&) =
    default;

RandomizedResponseData& RandomizedResponseData::operator=(
    const RandomizedResponseData&) = default;

RandomizedResponseData::RandomizedResponseData(RandomizedResponseData&&) =
    default;

RandomizedResponseData& RandomizedResponseData::operator=(
    RandomizedResponseData&&) = default;

bool GenerateWithRate(double r) {
  DCHECK_GE(r, 0);
  DCHECK_LE(r, 1);
  return base::RandDouble() < r;
}

double GetRandomizedResponseRate(int64_t num_states, double epsilon) {
  DCHECK_GT(num_states, 0);
  return num_states / (num_states - 1 + std::exp(epsilon));
}

int64_t GetNumStates(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports) {
  return internal::GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/max_event_level_reports,
      /*num_bars=*/trigger_data_cardinality *
          event_report_windows.end_times().size());
}

RandomizedResponseData DoRandomizedResponse(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports,
    double epsilon) {
  const int64_t num_states = GetNumStates(
      trigger_data_cardinality, event_report_windows, max_event_level_reports);
  double rate = GetRandomizedResponseRate(num_states, epsilon);
  double channel_capacity = internal::ComputeChannelCapacity(num_states, rate);
  auto fake_reports = GenerateWithRate(rate)
                          ? absl::make_optional(internal::GetRandomFakeReports(
                                trigger_data_cardinality, event_report_windows,
                                max_event_level_reports, num_states))
                          : absl::nullopt;
  return RandomizedResponseData(rate, channel_capacity,
                                std::move(fake_reports));
}

namespace internal {

int64_t BinomialCoefficient(int n, int k) {
  DCHECK_GE(n, 0);
  DCHECK_GE(k, 0);

  if (k > n) {
    return 0;
  }

  // Speed up some trivial cases.
  if (k == n || n == 0) {
    return 1;
  }

  // BinomialCoefficient(n, k) == BinomialCoefficient(n, n - k),
  // So simplify if possible.
  if (k > n - k) {
    k = n - k;
  }

  // (n choose k) = n (n -1) ... (n - (k - 1)) / k!
  // = mul((n + i - i) / i), i from 1 -> k.
  //
  // You might be surprised that this algorithm works just fine with integer
  // division (i.e. division occurs cleanly with no remainder). However, this is
  // true for a very simple reason. Imagine a value of `i` causes division with
  // remainder in the below algorithm. This immediately implies that
  // (n choose i) is fractional, which we know is not the case.
  int64_t result = 1;
  for (int i = 1; i <= k; i++) {
    result = base::CheckMul(result, n + 1 - i).ValueOrDie();
    DCHECK_EQ(0, result % i);
    result = result / i;
  }
  return result;
}

// Computes the `combination_index`-th lexicographically smallest k-combination.
// https://en.wikipedia.org/wiki/Combinatorial_number_system

// A k-combination is a sequence of k non-negative integers in decreasing order.
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0.
// k-combinations can be ordered lexicographically, with the smallest
// k-combination being a_k=k-1, a_{k-1}=k-2, .., a_1=0. Given an index
// `combination_index`>=0, and an order k, this method returns the
// `combination_index`-th smallest k-combination.
//
// Given an index `combination_index`, the `combination_index`-th k-combination
// is the unique set of k non-negative integers
// a_k > a_{k-1} > ... > a_2 > a_1 >= 0
// such that `combination_index` = \sum_{i=1}^k {a_i}\choose{i}
//
// We find this set via a simple greedy algorithm.
// http://math0.wvstateu.edu/~baker/cs405/code/Combinadics.html
std::vector<int> GetKCombinationAtIndex(int64_t combination_index, int k) {
  DCHECK_GE(combination_index, 0);
  DCHECK_GE(k, 0);
  // `k` can be no more than max number of event level reports per source (20).
  DCHECK_LE(k, 20);

  std::vector<int> output_k_combination;
  output_k_combination.reserve(k);
  if (k == 0) {
    return output_k_combination;
  }

  // To find a_k, iterate candidates upwards from 0 until we've found the
  // maximum a such that (a choose k) <= `combination_index`. Let a_k = a. Use
  // the previous binomial coefficient to compute the next one. Note: possible
  // to speed this up via something other than incremental search.
  int64_t target = combination_index;
  int candidate = k - 1;
  int64_t binomial_coefficient = 0;       // BinomialCoefficient(candidate, k)
  int64_t next_binomial_coefficient = 1;  // BinomialCoefficient(candidate+1, k)
  while (next_binomial_coefficient <= target) {
    candidate++;
    binomial_coefficient = next_binomial_coefficient;
    DCHECK_EQ(binomial_coefficient, BinomialCoefficient(candidate, k));

    // (n + 1 choose k) = (n choose k) * (n + 1) / (n + 1 - k)
    next_binomial_coefficient =
        base::CheckMul(binomial_coefficient, candidate + 1).ValueOrDie();
    next_binomial_coefficient /= candidate + 1 - k;
  }
  // We know from the k-combination definition, all subsequent values will be
  // strictly decreasing. Find them all by decrementing `candidate`.
  // Use the previous binomial coefficient to compute the next one.
  int current_k = k;
  while (true) {
    // The optimized code below maintains this loop invariant.
    DCHECK_EQ(binomial_coefficient, BinomialCoefficient(candidate, current_k));
    if (binomial_coefficient <= target) {
      output_k_combination.push_back(candidate);
      target -= binomial_coefficient;
      if (static_cast<int>(output_k_combination.size()) == k) {
        DCHECK_EQ(target, 0);
        return output_k_combination;
      }
      // (n - 1 choose k - 1) = (n choose k) * k / n
      binomial_coefficient = binomial_coefficient * (current_k) / candidate;

      current_k--;
      candidate--;
    } else {
      // (n - 1 choose k) = (n choose k) * (n - k) / n
      binomial_coefficient =
          binomial_coefficient * (candidate - current_k) / candidate;

      candidate--;
    }
  }
}

int64_t GetNumberOfStarsAndBarsSequences(int num_stars, int num_bars) {
  return BinomialCoefficient(num_stars + num_bars, num_stars);
}

std::vector<int> GetStarIndices(int num_stars,
                                int num_bars,
                                int64_t sequence_index) {
  DCHECK_LT(sequence_index,
            GetNumberOfStarsAndBarsSequences(num_stars, num_bars));
  return GetKCombinationAtIndex(sequence_index, num_stars);
}

std::vector<int> GetBarsPrecedingEachStar(std::vector<int> out) {
  DCHECK(base::ranges::is_sorted(out, std::greater{}));

  for (size_t i = 0u; i < out.size(); i++) {
    int star_index = out[i];

    // There are `star_index` prior positions in the sequence, and `i` prior
    // stars, so there are `star_index` - `i` prior bars.
    out[i] = star_index - (out.size() - 1 - i);
  }
  return out;
}

double BinaryEntropy(double p) {
  if (p == 0 || p == 1) {
    return 0;
  }

  return -p * log2(p) - (1 - p) * log2(1 - p);
}

double ComputeChannelCapacity(int64_t num_states,
                              double randomized_response_rate) {
  DCHECK_GT(num_states, 0);
  DCHECK_GE(randomized_response_rate, 0);
  DCHECK_LE(randomized_response_rate, 1);

  // The capacity of a unary channel is 0. This follows from the definition
  // of mutual information.
  if (num_states == 1) {
    return 0;
  }

  double p = randomized_response_rate * (num_states - 1) / num_states;
  return log2(num_states) - BinaryEntropy(p) - p * log2(num_states - 1);
}

std::vector<FakeEventLevelReport> GetRandomFakeReports(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports,
    int64_t num_states) {
  const int64_t sequence_index =
      static_cast<int64_t>(base::RandGenerator(num_states));
  DCHECK_GE(sequence_index, 0);
  DCHECK_LE(sequence_index, kMaxNumCombinations);

  return GetFakeReportsForSequenceIndex(
      trigger_data_cardinality, event_report_windows, max_event_level_reports,
      sequence_index);
}

std::vector<FakeEventLevelReport> GetFakeReportsForSequenceIndex(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports,
    int64_t random_stars_and_bars_sequence_index) {
  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/max_event_level_reports,
          /*num_bars=*/trigger_data_cardinality *
              event_report_windows.end_times().size(),
          /*sequence_index=*/random_stars_and_bars_sequence_index));

  std::vector<FakeEventLevelReport> fake_reports;

  // an output state is uniquely determined by an ordering of c stars and w*d
  // bars, where:
  // w = the number of reporting windows
  // c = the maximum number of reports for a source
  // d = the trigger data cardinality for a source
  for (int num_bars : bars_preceding_each_star) {
    if (num_bars == 0) {
      continue;
    }

    auto result = std::div(num_bars - 1, trigger_data_cardinality);

    const int trigger_data = result.rem;
    DCHECK_GE(trigger_data, 0);
    DCHECK_LT(trigger_data, trigger_data_cardinality);

    fake_reports.push_back({.trigger_data = static_cast<uint64_t>(trigger_data),
                            .window_index = result.quot});
  }
  DCHECK_LE(fake_reports.size(), static_cast<size_t>(max_event_level_reports));
  return fake_reports;
}

}  // namespace internal

}  // namespace content

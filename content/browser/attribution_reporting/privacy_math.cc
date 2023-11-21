// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/privacy_math.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/trigger_config.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

// The max possible number of state combinations given a valid input.
// This comes from 20 maximum total reports, 20 reports per type, 5 windows per
// type, and 32 distinct trigger data values.
constexpr absl::uint128 kMaxNumCombinations =
    absl::MakeUint128(/*high=*/9494472u, /*low=*/10758590974061625903u);

absl::uint128 RandGenerator(absl::uint128 range) {
  DCHECK_GT(range, 0u);
  uint64_t high = absl::Uint128High64(range);
  return absl::MakeUint128(
      /*high=*/high == 0u ? 0u : base::RandGenerator(high),
      /*low=*/base::RandGenerator(absl::Uint128Low64(range)));
}

// Let B be the trigger data cardinality.
// For every trigger data i, there are wi windows and ci maximum reports.
// Let A[C, w1, ..., wB, c1, ..., cB] be the function which counts the number
// of output states.
//
// The following helper function memoizes the recurrence relation which computes
// this:
//
// 1. A[C,w1,...,wB,c1,...,cB] = 1 if B = 0
// If there are no trigger data types to consider, there is only one possible
// output, the null output.
//
// 2. A[C,w1,...,wB,c1,...,cB] = A[C,w1,...,w{B-1},c1,...,c{B-1}] if wB = 0
// If there are no windows to consider for a particular trigger data type, then
// consider only the remaining trigger data types.
//
// 3. A[C,w1,...,wB,c1,...,cB] = sum(A[C - j,w1,...,wB - 1,c1,...,cB - j],
//                                   for j from 0 to min(c_B, C))
// Otherwise, we look at the number of possible outputs assuming we emit some
// number of reports (up to the max) for the current trigger data type under
// consideration. Given that each choice produces a distinct output, we sum
// these up.
absl::uint128 GetNumStatesRecursive(
    attribution_reporting::TriggerSpecs::Iterator it,
    int max_reports,
    int window_val,
    int max_reports_per_type,
    internal::StateMap& map) {
  // Case 1: "B = 0" there is nothing left to assign for the last data index.
  // Also consider the trivial Case 2 -> Case 1 case without touching the cache
  // or recursive calls.
  auto cur = it++;
  if (!cur || (window_val == 0 && !it)) {
    return 1;
  }

  // Store these as 8 bit to optimize storage.
  absl::uint128& cached =
      map[std::make_tuple(base::checked_cast<uint8_t>(max_reports), it.index(),
                          base::checked_cast<uint8_t>(window_val),
                          base::checked_cast<uint8_t>(max_reports_per_type))];
  if (cached != 0) {
    return cached;
  }

  // Case 2: wB = 0.
  //
  // TODO(csharrison): Use the actual spec's max reports when that is
  // implemented. Currently we set `max_reports_per_type` to be equal to
  // `max_reports` for every type, but in the future it will be specified on the
  // `TriggerSpec` as part of the `summary_buckets` field.
  if (window_val == 0) {
    cached = GetNumStatesRecursive(
        it, max_reports, (*it).second.event_report_windows().end_times().size(),
        max_reports, map);
    return cached;
  }
  // Case 3.
  for (int i = 0; i <= std::min(max_reports_per_type, max_reports); i++) {
    cached += GetNumStatesRecursive(cur, max_reports - i, window_val - 1,
                                    max_reports_per_type - i, map);
  }
  return cached;
}

// A variant of the above algorithm which samples a report given an index.
// This follows a similarly structured algorithm.
void GetReportsFromIndexRecursive(
    attribution_reporting::TriggerSpecs::Iterator it,
    int max_reports,
    int window_val,
    int max_reports_per_type,
    absl::uint128 index,
    std::vector<FakeEventLevelReport>& reports,
    internal::StateMap& map) {
  // Case 1 and Case 2 -> 1. There are no more valid trigger data value, so
  // generate nothing.
  auto cur = it++;
  if (!cur || (window_val == 0 && !it)) {
    return;
  }
  // Case 2: there are no more windows to consider for the current trigger data,
  // so generate based on the remaining trigger data types.
  //
  // TODO(csharrison): Use the actual spec's max reports when that is
  // implemented. Currently we set `max_reports_per_type` to be equal to
  // `max_reports` for every type, but in the future it will be specified on the
  // `TriggerSpec` as part of the `summary_buckets` field.
  if (window_val == 0) {
    GetReportsFromIndexRecursive(
        it, max_reports, (*it).second.event_report_windows().end_times().size(),
        max_reports, index, reports, map);
    return;
  }

  // Case 3: For the current window and trigger data under consideration, we
  // need to choose how many reports we emit. Think of the index as pointing to
  // a particular output, where outputs are partitioned by the # of reports to
  // emit. E.g. think of each dash below as a possible output.
  //
  //       0 report              1 reports          2 reports
  // |----------------------|---------------------|-----------|
  //                        ^             ^
  //                     prev_sum       index
  //
  // The first thing we need to do is figure out how many reports to emit, this
  // is as simple as just computing the # of states with 0 reports, 1 report,
  // and so on until we find where the index slots in.
  //
  // Next, we "zoom in" to that partition of outputs in the recursive step to
  // figure out what other reports we need to emit (if any). We consider a new
  // index which just looks at the "dashes" before `index`, i.e. index' = index
  // - prev_sum.
  absl::uint128 prev_sum = 0;
  for (int i = 0; i <= std::min(max_reports_per_type, max_reports); i++) {
    absl::uint128 num_states = GetNumStatesRecursive(
        cur, max_reports - i, window_val - 1, max_reports_per_type - i, map);

    // The index is associated with emitting `i` reports
    if (num_states + prev_sum > index) {
      for (int k = 0; k < i; k++) {
        reports.push_back(FakeEventLevelReport{.trigger_data = (*cur).first,
                                               .window_index = window_val - 1});
      }
      DCHECK_GE(index - prev_sum, 0);

      // Zoom into all other outputs that are associated with picking `i`
      // reports for this config.
      GetReportsFromIndexRecursive(cur, max_reports - i, window_val - 1,
                                   max_reports_per_type - i, index - prev_sum,
                                   reports, map);
      return;
    }
    prev_sum += num_states;
  }
  NOTREACHED();
}

absl::uint128 GetNumStatesCached(
    const attribution_reporting::TriggerSpecs& specs,
    int max_reports,
    internal::StateMap& map) {
  if (specs.empty() || max_reports == 0) {
    return 1;
  }

  auto it = specs.begin();
  size_t num_windows = (*it).second.event_report_windows().end_times().size();

  // Optimized fast-path.
  if (specs.SingleSharedSpec()) {
    int64_t states = internal::GetNumberOfStarsAndBarsSequences(
        /*num_stars=*/max_reports,
        /*num_bars=*/specs.size() * num_windows);
    DCHECK_EQ(states, GetNumStatesRecursive(it, max_reports, num_windows,
                                            max_reports, map));
    DCHECK_GE(states, 0);
    return states;
  }
  return GetNumStatesRecursive(it, max_reports, num_windows, max_reports, map);
}

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

double GetRandomizedResponseRate(absl::uint128 num_states, double epsilon) {
  DCHECK_GT(num_states, 0);

  double num_states_double = static_cast<double>(num_states);
  return num_states_double / (num_states_double - 1 + std::exp(epsilon));
}

absl::uint128 GetNumStates(
    const attribution_reporting::TriggerSpecs& specs,
    attribution_reporting::MaxEventLevelReports max_reports) {
  internal::StateMap map;
  return GetNumStatesCached(specs, max_reports, map);
}

RandomizedResponseData DoRandomizedResponse(
    const attribution_reporting::TriggerSpecs& specs,
    attribution_reporting::MaxEventLevelReports max_reports,
    double epsilon) {
  internal::StateMap map;
  return internal::DoRandomizedResponseWithCache(specs, max_reports, epsilon,
                                                 map);
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

std::vector<FakeEventLevelReport> GetFakeReportsForSequenceIndex(
    const attribution_reporting::TriggerSpecs& specs,
    int max_reports,
    absl::uint128 index,
    StateMap& map) {
  std::vector<FakeEventLevelReport> reports;

  if (specs.empty() || max_reports == 0) {
    return reports;
  }

  auto it = specs.begin();
  GetReportsFromIndexRecursive(
      it, max_reports, (*it).second.event_report_windows().end_times().size(),
      max_reports, index, reports, map);
  return reports;
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

double ComputeChannelCapacity(absl::uint128 num_states,
                              double randomized_response_rate) {
  DCHECK_GT(num_states, 0u);
  DCHECK_GE(randomized_response_rate, 0);
  DCHECK_LE(randomized_response_rate, 1);

  // The capacity of a unary channel is 0. This follows from the definition
  // of mutual information.
  if (num_states == 1u || randomized_response_rate == 1) {
    return 0;
  }

  double num_states_double = static_cast<double>(num_states);
  double p =
      randomized_response_rate * (num_states_double - 1) / num_states_double;
  return log2(num_states_double) - BinaryEntropy(p) -
         p * log2(num_states_double - 1);
}

std::vector<FakeEventLevelReport> GetFakeReportsForSequenceIndex(
    const attribution_reporting::TriggerSpecs& specs,
    int max_reports,
    int64_t random_stars_and_bars_sequence_index) {
  const attribution_reporting::TriggerSpec* single_spec =
      specs.SingleSharedSpec();
  CHECK(single_spec);

  const int trigger_data_cardinality = specs.size();

  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/max_reports,
          /*num_bars=*/trigger_data_cardinality *
              single_spec->event_report_windows().end_times().size(),
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

    const int trigger_data_index = result.rem;
    DCHECK_GE(trigger_data_index, 0);
    DCHECK_LT(trigger_data_index, trigger_data_cardinality);

    fake_reports.push_back({
        .trigger_data =
            std::next(specs.trigger_data_indices().begin(), trigger_data_index)
                ->first,
        .window_index = result.quot,
    });
  }
  DCHECK_LE(fake_reports.size(), static_cast<size_t>(max_reports));
  return fake_reports;
}

RandomizedResponseData DoRandomizedResponseWithCache(
    const attribution_reporting::TriggerSpecs& specs,
    int max_reports,
    double epsilon,
    StateMap& map) {
  const absl::uint128 num_states = GetNumStatesCached(specs, max_reports, map);
  double rate = GetRandomizedResponseRate(num_states, epsilon);
  absl::optional<std::vector<FakeEventLevelReport>> fake_reports;
  if (GenerateWithRate(rate)) {
    // TODO(csharrison): Justify the fast path with `single_spec` with
    // profiling.
    //
    // Note: we can implement the fast path in more cases than a single shared
    // spec if all of the specs have the same # of windows and reports. We can
    // consider further optimizing if it's useful. The existing code will cover
    // the default specs for navigation / event sources.
    const absl::uint128 sequence_index = RandGenerator(num_states);
    DCHECK_GE(sequence_index, 0);
    DCHECK_LT(sequence_index, kMaxNumCombinations);
    fake_reports = specs.SingleSharedSpec()
                       ? internal::GetFakeReportsForSequenceIndex(
                             specs, max_reports,
                             base::checked_cast<int64_t>(
                                 absl::Uint128Low64(sequence_index)))
                       : internal::GetFakeReportsForSequenceIndex(
                             specs, max_reports, sequence_index, map);
  }
  return RandomizedResponseData(
      rate, internal::ComputeChannelCapacity(num_states, rate),
      std::move(fake_reports));
}

}  // namespace internal

}  // namespace content

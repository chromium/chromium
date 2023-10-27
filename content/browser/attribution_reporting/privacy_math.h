// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_

#include <stdint.h>

#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
class EventReportWindows;
}

namespace content {

struct FakeEventLevelReport {
  uint64_t trigger_data;
  int window_index;
};

// Corresponds to `StoredSource::AttributionLogic` as follows:
// `absl::nullopt` -> `StoredSource::AttributionLogic::kTruthfully`
// empty vector -> `StoredSource::AttributionLogic::kNever`
// non-empty vector -> `StoredSource::AttributionLogic::kFalsely`
using RandomizedResponse = absl::optional<std::vector<FakeEventLevelReport>>;

class CONTENT_EXPORT RandomizedResponseData {
 public:
  RandomizedResponseData(double rate,
                         double channel_capacity,
                         RandomizedResponse);

  ~RandomizedResponseData();

  RandomizedResponseData(const RandomizedResponseData&);
  RandomizedResponseData& operator=(const RandomizedResponseData&);

  RandomizedResponseData(RandomizedResponseData&&);
  RandomizedResponseData& operator=(RandomizedResponseData&&);

  double rate() const { return rate_; }
  double channel_capacity() const { return channel_capacity_; }

  const RandomizedResponse& response() const { return response_; }

 private:
  double rate_;
  double channel_capacity_;
  RandomizedResponse response_;
};

// Returns true with probability r.
CONTENT_EXPORT bool GenerateWithRate(double r);

// https://wicg.github.io/attribution-reporting-api/#obtain-a-randomized-source-response-pick-rate
CONTENT_EXPORT double GetRandomizedResponseRate(int64_t num_states,
                                                double epsilon);

// Returns the number of possible output states for the given API configuration.
CONTENT_EXPORT int64_t
GetNumStates(int trigger_data_cardinality,
             const attribution_reporting::EventReportWindows&,
             int max_event_level_reports);

// Determines the randomized response flip probability for the given API
// configuration, and performs randomized response on that otutput space.
//
// Returns `absl::nullopt` if the output should be determined truthfully.
// Otherwise will return a vector of fake reports.
CONTENT_EXPORT RandomizedResponseData
DoRandomizedResponse(int trigger_data_cardinality,
                     const attribution_reporting::EventReportWindows&,
                     int max_event_level_reports,
                     double epsilon);

// Exposed for testing purposes.
namespace internal {

// Computes the binomial coefficient aka (`n` choose `k`).
// https://en.wikipedia.org/wiki/Binomial_coefficient
// Negative inputs are not supported.
//
// Note: large values of `n` and `k` may overflow. This function internally uses
// checked_math to crash safely if this occurs.
CONTENT_EXPORT int64_t BinomialCoefficient(int n, int k);

// Returns the k-combination associated with the number `combination_index`. In
// other words, returns the combination of `k` integers uniquely indexed by
// `combination_index` in the combinatorial number system.
// https://en.wikipedia.org/wiki/Combinatorial_number_system
//
// The returned vector is guaranteed to have size `k`.
CONTENT_EXPORT std::vector<int> GetKCombinationAtIndex(
    int64_t combination_index,
    int k);

// Returns the number of possible sequences of "stars and bars" sequences
// https://en.wikipedia.org/wiki/Stars_and_bars_(combinatorics),
// which is equivalent to (num_stars + num_bars choose num_stars).
CONTENT_EXPORT int64_t GetNumberOfStarsAndBarsSequences(int num_stars,
                                                        int num_bars);

// Returns a vector of the indices of every star in the stars and bars sequence
// indexed by `sequence_index`. The indexing technique uses the k-combination
// utility documented above.
CONTENT_EXPORT std::vector<int> GetStarIndices(int num_stars,
                                               int num_bars,
                                               int64_t sequence_index);

// From a vector with the index of every star in a stars and bars sequence,
// returns a vector which, for every star, counts the number of bars preceding
// it. Assumes `star_indices` is in descending order. Output is also sorted
// in descending order.
CONTENT_EXPORT std::vector<int> GetBarsPrecedingEachStar(
    std::vector<int> star_indices);

// Computes the binary entropy function:
// https://en.wikipedia.org/wiki/Binary_entropy_function
CONTENT_EXPORT double BinaryEntropy(double p);

// Computes the channel capacity of a qary-symmetric channel.
// https://wicg.github.io/attribution-reporting-api/#computing-channel-capacity
CONTENT_EXPORT double ComputeChannelCapacity(int64_t num_states,
                                             double randomized_response_rate);

// Generates fake reports using a random "stars and bars" sequence index of a
// possible output of the API.
CONTENT_EXPORT std::vector<FakeEventLevelReport> GetRandomFakeReports(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows&,
    int max_event_level_reports,
    int64_t num_states);

// Generates fake reports from the "stars and bars" sequence index of a
// possible output of the API. This output is determined by the following
// algorithm:
// 1. Find all stars before the first bar. These stars represent suppressed
//    reports.
// 2. For all other stars, count the number of bars that precede them. Each
//    star represents a report where the reporting window and trigger data is
//    uniquely determined by that number.
CONTENT_EXPORT std::vector<FakeEventLevelReport> GetFakeReportsForSequenceIndex(
    int trigger_data_cardinality,
    const attribution_reporting::EventReportWindows&,
    int max_event_level_reports,
    int64_t random_stars_and_bars_sequence_index);

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_

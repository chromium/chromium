// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_

#include <stdint.h>

#include <vector>

#include "content/common/content_export.h"

namespace content {

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

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_

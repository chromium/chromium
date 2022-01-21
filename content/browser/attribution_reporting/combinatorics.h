// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_

#include <vector>

#include "content/common/content_export.h"

namespace content {

// Computes the binomial coefficient aka (`n` choose `k`).
// https://en.wikipedia.org/wiki/Binomial_coefficient
// Negative inputs are not supported.
//
// Note: large values of `n` and `k` may overflow. This function internally uses
// checked_math to crash safely if this occurs.
CONTENT_EXPORT int BinomialCoefficient(int n, int k);

// Returns the k-combination associated with the number `combination_index`. In
// other words, returns the combination of `k` integers uniquely indexed by
// `combination_index` in the combinatorial number system.
// https://en.wikipedia.org/wiki/Combinatorial_number_system
//
// The returned vector is guaranteed to have size `k`.
CONTENT_EXPORT std::vector<int> GetKCombinationAtIndex(int combination_index,
                                                       int k);

// Returns the index of every star in a uniformly random sampled "stars and
// bars" sequence given by `num_stars` and `num_bars`.
CONTENT_EXPORT std::vector<int> SampleStarsAndBars(int num_stars, int num_bars);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMBINATORICS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_REAL_TIME_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_REAL_TIME_REPORT_UTIL_H_

#include <map>
#include <optional>
#include <vector>

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Basic RAPPOR noises each coordinate of the bit vector (of size `num_buckets`)
// independently, and it is parameterized by epsilon, a measure of privacy loss.
// See the explainer for more details:
// https://github.com/WICG/turtledove/blob/main/PA_real_time_monitoring.md#histogram-contributions-and-the-rappor-noise-algorithm
// CONTENT_EXPORT for testing.
CONTENT_EXPORT std::vector<uint8_t> Rappor(std::optional<int32_t> maybe_bucket,
                                           double epsilon,
                                           int num_buckets);

// Randomly select one contribution from `contributions` and return its index if
// `contributions` is not empty, otherwise return nullopt. The select is based
// on each contribution's priority weight.
// CONTENT_EXPORT for testing.
CONTENT_EXPORT std::optional<int32_t> SampleContributions(
    const std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>&
        contributions);

// Randomly select one contribution per origin, based on each contribution's
// priority weight, and convert it to a histogram using Rappor.
CONTENT_EXPORT std::map<url::Origin, std::vector<uint8_t>>
CalculateRealTimeReportingHistograms(
    std::map<
        url::Origin,
        std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>>
        contributions);

// Get the destination of sending real time report.
CONTENT_EXPORT GURL GetRealTimeReportDestination(const url::Origin& origin);

// Returns false if contribution has an invalid bucket. Note that it treats
// platform buckets as valid.
CONTENT_EXPORT bool HasValidRealTimeBucket(
    const auction_worklet::mojom::RealTimeReportingContributionPtr&
        contribution);

// Returns false if contribution has an invalid priority weight.
CONTENT_EXPORT bool HasValidRealTimePriorityWeight(
    const auction_worklet::mojom::RealTimeReportingContributionPtr&
        contribution);

// Bit-pack a vector of 0 and 1s, i.e., use a bit to represent a 0/1, instead of
// a byte. The first 8 elements of `data` packs to byte 0, the next 8 elements
// packs to byte 1, until all elements are packed into the output. Within each
// group of 8 elements, the first element packs to the most significant bit of
// a byte. For example, the result of packing [0,0,0,0,0,0,0,1, 1]
// is output[0]: 1, output[1]: 128.
CONTENT_EXPORT std::vector<uint8_t> BitPacking(std::vector<uint8_t> data);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_REAL_TIME_REPORT_UTIL_H_

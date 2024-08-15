// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_real_time_report_util.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "content/services/auction_worklet/public/cpp/real_time_reporting.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// The URL path for sending real time reporting histograms.
constexpr char kRealTimeReportPath[] =
    "/.well-known/interest-group/real-time-report";

}  // namespace

std::vector<uint8_t> Rappor(std::optional<int32_t> maybe_bucket,
                            double epsilon,
                            int num_buckets) {
  std::vector<uint8_t> histogram(num_buckets, 0);
  if (maybe_bucket.has_value()) {
    // Browser side that receives contributions from worklets should have
    // guaranteed this.
    CHECK_GE(*maybe_bucket, 0);
    CHECK_LT(*maybe_bucket, num_buckets);
    histogram[*maybe_bucket] = 1;
  }

  double f = 2.0 / (1 + std::exp(epsilon / 2.0));
  for (size_t i = 0; i < static_cast<size_t>(num_buckets); i++) {
    if (base::RandDouble() < f / 2.0) {
      histogram[i] = 1 - histogram[i];
    }
  }
  return histogram;
}

std::optional<int32_t> SampleContributions(
    const std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>&
        contributions) {
  if (contributions.empty()) {
    return std::nullopt;
  }
  double priority_weight_sum = 0.0;
  for (const auto& contribution : contributions) {
    // Browser side that receives contributions from worklets should have
    // guaranteed this.
    CHECK(contribution->priority_weight > 0);
    priority_weight_sum += contribution->priority_weight;
  }
  // `random` is always smaller than `priority_weight_sum`, since RandDouble()
  // produces output in the range [0, 1).
  double random = base::RandDouble() * priority_weight_sum;
  priority_weight_sum = 0.0;
  for (const auto& contribution : contributions) {
    priority_weight_sum += contribution->priority_weight;
    if (priority_weight_sum >= random) {
      return contribution->bucket;
    }
  }

  // If `contributions` is not empty, one should have be chosen and returned.
  NOTREACHED();
}

std::map<url::Origin, std::vector<uint8_t>>
CalculateRealTimeReportingHistograms(
    std::map<
        url::Origin,
        std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>>
        contributions) {
  std::map<url::Origin, std::vector<uint8_t>> histograms;
  for (const auto& [origin, single_origin_contributions] : contributions) {
    std::optional<int32_t> maybe_bucket =
        SampleContributions(single_origin_contributions);
    // If an origin did not make any contributions, it will contribute an
    // array of zeros by default, which will still require the input going
    // through the noising mechanism to satisfy the privacy requirements.
    histograms.emplace(
        origin,
        Rappor(
            maybe_bucket,
            blink::features::kFledgeRealTimeReportingEpsilon.Get(),
            blink::features::kFledgeRealTimeReportingNumBuckets.Get() +
                auction_worklet::RealTimeReportingPlatformError::kNumValues));
  }
  return histograms;
}

GURL GetRealTimeReportDestination(const url::Origin& origin) {
  return origin.GetURL().Resolve(kRealTimeReportPath);
}

bool HasValidRealTimeBucket(
    const auction_worklet::mojom::RealTimeReportingContributionPtr&
        contribution) {
  return contribution->bucket >= 0 &&
         contribution->bucket <
             blink::features::kFledgeRealTimeReportingNumBuckets.Get() +
                 auction_worklet::RealTimeReportingPlatformError::kNumValues;
}

bool HasValidRealTimePriorityWeight(
    const auction_worklet::mojom::RealTimeReportingContributionPtr&
        contribution) {
  // WebIDL of priority weight was (restricted) double, which does not allow
  // NaN or infinite. But a compromised worklet can still send these values.
  return contribution->priority_weight > 0 &&
         std::isfinite(contribution->priority_weight);
}

std::vector<uint8_t> BitPacking(std::vector<uint8_t> data) {
  std::vector<uint8_t> packed;
  packed.reserve((data.size() + 7) / 8);
  uint8_t current_byte = 0;

  for (size_t i = 0; i < data.size(); i++) {
    current_byte = (current_byte << 1) | data[i];
    if ((i + 1) % 8 == 0) {
      packed.push_back(current_byte);
      current_byte = 0;
    } else if (i == data.size() - 1) {
      current_byte <<= 8 - (i + 1) % 8;
      packed.push_back(current_byte);
    }
  }
  return packed;
}

}  // namespace content

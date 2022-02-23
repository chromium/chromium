// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_policy.h"

#include <math.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_utils.h"

namespace content {

uint64_t SanitizeTriggerData(uint64_t trigger_data,
                             CommonSourceInfo::SourceType source_type) {
  const uint64_t cardinality = TriggerDataCardinality(source_type);
  return trigger_data % cardinality;
}

base::Time GetExpiryTimeForImpression(
    const absl::optional<base::TimeDelta>& declared_expiry,
    base::Time impression_time,
    CommonSourceInfo::SourceType source_type) {
  constexpr base::TimeDelta kMinImpressionExpiry = base::Days(1);
  constexpr base::TimeDelta kDefaultImpressionExpiry = base::Days(30);

  // Default to the maximum expiry time.
  base::TimeDelta expiry = declared_expiry.value_or(kDefaultImpressionExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == CommonSourceInfo::SourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::Days(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return impression_time +
         base::clamp(expiry, kMinImpressionExpiry, kDefaultImpressionExpiry);
}

absl::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts) {
  DCHECK_GT(failed_send_attempts, 0);

  const int kMaxFailedSendAttempts = 2;
  const base::TimeDelta kInitialReportDelay = base::Minutes(5);
  const int kDelayFactor = 3;

  if (failed_send_attempts > kMaxFailedSendAttempts)
    return absl::nullopt;

  return kInitialReportDelay * pow(kDelayFactor, failed_send_attempts - 1);
}

}  // namespace content

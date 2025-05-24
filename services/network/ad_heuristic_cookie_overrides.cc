// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ad_heuristic_cookie_overrides.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AdsHeuristicCookieOverride {
  kNone = 0,
  kNotAd = 1,
  kAny = 2,
  kSkipHeuristics = 3,
  kSkipMetadata = 4,
  kSkipTrial = 5,
  kSkipTopLevelTrial = 6,
  kMaxValue = kSkipTopLevelTrial
};

void LogCookieOverrideHistogram(AdsHeuristicCookieOverride override,
                                bool emit_metrics) {
  if (emit_metrics) {
    base::UmaHistogramEnumeration("Privacy.3PCD.AdsHeuristicAddedToOverrides",
                                  override);
  }
}

}  // namespace

// Adds cookie setting overrides for cookie accesses determined to be for
// advertising purposes.
void AddAdsHeuristicCookieSettingOverrides(
    bool is_ad_tagged,
    net::CookieSettingOverrides& overrides,
    bool emit_metrics) {
  if (!base::FeatureList::IsEnabled(features::kSkipTpcdMitigationsForAds)) {
    return;
  }
  if (!is_ad_tagged) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kNotAd,
                               emit_metrics);
    return;
  }

  bool has_override = false;
  if (features::kSkipTpcdMitigationsForAdsHeuristics.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipHeuristics,
                               emit_metrics);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsMetadata.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipMetadata,
                               emit_metrics);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDMetadataGrant);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsTrial.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipTrial,
                               emit_metrics);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDTrial);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsTopLevelTrial.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipTopLevelTrial,
                               emit_metrics);
    overrides.Put(net::CookieSettingOverride::kSkipTopLevelTPCDTrial);
    has_override = true;
  }

  if (has_override) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kAny, emit_metrics);
  } else {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kNone, emit_metrics);
  }
}

}  // namespace network

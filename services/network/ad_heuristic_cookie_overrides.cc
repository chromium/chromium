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

void LogCookieOverrideHistogram(AdsHeuristicCookieOverride override) {
  base::UmaHistogramEnumeration("Privacy.3PCD.AdsHeuristicAddedToOverrides",
                                override);
}

}  // namespace

// Adds cookie setting overrides for cookie accesses determined to be for
// advertising purposes.
void AddAdsHeuristicCookieSettingOverrides(
    bool is_ad_tagged,
    net::CookieSettingOverrides& overrides) {
  if (!base::FeatureList::IsEnabled(features::kSkipTpcdMitigationsForAds)) {
    return;
  }
  if (!is_ad_tagged) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kNotAd);
    return;
  }

  bool has_override = false;
  if (features::kSkipTpcdMitigationsForAdsHeuristics.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipHeuristics);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsMetadata.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipMetadata);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDMetadataGrant);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsTrial.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipTrial);
    overrides.Put(net::CookieSettingOverride::kSkipTPCDTrial);
    has_override = true;
  }

  if (features::kSkipTpcdMitigationsForAdsTopLevelTrial.Get()) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kSkipTopLevelTrial);
    overrides.Put(net::CookieSettingOverride::kSkipTopLevelTPCDTrial);
    has_override = true;
  }

  if (has_override) {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kAny);
  } else {
    LogCookieOverrideHistogram(AdsHeuristicCookieOverride::kNone);
  }
}

}  // namespace network

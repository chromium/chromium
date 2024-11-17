// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Please keep features in alphabetical order.

// Enable detecting inconsistency in the `PageImpl` used in the auction. Abort
// the auction when detected.
BASE_FEATURE(kDetectInconsistentPageImpl,
             "DetectInconsistentPageImpl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable parsing and using K-Anonymity features for B&A.
BASE_FEATURE(kEnableBandAKAnonEnforcement,
             "EnableBandAKAnonEnforcement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable parsing private aggregation contributions from B&A response.
BASE_FEATURE(kEnableBandAPrivateAggregation,
             "EnableBandAPrivateAggregation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable deals support from B&A response.
BASE_FEATURE(kEnableBandADealSupport,
             "EnableBandADealSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable parsing forDebuggingOnly reports from B&A response, for down sampling.
BASE_FEATURE(kEnableBandASampleDebugReports,
             "EnableBandASampleDebugReports",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable parsing triggered updates from B&A response.
BASE_FEATURE(kEnableBandATriggeredUpdates,
             "EnableBandATriggeredUpdates",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable parsing ad auction response headers for an iframe navigation request.
BASE_FEATURE(kEnableIFrameAdAuctionHeaders,
             "EnableIFrameAdAuctionHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the user agent header in auction requests to be overridden.
BASE_FEATURE(kFledgeEnableUserAgentOverrides,
             "FledgeEnableUserAgentOverrides",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable write ahead logging for interest group storage.
BASE_FEATURE(kFledgeEnableWALForInterestGroupStorage,
             "FledgeEnableWALForInterestGroupStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeFacilitatedTestingSignalsHeaders,
             "FledgeFacilitatedTestingSignalsHeaders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable prefetching B&A keys on the first joinAdInterestGroup call.
BASE_FEATURE(kFledgePrefetchBandAKeys,
             "FledgePrefetchBandAKeys",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables starting worklet processes at auction start time in anticipation
// of needing them for future worklets.
BASE_FEATURE(kFledgeStartAnticipatoryProcesses,
             "FledgeStartAnticipatoryProcesses",
             base::FEATURE_DISABLED_BY_DEFAULT);
// How long to hold onto anticipatory processes that are unused.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeStartAnticipatoryProcessExpirationTime,
                   &kFledgeStartAnticipatoryProcesses,
                   "AnticipatoryProcessHoldTime",
                   base::Seconds(5));

// Enable storing a retrieving B&A keys for the interest group
// database.
BASE_FEATURE(kFledgeStoreBandAKeysInDB,
             "FledgeStoreBandAKeysInDB",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables using a cache in the browser process for FLEDGE KVv2 fetches. The
// fetches are also initiated by and managed in the browser process. This
// feature also requires blink::features::kFledgeTrustedSignalsKVv2Support to
// also be enabled for KVv2 to be enabled.
BASE_FEATURE(kFledgeUseKVv2SignalsCache,
             "kFledgeUseKVv2SignalsCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables preconnecting to interest group owner origins and a bidding signals
// URL origin at the start of an auction.
BASE_FEATURE(kFledgeUsePreconnectCache,
             "FledgeUsePreconnectCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

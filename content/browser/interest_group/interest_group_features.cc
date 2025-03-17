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
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable parsing forDebuggingOnly reports from B&A response. B&A sends these
// reports back to Chrome in its response for (1) device orchestrated multi
// seller auctions, (2) fDO down sampling.
// This flag name is confusing because it controls whether Chrome will parse fDO
// reports from B&A, not about whether enabling fDO sampling on B&A. But since
// this flag has been enabled by default and will be removed after one Chrome
// version, so not worth renaming it which may affect B&A's end to end test.
BASE_FEATURE(kEnableBandASampleDebugReports,
             "EnableBandASampleDebugReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable parsing triggered updates from B&A response.
BASE_FEATURE(kEnableBandATriggeredUpdates,
             "EnableBandATriggeredUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable response authorization using the Ad-Auction-Result-Nonce header.
BASE_FEATURE(kFledgeBiddingAndAuctionNonceSupport,
             "FledgeBiddingAndAuctionNonceSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force sampling of forDebuggingOnly reports, for testing purpose. This flag
// will always be disabled by default, and will only be enabled in some tests,
// or manually enabling it in command for manual testing such as B&A's end to
// end test of forDebuggingOnly sampling.
BASE_FEATURE(kFledgeDoSampleDebugReportForTesting,
             "FledgeDoSampleDebugReportForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable un-noised real time reporting for certain user settings.
BASE_FEATURE(kFledgeEnableUnNoisedRealTimeReport,
             "FledgeAllowUnNoisedRealTimeReport",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Check if the owner of joinAdInterestGroup would be able to call
// joinAdInterestGroup in its own subframe with allow=join-ad-interest-group.
BASE_FEATURE(kFledgeModifyInterestGroupPolicyCheckOnOwner,
             "FledgeModifyInterestGroupPolicyCheckOnOwner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a configurable limit on the number of
// `selectableBuyerAndSellerReportingIds` for which the browser fetches k-anon
// keys. If the `SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` is
// negative, no limit is enforced.
BASE_FEATURE(kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
             "FledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit,
    &kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
    "SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit",
    -1);

// Turning on kFledgeQueryKAnonymity loads k-anonymity status at interest group
// join and update time. kFledgeQueryKAnonymity is enabled by default. It may
// be reasonable to disable kFledgeQueryKAnonymity on clients on which
// k-anonymity is not enforced (see related features kFledgeConsiderKAnonymity
// and kFledgeEnforceKAnonymity in third_party/blink/public/common/features.h),
// as k-anonymity status isn't used in those auctions.
BASE_FEATURE(kFledgeQueryKAnonymity,
             "FledgeQueryKAnonymity",
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
             "FledgeUseKVv2SignalsCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a non-transient NIK for trusted selling signals.
BASE_FEATURE(kFledgeUseNonTransientNIKForSeller,
             "FledgeUseNonTransientNIKForSeller",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables preconnecting to interest group owner origins and a bidding signals
// URL origin at the start of an auction.
BASE_FEATURE(kFledgeUsePreconnectCache,
             "FledgeUsePreconnectCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

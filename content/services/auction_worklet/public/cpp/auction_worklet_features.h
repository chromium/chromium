// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {
// Please keep features in alphabetical order.

// Reuse a single V8 context to generate all bids in a bidder worklet.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeAlwaysReuseBidderContext);
// Reuse a single V8 context to score all ads in a seller worklet.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeAlwaysReuseSellerContext);

CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFledgeAuctionDownloaderStaleWhileRevalidate);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeEagerJSCompilation);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeNoWasmLazyCompilation);

// If kFledgeNumberBidderWorkletGroupByOriginContextsToKeep is enabled,
// kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue sets the number of
// previously-used group-by-origin contexts to keep in case they can be reused
// in a bidder worklet. Defaulted to 10.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFledgeNumberBidderWorkletGroupByOriginContextsToKeep);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue);

// If kFledgeNumberSellerWorkletGroupByOriginContextsToKeep is enabled,
// kFledgeNumberSellerWorkletGroupByOriginContextsToKeepValue sets the number of
// previously-used group-by-origin contexts to keep in case they can be reused
// in a bidder worklet. Defaulted to 10.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFledgeNumberSellerWorkletGroupByOriginContextsToKeep);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeNumberSellerWorkletGroupByOriginContextsToKeepValue);

// Prepare bidder contexts, including running top level scripts, before
// we're ready to generate a worklet's first bid.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgePrepareBidderContextsInAdvance);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeMaxBidderContextsPerThreadInAdvance);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeMinBidderContextsPerThreadInAdvance);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kFledgeBidderContextsDivisor);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kFledgeBidderContextsMultiplier);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kFledgeWaitForPromisesToPrepareContexts);

// Instead of using a hash to assign group-by-origin IGs to threads, use
// a round robin on joining-origin while ensuring a maximum allowed imbalance
// is respected.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeBidderUseBalancingThreadSelector);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeBidderThreadSelectorMaxImbalance);

// Prepare seller contexts, including running top level scripts, before
// we're ready to score a worklet's first ad.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgePrepareSellerContextsInAdvance);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeMaxSellerContextsPerThreadInAdvance);

// Send each trusted seller signals request right after it is queued, so
// that it does not get batched.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeSellerSignalsRequestsOneAtATime);

// Provide encodeUtf8/decodeUtf8 helpers.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeTextConversionHelpers);

}  // namespace features

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_

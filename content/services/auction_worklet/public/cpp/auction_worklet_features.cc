// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"

#include "base/feature_list.h"

namespace features {

// Please keep features in alphabetical order.
BASE_FEATURE(kFledgeAlwaysReuseBidderContext,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeAlwaysReuseSellerContext,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeAuctionDownloaderStaleWhileRevalidate,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeEagerJSCompilation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeNoWasmLazyCompilation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
             "FledgeBidderWorkletGroupByOriginContextsToKeep",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue,
                   &kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
                   "GroupByOriginContextLimit",
                   10);

BASE_FEATURE(kFledgeNumberSellerWorkletGroupByOriginContextsToKeep,
             "FledgeSellerWorkletGroupByOriginContextsToKeep",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeNumberSellerWorkletGroupByOriginContextsToKeepValue,
                   &kFledgeNumberSellerWorkletGroupByOriginContextsToKeep,
                   "SellerGroupByOriginContextLimit",
                   10);

BASE_FEATURE(kFledgePrepareBidderContextsInAdvance,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeMaxBidderContextsPerThreadInAdvance,
                   &kFledgePrepareBidderContextsInAdvance,
                   "MaxBidderContextsPerThread",
                   10);
BASE_FEATURE_PARAM(int,
                   kFledgeMinBidderContextsPerThreadInAdvance,
                   &kFledgePrepareBidderContextsInAdvance,
                   "MinBidderContextsPerThread",
                   1);
BASE_FEATURE_PARAM(int,
                   kFledgeBidderContextsDivisor,
                   &kFledgePrepareBidderContextsInAdvance,
                   "BidderContextsDivisor",
                   2);
BASE_FEATURE_PARAM(int,
                   kFledgeBidderContextsMultiplier,
                   &kFledgePrepareBidderContextsInAdvance,
                   "BidderContextsMultiplier",
                   1);
BASE_FEATURE_PARAM(bool,
                   kFledgeWaitForPromisesToPrepareContexts,
                   &kFledgePrepareBidderContextsInAdvance,
                   "WaitForPromisesToPrepareContexts",
                   false);

BASE_FEATURE(kFledgeBidderUseBalancingThreadSelector,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeBidderThreadSelectorMaxImbalance,
                   &kFledgeBidderUseBalancingThreadSelector,
                   "BidderThreadSelectorMaxImbalance",
                   4);

BASE_FEATURE(kFledgePrepareSellerContextsInAdvance,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeMaxSellerContextsPerThreadInAdvance,
                   &kFledgePrepareSellerContextsInAdvance,
                   "MaxSellerContextsPerThread",
                   10);

BASE_FEATURE(kFledgeSellerSignalsRequestsOneAtATime,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeTextConversionHelpers, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

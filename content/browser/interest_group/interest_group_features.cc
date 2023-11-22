// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_features.h"

#include "base/feature_list.h"

namespace features {

// Please keep features in alphabetical order.

// Enable parsing ad auction response headers for an iframe navigation request.
BASE_FEATURE(kEnableIFrameAdAuctionHeaders,
             "EnableIFrameAdAuctionHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable updating userBiddingSignals when updating a user's interests groups.
BASE_FEATURE(kEnableUpdatingUserBiddingSignals,
             "EnableUpdatingUserBiddingSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

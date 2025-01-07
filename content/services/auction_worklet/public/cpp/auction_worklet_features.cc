// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"

#include "base/feature_list.h"

namespace features {

// Please keep features in alphabetical order.
BASE_FEATURE(kFledgeAuctionDownloaderStaleWhileRevalidate,
             "FledgeAuctionDownloaderStaleWhileRevalidate",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

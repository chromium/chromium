// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace features {
// Please keep features in alphabetical order.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFledgeAuctionDownloaderStaleWhileRevalidate);

}  // namespace features

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_WORKLET_FEATURES_H_

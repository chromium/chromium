// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DEVTOOLS_ENUMS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DEVTOOLS_ENUMS_H_

namespace content {

enum class InterestGroupAuctionEventType { kStarted, kConfigResolved };

enum class InterestGroupAuctionFetchType {
  kBidderJs,
  kBidderWasm,
  kSellerJs,
  kBidderTrustedSignals,
  kSellerTrustedSignals
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DEVTOOLS_ENUMS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CONSTANTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CONSTANTS_H_

namespace blink {

// Maximum number of ad components a bid in an auction can have. When a renderer
// retrieves the component ad URLs from an auction, the API acts as if exactly
// this number of ad components were returned by padding the returned list with
// additional obfuscated URN URLs that map to about:blank, to avoid creating a
// side channel from a bidder worklet to the main ad.
constexpr size_t kMaxAdAuctionAdComponents = 20;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_AUCTION_CONSTANTS_H_

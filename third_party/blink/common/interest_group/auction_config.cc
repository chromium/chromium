// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

namespace blink {

AuctionConfig::NonSharedParams::NonSharedParams() = default;
AuctionConfig::NonSharedParams::NonSharedParams(const NonSharedParams&) =
    default;
AuctionConfig::NonSharedParams::NonSharedParams(NonSharedParams&&) = default;
AuctionConfig::NonSharedParams::~NonSharedParams() = default;

AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    const NonSharedParams&) = default;
AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    NonSharedParams&&) = default;

AuctionConfig::AuctionConfig() = default;
AuctionConfig::AuctionConfig(const AuctionConfig&) = default;
AuctionConfig::AuctionConfig(AuctionConfig&&) = default;
AuctionConfig::~AuctionConfig() = default;

AuctionConfig& AuctionConfig::operator=(const AuctionConfig&) = default;
AuctionConfig& AuctionConfig::operator=(AuctionConfig&&) = default;

}  // namespace blink

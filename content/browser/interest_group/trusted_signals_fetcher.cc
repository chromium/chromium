// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_fetcher.h"

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TrustedSignalsFetcher::BiddingPartition::BiddingPartition() = default;

TrustedSignalsFetcher::BiddingPartition::BiddingPartition(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::BiddingPartition::~BiddingPartition() = default;

TrustedSignalsFetcher::BiddingPartition&
TrustedSignalsFetcher::BiddingPartition::operator=(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::CompressionGroupResult::CompressionGroupResult() =
    default;
TrustedSignalsFetcher::CompressionGroupResult::CompressionGroupResult(
    CompressionGroupResult&&) = default;

TrustedSignalsFetcher::CompressionGroupResult::~CompressionGroupResult() =
    default;

TrustedSignalsFetcher::CompressionGroupResult&
TrustedSignalsFetcher::CompressionGroupResult::operator=(
    CompressionGroupResult&&) = default;

TrustedSignalsFetcher::TrustedSignalsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}
TrustedSignalsFetcher::~TrustedSignalsFetcher() = default;

void TrustedSignalsFetcher::FetchBiddingSignals(
    const GURL& trusted_bidding_signals_url,
    const std::map<int, std::vector<BiddingPartition>>& compression_groups,
    Callback callback) {}

}  // namespace content

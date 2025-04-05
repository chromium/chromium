// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/dwa_auction_metrics.h"

#include <cstdint>

#include "components/metrics/dwa/dwa_recorder.h"
#include "url/origin.h"

namespace content {

void DwaAuctionMetrics::SetSellerInfo(const url::Origin& seller_origin) {
  seller_origin_ = seller_origin;
}

void DwaAuctionMetrics::OnAuctionEnd(AuctionResult auction_result) {
  dwa_auction_builder_.SetContent(seller_origin_.Serialize())
      .SetResult(static_cast<int64_t>(auction_result))
      .Record(metrics::dwa::DwaRecorder::Get());
}

DwaAuctionMetrics* DwaAuctionMetricsManager::CreateDwaAuctionMetrics() {
  dwa_auction_metrics_.emplace_back(std::make_unique<DwaAuctionMetrics>());
  return dwa_auction_metrics_.back().get();
}

DwaAuctionMetricsManager::DwaAuctionMetricsManager() = default;
DwaAuctionMetricsManager::~DwaAuctionMetricsManager() = default;

}  // namespace content

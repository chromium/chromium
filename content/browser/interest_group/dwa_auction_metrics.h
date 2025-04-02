// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DWA_AUCTION_METRICS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DWA_AUCTION_METRICS_H_

#include <memory>
#include <vector>

#include "components/metrics/dwa/dwa_builders.h"
#include "content/browser/url_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/auction_result.h"

namespace content {

// Interface to record DWA style metrics broken down by auction seller. An
// instance of this class should be created for each component auction as well.
class CONTENT_EXPORT DwaAuctionMetrics {
 public:
  virtual void SetSellerInfo(const url::Origin& seller_origin);
  virtual void OnAuctionEnd(AuctionResult auction_result);
  virtual ~DwaAuctionMetrics() = default;

 private:
  dwa::builders::InterestGroupAuction dwa_auction_builder_;
  url::Origin seller_origin_;
};

// Manager to create instance of DwaAuctionMetrics. Since multiple DWA metrics
// are being tracked per auction. This class is responsible for creating and
// managing all those instances.
class CONTENT_EXPORT DwaAuctionMetricsManager {
 public:
  DwaAuctionMetricsManager();
  virtual DwaAuctionMetrics* CreateDwaAuctionMetrics();
  virtual ~DwaAuctionMetricsManager();

 private:
  std::vector<std::unique_ptr<DwaAuctionMetrics>> dwa_auction_metrics_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DWA_AUCTION_METRICS_H_

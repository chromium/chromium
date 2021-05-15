// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AdAuctionServiceImpl;
class AuctionURLLoaderFactoryProxy;

// Class for running a single FLEDGE auction.
class AdAuction {
 public:
  // Callback for completion, invoked on both success and error. On success,
  // when the auction had a winner, `render_url` is non-null. On failure or when
  // an auction had no winner, all URLs are null.
  using AuctionCompleteCallback =
      base::OnceCallback<void(AdAuction* auction,
                              absl::optional<GURL> render_url,
                              absl::optional<GURL> bidder_report_url,
                              absl::optional<GURL> seller_report_url)>;

  // `ad_auction_service` must remain valid for the lifetime of the AdAuction.
  AdAuction(AdAuctionServiceImpl* ad_auction_service,
            blink::mojom::AuctionAdConfigPtr config,
            AuctionCompleteCallback callback);
  ~AdAuction();

  // May invoke `AuctionCompleteCallback` synchronously.
  void StartAuction();

  // Must be called when the worklet service crashes, to ensure the renderer's
  // callback is invoked. Synchronously invokes AuctionCompleteCallback.
  void OnServiceCrash();

 private:
  // Retrieves the next interest group in `pending_buyers_` from storage,
  // removing it from the vector. OnInterestGroupRead() will be invoked
  // with the lookup results.
  void ReadNextInterestGroup();

  // Adds `interest_groups` to `bidders_`. Continues retrieving bidders from
  // `pending_buyers_` if non-empty. Otherwise, starts running the worklets.
  void OnInterestGroupRead(
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
          interest_groups);

  // Starts running the auction out of process.
  void StartWorklets();

  // Called from the worklet service once a worklet is complete.
  //
  // Validates the results, reports them to `callback_`, and updates the
  // InterestGroupStorage as needed.
  void WorkletComplete(
      const GURL& render_url,
      const url::Origin& owner,
      const std::string& name,
      auction_worklet::mojom::WinningBidderReportPtr bidder_report,
      auction_worklet::mojom::SellerReportPtr seller_report,
      const std::vector<std::string>& errors);

  // Invokes `callback_` with empty parameters, to inform it of the failure.
  void OnAuctionFailed();

  AdAuctionServiceImpl* ad_auction_service_;

  // Populated on construction, moved to the worklet service when running an
  // auction, since it's not used afterwards.
  blink::mojom::AuctionAdConfigPtr config_;

  // List of loaded interest groups. Populated by the calls to
  // OnInterestGroupRead().
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_;

  AuctionCompleteCallback callback_;

  // Buyers from `config->interest_group_buyers` that have yet to be retrieved
  // from interest group storage.
  std::vector<url::Origin> pending_buyers_;

  // Proxy used for requests from the worklet process.
  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;

  base::WeakPtrFactory<AdAuction> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_H_

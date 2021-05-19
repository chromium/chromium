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
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AdAuctionServiceImpl;
class AuctionRunner;

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

 private:
  // Starts running the auction out of process.
  void StartWorklets();

  // Called from the worklet service once a worklet is complete.
  //
  // Reports them to `callback_`.
  void WorkletComplete(const GURL& render_url,
                       const GURL& bidder_report_url,
                       const GURL& seller_report_url,
                       const std::vector<std::string>& errors);

  // Invokes `callback_` with empty parameters, to inform it of the failure.
  void OnAuctionFailed();

  AdAuctionServiceImpl* ad_auction_service_;

  // Populated on construction, moved to the worklet service when running an
  // auction, since it's not used afterwards.
  blink::mojom::AuctionAdConfigPtr config_;

  AuctionCompleteCallback callback_;

  // Buyers from `config->interest_group_buyers` that have yet to be retrieved
  // from interest group storage.
  std::vector<url::Origin> pending_buyers_;

  std::unique_ptr<AuctionRunner> auction_runner_;

  base::WeakPtrFactory<AdAuction> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_H_

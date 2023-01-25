// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class AuctionWorkletManager;

// Handles the reporting phase of FLEDGE auctions with a winner. Loads the
// bidder, seller, and (if present) component seller worklets and invokes
// reporting-related methods, and invokes reportResult() and reportWin() on
// them, as needed.
class CONTENT_EXPORT InterestGroupAuctionReporter {
 public:
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  // Seller-specific information about the winning bid. The top-level seller and
  // (if present) component seller associated with the winning bid have separate
  // SellerWinningBidInfos.
  struct SellerWinningBidInfo {
    SellerWinningBidInfo();
    SellerWinningBidInfo(SellerWinningBidInfo&&);
    ~SellerWinningBidInfo();

    // AuctionConfig associated with the seller. For a component auction, this
    // is the nested AuctionConfig.
    //
    // TODO(mmenke):  Figure out how to make this survive the auction (perhaps
    // pass ownership to the constructor).
    base::raw_ptr<const blink::AuctionConfig, DanglingUntriaged> auction_config;

    std::unique_ptr<SubresourceUrlBuilder> subresource_url_builder;

    // Bid fed as input to the seller. If this is the top level seller and the
    // bid came from a component auction, it's the (optionally) modified bid
    // returned by the component seller. Otherwise, it's the bid from the
    // bidder.
    double bid;

    // Score this seller assigned the bid.
    double score;

    double highest_scoring_other_bid;
    absl::optional<url::Origin> highest_scoring_other_bid_owner;

    absl::optional<uint32_t> scoring_signals_data_version;

    uint64_t trace_id;

    // If this is a component seller, information about how the component seller
    // modified the bid.
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params;
  };

  // Information about the winning bit that is not specific to a seller.
  struct WinningBidInfo {
    WinningBidInfo();
    WinningBidInfo(WinningBidInfo&&);
    ~WinningBidInfo();

    std::unique_ptr<StorageInterestGroup> storage_interest_group;

    GURL render_url;
    std::vector<GURL> ad_components;

    // Bid returned by the bidder.
    double bid;

    // How long it took to generate the bid.
    base::TimeDelta bid_duration;

    absl::optional<uint32_t> bidding_signals_data_version;
  };

  // All passed in raw pointers, including those in *BidInfo fields must outlive
  // the created InterestGroupAuctionReporter.
  InterestGroupAuctionReporter(
      AuctionWorkletManager* auction_worklet_manager,
      std::unique_ptr<blink::AuctionConfig> auction_config,
      WinningBidInfo winning_bid_info,
      SellerWinningBidInfo top_level_seller_winning_bid_info,
      absl::optional<SellerWinningBidInfo> component_seller_winning_bid_info,
      std::map<url::Origin, PrivateAggregationRequests>
          private_aggregation_requests);

  ~InterestGroupAuctionReporter();

  InterestGroupAuctionReporter(const InterestGroupAuctionReporter&) = delete;
  InterestGroupAuctionReporter& operator=(const InterestGroupAuctionReporter&) =
      delete;

  // Starts running reporting scripts. `callback` will be invoked once all
  // reporting scripts have completed, and the callback returned by
  // OnNavigateToWinningAdCallback() has been invoked, at which point reports
  // should be sent, and the reporter can be destroyed.
  void Start(base::OnceClosure callback);

  // Returns a callback that should be invoked once a fenced frame has been
  // navigated to the winning ad. May be invoked multiple times, safe to invoke
  // after destruction. `this` will not invoke the callback passed to Start()
  // until the callback this method returns has been invoked at least once.
  base::RepeatingClosure OnNavigateToWinningAdCallback();

  // Accessors so the owner can pass along the results of the auction.
  //
  // TODO(mmenke): Remove these, and make the reporter use them itself (or maybe
  // pass them along via a callback that can outlive the InterestGroupAuction
  // that created it).
  const std::vector<std::string>& errors() const { return errors_; }
  std::map<url::Origin, PrivateAggregationRequests>
  TakePrivateAggregationRequests() {
    return std::move(private_aggregation_requests_);
  }

  // Retrieves the ad beacon map. May only be called once, since it takes
  // ownership of the stored ad beacon map.
  ReportingMetadata TakeAdBeaconMap() { return std::move(ad_beacon_map_); }

  // Retrieves any reporting URLs returned by ReportWin() and ReportResult()
  // methods. May only be called after the reporter has completed. May only be
  // called once, since it takes ownership of stored reporting URLs.
  std::vector<GURL> TakeReportUrls() { return std::move(report_urls_); }

 private:
  // Starts request for a seller worklet. Invokes OnSellerWorkletReceived() on
  // success, OnSellerWorkletFatalError() on error.
  void RequestSellerWorklet(
      const SellerWinningBidInfo* seller_info,
      const absl::optional<std::string>& top_seller_signals);

  // Invoked when a seller worklet crashes or fails to load.
  void OnSellerWorkletFatalError(
      const SellerWinningBidInfo* seller_info,
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Invoked when a seller worklet is received. Invokes ReportResult() on the
  // worklet.
  void OnSellerWorkletReceived(
      const SellerWinningBidInfo* seller_info,
      const absl::optional<std::string>& top_seller_signals);

  // Invoked once a seller's ReportResult() call has completed. Either starts
  // loading the component seller worklet, If the winning bid is from a
  // component seller and it was the top-level seller worklet that completed,
  // or starts loading the bidder worklet, otherwise.
  void OnSellerReportResultComplete(
      const SellerWinningBidInfo* seller_info,
      const absl::optional<std::string>& signals_for_winner,
      const absl::optional<GURL>& seller_report_url,
      const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& errors);

  // Starts request for a bidder worklet. Invokes OnBidderWorkletReceived() on
  // success, OnBidderWorkletFatalError() on error.
  void RequestBidderWorklet(const std::string& signals_for_winner);

  // Invoked when a bidder worklet crashes or fails to load.
  void OnBidderWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Invoked when a bidder worklet is received. Invokes ReportWin() on the
  // worklet.
  void OnBidderWorkletReceived(const std::string& signals_for_winner);

  // Invoked the winning bidder's ReportWin() call has completed. Invokes
  // OnReportingComplete().
  void OnBidderReportWinComplete(
      const absl::optional<GURL>& bidder_report_url,
      const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& errors);

  // Sets `reporting_complete_` to true an invokes MaybeCompleteCallback().
  void OnReportingComplete(
      const std::vector<std::string>& errors = std::vector<std::string>());

  // Invoked when the winning ad has been navigated to. If
  // `navigated_to_winning_ad_` is false, sets it to true and invokes
  // MaybeInvokeCallback(). Otherwise, does nothing.
  void OnNavigateToWinningAd();

  // Invokes callback passed in to Start() if both OnReportingComplete() and
  // OnNavigateToWinningAd() have been invoked.
  void MaybeInvokeCallback();

  // Retrieves the SellerWinningBidInfo of the auction the bidder was
  // participating in - i.e., for the component auction, if the bidder was in a
  // component auction, and for the top level auction, otherwise.
  const SellerWinningBidInfo& GetBidderAuction();

  const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;

  // Top-level AuctionConfig. It owns the `auction_config` objects pointed at by
  // the the top-level SellerWinningBidInfo. If there's a component auction
  // SellerWinningBidInfo, it points to an AuctionConfig contained within it.
  const std::unique_ptr<blink::AuctionConfig> auction_config_;

  const WinningBidInfo winning_bid_info_;
  const SellerWinningBidInfo top_level_seller_winning_bid_info_;
  const absl::optional<SellerWinningBidInfo> component_seller_winning_bid_info_;

  base::OnceClosure callback_;

  // Handle used for the seller worklet. First used for the top-level seller,
  // and then the component-seller, if needed.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> seller_worklet_handle_;

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> bidder_worklet_handle_;

  // Results from calling reporting methods.

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  // Stores all pending Private Aggregation API report requests until they have
  // been flushed. Keyed by the origin of the script that issued the request
  // (i.e. the reporting origin).
  std::map<url::Origin, PrivateAggregationRequests>
      private_aggregation_requests_;

  // Ad Beacon URL mapping generated from reportResult() or reportWin() from
  // this auction and its components. Destination is relative to this auction.
  // Returned to `callback_` to deal with, so the Auction itself can be
  // deleted at the end of the auction.
  ReportingMetadata ad_beacon_map_;

  std::vector<GURL> report_urls_;

  bool reporting_complete_ = false;
  bool navigated_to_winning_ad_ = false;

  base::WeakPtrFactory<InterestGroupAuctionReporter> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/bidding_and_auction_response.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class AuctionWorkletManager;
class BrowserContext;
class InterestGroupManagerImpl;
class PrivateAggregationManager;

// Configures rounding on reported results from FLEDGE. This feature is intended
// to be always enabled, but available to attach FeatureParams to so that we can
// adjust the rounding setting via Finch.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeRounding);
CONTENT_EXPORT extern const base::FeatureParam<int> kFledgeBidReportingBits;
CONTENT_EXPORT extern const base::FeatureParam<int> kFledgeScoreReportingBits;
CONTENT_EXPORT extern const base::FeatureParam<int> kFledgeAdCostReportingBits;

// Handles the reporting phase of FLEDGE auctions with a winner. Loads the
// bidder, seller, and (if present) component seller worklets and invokes
// reporting-related methods, and invokes reportResult() and reportWin() on
// them, as needed.
class CONTENT_EXPORT InterestGroupAuctionReporter {
 public:
  // State of the InterestGroupAuctionReporter. Used to record UMA histograms
  // the status of a reporter when it's destroyed, to get data on when reports
  // are dropped. Since these values are reported to a metrics server, the
  // numeric value for each state must be preserved.
  //
  // Public for tests.
  enum class ReporterState {
    // The winning ad was never navigated to, so there was nothing to report.
    // While reporting worklets will start running in this case, this state
    // takes precedence in histograms over the others.
    kAdNotUsed = 0,
    // The top-level seller's reportResult() method is being invoked (or the
    // reporter is waiting on a process in which to do so).
    kSellerReportResult = 1,
    // The component seller's reportResult() method is being invoked (or the
    // reporter is waiting on a process in which to do so).
    kComponentSellerReportResult = 2,
    // The buyer's reportWin() method is being invoked (or the reporter is
    // waiting on a process in which to do so).
    kBuyerReportWin = 3,
    // All needed worklet reporting methods were invoked.
    kAllWorkletsCompleted = 4,

    // This reporter has not yet started. Used as the initial value before
    // `Start()` or `InitializeFromServerResponse()` are called.
    kNotStarted = 5,
    kMaxValue = kNotStarted
  };

  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  using PrivateAggregationAllParticipantsData =
      std::array<PrivateAggregationParticipantData,
                 base::checked_cast<size_t>(
                     PrivateAggregationPhase::kNumPhases)>;

  // Invoked when private aggregation requests are received from the worklet.
  using LogPrivateAggregationRequestsCallback = base::RepeatingCallback<void(
      const PrivateAggregationRequests& private_aggregation_requests)>;

  using RealTimeReportingContributions =
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;

  using AdAuctionPageDataCallback =
      base::RepeatingCallback<AdAuctionPageData*()>;

  // Seller-specific information about the winning bid. The top-level seller and
  // (if present) component seller associated with the winning bid have separate
  // SellerWinningBidInfos.
  struct CONTENT_EXPORT SellerWinningBidInfo {
    SellerWinningBidInfo();
    SellerWinningBidInfo(SellerWinningBidInfo&&);
    ~SellerWinningBidInfo();

    SellerWinningBidInfo& operator=(SellerWinningBidInfo&&);

    // AuctionConfig associated with the seller. For a component auction, this
    // is the nested AuctionConfig.
    raw_ptr<const blink::AuctionConfig> auction_config;

    std::unique_ptr<SubresourceUrlBuilder> subresource_url_builder;
    scoped_refptr<HeaderDirectFromSellerSignals::Result>
        direct_from_seller_signals_header_ad_slot;

    // Bid fed as input to the seller. If this is the top level seller and the
    // bid came from a component auction, it's the (optionally) modified bid
    // returned by the component seller. Otherwise, it's the bid from the
    // bidder.
    double bid;
    double rounded_bid;

    // Currency the bid is in.
    std::optional<blink::AdCurrency> bid_currency;

    // Bid converted to the appropriate auction's sellerCurrency;
    double bid_in_seller_currency;

    // Score this seller assigned the bid.
    double score;

    double highest_scoring_other_bid;
    std::optional<double> highest_scoring_other_bid_in_seller_currency;
    std::optional<url::Origin> highest_scoring_other_bid_owner;

    std::optional<uint32_t> scoring_signals_data_version;

    uint64_t trace_id;

    // Saved response from the server if the actual auction ran on a B&A server.
    std::optional<BiddingAndAuctionResponse> saved_response;

    // If this is a component seller, information about how the component seller
    // modified the bid.
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params;
  };

  // Information about the winning bit that is not specific to a seller.
  struct CONTENT_EXPORT WinningBidInfo {
    explicit WinningBidInfo(
        const SingleStorageInterestGroup& storage_interest_group);
    WinningBidInfo(WinningBidInfo&&);
    ~WinningBidInfo();

    const SingleStorageInterestGroup storage_interest_group;

    GURL render_url;
    std::vector<GURL> ad_components;
    std::optional<std::vector<url::Origin>> allowed_reporting_origins;

    // Bid returned by the bidder.
    double bid;

    // Currency the bid is in.
    std::optional<blink::AdCurrency> bid_currency;

    // Ad cost returned by the bidder.
    std::optional<double> ad_cost;

    // Modeling signals returned by the bidder.
    std::optional<uint16_t> modeling_signals;

    // How long it took to generate the bid.
    base::TimeDelta bid_duration;

    std::optional<uint32_t> bidding_signals_data_version;

    std::optional<std::string> selected_buyer_and_seller_reporting_id;

    // The metadata associated with the winning ad, to be made available to the
    // interest group in future auctions in the `prevWins` field.
    std::string ad_metadata;

    bool provided_as_additional_bid = false;
  };

  // All passed in raw pointers, including those in *BidInfo fields must outlive
  // the created InterestGroupAuctionReporter.
  //
  // `browser_context` is needed to create `FencedFrameReporter`.
  //
  // `log_private_aggregation_requests_callback` will be passed all private
  //  aggregation requests for logging purposes.
  //
  // `frame_origin` is the origin of the frame that ran the auction.
  //
  // `client_security_state` is the ClientSecurityState of the frame.
  //
  // `url_loader_factory` is used to send reports.
  //
  // `interest_groups_that_bid`, `debug_win_report_urls`,
  // `debug_loss_report_urls`, and `k_anon_keys_to_join`,  are reported to the
  // InterestGroupManager when/if the URL of the winning ad is navigated to in a
  // fenced frame, which is indicated by invoking the callback returned by
  // OnNavigateToWinningAdCallback().
  //
  // `private_aggregation_requests_reserved` Requests made to the Private
  //  Aggregation API, either sendHistogram(), or contributeToHistogramOnEvent()
  //  with reserved event type. Keyed by reporting origin of the associated
  //  requests.
  //
  // `private_aggregation_requests_non_reserved` Requests made to the Private
  //  Aggregation API contributeToHistogramOnEvent() with non-reserved event
  //  type like "click". Keyed by event type of the associated requests.
  //
  // `real_time_contributions` Real time reporting contributions, including both
  //  from the Real Time Reporting APIs, and platform contributions.
  InterestGroupAuctionReporter(
      InterestGroupManagerImpl* interest_group_manager,
      AuctionWorkletManager* auction_worklet_manager,
      BrowserContext* browser_context,
      PrivateAggregationManager* private_aggregation_manager,
      LogPrivateAggregationRequestsCallback
          log_private_aggregation_requests_callback,
      AdAuctionPageDataCallback ad_auction_page_data_callback,
      std::unique_ptr<blink::AuctionConfig> auction_config,
      const std::string& devtools_auction_id,
      const url::Origin& main_frame_origin,
      const url::Origin& frame_origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      bool bid_is_kanon,
      WinningBidInfo winning_bid_info,
      SellerWinningBidInfo top_level_seller_winning_bid_info,
      std::optional<SellerWinningBidInfo> component_seller_winning_bid_info,
      blink::InterestGroupSet interest_groups_that_bid,
      std::vector<GURL> debug_win_report_urls,
      std::vector<GURL> debug_loss_report_urls,
      base::flat_set<std::string> k_anon_keys_to_join,
      std::map<PrivateAggregationKey, PrivateAggregationRequests>
          private_aggregation_requests_reserved,
      std::map<std::string, PrivateAggregationRequests>
          private_aggregation_requests_non_reserved,
      PrivateAggregationAllParticipantsData all_participants_data,
      std::map<url::Origin, RealTimeReportingContributions>
          real_time_contributions);

  ~InterestGroupAuctionReporter();

  InterestGroupAuctionReporter(const InterestGroupAuctionReporter&) = delete;
  InterestGroupAuctionReporter& operator=(const InterestGroupAuctionReporter&) =
      delete;

  // Starts running reporting scripts.
  //
  // `callback` will be invoked once all reporting scripts have completed, and
  // the callback returned by OnNavigateToWinningAdCallback() has been invoked,
  // at which point reports not managed by the InterestGroupAuctionReporter
  // should be sent, and the reporter can be destroyed.
  //
  // TODO(crbug.com/40248758): Make InterestGroupAuctionReporter send all
  // reports itself, and decouple its lifetime from the frame, so that it can
  // continue running scripts after a frame is navigated away from.
  void Start(base::OnceClosure callback);

  // Initializes the reporter based on the provided server response. This skips
  // running reporting worklets and instead uses the results provided in the
  // `response`. `Start()` still needs to be invoked to start reporting.
  void InitializeFromServerResponse(
      const BiddingAndAuctionResponse& response,
      blink::FencedFrame::ReportingDestination seller_destination);

  // Returns a callback that should be invoked once a fenced frame has been
  // navigated to the winning ad. May be invoked multiple times, safe to invoke
  // after destruction. `this` will not invoke the callback passed to Start()
  // until the callback this method returns has been invoked at least once.
  base::RepeatingClosure OnNavigateToWinningAdCallback(
      FrameTreeNodeId frame_tree_node_id);

  const std::vector<std::string>& errors() const { return errors_; }

  const WinningBidInfo& winning_bid_info() const { return winning_bid_info_; }

  // The FencedFrameReporter that `this` will pass event-level ad beacon
  // information received from reporting worklets to, as they're received.
  // Created by `this`. The consumer is responsible for wiring this up to a
  // fenced frame URN mapping, so that any fenced frame the winning ad is loaded
  // into can find it to send reports.
  //
  // This is refcounted, so both the InterestGroupAuctionReporter and fenced
  // frame URN mapping can continue to access it if the other is destroyed
  // first.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter() {
    return fenced_frame_reporter_.get();
  }

  // Sends requests for the Private Aggregation API to
  // private_aggregation_manager. This does not handle requests conditional on
  // non-reserved events, but does handle requests conditional on reserved
  // events (and requests that aren't conditional on an event). The map should
  // be keyed by reporting origin of the corresponding requests. Does nothing if
  // `private_aggregation_requests` is empty. This should only be called once
  // per auction.
  //
  // Static so that this can be invoked when there's no winner, and a reporter
  // isn't needed.
  static void OnFledgePrivateAggregationRequests(
      PrivateAggregationManager* private_aggregation_manager,
      const url::Origin& main_frame_origin,
      std::map<
          PrivateAggregationKey,
          std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>
          private_aggregation_requests);

  static double RoundBidStochastically(double bid);

  // Returns the result of performing stochastic rounding on `value`. We limit
  // the value to `k` bits of precision in the mantissa (not including sign) and
  // 8 bits in the exponent. So k=8 would correspond to a 16 bit floating point
  // number (more specifically, bfloat16). Public to enable testing.
  static double RoundStochasticallyToKBits(double value, unsigned k);

  // As above, but passes nullopts through.
  static std::optional<double> RoundStochasticallyToKBits(
      std::optional<double> maybe_value,
      unsigned k);

 private:
  // Starts request for a seller worklet. Invokes OnSellerWorkletReceived() on
  // success, OnSellerWorkletFatalError() on error.
  void RequestSellerWorklet(
      const SellerWinningBidInfo* seller_info,
      const std::optional<std::string>& top_seller_signals);

  // Invoked when a seller worklet crashes or fails to load.
  void OnSellerWorkletFatalError(
      const SellerWinningBidInfo* seller_info,
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Invoked when a seller worklet is received. Invokes ReportResult() on the
  // worklet.
  void OnSellerWorkletReceived(
      const SellerWinningBidInfo* seller_info,
      const std::optional<std::string>& top_seller_signals);

  // Invoked once a seller's ReportResult() call has completed. Either starts
  // loading the component seller worklet, If the winning bid is from a
  // component seller and it was the top-level seller worklet that completed,
  // or starts loading the bidder worklet, otherwise. `winning_bid` and
  // `highest_scoring_other_bid` are in appropriate currency for private
  // aggregation depending on the currency mode.
  void OnSellerReportResultComplete(
      const SellerWinningBidInfo* seller_info,
      double winning_bid,
      double highest_scoring_other_bid,
      const std::optional<std::string>& signals_for_winner,
      const std::optional<GURL>& seller_report_url,
      const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
      const std::vector<std::string>& errors);

  // Invoked with the results from ReportResult. Split out as a separate
  // function from OnSellerReportResultComplete since this is also called by
  // `InitializeFromServerResponse()`.
  bool AddReportResultResult(
      const url::Origin& seller_origin,
      const std::optional<GURL>& seller_report_url,
      const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
      blink::FencedFrame::ReportingDestination destination,
      std::vector<std::string>& errors_out);

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
  // OnReportingComplete(). `winning_bid` and `highest_scoring_other_bid` are in
  // appropriate currency for private aggregation depending on the currency
  // mode.
  void OnBidderReportWinComplete(
      double winning_bid,
      double highest_scoring_other_bid,
      const std::optional<GURL>& bidder_report_url,
      const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
      const base::flat_map<std::string, std::string>& bidder_ad_macro_map,
      PrivateAggregationRequests pa_requests,
      auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
      const std::vector<std::string>& errors);

  // Invoked with the results from ReportWin. Split out as a separate function
  // from OnBidderReportWinComplete since this is also called by
  // `InitializeFromServerResponse()`.
  // `bidder_ad_macro_map` is always std::nullopt from
  // `InitializeFromServerResponse()` since macro expanded reporting is not
  // supported from server auction.
  bool AddReportWinResult(
      const url::Origin& bidder_origin,
      const std::optional<GURL>& bidder_report_url,
      const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
      const std::optional<base::flat_map<std::string, std::string>>&
          bidder_ad_macro_map,
      std::vector<std::string>& errors_out);

  // Sets `reporting_complete_` to true an invokes MaybeCompleteCallback().
  void OnReportingComplete(
      const std::vector<std::string>& errors = std::vector<std::string>());

  // Invoked when the winning ad has been navigated to. If
  // `navigated_to_winning_ad_` is false, sets it to true and invokes
  // MaybeInvokeCallback(). Otherwise, does nothing.
  void OnNavigateToWinningAd(FrameTreeNodeId frame_tree_node_id);

  // Invokes callback passed in to Start() if both OnReportingComplete() and
  // OnNavigateToWinningAd() have been invoked.
  void MaybeInvokeCallback();

  // Retrieves the SellerWinningBidInfo of the auction the bidder was
  // participating in - i.e., for the component auction, if the bidder was in a
  // component auction, and for the top level auction, otherwise.
  const SellerWinningBidInfo& GetBidderAuction();

  // Called each time a report script completes and has a valid report URL.
  // Updates `pending_report_urls_` and calls SendPendingReportsIfNavigated();
  void AddPendingReportUrl(const GURL& report_url);

  // Sends all reports in `pending_report_urls_` and clears it, if
  // OnNavigateToWinningAd() has been invoked. Called each time reports are
  // added, and on first invocation of OnNavigateToWinningAd().
  // Does not send reports that are populated only on construction - those are
  // handled in OnNavigateToWinningAd(), since they never need to be sent when a
  // reporting script completes. Does not trigger Private Aggregation reports.
  void SendPendingReportsIfNavigated();

  // This checks if the winning ad has been navigated to and if reporting is
  // complete and sends all pending private aggregation requests if both are
  // true. It should be called when either of these conditions becomes true.
  void MaybeSendPrivateAggregationReports();

  // Checks that `url` is attested for reporting. On success, returns true. On
  // failure, return false, and appends an error to `errors_`. The `url` passed
  // to this function should be the report url of either `reportWin()` or
  // `reportResult()`. They are not post-impression beacons.
  bool CheckReportUrl(const GURL& url);

  // For each url in `urls`, erases that url iff CheckReportUrl(url) returns
  // false.
  void EnforceAttestationsReportUrls(std::vector<GURL>& urls);

  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;
  const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
  const raw_ptr<PrivateAggregationManager> private_aggregation_manager_;

  const LogPrivateAggregationRequestsCallback
      log_private_aggregation_requests_callback_;

  const AdAuctionPageDataCallback ad_auction_page_data_callback_;

  // Top-level AuctionConfig. It owns the `auction_config` objects pointed at by
  // the the top-level SellerWinningBidInfo. If there's a component auction
  // SellerWinningBidInfo, it points to an AuctionConfig contained within it.
  const std::unique_ptr<blink::AuctionConfig> auction_config_;

  const std::string devtools_auction_id_;
  const url::Origin main_frame_origin_;
  const url::Origin frame_origin_;
  const network::mojom::ClientSecurityStatePtr client_security_state_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const auction_worklet::mojom::KAnonymityBidMode kanon_mode_;
  const bool bid_is_kanon_;

  const WinningBidInfo winning_bid_info_;
  const SellerWinningBidInfo top_level_seller_winning_bid_info_;
  const std::optional<SellerWinningBidInfo> component_seller_winning_bid_info_;

  blink::InterestGroupSet interest_groups_that_bid_;

  base::OnceClosure callback_;

  // Handle used for the seller worklet. First used for the top-level seller,
  // and then the component-seller, if needed.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> seller_worklet_handle_;

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> bidder_worklet_handle_;

  // Results from calling reporting methods.

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  // Win/loss report URLs from generateBid() and scoreAd() calls.
  std::vector<GURL> debug_win_report_urls_;
  std::vector<GURL> debug_loss_report_urls_;

  base::flat_set<std::string> k_anon_keys_to_join_;

  // Stores all pending Private Aggregation API report requests until they have
  // been flushed. Keyed by the origin of the script that issued the request
  // (i.e. the reporting origin).
  std::map<PrivateAggregationKey, PrivateAggregationRequests>
      private_aggregation_requests_reserved_;
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_requests_non_reserved_;
  // Metrics from the parties that took part in winning, in case they want to
  // request them from the reporting scripts.
  PrivateAggregationAllParticipantsData all_participants_data_;

  // Stores all received pending Real Time Reporting contributions. until their
  // converted histograms flushed. Keyed by the origin of the script that issued
  // the request (i.e. the reporting origin).
  std::map<url::Origin, RealTimeReportingContributions>
      real_time_contributions_;

  std::vector<GURL> pending_report_urls_;

  const scoped_refptr<FencedFrameReporter> fenced_frame_reporter_;

  const raw_ptr<BrowserContext> browser_context_;

  bool reporting_complete_ = false;
  bool navigated_to_winning_ad_ = false;

  const base::TimeTicks start_time_ = base::TimeTicks::Now();

  // The current reporter phase of worklet invocation. This is never kAdNotUsed,
  // but rather one of the others, depending on worklet progress. On
  // destruction, if `navigated_to_winning_ad_` is true, this is the logged to
  // UMA. Otherwise, kAdNotUsed is.
  //
  // The initial value should never be logged, as it's overwritten on `Start()`
  // or `InitializeFromServerResponse()`, which should always be invoked.
  ReporterState reporter_worklet_state_ = ReporterState::kNotStarted;

  base::WeakPtrFactory<InterestGroupAuctionReporter> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_REPORTER_H_

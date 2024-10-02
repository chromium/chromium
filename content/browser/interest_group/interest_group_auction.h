// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "content/browser/interest_group/additional_bid_result.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_nonce_manager.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/bidding_and_auction_response.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/common/content_export.h"
#include "content/public/browser/auction_result.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class AdAuctionNegativeTargeter;
class AdAuctionPageData;
struct AdAuctionRequestContext;
class AuctionMetricsRecorder;
class BrowserContext;
class InterestGroupManagerImpl;
class PrivateAggregationManager;
struct SignedAdditionalBidSignature;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kBiddingAndAuctionEncryptionMediaType);

inline constexpr std::string_view kBiddingAndAuctionEncryptionRequestMediaType =
    "message/auction request";
inline constexpr std::string_view
    kBiddingAndAuctionEncryptionResponseMediaType = "message/auction response";

// An InterestGroupAuction Handles running an auction, or a component auction.
// Consumers should use AuctionRunner, which sets up InterestGroupAuction and
// extracts their results. Separate from AuctionRunner so that nested
// InterestGroupAuction can handle component auctions as well as top-level
// auction.
//
// Auctions have two phases, with phase transitions handled by the owner. All
// phases complete asynchronously:
//
// * Loading interest groups phase: This loads interest groups that can
// participate in an auction. Waiting for all component auctions to complete
// this phase before advancing to the next ensures that if any auctions share
// bidder worklets, they'll all be loaded together, and only send out a single
// trusted bidding signals request.
//
// * Bidding/scoring phase: This phase loads bidder and seller worklets,
// generates bids, scores bids, and the highest scoring bid for each component
// auction is passed to its parent auction, which also scores it. When this
// phase completes, the winner will have been decided.
class CONTENT_EXPORT InterestGroupAuction
    : public auction_worklet::mojom::ScoreAdClient {
 public:
  // Post auction signals (signals only available after auction completes such
  // as winning bid) for debug loss/win reporting.
  struct CONTENT_EXPORT PostAuctionSignals {
    PostAuctionSignals();

    // For now, top-level post auction signals do not have
    // `highest_scoring_other_bid` information.
    PostAuctionSignals(double winning_bid,
                       std::optional<blink::AdCurrency> winning_bid_currency,
                       bool made_winning_bid);

    PostAuctionSignals(
        double winning_bid,
        std::optional<blink::AdCurrency> winning_bid_currency,
        bool made_winning_bid,
        double highest_scoring_other_bid,
        std::optional<blink::AdCurrency> highest_scoring_other_bid_currency,
        bool made_highest_scoring_other_bid);

    ~PostAuctionSignals();

    PostAuctionSignals(PostAuctionSignals&) = delete;
    PostAuctionSignals& operator=(PostAuctionSignals&) = delete;

    // Computes appropriate information to provide for winningBid information,
    // dependent on whether bidder-currency or seller-currency is expected.
    static void FillWinningBidInfo(
        const url::Origin& owner,
        std::optional<url::Origin> winner_owner,
        double winning_bid,
        std::optional<double> winning_bid_in_seller_currency,
        const std::optional<blink::AdCurrency>& seller_currency,
        bool& out_made_winning_bid,
        double& out_winning_bid,
        std::optional<blink::AdCurrency>& out_winning_bid_currency);

    // Computes appropriate information to provide for highestScoringOtherBid
    // information, dependent on whether bidder-currency or seller-currency is
    // expected.
    static void FillRelevantHighestScoringOtherBidInfo(
        const url::Origin& owner,
        std::optional<url::Origin> highest_scoring_other_bid_owner,
        double highest_scoring_other_bid,
        std::optional<double> highest_scoring_other_bid_in_seller_currency,
        const std::optional<blink::AdCurrency>& seller_currency,
        bool& out_made_highest_scoring_other_bid,
        double& out_highest_scoring_other_bid,
        std::optional<blink::AdCurrency>&
            out_highest_scoring_other_bid_currency);

    double winning_bid = 0.0;
    std::optional<blink::AdCurrency> winning_bid_currency;
    bool made_winning_bid = false;
    double highest_scoring_other_bid = 0.0;
    std::optional<blink::AdCurrency> highest_scoring_other_bid_currency;
    bool made_highest_scoring_other_bid = false;
  };

  // Returns true if `origin` is allowed to use the interest group API. Will be
  // called on worklet / interest group origins before using them in any
  // interest group API.
  using IsInterestGroupApiAllowedCallback = base::RepeatingCallback<bool(
      ContentBrowserClient::InterestGroupApiOperation
          interest_group_api_operation,
      const url::Origin& origin)>;

  // May return null if the page is no longer available.
  using GetDataDecoderCallback =
      base::RepeatingCallback<data_decoder::DataDecoder*(
          const url::Origin& seller)>;

  using AdAuctionPageDataCallback =
      base::RepeatingCallback<AdAuctionPageData*()>;

  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  using RealTimeReportingContributions =
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;

  using PrivateAggregationAllParticipantsDataPtrs =
      std::array<const PrivateAggregationParticipantData*,
                 base::checked_cast<size_t>(
                     PrivateAggregationPhase::kNumPhases)>;

  struct CONTENT_EXPORT BidState {
    explicit BidState(const SingleStorageInterestGroup&& bidder);
    ~BidState();

    BidState(BidState&&);
    BidState& operator=(BidState&&);

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    // Populates `trace_id` with a new trace ID and logs the first trace event
    // for it. This is the tracing for bid portion of the process.
    void BeginTracing();

    // Logs the final event for `trace_id` and clears it. Automatically called
    // on destruction so trace events are all closed if an auction is cancelled.
    void EndTracing();

    const SingleStorageInterestGroup bidder;

    // Set of keys that are k-anonymous and correspond to ad and ad component
    // render URLs, and to reporting ids, for this interest group.
    // (Not set if we are not configured to care).
    base::flat_set<std::string> kanon_keys;

    // This starts off as the base priority of the interest group, but is
    // updated by sparse vector multiplications using first the priority vector
    // from the interest group, and then the one received from the trusted
    // server, if appropriate.
    double calculated_priority;

    // Holds a reference to the BidderWorklet, once created.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> worklet_handle;

    // Tracing ID associated with the BidState. A nestable async "Bid" trace
    // event is started for a bid state during the generate and score bid phase
    // when the worklet is requested, and ended once the bid is scored, or the
    // bidder worklet fails to bid.
    //
    // Nested events are logged using this ID both by the Auction and by Mojo
    // bidder and seller worklets, potentially in another process.
    //
    // std::nullopt means no ID is currently assigned, and there's no pending
    // event.
    std::optional<uint64_t> trace_id;

    // ReceiverId for use as a GenerateBidClient. Only populated while
    // generateBid() is running.
    std::optional<mojo::ReceiverId> generate_bid_client_receiver_id;

    // Mojo pipe to use to fill in potentially promise-provided arguments.
    // Only populated in between BeginGenerateBid and FinishGenerateBid().
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;

    // True when OnBiddingSignalsReceived() has been invoked. Needed to
    // correctly handle the case the bidder worklet pipe is closed before
    // OnBiddingSignalsReceived() is invoked.
    bool bidding_signals_received = false;

    // Callback to resume generating a bid after OnBiddingSignalsReceived() has
    // been invoked. Only used when `enabled_bidding_signals_prioritization` is
    // true for any interest group with the same owner, while waiting for all
    // interest groups to receive their final priorities. In other cases, the
    // callback is invoked immediately.
    base::OnceClosure resume_generate_bid_callback;

    // This is true if after this bid would be a good time to combine pending
    // trusted signals requests on its worklet and flush them. Currently set
    // when this is the last bid requested of the worklet.
    bool send_pending_trusted_signals_after_generate_bid = false;

    // Used to avoid sending direct-from-seller signals twice if they are
    // available by time of GenerateBid(). This can be true even if no signals
    // are actually available, just so long as that's known.
    bool handled_direct_from_seller_signals_in_begin_generate_bid = false;

    // True if the worklet successfully made a bid.
    bool made_bid = false;

    // True if the worklet execution on this IG was cancelled due to cumulative
    // timeout.
    bool affected_by_cumulative_timeout = false;

    // If this was provided as an additional bid, this is set to the origin it
    // claims to be.
    std::optional<url::Origin> additional_bid_buyer;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in generateBid().
    // They support post auction signal placeholders in their query string, for
    // example, "https://example.com/?${winningBid}".
    // Placeholders will be replaced by corresponding values. For a component
    // auction, post auction signals are only from the component auction, but
    // not the top-level auction.
    std::optional<GURL> bidder_debug_loss_report_url;
    std::optional<GURL> bidder_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd(). In the case
    // of a component auction, these are the values from component seller that
    // the scored ad was created in, and post auction signals are from both the
    // component auction and top-level auction.
    std::optional<GURL> seller_debug_loss_report_url;
    std::optional<GURL> seller_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd() from the
    // top-level seller, in the case this bidder was made in a component
    // auction, won it, and was then scored by the top-level seller.
    std::optional<GURL> top_level_seller_debug_win_report_url;
    std::optional<GURL> top_level_seller_debug_loss_report_url;

    // True if the bid is created from parsing B&A server response.
    bool is_from_server_response = false;

    // forDebuggingOnly reports that have been filtered (also sampled) by the
    // B&A server.
    std::map<url::Origin, std::vector<GURL>>
        server_filtered_debugging_only_reports;

    // Requests made to Private aggregation API in generateBid() and scoreAd().
    // Keyed by reporting origin of the associated requests, i.e., buyer origin
    // for generateBid() and seller origin for scoreAd(), an enum that
    // determines exactly which phase of the auction made that request, and an
    // optional aggregation coordinator origin.
    std::map<PrivateAggregationPhaseKey, PrivateAggregationRequests>
        private_aggregation_requests;

    // Requests made to Private aggregation API in generateBid() for the
    // non-k-anonymous enforced bid when k-anonymity enforcement is active.
    PrivateAggregationRequests non_kanon_private_aggregation_requests;

    // Private aggregation requests from B&A response that have been filtered by
    // B&A server. These can be simply be forwarded without further filtering on
    // Chrome side.
    std::map<PrivateAggregationKey, PrivateAggregationRequests>
        server_filtered_pagg_requests_reserved;
    std::map<std::string, PrivateAggregationRequests>
        server_filtered_pagg_requests_non_reserved;

    std::array<PrivateAggregationTimings,
               base::checked_cast<size_t>(PrivateAggregationPhase::kNumPhases)>
        private_aggregation_timings;

    PrivateAggregationTimings& pa_timings(PrivateAggregationPhase phase) {
      return private_aggregation_timings[static_cast<int>(phase)];
    }

    // The reason this bid was rejected by the auction (i.e., reason why score
    // was non-positive).
    auction_worklet::mojom::RejectReason reject_reason =
        auction_worklet::mojom::RejectReason::kNotAvailable;

    // Real time reporting contributions. Note that when an origin has no real
    // time contributions, there must still be an entry for it, and its value is
    // an empty vector.
    std::map<url::Origin, RealTimeReportingContributions>
        real_time_contributions;
  };

  // Result of generated a bid. Contains information that needs to score a bid
  // and is persisted to the end of the auction if the bidder wins. Largely
  // duplicates auction_worklet::mojom::BidderWorkletBid, with additional
  // information about the bidder.
  struct CONTENT_EXPORT Bid {
    Bid(auction_worklet::mojom::BidRole bid_role,
        std::string ad_metadata,
        double bid,
        std::optional<blink::AdCurrency> bid_currency,
        std::optional<double> ad_cost,
        blink::AdDescriptor ad_descriptor,
        std::vector<blink::AdDescriptor> ad_component_descriptors,
        std::optional<uint16_t> modeling_signals,
        base::TimeDelta bid_duration,
        std::optional<uint32_t> bidding_signals_data_version,
        const blink::InterestGroup::Ad* bid_ad,
        std::optional<std::string> selected_buyer_and_seller_reporting_id,
        BidState* bid_state,
        InterestGroupAuction* auction);

    Bid(Bid&);

    ~Bid();

    // Returns trace ID for the scoring portion of the process.
    uint64_t TraceIdForScoring() { return *trace_id; }

    // The trace_id in `bid_state` is used for bidding portion of the process,
    // while the ID here is used for scoring.
    void BeginTracingForScoring();
    void EndTracingForScoring();

    // Get a vector of ad component urls. For compatible with functions
    // expecting a vector of `GURL` instead of a vector of
    // `blink::AdDescriptor`.
    std::vector<GURL> GetAdComponentUrls() const;

    // These getters are necessary for handling the replacements within the
    // interest group auction.
    blink::AdDescriptor GetAdDescriptorWithReplacements();
    std::vector<blink::AdDescriptor>
    GetComponentAdDescriptorsWithReplacements();

    // Which auctions the bid participates in.
    auction_worklet::mojom::BidRole bid_role;

    // These are taken directly from the
    // auction_worklet::mojom::BidderWorkletBid.
    const std::string ad_metadata;
    const double bid;
    const std::optional<blink::AdCurrency> bid_currency;
    const std::optional<double> ad_cost;
    const blink::AdDescriptor ad_descriptor;
    const std::vector<blink::AdDescriptor> ad_component_descriptors;
    const std::optional<uint16_t> modeling_signals;
    const base::TimeDelta bid_duration;
    const std::optional<uint32_t> bidding_signals_data_version;

    const std::optional<std::string> selected_buyer_and_seller_reporting_id;

    // InterestGroup that made the bid. Owned by the BidState of that
    // InterestGroup.
    raw_ptr<const blink::InterestGroup> interest_group;

    // Points to the InterestGroupAd within `interest_group`.
    raw_ptr<const blink::InterestGroup::Ad> bid_ad;

    // `bid_state` of the InterestGroup that made the bid. This should not be
    // written to, except for adding seller debug reporting URLs.
    const raw_ptr<BidState> bid_state;

    // The Auction with the interest group that made this bid. Important in the
    // case of component auctions.
    const raw_ptr<InterestGroupAuction> auction;

    // Time where tracing for wait_seller_deps began; if it ever did.
    base::TimeTicks trace_wait_seller_deps_start;
    // How long various inputs were waited for.
    base::TimeDelta wait_worklet;
    base::TimeDelta wait_promises;

    // Time we called ScoreAd on the SellerWorklet.
    base::TimeTicks seller_worklet_score_ad_start;

    // Trace ID used for scoring this particular bid.
    std::optional<uint64_t> trace_id;
  };

  // Combines a Bid with seller score and seller state needed to invoke its
  // ReportResult() method.
  struct ScoredBid {
    ScoredBid(double score,
              std::optional<uint32_t> scoring_signals_data_version,
              std::unique_ptr<Bid> bid,
              std::optional<double> bid_in_seller_currency,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
                  component_auction_modified_bid_params);
    ~ScoredBid();

    // The seller's desirability score for the bid.
    const double score;

    // The seller's scoring signals version.
    const std::optional<uint32_t> scoring_signals_data_version;

    // The bid that came from the bidder or component Auction.
    const std::unique_ptr<Bid> bid;

    // Bidder's bid currency-converted by the seller to seller's own currency.
    const std::optional<double> bid_in_seller_currency;

    // Modifications that should be applied to `bid` before the parent
    // auction uses it. Only present for bids in component Auctions. When
    // the top-level auction creates a ScoredBid represending the result from
    // a component auction, the params have already been applied to the
    // underlying Bid, so the params are no longer needed.
    const auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params;
  };

  // Callback that's called when a phase of the InterestGroupAuction completes.
  // Always invoked asynchronously.
  using AuctionPhaseCompletionCallback = base::OnceCallback<void(bool success)>;

  // All passed in raw pointers must remain valid until the InterestGroupAuction
  // is destroyed. `config` is typically owned by the AuctionRunner's
  // `owned_auction_config_` field. `parent` should be the parent
  // InterestGroupAuction if this is a component auction, and null, otherwise.
  //
  // `is_interest_group_api_allowed_callback` will be used to check whether the
  // sellers of the auction and bids provided via interest groups or
  // additionalBids are permitted to participate.
  InterestGroupAuction(
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const blink::AuctionConfig* config,
      const InterestGroupAuction* parent,
      AuctionMetricsRecorder* auction_metrics_recorder,
      AuctionWorkletManager* auction_worklet_manager,
      AuctionNonceManager* auction_nonce_manager,
      InterestGroupManagerImpl* interest_group_manager,
      GetDataDecoderCallback get_data_decoder_callback,
      base::Time auction_start_time,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      base::RepeatingCallback<
          void(const PrivateAggregationRequests& private_aggregation_requests)>
          maybe_log_private_aggregation_web_features_callback);

  InterestGroupAuction(const InterestGroupAuction&) = delete;
  InterestGroupAuction& operator=(const InterestGroupAuction&) = delete;

  ~InterestGroupAuction() override;

  // The time when this InterestGroupAuction was created; used for UMA.
  base::TimeTicks creation_time() { return creation_time_; }

  // Starts loading the interest groups that can participate in an auction.
  //
  // Both seller and buyer origins are filtered by
  // `is_interest_group_api_allowed` passed to the constructor, and any any not
  // allowed to use the API are excluded from participating in the auction.
  //
  // Invokes `load_interest_groups_phase_callback` asynchronously on
  // completion. Passes it false if there are no interest groups that may
  // participate in the auction (possibly because sellers aren't allowed to
  // participate in the auction)
  void StartLoadInterestGroupsPhase(
      AuctionPhaseCompletionCallback load_interest_groups_phase_callback);

  // Starts bidding and scoring phase of the auction.
  //
  // `on_seller_receiver_callback`, if non-null, is invoked once the seller
  // worklet has been received, or if the seller worklet is no longer needed
  // (e.g., if all bidders fail to bid before the seller worklet has
  // been received). This is needed so that in the case of component auctions,
  // the top-level seller worklet will only be requested once all component
  // seller worklets have been received, to prevent deadlock (the top-level
  // auction could be waiting on a bid from a seller, while the top-level
  // seller worklet being is blocking a component seller worklet from being
  // created, due to the process limit). Unlike other callbacks,
  // `on_seller_receiver_callback` may be called synchronously.
  //
  // `bidding_and_scoring_phase_callback` is invoked asynchronously when
  // either the auction has failed to produce a winner, or the auction has a
  // winner. `success` is true only when there is a winner.
  void StartBiddingAndScoringPhase(
      std::optional<DebugReportLockoutAndCooldowns>
          debug_report_lockout_and_cooldowns,
      base::OnceClosure on_seller_receiver_callback,
      AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback);

  // Handles the server response for an auction.
  void HandleServerResponse(mojo_base::BigBuffer response,
                            AdAuctionPageData& ad_auction_page_data);

  // Handles a server response in a component auction.
  void HandleComponentServerResponse(uint32_t pos,
                                     mojo_base::BigBuffer response,
                                     AdAuctionPageData& ad_auction_page_data);

  // Creates an InterestGroupAuctionReporter, after the auction has completed.
  // Takes ownership of the `auction_config`, so that the reporter can outlive
  // other auction-related classes. This also means that various method on
  // `this` that use the configuration should not be called past this point.
  std::unique_ptr<InterestGroupAuctionReporter> CreateReporter(
      BrowserContext* browser_context,
      PrivateAggregationManager* private_aggregation_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AdAuctionPageDataCallback ad_auction_page_data_callback,
      std::unique_ptr<blink::AuctionConfig> auction_config,
      const url::Origin& main_frame_origin,
      const url::Origin& frame_origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      blink::InterestGroupSet interest_groups_that_bid);

  // Called by AuctionRunner (for component auctions, indirectly via
  // NotifyComponentConfigPromisesResolved) when all promises relevant to this
  // particular auction have been resolved (not called when there were no
  // promises to wait for to start with).
  void NotifyConfigPromisesResolved();

  // Called by AuctionRunner when all promises relevant to component auction
  // with position `pos` in the original configuration have been resolved.
  //
  // Assumes that `pos` has already been range-checked, and that this is
  // a parent auction.
  void NotifyComponentConfigPromisesResolved(uint32_t pos);

  // Called by AuctionRunner when the promise providing the additional_bids
  // array has been resolved, if one exists. Unlike other similar methods,
  // `auction_page_data` may be null.
  void NotifyAdditionalBidsConfig(AdAuctionPageData& auction_page_data);

  // Called by AuctionRunner when the promise for `additional_bids` for
  // component auction with position `pos` in the original configuration has
  // been resolved.
  //
  // Assumes that `pos` has already been range-checked, and that this is
  // a parent auction.
  //
  // Unlike other similar methods, `auction_page_data` may be null.
  void NotifyComponentAdditionalBidsConfig(
      uint32_t pos,
      AdAuctionPageData& auction_page_data);

  // Called by AuctionRunner when the promise providing the
  // `direct_from_seller_signals_header_ad_slot` string has been resolved, if
  // one exists.
  //
  // The implementation must not hold on to `auction_page_data` after returning,
  // since `auction_page_data` can be freed when navigating away.
  void NotifyDirectFromSellerSignalsHeaderAdSlotConfig(
      AdAuctionPageData& auction_page_data,
      const std::optional<std::string>&
          direct_from_seller_signals_header_ad_slot);

  // Called by AuctionRunner when the value of
  // `direct_from_seller_signals_header_ad_slot` for component
  // auction with position `pos` in the original configuration has been
  // resolved.
  //
  // Assumes that `pos` has already been range-checked, and that this is
  // a parent auction.
  //
  // The implementation must not hold on to `auction_page_data` after returning,
  // since `auction_page_data` can be freed when navigating away.
  void NotifyComponentDirectFromSellerSignalsHeaderAdSlotConfig(
      uint32_t pos,
      AdAuctionPageData& auction_page_data,
      const std::optional<std::string>&
          direct_from_seller_signals_header_ad_slot);

  // Close all Mojo pipes and release all weak pointers. Called when an
  // auction fails and on auction complete.
  void ClosePipes();

  // Returns the number of interest groups participating in the auction that can
  // potentially make bids. Includes interest groups in component auctions.
  // Double-counts interest groups participating multiple times in different
  // InterestGroupAuctions. Does not include synthetic interest groups for
  // additional bids.
  size_t NumPotentialBidders() const;

  // Returns all interest groups that bid in an auction. Expected to be called
  // after the bidding and scoring phase completes. Returns an empty set if the
  // auction failed for any reason other than the seller rejecting all bids.
  // Bids from additional bids are not returned, since they do not really have
  // interest groups (and we don't want to attribute them to database IGs with
  // aliasing names).
  //
  // All bids (including additional bids) are also reported to the observer.
  void GetInterestGroupsThatBidAndReportBidCounts(
      blink::InterestGroupSet& interest_groups) const;

  // Returns the requested ad size specified by the auction config. Called
  // after the bidding and scoring phase completes, to set the container size
  // in the fenced frame config resulting from the auction.
  std::optional<blink::AdSize> RequestedAdSize() const;

  // Retrieves any debug reporting URLs. May only be called once, since it takes
  // ownership of stored reporting URLs. This is called internally by
  // CreateReporter() so may only be called in the case an auction has no
  // winner, and thus CreateReporter() need not be called.
  //
  // Note: Temporarily, this function also fills post auction signals to private
  // aggregation requests from generateBid() and scoreAd(), so this function
  // must be called before TakePrivateAggregationRequests() to make sure that
  // function gets private aggregation requests with post auction signals filled
  // in.
  // TODO(qingxinwu): Refactor this to fill post auction signals to private
  // aggregation report in TakePrivateAggregationRequests(), ideally reuse the
  // post auction signals calculated from this method.
  void TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
      std::vector<GURL>& debug_win_report_urls,
      std::vector<GURL>& debug_loss_report_urls);

  // Retrieves all requests with reserved event type to the Private Aggregation
  // API returned by GenerateBid() and ScoreAd(). The return value is keyed by
  // reporting origin and aggregation coordinator origin of the associated
  // requests. May only be called by external consumers after an auction has
  // failed (on success, used internally to pass them to the
  // InterestGroupAuctionReporter). May only be called once, since it takes
  // ownership of stored reporting URLs.
  std::map<PrivateAggregationKey, PrivateAggregationRequests>
  TakeReservedPrivateAggregationRequests();

  // Retrieves all requests with non-reserved event type to the Private
  // Aggregation API returned by GenerateBid(). The return value is keyed by
  // event type of the associated requests. May only be called by external
  // consumers after an auction has failed (on success, used internally to pass
  // them to the InterestGroupAuctionReporter). May only be called once, since
  // it takes ownership of stored reporting URLs.
  std::map<std::string, PrivateAggregationRequests>
  TakeNonReservedPrivateAggregationRequests();

  // Assembles per-participant metrics values relevant to the buyer and
  // seller(s) of the winning bid.
  InterestGroupAuctionReporter::PrivateAggregationAllParticipantsData
  ComputePrivateAggregationParticipantData();

  // Retrieves all real time report contributions.
  std::map<url::Origin, InterestGroupAuction::RealTimeReportingContributions>
  TakeRealTimeReportingContributions();

  // Retrieves any errors from the auction. May only be called once, since it
  // takes ownership of stored errors.
  std::vector<std::string> TakeErrors();

  // Retrieves (by appending) all owners of interest groups that participated
  // in this auction (or any of its child auctions) that successfully loaded
  // at least one interest group. May only be called after the auction has
  // completed, for either success or failure. Duplication is possible,
  // particularly if an owner is listed in multiple auction components. May
  // only be called once, since it moves the stored origins.
  void TakePostAuctionUpdateOwners(std::vector<url::Origin>& owners);

  // Reports (via extended private aggregation) the number of interest groups
  // loaded for the owner of `interest_group` iff `interest_group` has
  // authorized this auction's seller to receive such information.
  //
  // The reported value isn't limited by the auction config's
  // perBuyerGroupLimits.
  //
  // Returns true iff a report was issued.
  bool ReportInterestGroupCount(const blink::InterestGroup& interest_group,
                                size_t count);

  // Reports (via extended private aggregation) the number of interest groups
  // that bid for the owner of `interest_group` iff `interest_group` has
  // authorized this auction's seller to receive such information.
  //
  // Returns true iff a report was issued.
  bool ReportBidCount(const blink::InterestGroup& interest_group, size_t count);

  // Reports (via extended private aggregation) the time taken to fetch trusted
  // signals iff `interest_group` has authorized this auction's seller to
  // receive such information.
  void ReportTrustedSignalsFetchLatency(
      const blink::InterestGroup& interest_group,
      base::TimeDelta trusted_signals_fetch_latency);

  // Reports (via extended private aggregation) the time taken to perform
  // bidding (including the pre-kanonymous bid, and failed bids) iff
  // `interest_group` has authorized this auction's seller to receive such
  // information.
  void ReportBiddingLatency(const blink::InterestGroup& interest_group,
                            base::TimeDelta bidding_latency);

  // Returns all sellers and interest group buyers for the entire auction,
  // including both top-level and component auctions.
  base::flat_set<url::Origin> GetSellersAndBuyers();

  // Retrieves the keys that need to be joined as a result of the auction. A
  // failed auction may result in keys that still need to be joined, for
  // instance if the reason the auction failed was that none of the bids were
  // k-anonymous.
  //
  // If CreateReporter() is invoked, the returned reporter will automatically
  // join the k-anon sets if it's informed the winning ad has been navigated to,
  // so there's no need for anything else to invoke this method.
  base::flat_set<std::string> GetKAnonKeysToJoin() const;

  // Gets GetDeprecatedRenderURLReplacements in order to handle the
  // ad render url replacements within the interest group auction.
  const std::vector<blink::AuctionConfig::AdKeywordReplacement>&
  GetDeprecatedRenderURLReplacements();

  BiddingAndAuctionResponse TakeBiddingAndAuctionResponse() {
    return std::move(saved_response_).value();
  }

  // Depending on the requests present and whether the features have already
  // been logged for this page, may log one or more Private Aggregation API web
  // features.
  void MaybeLogPrivateAggregationWebFeatures(
      const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
          private_aggregation_requests);

  // Returns true if the config is using cross-origin trusted seller signals
  // that are disallowed by the permissions callback (and adds an appropriate
  // error).
  bool BlockDueToDisallowedCrossOriginTrustedSellerSignals();

  // Returns the top bid of whichever auction (k-anon or not, depending on the
  // configuration) is actually to be used for the user-facing results. May only
  // be invoked after the bidding and scoring phase has completed. Will be null
  // if there is no winner.
  ScoredBid* top_bid() const { return leader_info().top_bid.get(); }

  // Final result of the auction, once completed. Null before completion.
  std::optional<AuctionResult> final_auction_result() const {
    return final_auction_result_;
  }

  // Gets the buyer experiment ID in `config` for buyer. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::optional<uint16_t> GetBuyerExperimentId(
      const blink::AuctionConfig& config,
      const url::Origin& buyer);

  // Creates a query param that should be appended to the trusted bidding
  // signals fetch based on the specified TrustedBiddingSignalsSlotSizeMode.
  // Returns an empty string if no such query param should be appended, either
  // based on the auction/InterestGroup configuration or due to the the
  // associated feature not being enabled. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::string CreateTrustedBiddingSignalsSlotSizeParam(
      const blink::AuctionConfig& config,
      blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode
          trusted_bidding_signals_slot_size_mode);

  // Gets the buyer per-buyer-signals in `config` for buyer. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::optional<std::string> GetPerBuyerSignals(
      const blink::AuctionConfig& config,
      const url::Origin& buyer);

  // Gets the DirectFromSellerSignals auction-signals. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::optional<GURL> GetDirectFromSellerAuctionSignals(
      const SubresourceUrlBuilder* subresource_url_builder);

  // Gets the DirectFromSellerSignalsHeaderAdSlot auction-signals. Public so
  // that InterestGroupAuctionReporter can use it.
  static std::optional<std::string>
  GetDirectFromSellerAuctionSignalsHeaderAdSlot(
      const HeaderDirectFromSellerSignals::Result& signals);

  // Gets the buyer DirectFromSellerSignals per-buyer-signals in `config` for
  // buyer. Public so that InterestGroupAuctionReporter can use it.
  static std::optional<GURL> GetDirectFromSellerPerBuyerSignals(
      const SubresourceUrlBuilder* subresource_url_builder,
      const url::Origin& owner);

  // Gets the buyer DirectFromSellerSignalsHeaderAdSlot per-buyer-signals
  // for `owner`. Public so that InterestGroupAuctionReporter can use it.
  static std::optional<std::string>
  GetDirectFromSellerPerBuyerSignalsHeaderAdSlot(
      const HeaderDirectFromSellerSignals::Result& signals,
      const url::Origin& owner);

  // Gets DirectFromSellerSignals seller-signals. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::optional<GURL> GetDirectFromSellerSellerSignals(
      const SubresourceUrlBuilder* subresource_url_builder);

  // Gets DirectFromSellerSignalsHeaderAdSlot seller-signals. Public so that
  // InterestGroupAuctionReporter can use it.
  static std::optional<std::string>
  GetDirectFromSellerSellerSignalsHeaderAdSlot(
      const HeaderDirectFromSellerSignals::Result& signals);

  // Replaces `${}` placeholders in a debug report URL's query string for post
  // auction signals if exist. Only replaces unescaped placeholder ${}, but
  // not escaped placeholder (i.e., %24%7B%7D).
  static GURL FillPostAuctionSignals(
      const GURL& url,
      const PostAuctionSignals& signals,
      const std::optional<PostAuctionSignals>& top_level_signals = std::nullopt,
      const std::optional<auction_worklet::mojom::RejectReason> reject_reason =
          std::nullopt);

 private:
  // Note: this needs to be a type with iterator stability, since we both pass
  // iterators around and remove things from here.
  using AuctionMap = std::map<uint32_t, std::unique_ptr<InterestGroupAuction>>;

  // BuyerHelpers create and own the BidStates for a particular buyer, to
  // better handle per-buyer cross-interest-group behavior (e.g., enforcing
  // a shared per-buyer timeout, only generating bids for the highest priority N
  // interest groups of a particular buyer, etc).
  class BuyerHelper;

  struct LeaderInfo {
    LeaderInfo();
    ~LeaderInfo();

    LeaderInfo(const LeaderInfo&) = delete;
    LeaderInfo& operator=(LeaderInfo&) = delete;

    // The highest scoring bid so far. Null if no bid has been accepted yet.
    std::unique_ptr<ScoredBid> top_bid;
    // Number of bidders with the same score as `top_bid`.
    size_t num_top_bids = 0;
    // Number of bidders with the same score as `second_highest_score`. If the
    // second highest score matches the highest score, this does not include the
    // top bid.
    size_t num_second_highest_bids = 0;

    // The numeric value of the bid that got the second highest score. When
    // there's a tie for the second highest score, one of the second highest
    // scoring bids is randomly chosen.
    double highest_scoring_other_bid = 0.0;
    std::optional<double> highest_scoring_other_bid_in_seller_currency;
    double second_highest_score = 0.0;
    // Whether all bids of the highest score are from the same interest group
    // owner.
    bool at_most_one_top_bid_owner = true;
    // Will be null in the end if there are interest groups having the second
    // highest score with different owners. That includes the top bid itself, in
    // the case there's a tie for the top bid.
    std::optional<url::Origin> highest_scoring_other_bid_owner;
  };

  // ---------------------------------
  // Load interest group phase methods
  // ---------------------------------

  // Invoked whenever the interest groups for a buyer have loaded. Adds
  // `interest_groups` to `bid_states_`.
  void OnInterestGroupRead(
      scoped_refptr<StorageInterestGroups> interest_groups);

  // Invoked when the interest groups for an entire component auction have
  // loaded. If `success` is false, removes the component auction.
  void OnComponentInterestGroupsRead(AuctionMap::iterator component_auction,
                                     bool success);

  // Invoked when the interest groups for a buyer or for an entire component
  // auction have loaded. Completes the loading phase if no pending loads
  // remain.
  void OnOneLoadCompleted();

  // Invoked once the interest group load phase has completed. Never called
  // synchronously from StartLoadInterestGroupsPhase(), to avoid reentrancy
  // (AuctionRunner::callback_ cannot be invoked until
  // AuctionRunner::CreateAndStart() completes). `auction_result` is the
  // result of trying to load the interest groups that can participate in the
  // auction. It's AuctionResult::kSuccess if there are interest groups that
  // can take part in the auction, and a failure value otherwise.
  void OnStartLoadInterestGroupsPhaseComplete(AuctionResult auction_result);

  // -------------------------------------
  // Generate and score bids phase methods
  // -------------------------------------

  // Called when a component auction has received a worklet. Calls
  // RequestSellerWorklet() if all component auctions have received worklets.
  // See StartBiddingAndScoringPhase() for discussion of this.
  void OnComponentSellerWorkletReceived();

  // Requests a seller worklet from the AuctionWorkletManager.
  void RequestSellerWorklet();

  // True if all async prerequisites for calling ScoreBid on the SellerWorklet
  // are done.
  bool ReadyToScoreBids() const {
    return seller_worklet_received_ && config_promises_resolved_ &&
           !direct_from_seller_signals_header_ad_slot_pending_;
  }

  // True if the auction may have additional bids participating.
  bool MayHaveAdditionalBids() const {
    return config_->expects_additional_bids ||
           !encoded_signed_additional_bids_.empty() ||
           currently_decoding_additional_bids_ ||
           !bid_states_for_additional_bids_.empty();
  }

  // Called when RequestSellerWorklet() returns. Starts scoring bids, if there
  // are any and config has been resolved.
  void OnSellerWorkletReceived();

  // Score bids if both the seller worklet and config with all promises resolved
  // are ready.
  void ScoreQueuedBidsIfReady();

  void HandleUpdateIfOlderThan(
      const url::Origin& owner,
      std::string_view name,
      std::optional<base::TimeDelta> update_if_older_than);

  // Performs errors handling when an error is encountered while decoding an
  // additional bid. The caller of this should return immediately after calling
  // this function.
  void HandleAdditionalBidError(AdditionalBidResult result, std::string error);

  // If we're in the bidding and scoring phase, and
  // `encoded_signed_additional_bids_` has been filled in, starts of the process
  // of converting these into actual bids, keeping track of it via
  // `num_scoring_dependencies_`.
  void DecodeAdditionalBidsIfReady();

  // Processes a singled signed additional bid.
  void HandleDecodedSignedAdditionalBid(
      data_decoder::DataDecoder::ValueOrError result);

  // Processes payload of a single additionalBids entry.
  // `signatures` are the signatures it was supposedly signed with.
  // `valid_signatures` are the indices of signatures in `signatures` that
  // actually verify.
  void HandleDecodedAdditionalBid(
      const std::vector<SignedAdditionalBidSignature>& signatures,
      const std::vector<size_t>& valid_signatures,
      data_decoder::DataDecoder::ValueOrError result);

  // Invoked by the AuctionWorkletManager on fatal errors, at any point after
  // a SellerWorklet has been provided. Results in auction immediately
  // failing.
  void OnSellerWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // True if all bids have been generated (or decoded from additional_bids) and
  // scored and all config promises resolved.
  bool IsBiddingAndScoringPhaseComplete() const {
    CHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
    return num_scoring_dependencies_ == 0 && bids_being_scored_ == 0 &&
           unscored_bids_.empty() &&
           (!is_server_auction_ || saved_response_.has_value());
  }

  // True if `owner` opted in for real time reporting.
  bool IsBuyerOptedInToRealTimeReporting(const url::Origin& owner);

  // Add a real time reporting contribution for `origin` for script load
  // failure. If `is_buyer` is true, it's bidding script load failure.
  // Otherwise, it's scoring script load failure.
  void MaybeAddScriptFailureRealTimeContribution(bool is_buyer,
                                                 const url::Origin& origin);

  // Invoked when a component auction completes. If `success` is true, gets
  // the Bid from `component_auction` and passes a copy of it to ScoreBid().
  void OnComponentAuctionComplete(InterestGroupAuction* component_auction,
                                  bool success);

  static std::unique_ptr<Bid> CreateBidFromComponentAuctionWinner(
      const ScoredBid* scored_bid,
      auction_worklet::mojom::BidRole bid_role);

  // Called when a potential source of bids or other scoring dependency has
  // finished. This could be a component auction completing (with or without
  // generating a bid) or a BuyerHelper that has finished generating bids.
  // Bid scores should pass their bids to ScoreBidIfReady() before calling this.
  // It can also be called when all the config promises got resolved, if that
  // happens during the bidding and scoring phase.
  //
  // Updates `num_scoring_dependencies_`, flushes pending scoring signals
  // requests, and advances to the next state of the auction, if the bidding and
  // scoring phase is complete.
  void OnScoringDependencyDone();

  // If the seller is ready to score bids, calls to it, otherwise saves the bid
  // to `unscored_bids_`.
  void ScoreBidIfReady(std::unique_ptr<Bid> bid);

  // Calls into the seller asynchronously to score the passed in bid.
  void ScoreBid(std::unique_ptr<Bid> bid);

  // Validates the passed in result from ScoreBidComplete(). On failure, reports
  // a bad message to the active receiver in `score_ad_receivers_` and returns
  // false.
  bool ValidateScoreBidCompleteResult(
      double score,
      auction_worklet::mojom::ComponentAuctionModifiedBidParams*
          component_auction_modified_bid_params,
      std::optional<double> bid_in_seller_currency,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      const PrivateAggregationRequests& pa_requests,
      const RealTimeReportingContributions& real_time_contributions);

  // auction_worklet::mojom::ScoreAdClient implementation:
  void OnScoreAdComplete(
      double score,
      auction_worklet::mojom::RejectReason reject_reason,
      auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      std::optional<double> bid_in_seller_currency,
      std::optional<uint32_t> scoring_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      RealTimeReportingContributions real_time_contributions,
      auction_worklet::mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
      auction_worklet::mojom::ScoreAdDependencyLatenciesPtr
          score_ad_dependency_latencies,
      const std::vector<std::string>& errors) override;

  PrivateAggregationPhase seller_phase() const {
    return parent_ ? PrivateAggregationPhase::kNonTopLevelSeller
                   : PrivateAggregationPhase::kTopLevelSeller;
  }

  // Compares `bid` with current auction leaders in `leader_info`, updating
  // `leader_info` if needed.
  void UpdateAuctionLeaders(
      std::unique_ptr<Bid> bid,
      double score,
      auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      std::optional<double> bid_in_seller_currency,
      std::optional<uint32_t> scoring_signals_data_version,
      LeaderInfo& leader_info);

  // Invoked when the bid becomes the new highest scoring other bid, to handle
  // calculation of post auction signals. `owner` is nullptr in the event the
  // bid is tied with the top bid, and they have different origins.
  void OnNewHighestScoringOtherBid(double score,
                                   double bid_value,
                                   std::optional<double> bid_in_seller_currency,
                                   const url::Origin* owner,
                                   LeaderInfo& leader_info);

  std::optional<base::TimeDelta> SellerTimeout();

  // If IsBiddingAndScoringPhaseComplete() is true, completes the bidding and
  // scoring phase.
  void MaybeCompleteBiddingAndScoringPhase();

  // Invoked when the bidding and scoring phase of an auction completes.
  // `auction_result` is AuctionResult::kSuccess if the auction has a winner,
  // and some other value otherwise. Appends `errors` to `errors_`.
  void OnBiddingAndScoringComplete(AuctionResult auction_result,
                                   const std::vector<std::string>& errors = {});

  // Like top_bid() but returns all leader information.
  const LeaderInfo& leader_info() const;

  // These may be null. They should only be invoked after the bidding and
  // scoring phase has completed.
  ScoredBid* top_kanon_enforced_bid();
  const ScoredBid* top_kanon_enforced_bid() const;
  ScoredBid* top_non_kanon_enforced_bid();
  const ScoredBid* top_non_kanon_enforced_bid() const;

  // Fills in `signals_out` and `top_level_signals_out` for reporting for bid
  // by `bid_owner` based on the winner.
  void ComputePostAuctionSignals(
      const url::Origin& bid_owner,
      PostAuctionSignals& signals_out,
      std::optional<PostAuctionSignals>& top_level_signals_out);

  // Fills in `seller_metrics_` based on the collected state.
  // Used by TakeDebugReportUrlsAndFillInPrivateAggregationRequests().
  void FillInSellerParticipantDataMetrics();

  // Returns the multi-bid limit configured for `buyer` by `config_`,
  // ensuring that it's at least 1.
  uint16_t GetBuyerMultiBidLimit(const url::Origin& buyer);

  // -----------------------------------
  // Methods not associated with a phase
  // -----------------------------------

  // Creates a ComponentAuctionOtherSeller to pass to SellerWorklets when
  // dealing with `bid`. If `this` is a component auction, returns an object
  // with a `top_level_seller`. If this is a top-level auction and `bid` comes
  // from a component auction, returns an object with a `component_seller` to
  // `bid's` seller.
  auction_worklet::mojom::ComponentAuctionOtherSellerPtr GetOtherSellerParam(
      const Bid& bid) const;

  // Computes a key for a worklet associated with `bid_state`
  AuctionWorkletManager::WorkletKey BidderWorkletKey(BidState& bid_state);

  // Returns the query string for the associated
  // TrustedBiddingSignalsSlotSizeMode. Much like
  // CreateTrustedBiddingSignalsSlotSizeParam(), but Caches strings that have
  // previously been generated.
  const std::string& GetTrustedBiddingSignalsSlotSizeParam(
      blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode
          trusted_bidding_signals_slot_size_mode);

  // Determines if an extended private aggregation buyers request should be
  // made, and if so, issues the request. Otherwise, does nothing.
  //
  // That is, issues the request if all of the following are true:
  //
  // 1. `interest_group` has authorized the seller of this auction the
  // capability of type `capability`.
  //
  // 2. `config_`'s `auction_report_buyers` and `auction_report_buyer_keys` have
  // requested that such a report be made for the owner of `interest_group`.
  //
  // 3. `config_`'s `auction_report_buyers` has a key equal to
  // `buyer_report_type`.
  //
  // The issued extended private aggregation report's bucket is calculated from
  // `config_`'s `auction_report_buyer_keys` and `auction_report_buyers`, and
  // value equals to `value` times the `scalar` from `config_`'s
  // `auction_report_buyers`.
  //
  // Returns true iff a report was issued.
  //
  // TODO(crbug.com/40256945): Consider pre-aggregating metrics before sending
  // to the server.
  bool ReportPaBuyersValueIfAllowed(
      const blink::InterestGroup& interest_group,
      blink::SellerCapabilities capability,
      blink::AuctionConfig::NonSharedParams::BuyerReportType buyer_report_type,
      int value);

  // Returns how and whether k-anonymity is being handled.
  auction_worklet::mojom::KAnonymityBidMode kanon_mode() const {
    return kanon_mode_;
  }

  // Returns true if the auction had a non-k-anonymous winner.
  bool HasNonKAnonWinner() const;
  // Returns true if the non-k-anonymous winner of the auction is k-anonymous.
  bool NonKAnonWinnerIsKAnon() const;

  // Returns true if this auction (or one of its component auctions)
  // successfully loaded some interest groups.
  bool HasInterestGroups() const;

  // Returns the subresource builder if the promise configuring it has resolved,
  // creating it if needed.
  SubresourceUrlBuilder* SubresourceUrlBuilderIfReady();

  const HeaderDirectFromSellerSignals::Result*
  direct_from_seller_signals_header_ad_slot() const {
    return direct_from_seller_signals_header_ad_slot_.get();
  }

  // Some of these methods are split to ensure that regardless of how they
  // return they still call MaybeCompleteBiddingAndScoringPhase if they are
  // called during the scoring phase.

  // Returns false if we need to fail the auction instead of continuing in
  // OnDecompressedServerResponse.
  bool HandleServerResponseImpl(mojo_base::BigBuffer response,
                                AdAuctionPageData& ad_auction_page_data);

  void OnDecompressedServerResponse(
      AdAuctionRequestContext* request_context,
      base::expected<mojo_base::BigBuffer, std::string> result);

  void OnParsedServerResponse(AdAuctionRequestContext* request_context,
                              data_decoder::DataDecoder::ValueOrError result);

  // Returns false if we need to fail the auction instead of continuing in
  // OnLoadedWinningGroup.
  bool OnParsedServerResponseImpl(
      AdAuctionRequestContext* request_context,
      data_decoder::DataDecoder::ValueOrError result);

  void OnLoadedWinningGroup(
      BiddingAndAuctionResponse response,
      std::optional<SingleStorageInterestGroup> maybe_group);

  void OnLoadedWinningGroupImpl(
      BiddingAndAuctionResponse response,
      std::optional<SingleStorageInterestGroup> maybe_group);

  void MaybeLoadDebugReportLockoutAndCooldowns();

  void OnLoadDebugReportLockoutAndCooldownsComplete(
      std::optional<DebugReportLockoutAndCooldowns>
          debug_report_lockout_and_cooldowns);

  void CreateBidFromServerResponse();

  // Completion callback for AdAuctionPageData::ParseAndFindAdAuctionSignals().
  // Sets `direct_from_seller_signals_header_ad_slot_`, and sets
  // `direct_from_seller_signals_header_ad_slot_pending_` to false, appending
  // `errors` to `errors_`.
  void OnDirectFromSellerSignalHeaderAdSlotResolved(
      std::string ad_slot,
      scoped_refptr<HeaderDirectFromSellerSignals::Result> signals);

  // For metrics only -- update `interest_groups_bytes_for_metrics_` and
  // `ads_and_ad_components_bytes_for_metrics_` based on the currently loaded
  // `interest_groups`.
  void UpdateIgSizeMetrics(
      const std::vector<SingleStorageInterestGroup>& interest_groups);

  // For associating various events with a particular auction. Note that
  // component auctions have their own.
  const std::string devtools_auction_id_;

  // Tracing ID associated with the Auction. A nestable
  // async "Auction" trace event lasts for the combined lifetime of `this`
  // and a possible InterestGroupAuctionReporter. Sequential events that
  // apply to the entire auction are logged using this ID, including
  // potentially out-of-process events by bidder and seller worklet
  // reporting methods.
  //
  // Cleared if the ID got transferred to InterestGroupAuctionReporter.
  std::optional<uint64_t> trace_id_;

  // Whether k-anonymity enforcement or simulation (or none) are performed.
  const auction_worklet::mojom::KAnonymityBidMode kanon_mode_;

  const raw_ptr<AuctionMetricsRecorder> auction_metrics_recorder_;
  const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
  const raw_ptr<AuctionNonceManager> auction_nonce_manager_;
  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  // Configuration of this auction.
  raw_ptr<const blink::AuctionConfig> config_;

  // True once all promises in this and component auction's configuration have
  // been resolved. (Note that if `this` is a component auction, it only looks
  // at itself; while main auctions do look at their components recursively).
  bool config_promises_resolved_ = false;

  // Will be set to `true` while parsing JSON to find a matching
  // directFromSellerSignalsHeaderAdSlot response. Bid generation will be
  // blocked while true, even if promises have all resolved.
  bool direct_from_seller_signals_header_ad_slot_pending_ = false;

  // This is true during the window where the additional bids have been moved
  // away from `encoded_signed_additional_bids_` but haven't yet been put into
  // `bid_states_for_additional_bids_` (and a little bit afterwards).
  bool currently_decoding_additional_bids_ = false;

  // If this is a component auction, the parent Auction. Null, otherwise.
  const raw_ptr<const InterestGroupAuction> parent_;

  // flat_set copy of the interestGroupBuyers from the config, for efficient
  // finds. This is only populated when encoded_signed_additional_bids_ is.
  base::flat_set<url::Origin> interest_group_buyers_;

  // Base64-encoded signed additional bid entries.
  std::vector<std::string> encoded_signed_additional_bids_;

  // This needs pointer stability for the BidState*.
  std::vector<std::unique_ptr<BidState>> bid_states_for_additional_bids_;

  // Helper for computing negative targeting for additional bids.
  std::unique_ptr<AdAuctionNegativeTargeter> negative_targeter_;

  // Component auctions that are part of this auction. This auction manages
  // their state transition, and their bids may participate in this auction as
  // well. Component auctions that fail in the load phase are removed from
  // this map, to avoid trying to load their worklets during the scoring
  // phase.
  //
  // The key of the map is the original index of the auction's AuctionConfig
  // in `config_->non_shared_params.component_auctions`; there may be
  // discontinuities if some entries got removed in the load phase.
  AuctionMap component_auctions_;

  // Final result of the auction, once completed. Null before completion.
  std::optional<AuctionResult> final_auction_result_;

  // Each phases uses its own callback, to make sure that the right callback
  // is invoked when the phase completes.
  AuctionPhaseCompletionCallback load_interest_groups_phase_callback_;
  AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback_;

  // Start time of the BiddingAndScoring phase for UKM metrics.
  base::TimeTicks bidding_and_scoring_phase_start_time_;

  // Time at which we began decoding the additional bids.
  base::TimeTicks decode_additional_bids_start_time_;

  // Invoked in the bidding and scoring phase, once the seller worklet has
  // loaded. May be null.
  base::OnceClosure on_seller_receiver_callback_;

  // The number of buyers and component auctions with pending interest group
  // loads from storage. Decremented each time either the interest groups for
  // a buyer or all buyers for a component are read.
  // `load_interest_groups_phase_callback` is invoked once this hits 0.
  size_t num_pending_loads_ = 0;

  // True once a seller worklet has been received from the
  // AuctionWorkletManager.
  bool seller_worklet_received_ = false;

  enum class PhaseState { kBefore, kDuring, kAfter };
  PhaseState bidding_and_scoring_phase_state_ = PhaseState::kBefore;

  // True if creating bid from server response has started.
  bool started_creating_bid_from_response_ = false;

  // Number of things that are pending that are needed to score everything.
  // This includes bidders that are still attempting to generate bids ---
  // both BuyerHelpers and component auctions. BuyerHelpers may generate
  // multiple bids (or no bids). It also includes waiting for promises in
  // configuration to resolve, waiting for directFromSellerSignalsHeaderAdSlot
  // to parse, and waiting for additional bids to parse.
  //
  // When this reaches 0, the SellerWorklet's SendPendingSignalsRequests()
  // method should be invoked, so it can send any pending scoring signals
  // requests.
  int num_scoring_dependencies_ = 0;

  // Number of bids that have been send to the seller worklet to score, but
  // that haven't yet had their score received from the seller worklet.
  int bids_being_scored_ = 0;

  // The number of `component_auctions_` that have yet to request seller
  // worklets. Once it hits 0, the seller worklet for `this` is loaded. See
  // StartBiddingAndScoringPhase() for more details.
  size_t pending_component_seller_worklet_requests_ = 0;

  bool any_bid_made_ = false;

  // Lockout and cooldowns for sending forDebuggingOnly reports. It's read from
  // DB when the auction started for local auctions, or after B&A server
  // response is parsed for server auctions.
  std::optional<DebugReportLockoutAndCooldowns>
      debug_report_lockout_and_cooldowns_;

  // True if lockout and cooldowns are loaded for the server auction, to avoid
  // reading it more than once.
  bool server_auction_debug_report_lockout_loaded_ = false;

  // New lockout and cooldowns for sending forDebuggingOnly reports. It's
  // generated from this auction and updated during collecting debug reports.
  // Used to decide whether forDebuggingOnly API is in lockout or cooldown
  // together with `debug_report_lockout_and_cooldowns_`, and to update the DB
  // after this auction's debug reports are collected.
  // TODO(b/310944302): Fix handling of simultaneous auctions.
  DebugReportLockoutAndCooldowns new_debug_report_lockout_and_cooldowns_;

  // State of all buyers participating in the auction. Excludes buyers that
  // don't own any interest groups the user belongs to.
  std::vector<std::unique_ptr<BuyerHelper>> buyer_helpers_;

  // Bids waiting on the seller worklet to load before scoring. Does not
  // include bids that are currently waiting on the worklet's ScoreAd() method
  // to complete.
  std::vector<std::unique_ptr<Bid>> unscored_bids_;

  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_;
  // The time when this InterestGroupAuction was created; used for UMA.
  const base::TimeTicks creation_time_;

  // Holds the computed subresource URLs (renderer-supplied prefix + browser
  // produced suffix). This gets constructed on-demand once the prefix actually
  // comes in from a potential promises, and in successful auctions gets
  // transferred to InterestGroupAuctionReporter.
  std::unique_ptr<SubresourceUrlBuilder> subresource_url_builder_;

  // Stores the loaded HeaderDirectFromSellerSignals::Result, if there were any.
  // Should never be null until moved to the reporter.
  //
  // After `direct_from_seller_signals_header_ad_slot_` has been
  // set to true, the default constructed value gets replaced with the found
  // signals, if the auction config provided an ad-slot, and it matched one of
  // the captured responses for the seller's origin.
  scoped_refptr<HeaderDirectFromSellerSignals::Result>
      direct_from_seller_signals_header_ad_slot_ =
          base::MakeRefCounted<HeaderDirectFromSellerSignals::Result>();

  // The number of buyers in the `AuctionConfig` that passed the
  // `IsInterestGroupApiAllowedCallback` filter. Includes buyers from nested
  // component auctions. Double-counts buyers in multiple auctions.
  int num_owners_loaded_ = 0;

  // The number of buyers with `InterestGroup`s participating in an auction.
  // Includes buyers from nested component auctions, but excludes buyers with no
  // ads or no script URL. Double-counts buyers that participate in multiple
  // auctions.
  int num_owners_with_interest_groups_ = 0;

  // A list of all buyer owners that participated in this auction and had at
  // least one interest group. These owners will have their interest groups
  // updated after a successful auction, barring rate-limiting.
  std::vector<url::Origin> post_auction_update_owners_;

  // A list of all interest groups that need to have their priority adjusted.
  // The new rates will be committed after a successful auction.
  std::vector<std::pair<blink::InterestGroupKey, double>>
      post_auction_priority_updates_;

  LeaderInfo non_kanon_enforced_auction_leader_;
  LeaderInfo kanon_enforced_auction_leader_;

  // Holds a reference to the SellerWorklet used by the auction.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> seller_worklet_handle_;

  // Metrics for this auction's seller.
  PrivateAggregationParticipantData seller_metrics_;
  AuctionMetricsRecorder::LatencyAggregator code_fetch_time_;
  int seller_scripts_ran_ = 0;
  int seller_scripts_timed_out_ = 0;

  // Stores all pending Private Aggregation API report requests of reserved
  // event type from the bidding and scoring phase. These are passed to the
  // InterestGroupAuctionReporter when it's created. Keyed by the origin of the
  // script that issued the request (i.e. the reporting origin) and the
  // aggregation coordinator origin.
  std::map<PrivateAggregationKey, PrivateAggregationRequests>
      private_aggregation_requests_reserved_;

  // Stores all pending Private Aggregation API report requests of non-reserved
  // event type. Only comes from bidding phase of winning buyer. These are
  // passed to the InterestGroupAuctionReporter when it's created. Keyed by the
  // request's event type.
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_requests_non_reserved_;

  // This is used to keep track of which scoreAd execution's PA contributions on
  // "reserved.once" to use; it's incrementally updated as the scores come in.
  raw_ptr<BidState> seller_reserved_once_rep_ = nullptr;
  int seller_reserved_once_rep_count_ = 0;

  // A cache of feature params to avoid getting these values many times which
  // can be slow.
  std::optional<int> real_time_reporting_num_buckets_;
  std::optional<double> real_time_platform_contribution_priority_weight_;

  // Stores all real time reporting contributions. These will go through
  // sampling and converting to histograms of 0 and 1s.
  std::map<url::Origin, RealTimeReportingContributions>
      real_time_contributions_;

  // Callback for checking who can participate in the auction.
  IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback_;

  base::flat_map<blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode,
                 std::string>
      trusted_bidding_signals_size_mode_strings_;

  // Callback for passing encountered PrivateAggregationRequests up in order to
  // maybe trigger Private Aggregation web features, as appropriate.
  base::RepeatingCallback<void(
      const PrivateAggregationRequests& private_aggregation_requests)>
      maybe_log_private_aggregation_web_features_callback_;

  // This is set to true if the actual auction ran on a B&A server and we are
  // just handling the response.
  bool is_server_auction_ = false;

  // Saved response from the server if the actual auction ran on a B&A server.
  std::optional<BiddingAndAuctionResponse> saved_response_;

  // Time when `getInterestGroupAdAuctionData()` was called. Only for auctions
  // running on B&A servers.
  base::TimeTicks get_ad_auction_data_start_time_;

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  // This is set to true if the scoring phase ran and was able to score all
  // bids that were made (of which there may have been none). This is used to
  // gate accessors that should return nothing if the entire auction failed
  // (e.g., don't want to report bids as having "lost" an auction if the
  // seller failed to load, since neither the bids nor the bidders were the
  // problem).
  bool all_bids_scored_ = false;

  // Receivers for OnScoreAd() callbacks. Owns Bids, which have raw pointers to
  // other objects, so must be last, to avoid triggering tooling to check for
  // dangling pointers.
  mojo::ReceiverSet<auction_worklet::mojom::ScoreAdClient, std::unique_ptr<Bid>>
      score_ad_receivers_;

  GetDataDecoderCallback get_data_decoder_callback_;

  // For metrics only -- stores the size of interest groups and their internal
  // ads and ad components, respectively, as computed by EstimateSize(). Only
  // stores for the current auction; if this is the parent auction of component
  // auctions, their sizes are not included.
  size_t interest_groups_bytes_for_metrics_ = 0u;
  size_t ads_and_ad_components_bytes_for_metrics_ = 0u;

  base::WeakPtrFactory<InterestGroupAuction> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_AUCTION_H_

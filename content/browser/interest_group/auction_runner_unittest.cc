// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/aggregatable_report.mojom-shared.h"
#include "content/common/private_aggregation_features.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"

using auction_worklet::TestDevToolsAgentClient;

namespace content {
namespace {

using InterestGroupKey = blink::InterestGroupKey;
using PostAuctionSignals = InterestGroupAuction::PostAuctionSignals;
using blink::FencedFrame::ReportingDestination;
using PrivateAggregationRequests = AuctionRunner::PrivateAggregationRequests;

const std::string kBidder1Name{"Ad Platform"};
const char kBidder1DebugLossReportUrl[] =
    "https://bidder1-debug-loss-reporting.com/";
const char kBidder1DebugWinReportUrl[] =
    "https://bidder1-debug-win-reporting.com/";
const char kBidder2DebugLossReportUrl[] =
    "https://bidder2-debug-loss-reporting.com/";
const char kBidder2DebugWinReportUrl[] =
    "https://bidder2-debug-win-reporting.com/";

const char kBidderDebugLossReportBaseUrl[] =
    "https://bidder-debug-loss-reporting.com/";
const char kBidderDebugWinReportBaseUrl[] =
    "https://bidder-debug-win-reporting.com/";
const char kSellerDebugLossReportBaseUrl[] =
    "https://seller-debug-loss-reporting.com/";
const char kSellerDebugWinReportBaseUrl[] =
    "https://seller-debug-win-reporting.com/";

// Trusted bidding signals typically used for bidder1 and bidder2.
const char kBidder1SignalsJson[] =
    R"({"keys": {"k1":"a", "k2": "b", "extra": "c"}})";
const char kBidder2SignalsJson[] =
    R"({"keys": {"l1":"a", "l2": "b", "extra": "c"}})";

const char kPostAuctionSignalsPlaceholder[] =
    "?winningBid=${winningBid}&madeWinningBid=${madeWinningBid}&"
    "highestScoringOtherBid=${highestScoringOtherBid}&"
    "madeHighestScoringOtherBid=${madeHighestScoringOtherBid}";

const char kTopLevelPostAuctionSignalsPlaceholder[] =
    "topLevelWinningBid=${topLevelWinningBid}&"
    "topLevelMadeWinningBid=${topLevelMadeWinningBid}";

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedGenerateBidPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            content::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/1,
                /*value=*/2),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedReportWinPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            content::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/3,
                /*value=*/4),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedScoreAdPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            content::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/5,
                /*value=*/6),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedReportResultPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            content::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/7,
                /*value=*/8),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New());

// Helper to avoid excess boilerplate.
template <typename... Ts>
auto ElementsAreRequests(Ts&... requests) {
  static_assert(
      std::conjunction<std::is_same<
          std::remove_const_t<Ts>,
          auction_worklet::mojom::PrivateAggregationRequestPtr>...>::value);
  // Need to use `std::ref` as `mojo::StructPtr`s are move-only.
  return testing::UnorderedElementsAre(testing::Eq(std::ref(requests))...);
}

// 0 `num_component_urls` means no component URLs, as opposed to an empty list
// (which isn't tested at this layer).
std::string MakeBidScript(const url::Origin& seller,
                          const std::string& bid,
                          const std::string& render_url,
                          int num_ad_components,
                          const url::Origin& interest_group_owner,
                          const std::string& interest_group_name,
                          bool has_signals = false,
                          const std::string& signal_key = "",
                          const std::string& signal_val = "",
                          bool report_post_auction_signals = false,
                          const std::string& debug_loss_report_url = "",
                          const std::string& debug_win_report_url = "",
                          bool report_reject_reason = false) {
  // TODO(morlovich): Use JsReplace.
  constexpr char kBidScript[] = R"(
    const seller = "%s";
    const bid = %s;
    const renderUrl = "%s";
    const numAdComponents = %i;
    const interestGroupOwner = "%s";
    const interestGroupName = "%s";
    const hasSignals = %s;
    const reportPostAuctionSignals = %s;
    const reportRejectReason = %s;
    const postAuctionSignalsPlaceholder = "%s";
    let debugLossReportUrl = "%s";
    let debugWinReportUrl = "%s";
    const signalsKey = "%s";
    const signalsValue = "%s";
    const topLevelSeller = "https://adstuff.publisher1.com";

    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      let result = {ad: {"bidKey": "data for " + bid,
                         "groupName": interestGroupName,
                         "renderUrl": "data for " + renderUrl,
                         "seller": seller},
                    bid: bid,
                    render: renderUrl,
                    // Only need to allow component auction participation when
                    // `topLevelSeller` is populated.
                    allowComponentAuction: "topLevelSeller" in browserSignals};
      if (interestGroup.adComponents) {
        result.adComponents = [interestGroup.adComponents[0].renderUrl];
        result.ad.adComponentsUrl = interestGroup.adComponents[0].renderUrl;
      }

      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      // The actual priority should be hidden from the worklet.
      if (interestGroup.priority !== undefined)
        throw new Error("wrong priority: " + interestGroup.priority);
      // None of these tests set a dailyUpdateUrl. Non-empty values are tested
      // by browser tests.
      if ("dailyUpdateUrl" in interestGroup)
        throw new Error("Unexpected dailyUpdateUrl");
      if (interestGroup.ads.length != 1)
        throw new Error("wrong interestGroup.ads length");
      if (interestGroup.ads[0].renderUrl != renderUrl)
        throw new Error("wrong interestGroup.ads URL");
      if (numAdComponents == 0) {
        if (interestGroup.adComponents !== undefined)
          throw new Error("Non-empty adComponents");
      } else {
        if (interestGroup.adComponents.length !== numAdComponents)
          throw new Error("Wrong adComponents length");
        for (let i = 0; i < numAdComponents; ++i) {
          if (interestGroup.adComponents[i].renderUrl !=
              renderUrl.slice(0, -1) + "-component" + (i+1) + ".com/") {
            throw new Error("Wrong adComponents renderUrl");
          }
        }
      }
      // Skip the `perBuyerSignals` check if the interest group name matches
      // the bid. This is for auctions that use more than the two standard
      // bidders, since there's currently no way to inject new perBuyerSignals
      // into the top-level auction.
      // TODO(mmenke): Worth fixing that?
      if (interestGroupName !== bid + '') {
        if (perBuyerSignals[seller + 'Signals'] !==
            interestGroupName + 'Signals') {
          throw new Error("wrong perBuyerSignals");
        }
      }
      if (auctionSignals !== "auctionSignalsFor " + seller)
        throw new Error("wrong auctionSignals");
      if (hasSignals) {
        if ('extra' in trustedBiddingSignals)
          throw new Error("why extra?");
        if (!interestGroup.trustedBiddingSignalsKeys.includes(signalsKey))
          throw new Error("Wrong interestGroup.trustedBiddingSignalsKeys");
        if (trustedBiddingSignals[signalsKey] !== signalsValue)
          throw new Error("wrong signals");
      } else {
        if (trustedBiddingSignals !== null) {
          throw new Error("Expected null trustedBiddingSignals");
        }
      }
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (browserSignals.seller !== seller)
         throw new Error("wrong seller");
      if (browserSignals.seller === topLevelSeller) {
        if ("topLevelSeller" in browserSignals)
          throw new Error("expected no browserSignals.topLevelSeller");
      } else {
        if (browserSignals.topLevelSeller !== topLevelSeller)
          throw new Error("wrong browserSignals.topLevelSeller");
      }
      if (browserSignals.joinCount !== 3)
        throw new Error("joinCount")
      if (browserSignals.bidCount !== 5)
        throw new Error("bidCount");
      if (browserSignals.prevWins.length !== 3)
        throw new Error("prevWins");
      for (let i = 0; i < browserSignals.prevWins.length; ++i) {
        if (!(browserSignals.prevWins[i] instanceof Array))
          throw new Error("prevWins entry not an array");
        if (typeof browserSignals.prevWins[i][0] != "number")
          throw new Error("Not a Number in prevWin?");
        if (browserSignals.prevWins[i][1].winner !== -i)
          throw new Error("prevWin MD not what passed in");
      }
      if (debugLossReportUrl) {
        if (reportPostAuctionSignals)
          debugLossReportUrl += postAuctionSignalsPlaceholder;
        if (reportRejectReason) {
          debugLossReportUrl += reportPostAuctionSignals ? '&' : '?';
          debugLossReportUrl += 'rejectReason=${rejectReason}';
        }
        forDebuggingOnly.reportAdAuctionLoss(debugLossReportUrl);
      }
      if (debugWinReportUrl) {
        if (reportPostAuctionSignals)
          debugWinReportUrl += postAuctionSignalsPlaceholder;
        forDebuggingOnly.reportAdAuctionWin(debugWinReportUrl);
      }
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
      return result;
    }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      if (auctionSignals !== "auctionSignalsFor " + seller)
        throw new Error("wrong auctionSignals");
      // Skip the `perBuyerSignals` check if the interest group name matches
      // the bid. This is for auctions that use more than the two standard
      // bidders, since there's currently no way to inject new perBuyerSignals
      // into the top-level auction.
      // TODO(mmenke): Worth fixing that?
      if (interestGroupName !== bid + '') {
        if (perBuyerSignals[seller + 'Signals'] !==
            interestGroupName + 'Signals') {
          throw new Error("wrong perBuyerSignals");
        }
      }

      // sellerSignals in these tests is just sellers' browserSignals, since
      // that's what reportResult passes through.
      if (sellerSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (sellerSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (sellerSignals.renderUrl !== renderUrl)
        throw new Error("wrong renderUrl");
      if (sellerSignals.bid !== bid)
        throw new Error("wrong bid");
      // `sellerSignals` is the `browserSignals` for the seller that's
      // associated with the bid. If it's the top-level seller, the seller's
      // `browserSignals` should have no `componentSeller`, since the bid
      // was made directly to the top-level seller. If it's the component
      // seller, the seller's `browserSignals` should have a `topLevelSeller`
      // instead of a `componentSeller`, so `componentSeller` should never
      // be present in `sellerSignals` here.
      if ("componentSeller" in sellerSignals)
        throw new Error("wrong componentSeller in sellerSignals");
      if (browserSignals.seller === topLevelSeller) {
        if ("topLevelSeller" in sellerSignals)
          throw new Error("wrong topLevelSeller in sellerSignals");
      } else {
        // If the seller is a component seller, then then the seller's
        // `browserSignals` should have the top-level seller.
        if (sellerSignals.topLevelSeller !== topLevelSeller)
          throw new Error("wrong topLevelSeller in browserSignals");
      }

      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong browserSignals.topWindowHostname");
      if (browserSignals.seller !== seller)
         throw new Error("wrong seller");
      if (browserSignals.seller === topLevelSeller) {
        if ("topLevelSeller" in browserSignals)
          throw new Error("expected no browserSignals.topLevelSeller");
      } else {
        if (browserSignals.topLevelSeller !== topLevelSeller)
          throw new Error("wrong browserSignals.topLevelSeller");
      }
      if ("desirability" in browserSignals)
        throw new Error("why is desirability here?");
      if (browserSignals.interestGroupName !== interestGroupName)
        throw new Error("wrong browserSignals.interestGroupName");
      if (browserSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong browserSignals.interestGroupOwner");

      if (browserSignals.renderUrl !== renderUrl)
        throw new Error("wrong browserSignals.renderUrl");
      if (browserSignals.bid !== bid)
        throw new Error("wrong browserSignals.bid");
      if (browserSignals.seller != seller)
         throw new Error("wrong seller");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      let sendReportUrl = "https://buyer-reporting.example.com/";
      if (reportPostAuctionSignals) {
        sendReportUrl +=
            '?highestScoringOtherBid=' + browserSignals.highestScoringOtherBid +
            '&madeHighestScoringOtherBid=' +
            browserSignals.madeHighestScoringOtherBid + '&bid=';
      }
      sendReportTo(sendReportUrl + bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*bid,
      });
      privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
    }
  )";
  return base::StringPrintf(
      kBidScript, seller.Serialize().c_str(), bid.c_str(), render_url.c_str(),
      num_ad_components, interest_group_owner.Serialize().c_str(),
      interest_group_name.c_str(), has_signals ? "true" : "false",
      report_post_auction_signals ? "true" : "false",
      report_reject_reason ? "true" : "false", kPostAuctionSignalsPlaceholder,
      debug_loss_report_url.c_str(), debug_win_report_url.c_str(),
      signal_key.c_str(), signal_val.c_str());
}

// This can be appended to the standard script to override the function.
constexpr char kReportWinNoUrl[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
  }
)";

constexpr char kSimpleReportWin[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
    sendReportTo(
        "https://buyer-reporting.example.com/" +
        '?highestScoringOtherBid=' +  browserSignals.highestScoringOtherBid +
        '&madeHighestScoringOtherBid=' +
        browserSignals.madeHighestScoringOtherBid +
        '&bid=' + browserSignals.bid);
  }
)";

// A simple bid script that returns either `bid` or nothing depending on whether
// all incoming ads got filtered. If the interestGroup has components, the
// ad URL with /1 and /2 generated will be returned as components in the bid.
std::string MakeFilteringBidScript(int bid) {
  return base::StringPrintf(R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      if (interestGroup.ads.length === 0)
        return;

      let result = {
        ad: {},
        bid: %d,
        render: interestGroup.ads[0].renderUrl,
        allowComponentAuction: true
      };

      if (interestGroup.adComponents) {
        result.adComponents = [
          interestGroup.ads[0].renderUrl + "1",
          interestGroup.ads[0].renderUrl + "2",
        ];
      }

      return result;
    })",
                            bid);
}

// A bid script that always bids the same value + URL.
std::string MakeConstBidScript(int bid, const std::string& url) {
  return base::StringPrintf(R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      return {ad: {},
              bid: %d,
              render: "%s",
              allowComponentAuction: true};
    })",
                            bid, url.c_str());
}

// This can be appended to the standard script to override the function.
constexpr char kReportWinExpectNullAuctionSignals[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
    if (sellerSignals === null)
      sendReportTo("https://seller.signals.were.null.test");
  }
)";

constexpr char kMinimumDecisionScript[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                    browserSignals) {
    return {desirability: bid,
            allowComponentAuction: true,
            ad: adMetadata};
  }
)";

std::string MakeDecisionScript(
    const GURL& decision_logic_url,
    absl::optional<GURL> send_report_url = absl::nullopt,
    bool bid_from_component_auction_wins = false,
    bool report_post_auction_signals = false,
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "",
    bool report_top_level_post_auction_signals = false) {
  constexpr char kCheckingAuctionScript[] = R"(
    const decisionLogicUrl = "%s";
    let sendReportUrl = "%s";
    const reportPostAuctionSignals = %s;
    const postAuctionSignalsPlaceholder = "%s";
    let debugLossReportUrl = "%s";
    let debugWinReportUrl = "%s";
    const topLevelSeller = "https://adstuff.publisher1.com";
    const bidFromComponentAuctionWins = %s;
    const reportTopLevelPostAuctionSignals = %s;
    const topLevelPostAuctionSignalsPlaceholder = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      if (adMetadata.bidKey !== ("data for " + bid)) {
        throw new Error("wrong data for bid:" +
                        JSON.stringify(adMetadata) + "/" + bid);
      }
      if (adMetadata.renderUrl !== ("data for " + browserSignals.renderUrl)) {
        throw new Error("wrong data for renderUrl:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.renderUrl);
      }
      let components = browserSignals.adComponents;
      if (adMetadata.adComponentsUrl) {
        if (components.length !== 1 ||
            components[0] !== adMetadata.adComponentsUrl) {
          throw new Error("wrong data for adComponents:" +
                          JSON.stringify(adMetadata) + "/" +
                          browserSignals.adComponents);
        }
      } else if (components !== undefined) {
        throw new Error("wrong data for adComponents:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.adComponents);
      }
      // If this is the top-level auction scoring a bid from a component
      // auction, the component auction should have added a
      // "fromComponentAuction" field to `adMetadata`.
      if ("fromComponentAuction" in adMetadata !=
          "componentSeller" in browserSignals) {
        throw new Error("wrong adMetadata.fromComponentAuction");
      }
      if (auctionConfig.decisionLogicUrl !== decisionLogicUrl)
        throw new Error("wrong decisionLogicUrl in auctionConfig");
      // Check `perBuyerSignals` for the first bidder.
      let signals1 = auctionConfig.perBuyerSignals['https://adplatform.com'];
      if (signals1[auctionConfig.seller + 'Signals'] !== 'Ad PlatformSignals')
        throw new Error("Wrong perBuyerSignals in auctionConfig");
      if (typeof auctionConfig.perBuyerTimeouts['https://adplatform.com'] !==
          "number") {
        throw new Error("timeout in auctionConfig.perBuyerTimeouts is not a " +
                        "number. huh");
      }
      if (typeof auctionConfig.perBuyerTimeouts['*'] !== "number") {
        throw new Error("timeout in auctionConfig.perBuyerTimeouts is not a " +
                        "number. huh");
      }
      if (auctionConfig.sellerSignals["url"] != decisionLogicUrl)
        throw new Error("Wrong sellerSignals");
      if (typeof auctionConfig.sellerTimeout !== "number")
        throw new Error("auctionConfig.sellerTimeout is not a number. huh");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");

      if (decisionLogicUrl.startsWith(topLevelSeller)) {
        // Top-level sellers should receive component sellers, but only for
        // bids received from component auctions.
        if ("topLevelSeller" in browserSignals)
          throw new Error("Expected no topLevelSeller in browserSignals.");
        if (adMetadata.seller == topLevelSeller) {
          // If the bidder sent its bid directly to this top-level seller,
          // there should be no `componentSeller`.
          if ("componentSeller" in browserSignals)
            throw new Error("Expected no componentSeller in browserSignals.");
        } else {
          // If the bidder sent its bid to a some other seller seller, that
          // was the component seller, so `componentSeller` should be populated.
          if (!browserSignals.componentSeller.includes("component"))
            throw new Error("Incorrect componentSeller in browserSignals.");
        }
      } else {
        // Component sellers should receive only the top-level seller.
        if (browserSignals.topLevelSeller !== topLevelSeller)
          throw new Error("Incorrect topLevelSeller in browserSignals.");
        if ("componentSeller" in browserSignals)
          throw new Error("Expected no componentSeller in browserSignals.");
      }

      if ("joinCount" in browserSignals)
        throw new Error("wrong kind of browser signals");
      if (typeof browserSignals.biddingDurationMsec !== "number")
        throw new Error("biddingDurationMsec is not a number. huh");
      if (browserSignals.biddingDurationMsec < 0)
        throw new Error("biddingDurationMsec should be non-negative.");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      if (debugLossReportUrl) {
        forDebuggingOnly.reportAdAuctionLoss(
            buildDebugReportUrl(debugLossReportUrl) + bid);
      }
      if (debugWinReportUrl) {
        forDebuggingOnly.reportAdAuctionWin(
            buildDebugReportUrl(debugWinReportUrl) + bid);
      }
      privateAggregation.sendHistogramReport({bucket: 5n, value: 6});

      adMetadata.fromComponentAuction = true;

      return {desirability: computeScore(bid),
              // Only allow a component auction when the passed in ad is from
              // one.
              allowComponentAuction:
                  browserSignals.topLevelSeller !== undefined ||
                  browserSignals.componentSeller !== undefined,
              ad: adMetadata}
    }

    // A helper function to build a debug report URL.
    function buildDebugReportUrl(debugReportUrl) {
      if (reportPostAuctionSignals)
        debugReportUrl += postAuctionSignalsPlaceholder;
      if (reportTopLevelPostAuctionSignals) {
        debugReportUrl += reportPostAuctionSignals ? '&' : '?';
        debugReportUrl += topLevelPostAuctionSignalsPlaceholder;
      }
      // Only add key "bid=" to the report URL when report post auction signals
      // where the URL has many keys. Otherwise it's the only key so only have
      // the value in the URL is fine.
      if (reportPostAuctionSignals || reportTopLevelPostAuctionSignals)
        debugReportUrl += "&bid=";
      return debugReportUrl;
    }

    function reportResult(auctionConfig, browserSignals) {
      // Check `perBuyerSignals` for the first bidder.
      let signals1 = auctionConfig.perBuyerSignals['https://adplatform.com'];
      if (signals1[auctionConfig.seller + 'Signals'] !== 'Ad PlatformSignals')
        throw new Error("Wrong perBuyerSignals in auctionConfig");
      if (auctionConfig.decisionLogicUrl !== decisionLogicUrl)
        throw new Error("wrong decisionLogicUrl in auctionConfig");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname in browserSignals");

      if (decisionLogicUrl.startsWith(topLevelSeller)) {
        // Top-level sellers should receive component sellers, but only for
        // bids received from component auctions.
        if ("topLevelSeller" in browserSignals)
          throw new Error("Expected no topLevelSeller in browserSignals.");
        if (bidFromComponentAuctionWins) {
          if (!browserSignals.componentSeller.includes("component"))
            throw new Error("Incorrect componentSeller in browserSignals.");
        } else {
          if ("componentSeller" in browserSignals)
            throw new Error("Expected no componentSeller in browserSignals.");
        }

        if ("topLevelSellerSignals" in browserSignals)
          throw new Error("Unexpected browserSignals.topLevelSellerSignals");
      } else {
        // Component sellers should receive only the top-level seller.
        if (browserSignals.topLevelSeller !== topLevelSeller)
          throw new Error("Incorrect topLevelSeller in browserSignals.");
        if ("componentSeller" in browserSignals)
          throw new Error("Expected no componentSeller in browserSignals.");

        // Component sellers should get the return value of the top-level
        // seller's `reportResult()` call, which is, in this case, the
        // `browserSignals` of the top-level seller.
        if (browserSignals.topLevelSellerSignals.componentSeller !=
                auctionConfig.seller) {
          throw new Error("Unexpected browserSignals.topLevelSellerSignals");
        }
      }

      if (browserSignals.desirability != computeScore(browserSignals.bid))
        throw new Error("wrong bid or desirability in browserSignals");
      // The default scoreAd() script does not modify bids.
      if ("modifiedBid" in browserSignals)
        throw new Error("modifiedBid unexpectedly in browserSignals");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      if (sendReportUrl) {
        registerAdBeacon({
          "click": sendReportUrl + 2*browserSignals.bid,
        });
        if (reportPostAuctionSignals) {
          sendReportUrl += "?highestScoringOtherBid=" +
              browserSignals.highestScoringOtherBid + "&bid=";
        }
        sendReportTo(sendReportUrl + browserSignals.bid);
      }
      privateAggregation.sendHistogramReport({bucket: 7n, value: 8});

      return browserSignals;
    }

    // Use different scoring functions for the top-level seller and component
    // sellers, so can verify that each ReportResult() method gets the score
    // from the correct seller, and so that the the wrong bidder will win
    // in some tests if either component auction scores are used for the
    // top-level auction, or if all bidders from component auctions are passed
    // to the top-level auction.
    function computeScore(bid) {
      if (decisionLogicUrl == "https://adstuff.publisher1.com/auction.js")
        return 2 * bid;
      return 100 - bid;
    }
  )";

  return base::StringPrintf(
      kCheckingAuctionScript, decision_logic_url.spec().c_str(),
      send_report_url ? send_report_url->spec().c_str() : "",
      report_post_auction_signals ? "true" : "false",
      kPostAuctionSignalsPlaceholder, debug_loss_report_url.c_str(),
      debug_win_report_url.c_str(),
      bid_from_component_auction_wins ? "true" : "false",
      report_top_level_post_auction_signals ? "true" : "false",
      kTopLevelPostAuctionSignalsPlaceholder);
}

std::string MakeAuctionScript(bool report_post_auction_signals = false,
                              const GURL& decision_logic_url = GURL(
                                  "https://adstuff.publisher1.com/auction.js"),
                              const std::string& debug_loss_report_url = "",
                              const std::string& debug_win_report_url = "") {
  return MakeDecisionScript(
      decision_logic_url,
      /*send_report_url=*/GURL("https://reporting.example.com"),
      /*bid_from_component_auction_wins=*/false,
      /*report_post_auction_signals=*/report_post_auction_signals,
      debug_loss_report_url, debug_win_report_url);
}

std::string MakeAuctionScriptNoReportUrl(
    const GURL& decision_logic_url =
        GURL("https://adstuff.publisher1.com/auction.js"),
    bool report_post_auction_signals = false,
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "") {
  return MakeDecisionScript(decision_logic_url,
                            /*send_report_url=*/absl::nullopt,
                            /*bid_from_component_auction_wins=*/false,
                            report_post_auction_signals, debug_loss_report_url,
                            debug_win_report_url);
}

const char kBasicReportResult[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    privateAggregation.sendHistogramReport({bucket: 7n, value: 8});
    sendReportTo("https://reporting.example.com/" + browserSignals.bid);
    registerAdBeacon({
      "click": "https://reporting.example.com/" + 2*browserSignals.bid,
    });
    return browserSignals;
  }
)";

std::string MakeAuctionScriptReject2(
    const std::string& reject_reason = "not-available") {
  constexpr char kAuctionScriptRejects2[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.sendHistogramReport({bucket: 5n, value: 6});
      if (bid === 2)
        return {desirability: -1, rejectReason: '%s'};
      return bid + 1;
    }
  )";
  return base::StringPrintf(kAuctionScriptRejects2, reject_reason.c_str()) +
         kBasicReportResult;
}

std::string MakeAuctionScriptReject1And2WithDebugReporting(
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "") {
  constexpr char kReject1And2WithDebugReporting[] = R"(
    const debugLossReportUrl = "%s";
    const debugWinReportUrl = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      let result = bid + 1;
      let rejectReason = "not-available";
      if (bid === 1) {
        result = -1;
        rejectReason = 'invalid-bid';
      } else if (bid === 2) {
        result = -1;
        rejectReason = 'bid-below-auction-floor';
      }

      if (debugLossReportUrl) {
        forDebuggingOnly.reportAdAuctionLoss(
            debugLossReportUrl + '&bid=' + bid);
      }
      if (debugWinReportUrl)
        forDebuggingOnly.reportAdAuctionWin(debugWinReportUrl + "&bid=" + bid);
      return {
        desirability: result,
        allowComponentAuction: true,
        rejectReason: rejectReason
      };
    }
  )";
  return base::StringPrintf(kReject1And2WithDebugReporting,
                            debug_loss_report_url.c_str(),
                            debug_win_report_url.c_str()) +
         kBasicReportResult;
}

// Treats interest group name as bid. Interest group name needs to be
// convertible to a valid number in order to use this script.
std::string MakeBidScriptSupportsTie() {
  constexpr char kBidScriptSupportsTie[] = R"(
    const debugLossReportUrl = '%s';
    const debugWinReportUrl = '%s';

    const postAuctionSignalsPlaceholder = '%s';
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      const bid = parseInt(interestGroup.name);
      forDebuggingOnly.reportAdAuctionLoss(
          debugLossReportUrl + postAuctionSignalsPlaceholder + '&bid=' + bid);
      forDebuggingOnly.reportAdAuctionWin(
          debugWinReportUrl + postAuctionSignalsPlaceholder + '&bid=' + bid);
      return {ad: [], bid: bid, render: interestGroup.ads[0].renderUrl};
    }
    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo(
          'https://buyer-reporting.example.com/?highestScoringOtherBid=' +
          browserSignals.highestScoringOtherBid +
          '&madeHighestScoringOtherBid=' +
          browserSignals.madeHighestScoringOtherBid +
          '&bid=' + browserSignals.bid);
    }
  )";
  return base::StringPrintf(
      kBidScriptSupportsTie, kBidderDebugLossReportBaseUrl,
      kBidderDebugWinReportBaseUrl, kPostAuctionSignalsPlaceholder);
}

// Score is 3 if bid is 3 or 4, otherwise score is 1.
std::string MakeAuctionScriptSupportsTie() {
  constexpr char kAuctionScriptSupportsTie[] = R"(
    const debugLossReportUrl = "%s";
    const debugWinReportUrl = "%s";
    const postAuctionSignalsPlaceholder = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          debugLossReportUrl + postAuctionSignalsPlaceholder + "&bid=" + bid);
      forDebuggingOnly.reportAdAuctionWin(
          debugWinReportUrl + postAuctionSignalsPlaceholder + "&bid=" + bid);
      return bid = (bid == 3 || bid == 4) ? 3 : 1;
    }
    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(
          "https://reporting.example.com/?highestScoringOtherBid=" +
          browserSignals.highestScoringOtherBid + "&bid=" +
          browserSignals.bid);
    }
  )";
  return base::StringPrintf(
      kAuctionScriptSupportsTie, kSellerDebugLossReportBaseUrl,
      kSellerDebugWinReportBaseUrl, kPostAuctionSignalsPlaceholder);
}

// Represents an entry in trusted bidding signal's `perInterestGroupData` field.
struct BiddingSignalsPerInterestGroupData {
  std::string interest_group_name;
  absl::optional<base::flat_map<std::string, double>> priority_vector;
};

// Creates a trusted bidding signals response body with the provided data.
std::string MakeBiddingSignalsWithPerInterestGroupData(
    std::vector<BiddingSignalsPerInterestGroupData> per_interest_group_data) {
  base::Value::Dict per_interest_group_dict;
  for (const auto& data : per_interest_group_data) {
    base::Value::Dict interest_group_dict;
    if (data.priority_vector) {
      base::Value::Dict priority_vector;
      for (const auto& pair : *data.priority_vector) {
        priority_vector.Set(pair.first, pair.second);
      }
      interest_group_dict.Set("priorityVector", std::move(priority_vector));
    }
    per_interest_group_dict.Set(data.interest_group_name,
                                std::move(interest_group_dict));
  }

  base::Value::Dict bidding_signals_dict;
  bidding_signals_dict.Set("perInterestGroupData",
                           std::move(per_interest_group_dict));

  std::string bidding_signals_string;
  CHECK(base::JSONWriter::Write(bidding_signals_dict, &bidding_signals_string));
  return bidding_signals_string;
}

// Returns a report URL with given parameters for reportWin(), with post auction
// signals included in the URL
const GURL ReportWinUrl(
    double bid,
    double highest_scoring_other_bid,
    bool made_highest_scoring_other_bid,
    const std::string& url = "https://buyer-reporting.example.com/") {
  // Only keeps integer part of bid values for simplicity for now.
  return GURL(base::StringPrintf(
      "%s"
      "?highestScoringOtherBid=%.0f&madeHighestScoringOtherBid=%s&bid=%.0f",
      url.c_str(), highest_scoring_other_bid,
      made_highest_scoring_other_bid ? "true" : "false", bid));
}

// Returns a report URL with given parameters for forDebuggingOnly win/loss
// report APIs, with post auction signals included in the URL.
const GURL DebugReportUrl(
    const std::string& url,
    const PostAuctionSignals& signals,
    absl::optional<double> bid = absl::nullopt,
    absl::optional<std::string> reject_reason = absl::nullopt) {
  // Post auction signals needs to be consistent with
  // `kPostAuctionSignalsPlaceholder`. Only keeps integer part of bid values for
  // simplicity for now.
  std::string report_url_string = base::StringPrintf(
      "%s"
      // Post auction signals
      "?winningBid=%.0f&madeWinningBid=%s&highestScoringOtherBid=%.0f&"
      "madeHighestScoringOtherBid=%s",
      url.c_str(), signals.winning_bid,
      signals.made_winning_bid ? "true" : "false",
      signals.highest_scoring_other_bid,
      signals.made_highest_scoring_other_bid ? "true" : "false");
  if (reject_reason.has_value()) {
    report_url_string.append(
        base::StringPrintf("&rejectReason=%s", reject_reason.value().c_str()));
  }

  if (bid.has_value()) {
    return GURL(base::StringPrintf("%s&bid=%.0f", report_url_string.c_str(),
                                   bid.value()));
  }
  return GURL(report_url_string);
}

// Returns a report URL for component auction seller with given parameters for
// forDebuggingOnly win/loss report APIs, with post auction signals from both
// component auction and top level auction included in the URL. When no
// `top_level_signals` is needed, just use function DebugReportUrl().
const GURL ComponentSellerDebugReportUrl(
    const std::string& url,
    const PostAuctionSignals& signals,
    const PostAuctionSignals& top_level_signals,
    double bid) {
  // Post auction signals needs to be consistent with
  // `kPostAuctionSignalsPlaceholder`, and top level post auction signals needs
  // to be consistent with `kTopLevelPostAuctionSignalsPlaceholder`. Only keeps
  // integer part of bid values for simplicity for now.
  return GURL(base::StringPrintf(
      "%s"
      // Post auction signals.
      "?winningBid=%.0f&madeWinningBid=%s&highestScoringOtherBid=%.0f&"
      "madeHighestScoringOtherBid=%s"
      // Top level post auction signals.
      "&topLevelWinningBid=%.0f&topLevelMadeWinningBid=%s"
      // Bid value.
      "&bid=%.0f",
      url.c_str(), signals.winning_bid,
      signals.made_winning_bid ? "true" : "false",
      signals.highest_scoring_other_bid,
      signals.made_highest_scoring_other_bid ? "true" : "false",
      top_level_signals.winning_bid,
      top_level_signals.made_winning_bid ? "true" : "false", bid));
}

// Marks `ad` in `group` k-anonymous, double-checking that its url is `url`.
void AuthorizeKAnon(const blink::InterestGroup::Ad& ad,
                    const char* url,
                    StorageInterestGroup& group) {
  group.bidding_ads_kanon.emplace_back();
  group.bidding_ads_kanon.back().key =
      KAnonKeyForAdBid(group.interest_group, ad.render_url);
  DCHECK_EQ(GURL(url),
            RenderUrlFromKAnonKeyForAdBid(group.bidding_ads_kanon.back().key));
  group.bidding_ads_kanon.back().is_k_anonymous = true;
  group.bidding_ads_kanon.back().last_updated = base::Time::Now();
}

// BidderWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet {
 public:
  explicit MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockBidderWorklet::OnPipeClosed, base::Unretained(this)));
  }

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override {
    // `send_pending_signals_requests_called_` should always be called if any
    // bids are generated, except in the unlikely event that the Mojo pipe is
    // closed before a posted task is executed (this cannot be simulated by
    // closing a pipe in tests, due to vagaries of timing of the two messages).
    if (generate_bid_called_) {
      // Flush the receiver in case the message is pending on the pipe. This
      // doesn't happen when the auction has run successfully, where the auction
      // only completes when all messages have been received, but may happen in
      // failure cases where the message is sent, but the AuctionRunner is torn
      // down early.
      if (receiver_.is_bound())
        receiver_.FlushForTesting();
      EXPECT_TRUE(send_pending_signals_requests_called_);
    }
  }

  // auction_worklet::mojom::BidderWorklet implementation:

  void GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
          bidder_worklet_non_shared_params,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const url::Origin& interest_group_join_origin,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const absl::optional<base::TimeDelta> per_buyer_timeout,
      const url::Origin& browser_signal_seller_origin,
      const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
      auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
      base::Time auction_start_time,
      uint64_t trace_id,
      mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
          generate_bid_client) override {
    generate_bid_called_ = true;
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!generate_bid_client_);

    // per_buyer_timeout passed to GenerateBid() should not be empty, because
    // auction_config's all_buyers_timeout (which is the key of '*' in
    // perBuyerTimeouts) is set in the AuctionRunnerTest.
    ASSERT_TRUE(per_buyer_timeout.has_value());
    if (bidder_worklet_non_shared_params->name == kBidder1Name) {
      // Any per buyer timeout in auction_config higher than 500 ms should be
      // clamped to 500 ms by the AuctionRunner before passed to GenerateBid(),
      // and kBidder1's per buyer timeout is 1000 ms in auction_config so it
      // should be 500 ms here.
      EXPECT_EQ(per_buyer_timeout.value(), base::Milliseconds(500));
    } else {
      // Any other bidder's per buyer timeout should be 150 ms, since
      // auction_config's all_buyers_timeout is set to 150 ms in the
      // AuctionRunnerTest.
      EXPECT_EQ(per_buyer_timeout.value(), base::Milliseconds(150));
    }

    // Single auctions should invoke all GenerateBid() calls on a worklet
    // before invoking SendPendingSignalsRequests().
    EXPECT_FALSE(send_pending_signals_requests_called_);

    generate_bid_client_.Bind(std::move(generate_bid_client));
    if (generate_bid_run_loop_)
      generate_bid_run_loop_->Quit();
  }

  void SendPendingSignalsRequests() override {
    // This allows multiple calls.
    send_pending_signals_requests_called_ = true;
  }

  void ReportWin(
      const std::string& interest_group_name,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const std::string& seller_signals_json,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      double browser_signal_highest_scoring_other_bid,
      bool browser_signal_made_highest_scoring_other_bid,
      const url::Origin& browser_signal_seller_origin,
      const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
      uint32_t bidding_signals_data_version,
      bool has_bidding_signals_data_version,
      uint64_t trace_id,
      ReportWinCallback report_win_callback) override {
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!report_win_callback_);
    report_win_callback_ = std::move(report_win_callback);
    if (report_win_run_loop_)
      report_win_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
  }

  void WaitForGenerateBid() {
    if (!generate_bid_client_) {
      generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
      generate_bid_run_loop_->Run();
      generate_bid_run_loop_.reset();
      DCHECK(generate_bid_client_);
    }
  }

  // Invokes the GenerateBid callback. A bid of base::nullopt means no bid
  // should be offered. Waits for the GenerateBid() call first, if needed.
  void InvokeGenerateBidCallback(
      absl::optional<double> bid,
      const GURL& render_url = GURL(),
      auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_kanon_bid =
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt,
      base::TimeDelta duration = base::TimeDelta(),
      const absl::optional<uint32_t>& bidding_signals_data_version =
          absl::nullopt,
      const absl::optional<GURL>& debug_loss_report_url = absl::nullopt,
      const absl::optional<GURL>& debug_win_report_url = absl::nullopt,
      PrivateAggregationRequests pa_requests = {}) {
    WaitForGenerateBid();

    base::RunLoop run_loop;
    generate_bid_client_->OnBiddingSignalsReceived(
        /*priority_vector=*/{}, run_loop.QuitClosure());
    run_loop.Run();

    if (!bid.has_value()) {
      generate_bid_client_->OnGenerateBidComplete(
          /*bid=*/nullptr,
          /*kanon_bid=*/std::move(mojo_kanon_bid),
          /*bidding_signals_data_version=*/0,
          /*has_bidding_signals_data_version=*/false, debug_loss_report_url,
          /*debug_win_report_url=*/absl::nullopt,
          /*set_priority=*/0,
          /*has_set_priority=*/false,
          /*update_priority_signals_overrides=*/
          base::flat_map<std::string,
                         auction_worklet::mojom::PrioritySignalsDoublePtr>(),
          /*pa_requests=*/std::move(pa_requests),
          /*errors=*/std::vector<std::string>());
      return;
    }

    generate_bid_client_->OnGenerateBidComplete(
        auction_worklet::mojom::BidderWorkletBid::New(
            "ad", *bid, render_url, ad_component_urls, duration),
        /*kanon_bid=*/std::move(mojo_kanon_bid),
        bidding_signals_data_version.value_or(0),
        bidding_signals_data_version.has_value(), debug_loss_report_url,
        debug_win_report_url,
        /*set_priority=*/0,
        /*has_set_priority=*/false,
        /*update_priority_signals_overrides=*/
        base::flat_map<std::string,
                       auction_worklet::mojom::PrioritySignalsDoublePtr>(),
        /*pa_requests=*/std::move(pa_requests),
        /*errors=*/std::vector<std::string>());
  }

  void WaitForReportWin() {
    DCHECK(!generate_bid_client_);
    DCHECK(!report_win_run_loop_);
    if (!report_win_callback_) {
      report_win_run_loop_ = std::make_unique<base::RunLoop>();
      report_win_run_loop_->Run();
      report_win_run_loop_.reset();
      DCHECK(report_win_callback_);
    }
  }

  void InvokeReportWinCallback(
      absl::optional<GURL> report_url = absl::nullopt,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      PrivateAggregationRequests pa_requests = {}) {
    DCHECK(report_win_callback_);
    std::move(report_win_callback_)
        .Run(report_url, ad_beacon_map, std::move(pa_requests),
             /*errors=*/std::vector<std::string>());
  }

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed() {
    receiver_.FlushForTesting();
    return pipe_closed_;
  }

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidClient>
      generate_bid_client_;

  bool pipe_closed_ = false;

  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;
  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  bool generate_bid_called_ = false;
  bool send_pending_signals_requests_called_ = false;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::BidderWorklet> receiver_;
};

// SellerWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockSellerWorklet : public auction_worklet::mojom::SellerWorklet {
 public:
  // Subset of parameters passed to SellerWorklet's ScoreAd method.
  struct ScoreAdParams {
    mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient> score_ad_client;
    double bid;
    url::Origin interest_group_owner;
  };

  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override {
    // Flush the receiver in case the message is pending on the pipe. This
    // doesn't happen when the auction has run successfully, where the auction
    // only completes when all messages have been received, but may happen in
    // failure cases where the message is sent, but the AuctionRunner is torn
    // down early.
    if (receiver_.is_bound())
      receiver_.FlushForTesting();

    EXPECT_EQ(expect_send_pending_signals_requests_called_,
              send_pending_signals_requests_called_);

    // Every received ScoreAd() call should have been waited for.
    EXPECT_TRUE(score_ad_params_.empty());
  }

  // auction_worklet::mojom::SellerWorklet implementation:

  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               const blink::AuctionConfig::NonSharedParams&
                   auction_ad_config_non_shared_params,
               const absl::optional<GURL>& direct_from_seller_seller_signals,
               const absl::optional<GURL>& direct_from_seller_auction_signals,
               auction_worklet::mojom::ComponentAuctionOtherSellerPtr
                   browser_signals_other_seller,
               const url::Origin& browser_signal_interest_group_owner,
               const GURL& browser_signal_render_url,
               const std::vector<GURL>& browser_signal_ad_components,
               uint32_t browser_signal_bidding_duration_msecs,
               const absl::optional<base::TimeDelta> seller_timeout,
               uint64_t trace_id,
               mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
                   score_ad_client) override {
    // SendPendingSignalsRequests() should only be called once all ads are
    // scored.
    EXPECT_FALSE(send_pending_signals_requests_called_);

    ASSERT_TRUE(seller_timeout.has_value());
    // seller_timeout in auction_config higher than 500 ms should be clamped to
    // 500 ms by the AuctionRunner before passed to ScoreAd(), and
    // auction_config's seller_timeout is 1000 ms so it should be 500 ms here.
    EXPECT_EQ(seller_timeout.value(), base::Milliseconds(500));

    ScoreAdParams score_ad_params;
    score_ad_params.score_ad_client = std::move(score_ad_client);
    score_ad_params.bid = bid;
    score_ad_params.interest_group_owner = browser_signal_interest_group_owner;
    score_ad_params_.emplace_front(std::move(score_ad_params));
    if (score_ad_run_loop_)
      score_ad_run_loop_->Quit();
  }

  void SendPendingSignalsRequests() override {
    // SendPendingSignalsRequests() should only be called once by a single
    // AuctionRunner.
    EXPECT_FALSE(send_pending_signals_requests_called_);

    send_pending_signals_requests_called_ = true;
  }

  void ReportResult(
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const absl::optional<GURL>& direct_from_seller_seller_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      auction_worklet::mojom::ComponentAuctionOtherSellerPtr
          browser_signals_other_seller,
      const url::Origin& browser_signal_interest_group_owner,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      double browser_signal_desirability,
      double browser_signal_highest_scoring_other_bid,
      auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
          browser_signals_component_auction_report_result_params,
      uint32_t browser_signal_data_version,
      bool browser_signal_has_data_version,
      uint64_t trace_id,
      ReportResultCallback report_result_callback) override {
    report_result_callback_ = std::move(report_result_callback);
    if (report_result_run_loop_)
      report_result_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockSellerWorklet";
  }

  void ResetReceiverWithReason(const std::string& reason) {
    receiver_.ResetWithReason(/*custom_reason_code=*/0, reason);
  }

  // Waits until ScoreAd() has been invoked, if it hasn't been already. It's up
  // to the caller to invoke the returned ScoreAdParams::callback to continue
  // the auction.
  ScoreAdParams WaitForScoreAd() {
    DCHECK(!score_ad_run_loop_);
    if (score_ad_params_.empty()) {
      score_ad_run_loop_ = std::make_unique<base::RunLoop>();
      score_ad_run_loop_->Run();
      score_ad_run_loop_.reset();
      DCHECK(!score_ad_params_.empty());
    }
    ScoreAdParams out = std::move(score_ad_params_.front());
    score_ad_params_.pop_front();
    return out;
  }

  void WaitForReportResult() {
    DCHECK(!report_result_run_loop_);
    if (!report_result_callback_) {
      report_result_run_loop_ = std::make_unique<base::RunLoop>();
      report_result_run_loop_->Run();
      report_result_run_loop_.reset();
      DCHECK(report_result_callback_);
    }
  }

  // Invokes the ReportResultCallback for the most recent ScoreAd() call with
  // the provided score. WaitForReportResult() must have been invoked first.
  void InvokeReportResultCallback(
      absl::optional<GURL> report_url = absl::nullopt,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      PrivateAggregationRequests pa_requests = {},
      std::vector<std::string> errors = {}) {
    DCHECK(report_result_callback_);
    std::move(report_result_callback_)
        .Run(/*signals_for_winner=*/absl::nullopt, std::move(report_url),
             ad_beacon_map, std::move(pa_requests), errors);
  }

  void Flush() { receiver_.FlushForTesting(); }

  // `expect_send_pending_signals_requests_called_` needs to be set to false in
  // the case a SellerWorklet is destroyed before it receives a request to score
  // the final bid.
  void set_expect_send_pending_signals_requests_called(bool value) {
    expect_send_pending_signals_requests_called_ = value;
  }

 private:
  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::list<ScoreAdParams> score_ad_params_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  bool expect_send_pending_signals_requests_called_ = true;
  bool send_pending_signals_requests_called_ = false;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionWorkletService that creates MockBidderWorklets and MockSellerWorklets
// to hold onto passed in PendingReceivers and Callbacks.

// AuctionProcessManager and AuctionWorkletService - combining the two with a
// mojo::ReceiverSet makes it easier to track which call came over which
// receiver than using separate classes.
class MockAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  MockAuctionProcessManager() = default;
  ~MockAuctionProcessManager() override = default;

  // AuctionProcessManager implementation:
  RenderProcessHost* LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const ProcessHandle* handle,
      const std::string& display_name) override {
    mojo::ReceiverId receiver_id =
        receiver_set_.Add(this, std::move(auction_worklet_service_receiver));

    // Have to flush the receiver set, so that any closed receivers are removed,
    // before searching for duplicate process names.
    receiver_set_.FlushForTesting();

    // Each receiver should get a unique display name. This check serves to help
    // ensure that processes are correctly reused.
    EXPECT_EQ(0u, receiver_display_name_map_.count(receiver_id));
    for (auto receiver : receiver_display_name_map_) {
      // Ignore closed receivers. ReportWin() will result in re-loading a
      // worklet, after closing the original worklet, which may require
      // re-creating the AuctionWorkletService.
      if (receiver_set_.HasReceiver(receiver.first))
        EXPECT_NE(receiver.second, display_name);
    }

    receiver_display_name_map_[receiver_id] = display_name;
    return nullptr;
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& bidding_wasm_helper_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin,
      bool has_experiment_group_id,
      uint16_t experiment_group_id) override {
    // Make sure this request came over the right pipe.
    url::Origin owner = url::Origin::Create(script_source_url);
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 url::Origin::Create(script_source_url)));

    EXPECT_EQ(0u, bidder_worklets_.count(script_source_url));
    bidder_worklets_.emplace(std::make_pair(
        script_source_url, std::make_unique<MockBidderWorklet>(
                               std::move(bidder_worklet_receiver))));
    // Whenever a worklet is created, one of the RunLoops should be waiting for
    // worklet creation.
    if (wait_for_bidder_reload_run_loop_) {
      wait_for_bidder_reload_run_loop_->Quit();
    } else {
      ASSERT_GT(waiting_for_num_bidders_, 0);
      --waiting_for_num_bidders_;
      MaybeQuitWaitForWorkletsRunLoop();
    }
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      bool has_experiment_group_id,
      uint16_t experiment_group_id) override {
    EXPECT_EQ(0u, seller_worklets_.count(script_source_url));

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklets_.emplace(std::make_pair(
        script_source_url, std::make_unique<MockSellerWorklet>(
                               std::move(seller_worklet_receiver))));

    // Whenever a worklet is created, one of the RunLoops should be waiting for
    // worklet creation.
    if (wait_for_seller_reload_run_loop_) {
      wait_for_seller_reload_run_loop_->Quit();
    } else {
      EXPECT_GT(waiting_for_num_sellers_, 0);
      --waiting_for_num_sellers_;
      MaybeQuitWaitForWorkletsRunLoop();
    }
  }

  // Waits for `num_bidders` bidder worklets and `num_sellers` seller worklets
  // to be created.
  void WaitForWorklets(int num_bidders, int num_sellers = 1) {
    waiting_for_num_bidders_ = num_bidders;
    waiting_for_num_sellers_ = num_sellers;
    wait_for_worklets_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_worklets_run_loop_->Run();
    wait_for_worklets_run_loop_.reset();
  }

  // Waits for a single bidder script to be loaded. Intended to be used to wait
  // for the winning bidder script to be reloaded. WaitForWorklets() should be
  // used when waiting for worklets to be loaded at the start of an auction.
  void WaitForWinningBidderReload() {
    EXPECT_TRUE(bidder_worklets_.empty());
    wait_for_bidder_reload_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_bidder_reload_run_loop_->Run();
    wait_for_bidder_reload_run_loop_.reset();
    EXPECT_EQ(1u, bidder_worklets_.size());
  }

  void WaitForWinningSellerReload() {
    EXPECT_TRUE(seller_worklets_.empty());
    wait_for_seller_reload_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_seller_reload_run_loop_->Run();
    wait_for_seller_reload_run_loop_.reset();
    EXPECT_EQ(1u, seller_worklets_.size());
  }

  // Returns the MockBidderWorklet created for the specified script URL, if
  // there is one.
  std::unique_ptr<MockBidderWorklet> TakeBidderWorklet(
      const GURL& script_source_url) {
    auto it = bidder_worklets_.find(script_source_url);
    if (it == bidder_worklets_.end())
      return nullptr;
    std::unique_ptr<MockBidderWorklet> out = std::move(it->second);
    bidder_worklets_.erase(it);
    return out;
  }

  // Returns the MockSellerWorklet created for the specified script URL, if
  // there is one. If no URL is provided, and there's only one pending seller
  // worklet, returns that seller worklet.
  std::unique_ptr<MockSellerWorklet> TakeSellerWorklet(
      GURL script_source_url = GURL()) {
    if (seller_worklets_.empty())
      return nullptr;

    if (script_source_url.is_empty()) {
      CHECK_EQ(1u, seller_worklets_.size());
      script_source_url = seller_worklets_.begin()->first;
    }

    auto it = seller_worklets_.find(script_source_url);
    if (it == seller_worklets_.end())
      return nullptr;
    std::unique_ptr<MockSellerWorklet> out = std::move(it->second);
    seller_worklets_.erase(it);
    return out;
  }

  void Flush() { receiver_set_.FlushForTesting(); }

 private:
  void MaybeQuitWaitForWorkletsRunLoop() {
    DCHECK(wait_for_worklets_run_loop_);
    if (waiting_for_num_bidders_ == 0 && waiting_for_num_sellers_ == 0)
      wait_for_worklets_run_loop_->Quit();
  }

  // Maps of script URLs to worklets.
  std::map<GURL, std::unique_ptr<MockBidderWorklet>> bidder_worklets_;
  std::map<GURL, std::unique_ptr<MockSellerWorklet>> seller_worklets_;

  // Used to wait for the worklets to be loaded at the start of the auction.
  std::unique_ptr<base::RunLoop> wait_for_worklets_run_loop_;
  int waiting_for_num_bidders_ = 0;
  int waiting_for_num_sellers_ = 0;

  // Used to wait for a worklet to be reloaded at the end of an auction.
  std::unique_ptr<base::RunLoop> wait_for_bidder_reload_run_loop_;
  std::unique_ptr<base::RunLoop> wait_for_seller_reload_run_loop_;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class SameProcessAuctionProcessManager : public AuctionProcessManager {
 public:
  SameProcessAuctionProcessManager() = default;
  SameProcessAuctionProcessManager(const SameProcessAuctionProcessManager&) =
      delete;
  SameProcessAuctionProcessManager& operator=(
      const SameProcessAuctionProcessManager&) = delete;
  ~SameProcessAuctionProcessManager() override = default;

  // Resume all worklets paused waiting for debugger on startup.
  void ResumeAllPaused() {
    for (const auto& svc : auction_worklet_services_) {
      for (const auto& v8_helper : svc->AuctionV8HelpersForTesting()) {
        v8_helper->v8_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](scoped_refptr<auction_worklet::AuctionV8Helper> v8_helper) {
                  v8_helper->ResumeAllForTesting();
                },
                v8_helper));
      }
    }
  }

 private:
  RenderProcessHost* LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const ProcessHandle* handle,
      const std::string& display_name) override {
    // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
    // production code. Don't bother to delete the service on pipe close,
    // though; just keep it in a vector instead.
    auction_worklet_services_.push_back(
        auction_worklet::AuctionWorkletServiceImpl::CreateForService(
            std::move(auction_worklet_service_receiver)));
    return nullptr;
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

class AuctionRunnerTest : public testing::Test,
                          public AuctionWorkletManager::Delegate,
                          public DebuggableAuctionWorkletTracker::Observer {
 protected:
  // Output of the RunAuctionCallback passed to AuctionRunner::CreateAndStart().
  struct Result {
    Result() = default;
    // Can't use default copy logic, since it contains Mojo types.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    bool manually_aborted = false;
    absl::optional<InterestGroupKey> winning_group_id;
    absl::optional<GURL> ad_url;
    std::vector<GURL> ad_component_urls;
    std::string winning_group_ad_metadata;
    std::vector<GURL> report_urls;
    std::vector<GURL> debug_loss_report_urls;
    std::vector<GURL> debug_win_report_urls;
    ReportingMetadata ad_beacon_map;
    std::map<url::Origin, PrivateAggregationRequests>
        private_aggregation_requests;
    blink::InterestGroupSet interest_groups_that_bid;
    base::flat_set<std::string> k_anon_keys_to_join;

    std::vector<std::string> errors;
  };

  explicit AuctionRunnerTest(
      bool should_enable_private_aggregation = true,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode =
          auction_worklet::mojom::KAnonymityBidMode::kNone)
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionRunnerTest::OnBadMessage, base::Unretained(this)));
    DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (should_enable_private_aggregation)
      enabled_features.push_back(content::kPrivateAggregationApi);
    else
      disabled_features.push_back(content::kPrivateAggregationApi);

    switch (kanon_mode) {
      case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
        enabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        enabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
        enabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kNone:
        disabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~AuctionRunnerTest() override {
    DebuggableAuctionWorkletTracker::GetInstance()->RemoveObserver(this);

    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    // Give off-thread things a chance to delete.
    task_environment_.RunUntilIdle();
  }

  void OnBadMessage(const std::string& reason) {
    // No test expects multiple bad messages at a time
    EXPECT_EQ(std::string(), bad_message_);
    // Empty bad messages aren't expected. This check allows an empty
    // `bad_message_` field to mean no bad message, avoiding using an optional,
    // which has less helpful output on EXPECT failures.
    EXPECT_FALSE(reason.empty());

    bad_message_ = reason;
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

  blink::AuctionConfig::MaybePromiseJson MakeSellerSignals(
      bool use_promise,
      const GURL& seller_decision_logic_url) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseJson::FromPromise();
    } else {
      return blink::AuctionConfig::MaybePromiseJson::FromJson(
          base::StringPrintf(R"({"url": "%s"})",
                             seller_decision_logic_url.spec().c_str()));
    }
  }

  blink::AuctionConfig::MaybePromiseJson MakeAuctionSignals(
      bool use_promise,
      url::Origin seller) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseJson::FromPromise();
    } else {
      return blink::AuctionConfig::MaybePromiseJson::FromJson(
          base::StringPrintf(R"("auctionSignalsFor %s")",
                             seller.Serialize().c_str()));
    }
  }

  // Helper to create an auction config with the specified values.
  blink::AuctionConfig CreateAuctionConfig(
      const GURL& seller_decision_logic_url,
      absl::optional<std::vector<url::Origin>> buyers) {
    blink::AuctionConfig auction_config;
    auction_config.seller = url::Origin::Create(seller_decision_logic_url);
    auction_config.decision_logic_url = seller_decision_logic_url;

    auction_config.non_shared_params.interest_group_buyers = std::move(buyers);

    auction_config.non_shared_params.seller_signals = MakeSellerSignals(
        use_promise_for_seller_signals_, seller_decision_logic_url);
    auction_config.non_shared_params.seller_timeout = base::Milliseconds(1000);

    base::flat_map<url::Origin, std::string> per_buyer_signals;
    // Use a combination of bidder and seller values, so can make sure bidders
    // get the value from the correct seller script. Also append a fixed string,
    // as a defense against pulling the right values from the wrong places.
    per_buyer_signals[kBidder1] = base::StringPrintf(
        R"({"%sSignals": "%sSignals"})",
        auction_config.seller.Serialize().c_str(), kBidder1Name.c_str());
    per_buyer_signals[kBidder2] = base::StringPrintf(
        R"({"%sSignals": "%sSignals"})",
        auction_config.seller.Serialize().c_str(), kBidder2Name.c_str());
    auction_config.non_shared_params.per_buyer_signals =
        std::move(per_buyer_signals);

    base::flat_map<url::Origin, base::TimeDelta> per_buyer_timeouts;
    // Any per buyer timeout higher than 500 ms will be clamped to 500 ms by the
    // AuctionRunner.
    per_buyer_timeouts[kBidder1] = base::Milliseconds(1000);
    auction_config.non_shared_params.per_buyer_timeouts =
        std::move(per_buyer_timeouts);
    auction_config.non_shared_params.all_buyers_timeout =
        base::Milliseconds(150);

    auction_config.non_shared_params.auction_signals = MakeAuctionSignals(
        use_promise_for_auction_signals_, auction_config.seller);

    auction_config.seller_experiment_group_id = seller_experiment_group_id_;
    auction_config.all_buyer_experiment_group_id =
        all_buyer_experiment_group_id_;

    for (const auto& kv : per_buyer_experiment_group_id_) {
      auction_config.per_buyer_experiment_group_ids[kv.first] = kv.second;
    }

    auction_config.non_shared_params.all_buyers_group_limit =
        all_buyers_group_limit_;
    auction_config.non_shared_params.all_buyers_priority_signals =
        all_buyers_priority_signals_;

    return auction_config;
  }

  // Starts an auction without waiting for it to complete. Useful when using
  // MockAuctionProcessManager.
  //
  // `bidders` are added to a new InterestGroupManager before running the
  // auction. The times of their previous wins are ignored, as the
  // InterestGroupManager automatically attaches the current time, though their
  // wins will be added in order, with chronologically increasing times within
  // each InterestGroup.
  void StartAuction(const GURL& seller_decision_logic_url,
                    const std::vector<StorageInterestGroup>& bidders) {
    auction_complete_ = false;

    auto auction_config =
        CreateAuctionConfig(seller_decision_logic_url, interest_group_buyers_);

    auction_config.trusted_scoring_signals_url = trusted_scoring_signals_url_;

    for (const auto& component_auction : component_auctions_) {
      auction_config.non_shared_params.component_auctions.push_back(
          component_auction);
    }

    interest_group_manager_ = std::make_unique<InterestGroupManagerImpl>(
        base::FilePath(), /*in_memory=*/true,
        InterestGroupManagerImpl::ProcessMode::kDedicated,
        /*url_loader_factory=*/nullptr,
        /*k_anonymity_service=*/nullptr);
    if (!auction_process_manager_) {
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();
    }
    auction_worklet_manager_ = std::make_unique<AuctionWorkletManager>(
        auction_process_manager_.get(), top_frame_origin_, frame_origin_, this);
    interest_group_manager_->set_auction_process_manager_for_testing(
        std::move(auction_process_manager_));

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Add previous wins and bids to the interest group manager.
    for (auto& bidder : bidders) {
      for (int i = 0; i < bidder.bidding_browser_signals->join_count; i++) {
        interest_group_manager_->JoinInterestGroup(
            bidder.interest_group, bidder.joining_origin.GetURL());
      }
      for (int i = 0; i < bidder.bidding_browser_signals->bid_count; i++) {
        interest_group_manager_->RecordInterestGroupBids(
            {blink::InterestGroupKey(bidder.interest_group.owner,
                                     bidder.interest_group.name)});
      }
      for (const auto& prev_win : bidder.bidding_browser_signals->prev_wins) {
        interest_group_manager_->RecordInterestGroupWin(
            InterestGroupKey(bidder.interest_group.owner,
                             bidder.interest_group.name),
            prev_win->ad_json);
        // Add some time between interest group wins, so that they'll be added
        // to the database in the order they appear. Their times will *not*
        // match those in `prev_wins`.
        task_environment_.FastForwardBy(base::Seconds(1));
      }

      for (const auto& kanon_data : bidder.bidding_ads_kanon)
        interest_group_manager_->UpdateKAnonymity(kanon_data);
    }

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    abortable_ad_auction_.reset();
    auction_runner_ = AuctionRunner::CreateAndStart(
        auction_worklet_manager_.get(), interest_group_manager_.get(),
        std::move(auction_config), /*client_security_state=*/nullptr,
        IsInterestGroupApiAllowedCallback(),
        abortable_ad_auction_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  const Result& RunAuctionAndWait(const GURL& seller_decision_logic_url,
                                  std::vector<StorageInterestGroup> bidders) {
    StartAuction(seller_decision_logic_url, std::move(bidders));
    auction_run_loop_->Run();
    return result_;
  }

  void OnAuctionComplete(
      AuctionRunner* auction_runner,
      bool manually_aborted,
      absl::optional<InterestGroupKey> winning_group_key,
      absl::optional<GURL> render_url,
      std::vector<GURL> ad_component_urls,
      std::string winning_group_ad_metadata,
      std::vector<GURL> debug_loss_report_urls,
      std::vector<GURL> debug_win_report_urls,
      std::map<url::Origin, PrivateAggregationRequests>
          private_aggregation_requests,
      blink::InterestGroupSet interest_groups_that_bid,
      base::flat_set<std::string> k_anon_keys_to_join,
      std::vector<std::string> errors,
      std::unique_ptr<InterestGroupAuctionReporter> reporter) {
    DCHECK(auction_run_loop_);
    DCHECK(!auction_complete_);
    DCHECK_EQ(auction_runner, auction_runner_.get());

    // Delete the auction runner, which is needed to update histograms. Don't do
    // it immediately, so the Reporter is started before its destruction,
    // allowing reuse of the seller worklet, just as happens in production.
    std::unique_ptr<AuctionRunner> owned_auction_runner;
    if (!dont_reset_auction_runner_)
      owned_auction_runner = std::move(auction_runner_);

    auction_complete_ = true;
    result_.manually_aborted = manually_aborted;
    result_.winning_group_id = std::move(winning_group_key);
    result_.ad_url = std::move(render_url);
    result_.ad_component_urls = std::move(ad_component_urls);
    result_.winning_group_ad_metadata = std::move(winning_group_ad_metadata);
    result_.report_urls.clear();
    result_.errors = std::move(errors);
    result_.debug_loss_report_urls = std::move(debug_loss_report_urls);
    result_.debug_win_report_urls = std::move(debug_win_report_urls);
    result_.ad_beacon_map = ReportingMetadata();
    result_.interest_groups_that_bid = std::move(interest_groups_that_bid);
    result_.private_aggregation_requests =
        std::move(private_aggregation_requests);
    result_.k_anon_keys_to_join = std::move(k_anon_keys_to_join);

    if (!reporter) {
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_TRUE(result_.debug_win_report_urls.empty());
      auction_run_loop_->Quit();
      return;
    }

    EXPECT_TRUE(result_.winning_group_id);
    EXPECT_TRUE(result_.ad_url);
    // These are handled by the reporter, in the case an auction has a winner,
    // so they're only requested if the winning ad is used.
    EXPECT_TRUE(result_.private_aggregation_requests.empty());

    reporter_ = std::move(reporter);

    reporter_->Start(base::BindOnce(&AuctionRunnerTest::OnReportingComplete,
                                    base::Unretained(this)));
  }

  void OnReportingComplete() {
    DCHECK(reporter_);
    result_.report_urls = reporter_->TakeReportUrls();
    result_.ad_beacon_map = reporter_->TakeAdBeaconMap();
    result_.private_aggregation_requests =
        reporter_->TakePrivateAggregationRequests();
    const auto& report_errors = reporter_->errors();
    result_.errors.insert(result_.errors.end(), report_errors.begin(),
                          report_errors.end());

    reporter_.reset();
    auction_run_loop_->Quit();
  }

  // Returns the specified interest group.
  absl::optional<StorageInterestGroup> GetInterestGroup(
      const url::Origin& owner,
      const std::string& name) {
    base::RunLoop run_loop;
    absl::optional<StorageInterestGroup> out;
    interest_group_manager_->GetInterestGroup(
        {owner, name},
        base::BindLambdaForTesting(
            [&](absl::optional<StorageInterestGroup> interest_group) {
              out = std::move(interest_group);
              run_loop.Quit();
            }));
    run_loop.Run();
    return out;
  }

  StorageInterestGroup MakeInterestGroup(
      url::Origin owner,
      std::string name,
      absl::optional<GURL> bidding_url,
      absl::optional<GURL> trusted_bidding_signals_url,
      std::vector<std::string> trusted_bidding_signals_keys,
      absl::optional<GURL> ad_url,
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt) {
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    // Give only kBidder1 an InterestGroupAd ad with non-empty metadata, to
    // better test the `ad_metadata` output.
    if (ad_url) {
      ads.emplace();
      if (owner == kBidder1) {
        ads->emplace_back(
            blink::InterestGroup::Ad(*ad_url, R"({"ads": true})"));
      } else {
        ads->emplace_back(blink::InterestGroup::Ad(*ad_url, absl::nullopt));
      }
    }

    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    if (ad_component_urls) {
      ad_components.emplace();
      for (const GURL& ad_component_url : *ad_component_urls)
        ad_components->emplace_back(
            blink::InterestGroup::Ad(ad_component_url, absl::nullopt));
    }

    // Create fake previous wins. The time of these wins is ignored, since the
    // InterestGroupManager attaches the current time when logging a win.
    std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": 0})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -1})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -2})"));

    StorageInterestGroup storage_group;
    storage_group.interest_group = blink::InterestGroup(
        base::Time::Max(), std::move(owner), std::move(name), /*priority=*/1.0,
        /*enable_bidding_signals_prioritization=*/false,
        /*priority_vector=*/absl::nullopt,
        /*priority_signals_overrides=*/absl::nullopt,
        /*seller_capabilities=*/absl::nullopt,
        /*all_sellers_capabilities=*/
        {},
        /*execution_mode=*/
        blink::InterestGroup::ExecutionMode::kCompatibilityMode,
        std::move(bidding_url),
        /*bidding_wasm_helper_url=*/absl::nullopt,
        /*update_url=*/absl::nullopt, std::move(trusted_bidding_signals_url),
        std::move(trusted_bidding_signals_keys), absl::nullopt, std::move(ads),
        std::move(ad_components));
    storage_group.bidding_browser_signals =
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins));
    storage_group.joining_origin = storage_group.interest_group.owner;
    return storage_group;
  }

  void StartStandardAuction() {
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, GURL("https://ad1.com"),
        std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                          GURL("https://ad1.com-component2.com")}));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
        {"l1", "l2"}, GURL("https://ad2.com"),
        std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                          GURL("https://ad2.com-component2.com")}));

    StartAuction(kSellerUrl, std::move(bidders));
  }

  const Result& RunStandardAuction() {
    StartStandardAuction();
    auction_run_loop_->Run();
    return result_;
  }

  // Starts the standard auction with the mock worklet service, and waits for
  // the service to receive the worklet construction calls.
  void StartStandardAuctionWithMockService() {
    UseMockWorkletService();
    StartStandardAuction();
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/2, /*num_sellers=*/1 + component_auctions_.size());
  }

  // AuctionWorkletManager::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {}
  RenderFrameHostImpl* GetFrame() override { return nullptr; }
  scoped_refptr<SiteInstance> GetFrameSiteInstance() override {
    return scoped_refptr<SiteInstance>();
  }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return network::mojom::ClientSecurityState::New();
  }

  // DebuggableAuctionWorkletTracker::Observer implementation
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override {
    should_pause_on_start = (worklet->url() == pause_worklet_url_);
    observer_log_.push_back(base::StrCat({"Create ", worklet->url().spec()}));
    title_log_.push_back(worklet->Title());
  }

  void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) override {
    observer_log_.push_back(base::StrCat({"Destroy ", worklet->url().spec()}));
  }

  // Gets script URLs of currently live DebuggableAuctionWorklet.
  std::vector<std::string> LiveDebuggables() {
    std::vector<std::string> result;
    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      result.push_back(debuggable->url().spec());
    }
    return result;
  }

  // Enables use of a mock AuctionProcessManager when the next auction is run.
  void UseMockWorkletService() {
    auto mock_auction_process_manager =
        std::make_unique<MockAuctionProcessManager>();
    mock_auction_process_manager_ = mock_auction_process_manager.get();
    auction_process_manager_ = std::move(mock_auction_process_manager);
  }

  // Check histogram values. If `expected_interest_groups` or `expected_owners`
  // is null, expect the auction to be aborted before the corresponding
  // histograms are recorded.
  void CheckHistograms(InterestGroupAuction::AuctionResult expected_result,
                       absl::optional<int> expected_interest_groups,
                       absl::optional<int> expected_owners,
                       absl::optional<int> expected_sellers) {
    histogram_tester_->ExpectUniqueSample("Ads.InterestGroup.Auction.Result",
                                          expected_result, 1);

    if (expected_interest_groups.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumInterestGroups",
          *expected_interest_groups, 1);
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumInterestGroups", 0);
    }

    if (expected_owners.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          *expected_owners, 1);
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups", 0);
    }

    if (expected_sellers.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumSellersWithBidders", *expected_sellers,
          1);
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumSellersWithBidders", 0);
    }

    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AbortTime",
        expected_result == InterestGroupAuction::AuctionResult::kAborted);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
        expected_result == InterestGroupAuction::AuctionResult::kNoBids ||
            expected_result ==
                InterestGroupAuction::AuctionResult::kAllBidsRejected);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
        expected_result == InterestGroupAuction::AuctionResult::kSuccess);
  }

  AuctionRunner::IsInterestGroupApiAllowedCallback
  IsInterestGroupApiAllowedCallback() {
    return base::BindRepeating(&AuctionRunnerTest::IsInterestGroupAPIAllowed,
                               base::Unretained(this));
  }

  bool IsInterestGroupAPIAllowed(ContentBrowserClient::InterestGroupApiOperation
                                     interest_group_api_operation,
                                 const url::Origin& origin) {
    if (interest_group_api_operation ==
        ContentBrowserClient::InterestGroupApiOperation::kSell) {
      return disallowed_sellers_.find(origin) == disallowed_sellers_.end();
    }
    if (interest_group_api_operation ==
        ContentBrowserClient::InterestGroupApiOperation::kUpdate) {
      // Force the auction runner to not issue post-auction interest group
      // updates in this test environment; these are tested in other test
      // environments.
      return false;
    }
    DCHECK_EQ(ContentBrowserClient::InterestGroupApiOperation::kBuy,
              interest_group_api_operation);
    return disallowed_buyers_.find(origin) == disallowed_buyers_.end();
  }

  // Creates an auction with 1-2 component sellers and 2 bidders, and sets up
  // `url_loader_factory_` to provide the standard responses needed to run the
  // auction. `bidder1_seller` and `bidder2_seller` identify the seller whose
  // auction each bidder is in, and must be one of kSeller, kComponentSeller1,
  // and kComponentSeller2. kComponentSeller1 is always added to the auction,
  // kComponentSeller2 is only added to the auction if one of the bidders uses
  // it as a seller.
  void SetUpComponentAuctionAndResponses(
      const url::Origin& bidder1_seller,
      const url::Origin& bidder2_seller,
      bool bid_from_component_auction_wins,
      bool report_post_auction_signals = false) {
    interest_group_buyers_.emplace();
    std::vector<url::Origin> component1_buyers;
    std::vector<url::Origin> component2_buyers;

    if (bidder1_seller == kSeller) {
      interest_group_buyers_->push_back(kBidder1);
    } else if (bidder1_seller == kComponentSeller1) {
      component1_buyers.push_back(kBidder1);
    } else if (bidder1_seller == kComponentSeller2) {
      component2_buyers.push_back(kBidder1);
    } else {
      NOTREACHED();
    }

    if (bidder2_seller == kSeller) {
      interest_group_buyers_->push_back(kBidder2);
    } else if (bidder2_seller == kComponentSeller1) {
      component1_buyers.push_back(kBidder2);
    } else if (bidder2_seller == kComponentSeller2) {
      component2_buyers.push_back(kBidder2);
    } else {
      NOTREACHED();
    }

    component_auctions_.emplace_back(CreateAuctionConfig(
        kComponentSeller1Url, std::move(component1_buyers)));
    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kComponentSeller1Url,
        MakeDecisionScript(
            kComponentSeller1Url,
            /*send_report_url=*/GURL("https://component1-report.test/"),
            /*bid_from_component_auction_wins=*/false,
            /*report_post_auction_signals=*/report_post_auction_signals));

    if (!component2_buyers.empty()) {
      component_auctions_.emplace_back(CreateAuctionConfig(
          kComponentSeller2Url, std::move(component2_buyers)));
      auction_worklet::AddJavascriptResponse(
          &url_loader_factory_, kComponentSeller2Url,
          MakeDecisionScript(
              kComponentSeller2Url,
              /*send_report_url=*/GURL("https://component2-report.test/"),
              /*bid_from_component_auction_wins=*/false,
              /*report_post_auction_signals=*/report_post_auction_signals));
    }

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder1Url,
        MakeBidScript(bidder1_seller, "1", "https://ad1.com/",
                      /*num_ad_components=*/2, kBidder1, kBidder1Name,
                      /*has_signals=*/true, "k1", "a",
                      report_post_auction_signals));
    auction_worklet::AddBidderJsonResponse(
        &url_loader_factory_,
        GURL(kBidder1TrustedSignalsUrl.spec() +
             "?hostname=publisher1.com&keys=k1,k2"
             "&interestGroupNames=Ad+Platform"),
        kBidder1SignalsJson);

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder2Url,
        MakeBidScript(bidder2_seller, "2", "https://ad2.com/",
                      /*num_ad_components=*/2, kBidder2, kBidder2Name,
                      /*has_signals=*/true, "l2", "b",
                      report_post_auction_signals));
    auction_worklet::AddBidderJsonResponse(
        &url_loader_factory_,
        GURL(kBidder2TrustedSignalsUrl.spec() +
             "?hostname=publisher1.com&keys=l1,l2"
             "&interestGroupNames=Another+Ad+Thing"),
        kBidder2SignalsJson);

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kSellerUrl,
        MakeDecisionScript(
            kSellerUrl,
            /*send_report_url=*/GURL("https://reporting.example.com"),
            bid_from_component_auction_wins, report_post_auction_signals));
  }

  bool use_promise_for_seller_signals_ = false;
  bool use_promise_for_auction_signals_ = false;
  absl::optional<uint16_t> seller_experiment_group_id_;
  absl::optional<uint16_t> all_buyer_experiment_group_id_;
  std::map<url::Origin, uint16_t> per_buyer_experiment_group_id_;
  uint16_t all_buyers_group_limit_ = std::numeric_limits<std::uint16_t>::max();
  absl::optional<base::flat_map<std::string, double>>
      all_buyers_priority_signals_;

  const url::Origin top_frame_origin_ =
      url::Origin::Create(GURL("https://publisher1.com"));
  const url::Origin frame_origin_ =
      url::Origin::Create(GURL("https://frame.origin.test"));
  const GURL kSellerUrl{"https://adstuff.publisher1.com/auction.js"};
  const url::Origin kSeller = url::Origin::Create(kSellerUrl);
  absl::optional<GURL> trusted_scoring_signals_url_;

  const GURL kComponentSeller1Url{"https://component.seller1.test/foo.js"};
  const url::Origin kComponentSeller1 =
      url::Origin::Create(kComponentSeller1Url);
  const GURL kComponentSeller2Url{"https://component.seller2.test/bar.js"};
  const url::Origin kComponentSeller2 =
      url::Origin::Create(kComponentSeller2Url);

  const GURL kBidder1Url{"https://adplatform.com/offers.js"};
  const url::Origin kBidder1 = url::Origin::Create(kBidder1Url);
  const InterestGroupKey kBidder1Key{kBidder1, kBidder1Name};
  const GURL kBidder1TrustedSignalsUrl{"https://adplatform.com/signals1"};

  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 = url::Origin::Create(kBidder2Url);
  const std::string kBidder2Name{"Another Ad Thing"};
  const InterestGroupKey kBidder2Key{kBidder2, kBidder2Name};
  const GURL kBidder2TrustedSignalsUrl{"https://anotheradthing.com/signals2"};

  absl::optional<std::vector<url::Origin>> interest_group_buyers_ = {
      {kBidder1, kBidder2}};

  std::vector<blink::AuctionConfig> component_auctions_;

  // Origins which are not allowed to take part in auctions, as the
  // corresponding participant types.
  std::set<url::Origin> disallowed_sellers_;
  std::set<url::Origin> disallowed_buyers_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<AuctionWorkletManager> auction_worklet_manager_;

  // This is used (and consumed) when starting an auction, if non-null. Allows
  // either using a MockAuctionProcessManager instead of a
  // SameProcessAuctionProcessManager, or using a SameProcessAuctionProcessManager
  // that has already vended processes. If nullptr, a new
  // SameProcessAuctionProcessManager() is created when an auction is started.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Set by UseMockWorkletService(). Non-owning reference to the
  // AuctionProcessManager that will be / has been passed to the
  // InterestGroupManager.
  raw_ptr<MockAuctionProcessManager> mock_auction_process_manager_ = nullptr;

  // The InterestGroupManager is recreated and repopulated for each auction.
  std::unique_ptr<InterestGroupManagerImpl> interest_group_manager_;

  std::unique_ptr<AuctionRunner> auction_runner_;
  std::unique_ptr<InterestGroupAuctionReporter> reporter_;
  bool dont_reset_auction_runner_ = false;
  // This should be inspected using TakeBadMessage(), which also clears it.
  std::string bad_message_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::vector<std::string> observer_log_;
  std::vector<std::string> title_log_;

  // Can be used to interrupt currently running auction.
  mojo::Remote<blink::mojom::AbortableAdAuction> abortable_ad_auction_;

  // Which worklet to pause, if any.
  GURL pause_worklet_url_;
};

// Runs an auction with an empty buyers field.
TEST_F(AuctionRunnerTest, NullBuyers) {
  interest_group_buyers_->clear();
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);
}

// Runs a component auction with all buyers fields null.
TEST_F(AuctionRunnerTest, ComponentAuctionNullBuyers) {
  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/absl::nullopt));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);
}

// Runs an auction with an empty buyers field.
TEST_F(AuctionRunnerTest, EmptyBuyers) {
  interest_group_buyers_->clear();
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);
}

// Runs a component auction with all buyers fields empty.
TEST_F(AuctionRunnerTest, ComponentAuctionEmptyBuyers) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/{}));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);
}

// Runs the standard auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, NoInterestGroups) {
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0,
                  /*expected_sellers=*/0);
}

// Runs a component auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, ComponentAuctionNoInterestGroups) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0,
                  /*expected_sellers=*/0);
}

// Runs an standard auction, but with an interest group that does not list any
// ads.
TEST_F(AuctionRunnerTest, OneInterestGroupNoAds) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, /*ad_url=*/absl::nullopt));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0,
                  /*expected_sellers=*/0);
}

// Runs an auction with one component that has a buyer with an interest group,
// but that group has no ads.
TEST_F(AuctionRunnerTest, ComponentAuctionOneInterestGroupNoAds) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, /*ad_url=*/absl::nullopt));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0,
                  /*expected_sellers=*/0);
}

// Runs an standard auction, but with an interest group that does not list a
// bidding script.
TEST_F(AuctionRunnerTest, OneInterestGroupNoBidScript) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, /*bidding_url=*/absl::nullopt,
      kBidder1TrustedSignalsUrl, {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0,
                  /*expected_sellers=*/0);
}

// Runs the standard auction, but with only adding one of the two standard
// interest groups to the manager.
TEST_F(AuctionRunnerTest, OneInterestGroup) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=0&bid=1"),
          ReportWinUrl(/*bid=*/1, /*highest_scoring_other_bid=*/0,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/1);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction specifying buyer and seller experiment IDs.
TEST_F(AuctionRunnerTest, ExperimentId) {
  trusted_scoring_signals_url_ =
      GURL("https://adstuff.publisher1.com/seller_signals");
  seller_experiment_group_id_ = 498u;
  all_buyer_experiment_group_id_ = 940u;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"
           "&experimentGroupId=940"),
      kBidder1SignalsJson);
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com&renderUrls=https%3A%2F%2Fad1.com%2F" +
           "&experimentGroupId=498"),
      R"({"renderUrls":{"https://ad1.com/":"accept",
          "https://ad2.com/":"reject"}}
       )");

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
}

// An auction specifying a per-buyer experiment ID as well as fallback all-buyer
// experiment id.
TEST_F(AuctionRunnerTest, ExperimentIdPerBuyer) {
  all_buyer_experiment_group_id_ = 940u;
  per_buyer_experiment_group_id_[kBidder2] = 93u;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"
           "&experimentGroupId=940"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"
           "&experimentGroupId=93"),
      kBidder2SignalsJson);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
      {"l1", "l2"}, GURL("https://ad2.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
}

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(
      res.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=1&bid=2"),
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      res.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));

  EXPECT_THAT(
      res.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://anotheradthing.com/bids.js"));
  EXPECT_THAT(
      title_log_,
      testing::UnorderedElementsAre(
          "FLEDGE seller worklet for https://adstuff.publisher1.com/auction.js",
          "FLEDGE bidder worklet for https://adplatform.com/offers.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js"));
}

TEST_F(AuctionRunnerTest, BasicDebug) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name, /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name, /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  for (const GURL& debug_url : {kBidder1Url, kBidder2Url, kSellerUrl}) {
    SCOPED_TRACE(debug_url);
    pause_worklet_url_ = debug_url;

    // Seller breakpoint is expected to hit twice.
    int expected_hits = (debug_url == kSellerUrl ? 2 : 1);

    StartStandardAuction();
    task_environment_.RunUntilIdle();

    bool found = false;
    mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;

    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      if (debuggable->url() == debug_url) {
        found = true;
        debuggable->ConnectDevToolsAgent(
            agent.BindNewEndpointAndPassReceiver());
      }
    }
    ASSERT_TRUE(found);

    TestDevToolsAgentClient debug(std::move(agent), "S1",
                                  /*use_binary_protocol=*/true);
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
        R"({"id":1,"method":"Runtime.enable","params":{}})");
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
        R"({"id":2,"method":"Debugger.enable","params":{}})");

    // Set a breakpoint, and let the worklet run.
    const char kBreakpointTemplate[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 11,
          "url": "%s",
          "columnNumber": 0,
          "condition": ""
        }})";

    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 3,
        "Debugger.setBreakpointByUrl",
        base::StringPrintf(kBreakpointTemplate, debug_url.spec().c_str()));
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 4,
        "Runtime.runIfWaitingForDebugger",
        R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

    // Should get breakpoint hit eventually.
    for (int hit = 0; hit < expected_hits; ++hit) {
      TestDevToolsAgentClient::Event breakpoint_hit =
          debug.WaitForMethodNotification("Debugger.paused");

      ASSERT_TRUE(breakpoint_hit.value.is_dict());
      base::Value::List* hit_breakpoints =
          breakpoint_hit.value.GetDict().FindListByDottedPath(
              "params.hitBreakpoints");
      ASSERT_TRUE(hit_breakpoints);
      // This is LE and not EQ to work around
      // https://bugs.chromium.org/p/v8/issues/detail?id=12586
      ASSERT_LE(1u, hit_breakpoints->size());
      ASSERT_TRUE((*hit_breakpoints)[0].is_string());
      EXPECT_EQ(base::StringPrintf("1:11:0:%s", debug_url.spec().c_str()),
                (*hit_breakpoints)[0].GetString());

      // Just resume execution.
      debug.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
          R"({"id":6,"method":"Debugger.resume","params":{}})");
    }

    // In the case bidder 2 wins the auction, the script will be reloaded, and
    // the second time it's loaded the worklet will also start in the paused
    // state. Resume it, so the test doesn't hang.
    if (debug_url == kBidder2Url) {
      task_environment_.RunUntilIdle();
      found = false;
      mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent2;
      for (DebuggableAuctionWorklet* debuggable :
           DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
        if (debuggable->url() == debug_url) {
          found = true;
          debuggable->ConnectDevToolsAgent(
              agent2.BindNewEndpointAndPassReceiver());
        }
      }
      ASSERT_TRUE(found);

      TestDevToolsAgentClient debug2(std::move(agent2), "S2",
                                     /*use_binary_protocol=*/true);

      debug2.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 1,
          "Runtime.runIfWaitingForDebugger",
          R"({"id":1,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
    }

    // Let it finish --- result should as in Basic test since this didn't
    // actually change anything.
    auction_run_loop_->Run();
    EXPECT_EQ(kBidder2Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAre(
                    GURL("https://reporting.example.com/2"),
                    GURL("https://buyer-reporting.example.com/2")));
    EXPECT_THAT(
        result_.ad_beacon_map.metadata,
        testing::UnorderedElementsAre(
            testing::Pair(
                ReportingDestination::kSeller,
                testing::ElementsAre(testing::Pair(
                    "click", GURL("https://reporting.example.com/4")))),
            testing::Pair(
                ReportingDestination::kBuyer,
                testing::ElementsAre(testing::Pair(
                    "click", GURL("https://buyer-reporting.example.com/4"))))));
    EXPECT_THAT(
        result_.private_aggregation_requests,
        testing::UnorderedElementsAre(
            testing::Pair(kBidder1,
                          ElementsAreRequests(
                              kExpectedGenerateBidPrivateAggregationRequest)),
            testing::Pair(kBidder2,
                          ElementsAreRequests(
                              kExpectedGenerateBidPrivateAggregationRequest,
                              kExpectedReportWinPrivateAggregationRequest)),
            testing::Pair(
                kSeller, ElementsAreRequests(
                             kExpectedScoreAdPrivateAggregationRequest,
                             kExpectedScoreAdPrivateAggregationRequest,
                             kExpectedReportResultPrivateAggregationRequest))));
  }
}

TEST_F(AuctionRunnerTest, PauseBidder) {
  pause_worklet_url_ = kBidder2Url;

  // Save a pointer to SameProcessAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameProcessAuctionProcessManager>();
  SameProcessAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for script 2 until ready to resume.
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  process_manager_impl->ResumeAllPaused();

  // Need to resume a second time, when the script is re-loaded to run
  // ReportWin().
  task_environment_.RunUntilIdle();
  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
}

TEST_F(AuctionRunnerTest, PauseSeller) {
  pause_worklet_url_ = kSellerUrl;

  // Save a pointer to SameProcessAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameProcessAuctionProcessManager>();
  SameProcessAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for seller until ready to resume.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
}

// A component auction with two successful bids from different components.
TEST_F(AuctionRunnerTest, ComponentAuction) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=1&bid=2"),
          GURL(
              "https://component2-report.test/?highestScoringOtherBid=0&bid=2"),
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/0,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component2-report.test/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(kExpectedScoreAdPrivateAggregationRequest)),
          testing::Pair(kComponentSeller2,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/3);
}

// A component auction with two buyers in the top-level auction. The component
// seller has no buyers.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersHaveNoBuyers) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kSeller,
                                    /*bidder2_seller=*/kSeller,
                                    /*bid_from_component_auction_wins=*/false,
                                    /*report_post_auction_signals*/ true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=1&bid=2"),
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test a component auction where the top level seller rejects all bids. The
// only bids come from a component auction. This should fail with
// kAllBidsRejected instead of kNoBids.
TEST_F(AuctionRunnerTest, ComponentAuctionTopSellerRejectsBids) {
  // Run a standard component auction, but replace the default seller script
  // with one that rejects bids.
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/false);

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         R"(
    function scoreAd() {
      return {desirability: 0,
              allowComponentAuction: true};
    }
)");

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(absl::nullopt, result_.ad_url);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kAllBidsRejected,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// A component auction with one component. Both the top-level and component
// auction have one buyer. The top-level seller worklet has the winning buyer.
TEST_F(AuctionRunnerTest, ComponentAuctionTopLevelSellerBidWins) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kSeller,
                                    /*bid_from_component_auction_wins=*/false,
                                    /*report_post_auction_signals*/ true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=1&bid=2"),
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(kExpectedScoreAdPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// A component auction with one component. Both the top-level and component
// auction have one buyer. The component seller worklet has the winning buyer.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellerBidWins) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kSeller,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals*/ true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=1&bid=2"),
          GURL(
              "https://component1-report.test/?highestScoringOtherBid=0&bid=2"),
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/0,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component1-report.test/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// Test case where the top-level and a component auction share the same buyer,
// which makes different bids for both auctions. Tests both the case the bid
// made in the main auction wins, and the case the bid made in the component
// auction wins.
//
// This tests that parameters are separated, that bid counts are updated
// correctly, and how histograms are updated in these cases.
TEST_F(AuctionRunnerTest, ComponentAuctionSharedBuyer) {
  const GURL kTopLevelBidUrl = GURL("https://top-level-bid.test/");
  const GURL kComponentBidUrl = GURL("https://component-bid.test/");

  // Bid script used in both auctions. The bid amount is based on the seller:
  // It bids the most in auctions run by kComponentSeller2Url, and the least in
  // auctions run by kComponentSeller1Url, so one script can handle both test
  // cases.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        if (browserSignals.seller == "https://component.seller1.test") {
          return {ad: [], bid: 1, render: "https://component-bid.test/",
                  allowComponentAuction: true};
        }
        if (browserSignals.seller == "https://component.seller2.test") {
          return {ad: [], bid: 3, render: "https://component-bid.test/",
                  allowComponentAuction: true};
        }
        return {ad: [], bid: 2, render: "https://top-level-bid.test/",
                allowComponentAuction: false};
      }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*browserSignals.bid,
      });
      privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
    }
  )";

  // Script used for both sellers. Return different desireability scores based
  // on bid and seller, to make sure correct values are plumbed through.
  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.sendHistogramReport({bucket: 5n, value: 6});
      if (auctionConfig.seller == "https://adstuff.publisher1.com")
        return {desirability: 20 + bid, allowComponentAuction: true};
      return {desirability: 10 + bid, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" +
                   browserSignals.desirability);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2*browserSignals.desirability,
      });
      privateAggregation.sendHistogramReport({bucket: 7n, value: 8});
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kComponentSeller1Url, kSellerScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kComponentSeller2Url, kSellerScript);

  //--------------------------------------
  // Case the top-level auction's bid wins
  //--------------------------------------

  interest_group_buyers_ = {{kBidder1}};
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  // Custom interest group with two ads, so both bid URLs are valid.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(
      MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                        /*trusted_bidding_signals_url=*/absl::nullopt,
                        /*trusted_bidding_signals_keys=*/{}, kTopLevelBidUrl));
  bidders[0].interest_group.ads->push_back(
      blink::InterestGroup::Ad(kComponentBidUrl, absl::nullopt));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ(kTopLevelBidUrl, result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://adstuff.publisher1.com/22"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://adstuff.publisher1.com/44")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(kExpectedScoreAdPrivateAggregationRequest))));
  // Bid count should only be incremented by 1.
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://top-level-bid.test/",)"
            R"("metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  // Currently an interest groups participating twice in an auction is counted
  // twice.
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);

  //--------------------------------------
  // Case the component auction's bid wins
  //--------------------------------------

  histogram_tester_ = std::make_unique<base::HistogramTester>();

  // Add another kComponentSeller2Url as another seller, for a total of 2
  // component sellers.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder1}}));

  // Custom interest group with two ads, so both bid URLs are valid.
  bidders = std::vector<StorageInterestGroup>();
  bidders.emplace_back(
      MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                        /*trusted_bidding_signals_url=*/absl::nullopt,
                        /*trusted_bidding_signals_keys=*/{}, kTopLevelBidUrl));
  bidders[0].interest_group.ads->push_back(
      blink::InterestGroup::Ad(kComponentBidUrl, absl::nullopt));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ(kComponentBidUrl, result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://adstuff.publisher1.com/23"),
                  GURL("https://component.seller2.test/13"),
                  GURL("https://buyer-reporting.example.com/3")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://adstuff.publisher1.com/46")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component.seller2.test/26")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/6"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(kExpectedScoreAdPrivateAggregationRequest)),
          testing::Pair(kComponentSeller2,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://component-bid.test/"})",
            result_.winning_group_ad_metadata);
  // Currently a bidder participating twice in an auction is counted as two
  // participating interest groups.
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/3, /*expected_owners=*/3,
                  /*expected_sellers=*/3);
}

// Test case where a single component auction accepts one bid and rejects
// another. This is a regression test for https://crbug.com/1321941, where a
// rejected bid from a component auction would be treated as a security error,
// and result in bidding in the component auction being aborted, and all
// previous bids being thrown out.
TEST_F(AuctionRunnerTest, ComponentAuctionAcceptsBidRejectsBid) {
  // Script used by the winning bidder. It makes the lower bid.
  const char kBidder1Script[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {bid: 1, render: interestGroup.ads[0].renderUrl,
                allowComponentAuction: true};
      }

    function reportWin() {}
  )";

  // Script used by the losing bidder. It makes the higher bid.
  const char kBidder2Script[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {bid: 2, render: interestGroup.ads[0].renderUrl,
                allowComponentAuction: true};
      }
  )";

  // Script used for both sellers. It rejects bids over 1.
  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      if (bid > 1)
        return {desirability: 0, allowComponentAuction: true};
      return {desirability: bid, allowComponentAuction: true};
    }

    function reportResult() {}
  )";

  // Set up a component auction using the normal helper function, but then
  // overwrite the scripts.
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/false);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidder1Script);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         kBidder2Script);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kComponentSeller1Url, kSellerScript);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ("https://ad1.com/", result_.ad_url);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// A component auction with one component that has two buyers. In this auction,
// the top-level auction would score kBidder2 higher (since it bids more), but
// kBidder1 wins this auction, because the component auctions use a different
// scoring function, which favors kBidder1's lower bid.
TEST_F(AuctionRunnerTest, ComponentAuctionOneComponentTwoBidders) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/?highestScoringOtherBid=0&bid=1"),
          GURL(
              "https://component1-report.test/?highestScoringOtherBid=2&bid=1"),
          ReportWinUrl(/*bid=*/1, /*highest_scoring_other_bid=*/2,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component1-report.test/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// Test the case a top-level seller returns no signals in its reportResult
// method. The default scripts return signals, so only need to individually test
// the no-value case.
TEST_F(AuctionRunnerTest, ComponentAuctionNoTopLevelReportResultSignals) {
  // Basic bid script.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        privateAggregation.sendHistogramReport({bucket: 1n, value: 2});
        return {ad: [], bid: 2, render: interestGroup.ads[0].renderUrl,
                allowComponentAuction: true};
      }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*browserSignals.bid,
      });
      privateAggregation.sendHistogramReport({bucket: 3n, value: 4});
    }
  )";

  // Component seller script that makes a report to a URL based on whether the
  // top-level seller signals are null.
  const std::string kComponentSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.sendHistogramReport({bucket: 5n, value: 6});
      return {desirability: 10, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" +
                   (browserSignals.topLevelSellerSignals === null));
      registerAdBeacon({
        "click": auctionConfig.seller + "/" +
                   (browserSignals.topLevelSellerSignals === null),
      });
      privateAggregation.sendHistogramReport({bucket: 7n, value: 8});
    }
  )";

  // Top-level seller script with a reportResult method that has no return
  // value.
  const std::string kTopLevelSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.sendHistogramReport({bucket: 5n, value: 6});
      return {desirability: 10, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" + browserSignals.bid);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2 * browserSignals.bid,
      });
      privateAggregation.sendHistogramReport({bucket: 7n, value: 8});
      // Note that there's no return value here.
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kTopLevelSellerScript);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url, kComponentSellerScript);

  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://buyer-reporting.example.com/2"),
                  GURL("https://component.seller1.test/true"),
                  GURL("https://adstuff.publisher1.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://adstuff.publisher1.com/4")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component.seller1.test/true")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/2);
}

TEST_F(AuctionRunnerTest, ComponentAuctionModifiesBid) {
  // Basic bid script.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {ad: [], bid: 2, render: interestGroup.ads[0].renderUrl,
                allowComponentAuction: true};
      }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2 * browserSignals.bid,
      });
    }
  )";

  // Component seller script that modifies the bid to 3.
  const std::string kComponentSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return {desirability: 10, allowComponentAuction: true, bid: 3};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" + browserSignals.bid +
                   "_" + browserSignals.modifiedBid);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2 * browserSignals.bid +
                   "_" + browserSignals.modifiedBid,
      });
    }
  )";

  // Top-level seller script that rejects bids that aren't 3..
  const std::string kTopLevelSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      if (bid != 3)
        return 0;
      return {desirability: 10, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" + browserSignals.bid);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2 * browserSignals.bid,
      });
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kTopLevelSellerScript);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url, kComponentSellerScript);

  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  // Basic interest group.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);
  // The reporting URLs contain the bids - the top-level seller report should
  // see the modified bid, the other worklets see the original bid.
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://buyer-reporting.example.com/2"),
                  GURL("https://component.seller1.test/2_3"),
                  GURL("https://adstuff.publisher1.com/3")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://adstuff.publisher1.com/6")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component.seller1.test/4_3")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/2);
}

// An auction in which the seller origin is not allowed to use the interest
// group API.
TEST_F(AuctionRunnerTest, DisallowedSeller) {
  disallowed_sellers_.insert(url::Origin::Create(kSellerUrl));

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSellerRejected,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which the component seller is disallowed, and the
// top-level seller has no buyers.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionSeller) {
  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  disallowed_sellers_.insert(kComponentSeller1);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which the one component seller is disallowed, but the
// other is not.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionOneSeller) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true);

  // Bidder 2 bids more, so would win the auction if component seller 2 were
  // allowed to participate.
  disallowed_sellers_.insert(kComponentSeller2);

  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component1-report.test/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/2);
}

// An auction in which the buyer origins are not allowed to use the interest
// group API.
TEST_F(AuctionRunnerTest, DisallowedBuyers) {
  disallowed_buyers_.insert(kBidder1);
  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);

  // No requests for the seller worklet URL should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Run the standard auction, but disallow one bidder from participating.
TEST_F(AuctionRunnerTest, DisallowedSingleBuyer) {
  // The lack of a bidder script 2 means that this test should hang if bidder
  // 2's script is requested.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);

  disallowed_buyers_.insert(kBidder2);
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/1);

  // No requests for bidder2's worklet URL should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which all buyers are disallowed.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionBuyers) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));

  disallowed_buyers_.insert(kBidder1);
  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt,
                  /*expected_sellers=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which a single buyer is disallowed.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionSingleBuyer) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true);

  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component1-report.test/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/2);
}

// Disallow bidders as sellers and disallow seller as bidder. Auction should
// still succeed.
TEST_F(AuctionRunnerTest, DisallowedAsOtherParticipant) {
  disallowed_sellers_.insert(kBidder1);
  disallowed_sellers_.insert(kBidder2);
  disallowed_buyers_.insert(url::Origin::Create(kSellerUrl));

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre("Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);

  // 404 is detected after the worklet is created, so there are still events
  // for it.
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction where one component seller fails to load, but the other loads, so
// the auction succeeds.
TEST_F(AuctionRunnerTest, ComponentAuctionOneSeller404) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true);
  url_loader_factory_.AddResponse(kComponentSeller2Url.spec(), "",
                                  net::HTTP_NOT_FOUND);

  RunStandardAuction();
  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://component.seller2.test/bar.js "
                  "HTTP status = 404 Not Found."));
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component1-report.test/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller1,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  // The bid send to the failing component seller worklet isn't counted,
  // regardless of whether the bid completed before the worklet failed to load.
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/3);
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));

  // The auction script doesn't make any bids.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "`generateBid` is not a function."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where no bidding scripts load successfully.
TEST_F(AuctionRunnerTest, NoBids) {
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.winning_group_id);
  EXPECT_FALSE(res.ad_url);
  EXPECT_TRUE(res.ad_component_urls.empty());
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(res.private_aggregation_requests.empty());
  EXPECT_THAT(res.interest_groups_that_bid, testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://adplatform.com/offers.js "
                  "HTTP status = 404 Not Found.",
                  "Failed to load https://anotheradthing.com/bids.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where none of the bidding scripts has a valid bidding function.
TEST_F(AuctionRunnerTest, NoBidMadeByScript) {
  // MakeAuctionScript() is a valid script that doesn't have a bidding function.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.winning_group_id);
  EXPECT_FALSE(res.ad_url);
  EXPECT_TRUE(res.ad_component_urls.empty());
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(res.private_aggregation_requests.empty());
  EXPECT_THAT(res.interest_groups_that_bid, testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  EXPECT_THAT(
      res.errors,
      testing::UnorderedElementsAre(
          "https://adplatform.com/offers.js `generateBid` is not a function.",
          "https://anotheradthing.com/bids.js `generateBid` is not a "
          "function."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 =
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a");
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         bid_script1);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  // No seller scoring function in a bid script.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         bid_script1);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.winning_group_id);
  EXPECT_FALSE(res.ad_url);
  EXPECT_TRUE(res.ad_component_urls.empty());
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function.",
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kAllBidsRejected,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where seller rejects one bid when scoring.
TEST_F(AuctionRunnerTest, SellerRejectsOne) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptReject2());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.winning_group_id);
  EXPECT_FALSE(res.ad_url);
  EXPECT_TRUE(res.ad_component_urls.empty());
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(res.private_aggregation_requests.empty());

  EXPECT_EQ(0, url_loader_factory_.NumPending());
  EXPECT_THAT(res.interest_groups_that_bid, testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSellerWorkletLoadFailed,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where bidders don't request trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                         absl::nullopt, {"k1", "k2"},
                                         GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                         absl::nullopt, {"l1", "l2"},
                                         GURL("https://ad2.com")));

  const Result& res = RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  url_loader_factory_.AddResponse(kBidder1TrustedSignalsUrl.spec() +
                                      "?hostname=publisher1.com&keys=k1,k2"
                                      "&interestGroupNames=Ad+Platform",
                                  "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2TrustedSignalsUrl.spec() +
                                      "?hostname=publisher1.com&keys=l1,l2"
                                      "&interestGroupNames=Another+Ad+Thing",
                                  "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "Failed to load "
                              "https://adplatform.com/"
                              "signals1?hostname=publisher1.com&keys=k1,k2&"
                              "interestGroupNames=Ad+Platform "
                              "HTTP status = 404 Not Found.",
                              "Failed to load "
                              "https://anotheradthing.com/"
                              "signals2?hostname=publisher1.com&keys=l1,l2"
                              "&interestGroupNames=Another+Ad+Thing "
                              "HTTP status = 404 Not Found."));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// A successful auction where seller reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportResultUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(GURL(
                                   "https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(testing::Pair(
          ReportingDestination::kBuyer,
          testing::ElementsAre(testing::Pair(
              "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// A successful auction where bidder reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportWinUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(
                                   GURL("https://reporting.example.com/2")));
  EXPECT_THAT(result_.ad_beacon_map.metadata,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kSeller,
                  testing::ElementsAre(testing::Pair(
                      "click", GURL("https://reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            // ReportWin script override doesn't send a report
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// A successful auction where neither reporting worklets sets a URL.
TEST_F(AuctionRunnerTest, NeitherReportUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            // ReportWin script override doesn't send a report
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test the case where the seller worklet provides no signals for the winner,
// since it has no reportResult() method. The winning bidder's reportWin()
// function should be passed null as `sellerSignals`, and should still be able
// to send a report.
TEST_F(AuctionRunnerTest, NoReportResult) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                  browserSignals) {
  return bid * 2;
}
                                         )");
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(GURL(
                                   "https://seller.signals.were.null.test/")));
  EXPECT_TRUE(res.ad_beacon_map.metadata.empty());
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            // ReportWin script override doesn't send a report
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(res.errors, testing::ElementsAre(base::StringPrintf(
                              "%s `reportResult` is not a function.",
                              kSellerUrl.spec().c_str())));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

TEST_F(AuctionRunnerTest, TrustedScoringSignals) {
  trusted_scoring_signals_url_ =
      GURL("https://adstuff.publisher1.com/seller_signals");

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  // scoreAd() that only accepts bids where the scoring signals of the
  // `renderUrl` is "accept".
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         std::string(R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  let signal = trustedScoringSignals.renderUrl[browserSignals.renderUrl];
  if (browserSignals.dataVersion !== 2) {
    throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
  }
  // 2 * bid is expected by the BidderWorklet ReportWin() script.
  if (signal == "accept")
    return 2 * bid;
  if (signal == "reject")
    return 0;
  throw "incorrect trustedScoringSignals";
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo("https://reporting.example.com/" + browserSignals.bid);
  registerAdBeacon({
    "click": "https://reporting.example.com/" + 2 * browserSignals.bid,
  });
  if (browserSignals.dataVersion !== 2) {
    throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
  }
  return browserSignals;
}
                                         )"));

  // Response body that only accept first bidder's bid.
  const char kTrustedScoringSignalsBody[] =
      R"({"renderUrls":{"https://ad1.com/":"accept", "https://ad2.com/":"reject"}})";

  // There may be one merged trusted scoring signals request, or two separate
  // requests.

  // Response in the case of a single merged trusted scoring signals request.
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad1.com%2F,https%3A%2F%2Fad2.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad1.com-component1.com%2F,"
           "https%3A%2F%2Fad2.com-component1.com%2F"),
      kTrustedScoringSignalsBody,
      /*data_version=*/2);

  // Responses in the case of two separate trusted scoring signals requests.
  // Extra entries in the response dictionary will be ignored, so can use the
  // same body as in the merged request case.
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad1.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad1.com-component1.com%2F"),
      kTrustedScoringSignalsBody,
      /*data_version=*/2);
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad2.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad2.com-component1.com%2F"),
      kTrustedScoringSignalsBody,
      /*data_version=*/2);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      // Overridden script functions don't send reports
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// An auction that passes auctionSignals via promises.
TEST_F(AuctionRunnerTest, PromiseAuctionSignals) {
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .json_payload());

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// An auction that passes sellerSignals and auctionSignals via promises.
TEST_F(AuctionRunnerTest, PromiseSignals) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).json_payload());
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .json_payload());

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// An auction that passes sellerSignals and auctionSignals via promises.
// Empty values are provided, which causes the validation scripts to complain.
TEST_F(AuctionRunnerTest, PromiseSignals2) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);

  auction_run_loop_->Run();

  EXPECT_FALSE(result_.winning_group_id.has_value());
  EXPECT_FALSE(result_.ad_url.has_value());
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(
                                  "https://adplatform.com/offers.js:74 "
                                  "Uncaught Error: wrong auctionSignals.",
                                  "https://anotheradthing.com/bids.js:74 "
                                  "Uncaught Error: wrong auctionSignals."));
}

TEST_F(AuctionRunnerTest, PromiseSignalsResolveAfterAbort) {
  use_promise_for_seller_signals_ = true;
  dont_reset_auction_runner_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->Abort();
  auction_run_loop_->Run();
  EXPECT_TRUE(result_.manually_aborted);

  // Feed in sellerSignals. Nothing weird should happen.
  auction_run_loop_ = std::make_unique<base::RunLoop>();
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).json_payload());
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
  EXPECT_TRUE(result_.manually_aborted);
}

TEST_F(AuctionRunnerTest, PromiseSignalsComponentAuction) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/false);
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com"),
      std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                        GURL("https://ad1.com-component2.com")}));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
      {"l1", "l2"}, GURL("https://ad2.com"),
      std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                        GURL("https://ad2.com-component2.com")}));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in the signals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).json_payload());
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .json_payload());
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  for (int component = 0; component < 2; ++component) {
    const GURL& url =
        (component == 0) ? kComponentSeller1Url : kComponentSeller2Url;
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(component),
        blink::mojom::AuctionAdConfigField::kSellerSignals,
        MakeSellerSignals(/*use_promise=*/false, url).json_payload());
    auction_run_loop_->RunUntilIdle();
    EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(component),
        blink::mojom::AuctionAdConfigField::kAuctionSignals,
        MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(url))
            .json_payload());
    if (component != 1) {
      auction_run_loop_->RunUntilIdle();
      EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
    }
  }

  auction_run_loop_->Run();

  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals with wrong component ID.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedPromiseParam", TakeBadMessage());
}

// Trying to update auctionSignals which wasn't originally passed in as a
// promise.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = false;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals, which isn't a promise in the first place.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
}

// Trying to update auctionSignals twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise2) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals twice.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
}

// Trying to update sellerSignals which wasn't originally passed in as a
// promise.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise3) {
  use_promise_for_seller_signals_ = false;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
}

// Trying to update sellerSignals twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise4) {
  use_promise_for_seller_signals_ = true;
  use_promise_for_auction_signals_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      /*ad_component_urls=*/absl::nullopt));
  StartAuction(kSellerUrl, std::move(bidders));

  // Can't complete yet.
  auction_run_loop_->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals twice.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
}

// Test the case where the ProcessManager initially prevents creating worklets,
// due to being at its process limit.
TEST_F(AuctionRunnerTest, ProcessManagerBlocksWorkletCreation) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  // For the seller worklet, it only matters if the worklet process limit has
  // been hit or not.
  for (bool seller_worklet_creation_delayed : {false, true}) {
    SCOPED_TRACE(seller_worklet_creation_delayed);

    // For bidder worklets, in addition to testing the cases with no processes
    // and at the process limit, also test the case where we're one below the
    // limit, which should serialize bidder worklet creation and execution.
    for (size_t num_used_bidder_worklet_processes :
         std::vector<size_t>{static_cast<size_t>(0),
                             AuctionProcessManager::kMaxBidderProcesses - 1,
                             AuctionProcessManager::kMaxBidderProcesses}) {
      SCOPED_TRACE(num_used_bidder_worklet_processes);

      bool bidder_worklet_creation_delayed =
          num_used_bidder_worklet_processes ==
          AuctionProcessManager::kMaxBidderProcesses;

      // Create AuctionProcessManager in advance of starting the auction so can
      // create worklets before the auction starts.
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();

      AuctionProcessManager* auction_process_manager =
          auction_process_manager_.get();

      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
      if (seller_worklet_creation_delayed) {
        // Make kMaxSellerProcesses seller worklet requests for other origins so
        // seller worklet creation will be blocked by the process limit.
        for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses;
             ++i) {
          sellers.push_back(
              std::make_unique<AuctionProcessManager::ProcessHandle>());
          url::Origin origin = url::Origin::Create(
              GURL(base::StringPrintf("https://%zu.test", i)));
          EXPECT_TRUE(auction_process_manager->RequestWorkletService(
              AuctionProcessManager::WorkletType::kSeller, origin,
              scoped_refptr<SiteInstance>(), &*sellers.back(),
              base::BindOnce(
                  []() { ADD_FAILURE() << "This should not be called"; })));
        }
      }

      // Make `num_used_bidder_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
      for (size_t i = 0; i < num_used_bidder_worklet_processes; ++i) {
        bidders.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kBidder, origin,
            scoped_refptr<SiteInstance>(), &*bidders.back(),
            base::BindOnce(
                []() { ADD_FAILURE() << "This should not be called"; })));
      }

      // If neither sellers nor bidders are at their limit, the auction should
      // complete.
      if (!seller_worklet_creation_delayed &&
          !bidder_worklet_creation_delayed) {
        RunStandardAuction();
      } else {
        // Otherwise, the auction should be blocked.
        StartStandardAuction();
        task_environment_.RunUntilIdle();

        EXPECT_EQ(
            seller_worklet_creation_delayed ? 1u : 0u,
            auction_process_manager->GetPendingSellerRequestsForTesting());
        EXPECT_EQ(
            bidder_worklet_creation_delayed ? 2u : 0u,
            auction_process_manager->GetPendingBidderRequestsForTesting());
        EXPECT_FALSE(auction_complete_);

        // Free up a seller slot, if needed.
        if (seller_worklet_creation_delayed) {
          sellers.pop_front();
          task_environment_.RunUntilIdle();
          EXPECT_EQ(
              0u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
          EXPECT_EQ(
              bidder_worklet_creation_delayed ? 2u : 0,
              auction_process_manager->GetPendingBidderRequestsForTesting());
        }

        // Free up a single bidder slot, if needed.
        if (bidder_worklet_creation_delayed) {
          EXPECT_FALSE(auction_complete_);
          bidders.pop_front();
        }

        // The auction should now be able to run to completion.
        auction_run_loop_->Run();
      }
      EXPECT_TRUE(auction_complete_);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder2Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
                result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_THAT(
          result_.ad_beacon_map.metadata,
          testing::UnorderedElementsAre(
              testing::Pair(
                  ReportingDestination::kSeller,
                  testing::ElementsAre(testing::Pair(
                      "click", GURL("https://reporting.example.com/4")))),
              testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair(
                      "click",
                      GURL("https://buyer-reporting.example.com/4"))))));
      EXPECT_THAT(
          result_.private_aggregation_requests,
          testing::UnorderedElementsAre(
              testing::Pair(kBidder1,
                            ElementsAreRequests(
                                kExpectedGenerateBidPrivateAggregationRequest)),
              testing::Pair(kBidder2,
                            ElementsAreRequests(
                                kExpectedGenerateBidPrivateAggregationRequest,
                                kExpectedReportWinPrivateAggregationRequest)),
              testing::Pair(
                  kSeller,
                  ElementsAreRequests(
                      kExpectedScoreAdPrivateAggregationRequest,
                      kExpectedScoreAdPrivateAggregationRequest,
                      kExpectedReportResultPrivateAggregationRequest))));
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2,
                      /*expected_owners=*/2, /*expected_sellers=*/1);
    }
  }
}

// Tests ComponentAuctions and their interactions with the ProcessManager
// delaying worklet creation.
TEST_F(AuctionRunnerTest, ComponentAuctionProcessManagerBlocksWorkletCreation) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true);

  // For both worklet types, in addition to testing the cases with no processes
  // and at the process limit, also test the case where we're one below the
  // limit, which should serialize worklet creation and execution.
  for (size_t num_used_seller_worklet_processes :
       {static_cast<size_t>(0), AuctionProcessManager::kMaxSellerProcesses - 1,
        AuctionProcessManager::kMaxSellerProcesses}) {
    SCOPED_TRACE(num_used_seller_worklet_processes);

    bool seller_worklet_creation_delayed =
        num_used_seller_worklet_processes ==
        AuctionProcessManager::kMaxSellerProcesses;

    for (size_t num_used_bidder_worklet_processes :
         std::vector<size_t>{static_cast<size_t>(0),
                             AuctionProcessManager::kMaxBidderProcesses - 1,
                             AuctionProcessManager::kMaxBidderProcesses}) {
      SCOPED_TRACE(num_used_bidder_worklet_processes);

      bool bidder_worklet_creation_delayed =
          num_used_bidder_worklet_processes ==
          AuctionProcessManager::kMaxBidderProcesses;

      // Create AuctionProcessManager in advance of starting the auction so can
      // create worklets before the auction starts.
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();

      AuctionProcessManager* auction_process_manager =
          auction_process_manager_.get();

      // Make `num_used_seller_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
      for (size_t i = 0; i < num_used_seller_worklet_processes; ++i) {
        sellers.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kSeller, origin,
            scoped_refptr<SiteInstance>(), &*sellers.back(),
            base::BindOnce(
                []() { ADD_FAILURE() << "This should not be called"; })));
      }

      // Make `num_used_bidder_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
      for (size_t i = 0; i < num_used_bidder_worklet_processes; ++i) {
        bidders.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kBidder, origin,
            scoped_refptr<SiteInstance>(), &*bidders.back(),
            base::BindOnce(
                []() { ADD_FAILURE() << "This should not be called"; })));
      }

      // If neither sellers nor bidders are at their limit, the auction should
      // complete.
      if (!seller_worklet_creation_delayed &&
          !bidder_worklet_creation_delayed) {
        RunStandardAuction();
      } else {
        // Otherwise, the auction should be blocked.
        StartStandardAuction();
        task_environment_.RunUntilIdle();

        if (seller_worklet_creation_delayed) {
          // In the case of `seller_worklet_creation_delayed`, only the two
          // component worklet's loads should have been queued.
          EXPECT_EQ(
              2u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        } else if (num_used_seller_worklet_processes ==
                       AuctionProcessManager::kMaxSellerProcesses - 1 &&
                   bidder_worklet_creation_delayed) {
          // IF there's only one available seller worklet process, and
          // `bidder_worklet_creation_delayed` is true, one component seller
          // should have been created, the component seller should be queued,
          // waiting on a process slot, and the top-level seller should not have
          // been requested yet, waiting on the component sellers to both be
          // loaded.
          EXPECT_EQ(
              1u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        } else {
          // Otherwise, no seller worklet requests should be pending..
          EXPECT_EQ(
              0u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        }

        EXPECT_EQ(
            bidder_worklet_creation_delayed ? 2u : 0u,
            auction_process_manager->GetPendingBidderRequestsForTesting());

        // Free up a seller slot, if needed.
        if (seller_worklet_creation_delayed) {
          sellers.pop_front();
          task_environment_.RunUntilIdle();
          if (bidder_worklet_creation_delayed) {
            // If bidder creation was also delayed, one component seller should
            // have been made, but is waiting on a bid. Creating the other
            // component seller should be queued. The main seller should be
            // blocked on loading that component seller.
            EXPECT_EQ(
                1u,
                auction_process_manager->GetPendingSellerRequestsForTesting());
            EXPECT_EQ(
                2u,
                auction_process_manager->GetPendingBidderRequestsForTesting());
          } else {
            // Otherwise, the auction should have completed.
            EXPECT_TRUE(auction_complete_);
          }
        }

        // Free up a single bidder slot, if needed.
        if (bidder_worklet_creation_delayed) {
          EXPECT_FALSE(auction_complete_);
          bidders.pop_front();
        }

        // The auction should now be able to run to completion.
        auction_run_loop_->Run();
      }
      EXPECT_TRUE(auction_complete_);

      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
                result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://component2-report.test/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_THAT(
          result_.ad_beacon_map.metadata,
          testing::UnorderedElementsAre(
              testing::Pair(
                  ReportingDestination::kSeller,
                  testing::ElementsAre(testing::Pair(
                      "click", GURL("https://reporting.example.com/4")))),
              testing::Pair(
                  ReportingDestination::kComponentSeller,
                  testing::ElementsAre(testing::Pair(
                      "click", GURL("https://component2-report.test/4")))),
              testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair(
                      "click",
                      GURL("https://buyer-reporting.example.com/4"))))));
      EXPECT_THAT(
          result_.private_aggregation_requests,
          testing::UnorderedElementsAre(
              testing::Pair(kBidder1,
                            ElementsAreRequests(
                                kExpectedGenerateBidPrivateAggregationRequest)),
              testing::Pair(kBidder2,
                            ElementsAreRequests(
                                kExpectedGenerateBidPrivateAggregationRequest,
                                kExpectedReportWinPrivateAggregationRequest)),
              testing::Pair(
                  kSeller, ElementsAreRequests(
                               kExpectedScoreAdPrivateAggregationRequest,
                               kExpectedScoreAdPrivateAggregationRequest,
                               kExpectedReportResultPrivateAggregationRequest)),
              testing::Pair(kComponentSeller1,
                            ElementsAreRequests(
                                kExpectedScoreAdPrivateAggregationRequest)),
              testing::Pair(
                  kComponentSeller2,
                  ElementsAreRequests(
                      kExpectedScoreAdPrivateAggregationRequest,
                      kExpectedReportResultPrivateAggregationRequest))));
      CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2, /*expected_owners=*/2,
                      /*expected_sellers=*/3);
    }
  }
}

// Test a seller worklet load failure while waiting on bidder worklet processes
// to be allocated. Most of the tests for global seller worklet failures at a
// particular phase use seller crashes instead of load errors (see SellerCrash
// test), but this case is simplest to test with a seller load error.
TEST_F(AuctionRunnerTest, SellerLoadErrorWhileWaitingForBidders) {
  // Create AuctionProcessManager in advance of starting the auction so can
  // create worklets before the auction starts.
  auction_process_manager_ =
      std::make_unique<SameProcessAuctionProcessManager>();

  // Make kMaxBidderProcesses bidder worklet requests for different origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      other_bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    other_bidders.push_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin,
        scoped_refptr<SiteInstance>(), &*other_bidders.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);

  RunStandardAuction();

  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSellerWorkletLoadFailed,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Tests ComponentAuction where a component seller worklet has a load error with
// a hanging bidder worklet request. The auction runs when the process manager
// only has 1 bidder and 1 seller slot, so this test makes sure that in this
// case the bidder and seller processes are freed up, so they don't potentially
// cause deadlock preventing the auction from completing.
TEST_F(AuctionRunnerTest,
       ComponentAuctionSellerWorkletLoadErrorWithPendingBidderLoad) {
  interest_group_buyers_.emplace();

  // First component seller worklet request fails. No response is returned for
  // the bidder worklet, so it hangs.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  url_loader_factory_.AddResponse(kComponentSeller1Url.spec(), "",
                                  net::HTTP_NOT_FOUND);

  // Second component worklet loads as normal, as does its bidder.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(kComponentSeller2Url, /*send_report_url=*/GURL(
                             "https://component2-report.test/")));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  // Top-level seller uses the default script.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeDecisionScript(
          kSellerUrl,
          /*send_report_url=*/GURL("https://reporting.example.com"),
          /*bid_from_component_auction_wins=*/true));

  auction_process_manager_ =
      std::make_unique<SameProcessAuctionProcessManager>();

  // Take up all but 1 of the seller worklet process slots.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses - 1; ++i) {
    sellers.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kSeller, origin,
        scoped_refptr<SiteInstance>(), &*sellers.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  // Take up but 1 of the bidder worklet process slots.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses - 1; ++i) {
    bidders.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin,
        scoped_refptr<SiteInstance>(), &*bidders.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  RunStandardAuction();

  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://component.seller1.test/foo.js HTTP "
                  "status = 404 Not Found."));
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://component2-report.test/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map.metadata,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component2-report.test/4")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder2,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest)),
          testing::Pair(kComponentSeller2,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/3);
}

// Test the case where two interest groups use the same BidderWorklet, with a
// trusted bidding signals URL. The requests should be batched. This test
// basically makes sure that SendPendingSignalsRequests() is only invoked on the
// BidderWorklet after both GenerateBid() calls have been invoked.
TEST_F(AuctionRunnerTest, ReusedBidderWorkletBatchesSignalsRequests) {
  // Bidding script used by both interest groups. Since the default bid script
  // checks the interest group name, and this test uses two interest groups with
  // the same bidder script, have to use a different script for this test.
  //
  // This script uses trusted bidding signals and the interest group name to
  // select a winner, to make sure the trusted bidding signals makes it to the
  // bidder.
  const char kBidderScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      if (browserSignals.dataVersion !== 4) {
       throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      }
      return {
        ad: 0,
        bid: trustedBiddingSignals['key' + interestGroup.name],
        render: interestGroup.ads[0].renderUrl
      };
    }

    // Prevent an error about this method not existing.
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
      if (browserSignals.dataVersion !== 4) {
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      }
    }
  )";

  // Need to use a different seller script as well, due to the validation logic
  // in the default one being dependent on the details of the default bidder
  // script.
  const char kSellerScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      return 2 * bid;
    }

    // Prevent an error about this method not existing.
    function reportResult() {}
  )";

  // Two interest groups with all of the same URLs. They vary only in name,
  // render URL, and bidding signals key.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"0", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{"key0"}, GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{"key1"}, GURL("https://ad2.com")));

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidderScript);

  // Trusted signals response for the single expected request. Interest group
  // "0" bids 2, interest group "1" bids 1.
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=key0,key1&interestGroupNames=0,1"),
      R"({"key0":2, "key1": 1})", 4);

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_TRUE(auction_complete_);

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "0"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
}

TEST_F(AuctionRunnerTest, AllBiddersCrashBeforeBidding) {
  StartStandardAuctionWithMockService();
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  EXPECT_FALSE(auction_complete_);

  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js"));

  EXPECT_THAT(LiveDebuggables(),
              testing::UnorderedElementsAre(
                  "https://adplatform.com/offers.js",
                  "https://anotheradthing.com/bids.js",
                  "https://adstuff.publisher1.com/auction.js"));

  bidder1_worklet.reset();
  bidder2_worklet.reset();

  task_environment_.RunUntilIdle();

  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js"));

  EXPECT_THAT(LiveDebuggables(), testing::ElementsAre());

  auction_run_loop_->Run();

  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder1Url.spec().c_str()),
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder2Url.spec().c_str())));
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_EQ("", result_.winning_group_ad_metadata);

  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test the case a single bidder worklet crashes before bidding. The auction
// should continue, without that bidder's bid.
TEST_F(AuctionRunnerTest, BidderCrashBeforeBidding) {
  for (bool other_bidder_finishes_first : {false, true}) {
    SCOPED_TRACE(other_bidder_finishes_first);

    observer_log_.clear();
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    ASSERT_FALSE(auction_complete_);
    if (other_bidder_finishes_first) {
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      // The bidder pipe should be closed after it bids.
      EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
      bidder2_worklet.reset();
    }
    mock_auction_process_manager_->Flush();

    ASSERT_FALSE(auction_complete_);

    // Close Bidder1's pipe.
    bidder1_worklet.reset();
    // Can't flush the closed pipe without reaching into AuctionRunner, so use
    // RunUntilIdle() instead.
    task_environment_.RunUntilIdle();

    if (!other_bidder_finishes_first) {
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      // The bidder pipe should be closed after it bids.
      EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
      bidder2_worklet.reset();
    }
    mock_auction_process_manager_->Flush();

    EXPECT_THAT(observer_log_,
                testing::UnorderedElementsAre(
                    "Create https://adstuff.publisher1.com/auction.js",
                    "Create https://adplatform.com/offers.js",
                    "Create https://anotheradthing.com/bids.js",
                    "Destroy https://adplatform.com/offers.js",
                    "Destroy https://anotheradthing.com/bids.js"));

    EXPECT_THAT(
        LiveDebuggables(),
        testing::ElementsAre("https://adstuff.publisher1.com/auction.js"));

    // The auction should be scored without waiting on the crashed kBidder1.
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
    EXPECT_EQ(7, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/11,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});

    // Finish the auction.
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // Worklet 2 should be reloaded and ReportWin() invoked.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    bidder2_worklet->WaitForReportWin();
    bidder2_worklet->InvokeReportWinCallback();

    // Bidder2 won, Bidder1 crashed.
    auction_run_loop_->Run();
    EXPECT_THAT(result_.errors,
                testing::ElementsAre(base::StringPrintf(
                    "%s crashed while trying to run generateBid().",
                    kBidder1Url.spec().c_str())));
    EXPECT_EQ(kBidder2Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
    EXPECT_TRUE(result_.ad_component_urls.empty());
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
    EXPECT_TRUE(result_.private_aggregation_requests.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre(kBidder2Key));
    EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
              result_.winning_group_ad_metadata);
    CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2,
                    /*expected_sellers=*/1);
  }
}

// If the winning bidder crashes while coming up with the reporting URL, the
// auction should succeed. While the bidder cannot provide any reporting
// information, the seller's reporting information is respected.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileReporting) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  PrivateAggregationRequests bidder_1_pa_requests;
  bidder_1_pa_requests.push_back(
      kExpectedGenerateBidPrivateAggregationRequest.Clone());
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/7, GURL("https://ad1.com/"),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_urls=*/absl::nullopt,
      /*duration=*/base::TimeDelta(),
      /*bidding_signals_data_version=*/
      absl::nullopt,
      /*debug_loss_report_url=*/absl::nullopt,
      /*debug_win_report_url=*/absl::nullopt, std::move(bidder_1_pa_requests));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
  bidder1_worklet.reset();

  // Score Bidder1's bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(7, score_ad_params.bid);
  PrivateAggregationRequests score_ad_1_pa_requests;
  score_ad_1_pa_requests.push_back(
      kExpectedScoreAdPrivateAggregationRequest.Clone());
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/11,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          std::move(score_ad_1_pa_requests),
          /*errors=*/{});

  PrivateAggregationRequests bidder_2_pa_requests;
  bidder_2_pa_requests.push_back(
      kExpectedGenerateBidPrivateAggregationRequest.Clone());
  bidder2_worklet->InvokeGenerateBidCallback(
      /*bid=*/5, GURL("https://ad2.com/"),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_urls=*/absl::nullopt,
      /*duration=*/base::TimeDelta(),
      /*bidding_signals_data_version=*/
      absl::nullopt,
      /*debug_loss_report_url=*/absl::nullopt,
      /*debug_win_report_url=*/absl::nullopt, std::move(bidder_2_pa_requests));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
  bidder2_worklet.reset();

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  PrivateAggregationRequests score_ad_2_pa_requests;
  score_ad_2_pa_requests.push_back(
      kExpectedScoreAdPrivateAggregationRequest.Clone());
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          std::move(score_ad_2_pa_requests),
          /*errors=*/{});

  PrivateAggregationRequests report_result_pa_requests;
  report_result_pa_requests.push_back(
      kExpectedReportResultPrivateAggregationRequest.Clone());

  // Bidder1 crashes while running ReportWin.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      /*report_url=*/GURL("https://seller.report.test/"),
      /*ad_beacon_map=*/{}, std::move(report_result_pa_requests));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet.reset();
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                  "%s crashed.", kBidder1Url.spec().c_str())));
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://seller.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_THAT(
      result_.private_aggregation_requests,
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Should not have any debugging win/loss report URLs after auction when feature
// kBiddingAndScoringDebugReportingAPI is not enabled.
TEST_F(AuctionRunnerTest, ForDebuggingOnlyReporting) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    /*report_post_auction_signals=*/false,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    /*report_post_auction_signals=*/false,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/false, kSellerUrl,
                        kSellerDebugLossReportBaseUrl,
                        kSellerDebugWinReportBaseUrl));

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  EXPECT_EQ(0u, res.debug_loss_report_urls.size());
  EXPECT_EQ(0u, res.debug_win_report_urls.size());
}

// If the seller crashes before all bids are scored, the auction fails. If
// the seller crashes during the reporting phase, the auction completes
// successfully, and the bidder's reportWin() method is invoked. Seller load
// failures look the same to auctions, so this test also covers load failures in
// the same places. Note that a seller worklet load error while waiting for
// bidder worklet processes is covered in another test, and looks exactly like a
// crash at the same point to the AuctionRunner.
TEST_F(AuctionRunnerTest, SellerCrash) {
  enum class CrashPhase {
    kLoad,
    kScoreBid,
    kReportResult,
  };
  for (CrashPhase crash_phase :
       {CrashPhase::kLoad, CrashPhase::kScoreBid, CrashPhase::kReportResult}) {
    SCOPED_TRACE(static_cast<int>(crash_phase));

    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // While loop to allow breaking when the crash stage is reached.
    while (true) {
      if (crash_phase == CrashPhase::kLoad) {
        seller_worklet->set_expect_send_pending_signals_requests_called(false);
        seller_worklet.reset();
        break;
      }

      // Generate both bids, wait for seller to receive them..
      bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                                 GURL("https://ad1.com/"));
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      auto score_ad_params2 = seller_worklet->WaitForScoreAd();

      // Wait for SendPendingSignalsRequests() invocation.
      task_environment_.RunUntilIdle();

      if (crash_phase == CrashPhase::kScoreBid) {
        seller_worklet.reset();
        break;
      }
      // Score Bidder1's bid.
      bidder1_worklet.reset();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(5, score_ad_params.bid);
      mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
          std::move(score_ad_params.score_ad_client))
          ->OnScoreAdComplete(
              /*score=*/10,
              /*reject_reason=*/
              auction_worklet::mojom::RejectReason::kNotAvailable,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
              /*scoring_signals_data_version=*/0,
              /*has_scoring_signals_data_version=*/false,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*errors=*/{});

      // Score Bidder2's bid.
      EXPECT_EQ(kBidder2, score_ad_params2.interest_group_owner);
      EXPECT_EQ(7, score_ad_params2.bid);
      mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
          std::move(score_ad_params2.score_ad_client))
          ->OnScoreAdComplete(
              /*score=*/11,
              /*reject_reason=*/
              auction_worklet::mojom::RejectReason::kNotAvailable,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
              /*scoring_signals_data_version=*/0,
              /*has_scoring_signals_data_version=*/false,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*errors=*/{});

      seller_worklet->WaitForReportResult();
      // Crash the seller.
      DCHECK_EQ(CrashPhase::kReportResult, crash_phase);
      seller_worklet.reset();

      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder2_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback(
          /*report_url=*/GURL("https://bidder.report.test/"));
      break;
    }

    // Wait for auction to complete.
    auction_run_loop_->Run();

    if (crash_phase != CrashPhase::kReportResult) {
      EXPECT_THAT(result_.errors,
                  testing::ElementsAre(base::StringPrintf(
                      "%s crashed.", kSellerUrl.spec().c_str())));
      // No bidder won, seller crashed.
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre());
      EXPECT_EQ("", result_.winning_group_ad_metadata);
      CheckHistograms(
          InterestGroupAuction::AuctionResult::kSellerWorkletCrashed,
          /*expected_interest_groups=*/2, /*expected_owners=*/2,
          /*expected_sellers=*/1);
    } else {
      EXPECT_THAT(result_.errors,
                  testing::ElementsAre(base::StringPrintf(
                      "%s crashed.", kSellerUrl.spec().c_str())));
      // If the seller worklet crashes while calculating the report URL, the
      // auction completes, but reporting information is discarded.
      EXPECT_EQ(kBidder2Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(GURL("https://bidder.report.test/")));
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2, /*expected_owners=*/2,
                      /*expected_sellers=*/1);
    }
  }
}

TEST_F(AuctionRunnerTest, ComponentAuctionAllBiddersCrashBeforeBidding) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/false);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  bidder1_worklet.reset();

  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet.reset();

  auction_run_loop_->Run();

  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder1Url.spec().c_str()),
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder2Url.spec().c_str())));
  EXPECT_FALSE(result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/3);
}

// Test the case that one component has both bidders, one of which crashes, to
// make sure a single bidder crash doesn't result in the component auction
// failing.
TEST_F(AuctionRunnerTest, ComponentAuctionOneBidderCrashesBeforeBidding) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  // Close the first bidder worklet's pipe, simulating a crash.
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  bidder1_worklet.reset();
  // Wait for the AuctionRunner to observe the crash.
  task_environment_.RunUntilIdle();

  // The second bidder worklet bids.
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet->InvokeGenerateBidCallback(2, GURL("https://ad2.com/"));

  // Component worklet scores the bid.
  auto component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  ASSERT_TRUE(component_seller_worklet);
  auto score_ad_params = component_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/3,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
              /*ad=*/"null",
              /*bid=*/0,
              /*has_bid=*/false),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // Top-level seller worklet scores the bid.
  auto top_level_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
  ASSERT_TRUE(top_level_seller_worklet);
  score_ad_params = top_level_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/4,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // Top-level seller worklet returns a report url.
  top_level_seller_worklet->WaitForReportResult();
  top_level_seller_worklet->InvokeReportResultCallback(
      GURL("https://report1.test/"));

  // The component seller worklet should be reloaded and ReportResult() invoked.
  mock_auction_process_manager_->WaitForWinningSellerReload();
  component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  ASSERT_TRUE(component_seller_worklet);
  component_seller_worklet->WaitForReportResult();
  component_seller_worklet->InvokeReportResultCallback(
      GURL("https://report2.test/"));

  // Bidder worklet 2 should be reloaded and ReportWin() invoked.
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback(GURL("https://report3.test/"));

  // Bidder2 won, Bidder1 crashed.
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(base::StringPrintf(
                  "%s crashed while trying to run generateBid().",
                  kBidder1Url.spec().c_str())));
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                            GURL("https://report2.test/"),
                                            GURL("https://report3.test/")));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder2Key));
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// Test the three case where a component seller worklet fails during
// ReportResult:
//
// * Crash
// * Load failure
// * Error running the script.
//
// The auction should always complete successfully, running the bidder report
// script.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersReportResultFails) {
  interest_group_buyers_.emplace();
  // It's simpler to start a two bidder auction and throw away one of the
  // bidders rather than start a one-bidder auction.
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true);

  enum class TestCase { kCrash, kLoadError, kScriptError };

  // When false, simulates a seller workloet load failure instead.
  for (auto test_case :
       {TestCase::kCrash, TestCase::kLoadError, TestCase::kScriptError}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    StartStandardAuctionWithMockService();

    EXPECT_FALSE(auction_complete_);

    // Bidder worklet 1 bids.
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    bidder1_worklet->InvokeGenerateBidCallback(2, GURL("https://ad1.com/"));

    // Bidder worklet 2 makes no bid.
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);
    bidder2_worklet->InvokeGenerateBidCallback(absl::nullopt);

    // Component worklet scores the bid.
    auto component_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
    ASSERT_TRUE(component_seller_worklet);
    auto score_ad_params = component_seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/3,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
                /*ad=*/"null",
                /*bid=*/0,
                /*has_bid=*/false),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});

    // Top-level seller worklet scores the bid.
    auto top_level_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
    ASSERT_TRUE(top_level_seller_worklet);
    score_ad_params = top_level_seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/4,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});

    // Top-level seller worklet returns a report url.
    top_level_seller_worklet->WaitForReportResult();
    top_level_seller_worklet->InvokeReportResultCallback(
        GURL("https://report1.test/"));

    // The component seller worklet should be reloaded and ReportResult()
    // invoked.
    mock_auction_process_manager_->WaitForWinningSellerReload();
    component_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
    component_seller_worklet->set_expect_send_pending_signals_requests_called(
        false);
    ASSERT_TRUE(component_seller_worklet);
    component_seller_worklet->WaitForReportResult();
    std::string expected_error;

    if (test_case == TestCase::kCrash) {
      // A crash in the winning component seller worklet will cause the
      // reporting phase to abort, but the auction will otherwise complete
      // successfully.
      component_seller_worklet.reset();

      expected_error = base::StringPrintf("%s crashed.",
                                          kComponentSeller1Url.spec().c_str());
    } else if (test_case == TestCase::kLoadError) {
      const char kLoadError[] = "Load error";
      // A load error in the winning component seller worklet will cause the
      // auction to continue to completion.
      component_seller_worklet->ResetReceiverWithReason(kLoadError);

      expected_error = kLoadError;
    } else if (test_case == TestCase::kScriptError) {
      // A script error in the winning component seller worklet will cause the
      // auction to continue to completion.
      const char kScriptError[] = "Script error";

      component_seller_worklet->InvokeReportResultCallback(
          /*report_url=*/absl::nullopt, /*ad_beacon_map=*/{},
          /*pa_requests=*/{}, {kScriptError});
      expected_error = kScriptError;
    }

      // Winning bidder worklet should be reloaded and ReportWin() invoked.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder1_worklet->WaitForReportWin();
    bidder1_worklet->InvokeReportWinCallback(GURL("https://report3.test/"));

    // Auction completes.
    auction_run_loop_->Run();

    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(expected_error));
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                              GURL("https://report3.test/")));

    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre(kBidder1Key));
    CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2,
                    /*expected_sellers=*/2);
  }
}

// Test the case that all component sellers crash.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersAllCrash) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/false);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  // First component seller worklet crashes. Auction should not complete.
  auto component_seller_worklet1 =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet1->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  // Second component seller worklet crashes. Auction should complete.
  auto component_seller_worklet2 =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller2Url);
  component_seller_worklet2->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet2.reset();
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  base::StringPrintf("%s crashed.",
                                     kComponentSeller1Url.spec().c_str()),
                  base::StringPrintf("%s crashed.",
                                     kComponentSeller2Url.spec().c_str())));
  EXPECT_FALSE(result_.ad_url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/3);
}

// Test cases where a component seller returns an invalid
// ComponentAuctionModifiedBidParams.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellerBadBidParams) {
  const struct {
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr params;
    const char* expected_error;
  } kTestCases[] = {
      // Empty parameters are invalid.
      {auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
       "Invalid component_auction_modified_bid_params"},

      // Bad bids.
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/0,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-1,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/std::numeric_limits<double>::infinity(),
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-std::numeric_limits<double>::infinity(),
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-std::numeric_limits<double>::quiet_NaN(),
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
  };

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true);

  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();

    // First bidder doesn't finish scoring the bid. This should not stall the
    // auction, since these errors represent security errors from the component
    // auction's seller worklet.
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);

    // The second bidder worklet bids.
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);
    bidder2_worklet->InvokeGenerateBidCallback(2, GURL("https://ad2.com/"));

    // Component seller scores the bid, but returns a bad
    // ComponentAuctionModifiedBidParams.
    auto component_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
    ASSERT_TRUE(component_seller_worklet);
    component_seller_worklet->set_expect_send_pending_signals_requests_called(
        false);
    auto score_ad_params = component_seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(/*score=*/3,
                            /*reject_reason=*/
                            auction_worklet::mojom::RejectReason::kNotAvailable,
                            test_case.params.Clone(),
                            /*scoring_signals_data_version=*/0,
                            /*has_scoring_signals_data_version=*/false,
                            /*debug_loss_report_url=*/absl::nullopt,
                            /*debug_win_report_url=*/absl::nullopt,
                            /*pa_requests=*/{},
                            /*errors=*/{});

    // The auction fails, because of the bad ComponentAuctionModifiedBidParams.
    auction_run_loop_->Run();
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_FALSE(result_.ad_url);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());

    // Since these are security errors rather than script errors, they're
    // reported as bad Mojo messages, instead of in the return error list.
    EXPECT_EQ(test_case.expected_error, TakeBadMessage());

    // The component auction failed with a Mojo error, but the top-level auction
    // sees that as no bids.
    CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2,
                    /*expected_sellers=*/2);
  }
}

// Test cases where a top-level seller returns an
// ComponentAuctionModifiedBidParams, which should result in failing the
// auction.
TEST_F(AuctionRunnerTest, TopLevelSellerBadBidParams) {
  // Run a standard auction, with only a top-level seller.
  StartStandardAuctionWithMockService();

  // First bidder doesn't finish scoring the bid. This should not stall the
  // auction, since these errors represent security errors from the component
  // auction's seller worklet.
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  // The second bidder worklet bids.
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet->InvokeGenerateBidCallback(2, GURL("https://ad2.com/"));

  // Seller scores the bid, but returns a ComponentAuctionModifiedBidParams.
  auto seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
  ASSERT_TRUE(seller_worklet);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/3,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
              /*ad=*/"null",
              /*bid=*/0,
              /*has_bid=*/false),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  auction_run_loop_->Run();

  // The auction fails, because of the unexpected
  // ComponentAuctionModifiedBidParams.
  //
  // Since these are security errors rather than script errors, they're
  // reported as bad Mojo messages, instead of in the return error list.
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ("Invalid component_auction_modified_bid_params", TakeBadMessage());
  EXPECT_FALSE(result_.ad_url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());

  CheckHistograms(InterestGroupAuction::AuctionResult::kBadMojoMessage,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

TEST_F(AuctionRunnerTest, NullAdComponents) {
  const GURL kRenderUrl = GURL("https://ad1.com");
  const struct {
    absl::optional<std::vector<GURL>> bid_ad_component_urls;
    bool expect_successful_bid;
  } kTestCases[] = {
      {absl::nullopt, true},
      {std::vector<GURL>{}, false},
      {std::vector<GURL>{GURL("https://ad1.com-component1.com")}, false},
  };

  for (const auto& test_case : kTestCases) {
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(
        MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                          kBidder1TrustedSignalsUrl, {"k1", "k2"}, kRenderUrl,
                          /*ad_component_urls=*/absl::nullopt));

    StartAuction(kSellerUrl, std::move(bidders));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder_worklet);

    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, /*mojo_kanon_bid=*/nullptr,
        test_case.bid_ad_component_urls, base::TimeDelta());

    if (test_case.expect_successful_bid) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(1, score_ad_params.bid);
      mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
          std::move(score_ad_params.score_ad_client))
          ->OnScoreAdComplete(
              /*score=*/11,
              /*reject_reason=*/
              auction_worklet::mojom::RejectReason::kNotAvailable,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
              /*scoring_signals_data_version=*/0,
              /*has_scoring_signals_data_version=*/false,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*errors=*/{});

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder1Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key));
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/1,
                      /*expected_owners=*/1, /*expected_sellers=*/1);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Unexpected non-null ad component list", TakeBadMessage());

      // No bidder won.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre());
      EXPECT_EQ("", result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1,
                      /*expected_sellers=*/1);
    }
  }
}

// Test that the limit of kMaxAdComponents ad components per bid is enforced.
TEST_F(AuctionRunnerTest, AdComponentsLimit) {
  const GURL kRenderUrl = GURL("https://ad1.com");

  for (size_t num_components = 1;
       num_components < blink::kMaxAdAuctionAdComponents + 2;
       num_components++) {
    std::vector<GURL> ad_component_urls;
    for (size_t i = 0; i < num_components; ++i) {
      ad_component_urls.emplace_back(
          GURL(base::StringPrintf("https://%zu.com", i)));
    }
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, kRenderUrl, ad_component_urls));

    StartAuction(kSellerUrl, std::move(bidders));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder_worklet);

    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, /*mojo_kanon_bid=*/nullptr, ad_component_urls,
        base::TimeDelta());

    if (num_components <= blink::kMaxAdAuctionAdComponents) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(1, score_ad_params.bid);
      mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
          std::move(score_ad_params.score_ad_client))
          ->OnScoreAdComplete(
              /*score=*/11,
              /*reject_reason=*/
              auction_worklet::mojom::RejectReason::kNotAvailable,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
              /*scoring_signals_data_version=*/0,
              /*has_scoring_signals_data_version=*/false,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*errors=*/{});

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder1Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_EQ(ad_component_urls, result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key));
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/1,
                      /*expected_owners=*/1, /*expected_sellers=*/1);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Too many ad component URLs", TakeBadMessage());

      // No bidder won.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
      EXPECT_TRUE(result_.private_aggregation_requests.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre());
      EXPECT_EQ("", result_.winning_group_ad_metadata);
      CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1,
                      /*expected_sellers=*/1);
    }
  }
}

// Test cases where a bad bid is received over Mojo. Bad bids should be rejected
// in the Mojo process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadBid) {
  const struct TestCase {
    const char* expected_error_message;
    double bid;
    GURL render_url;
    absl::optional<std::vector<GURL>> ad_component_urls;
    base::TimeDelta duration;
  } kTestCases[] = {
      // Bids that aren't positive integers.
      {
          "Invalid bid value",
          -10,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          0,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::infinity(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::quiet_NaN(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid render URL.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL(":"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Non-HTTPS render URLs.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("data:,foo"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("http://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // HTTPS render URL that's not in the list of allowed renderUrls.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid component URL.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL(":")},
          base::TimeDelta(),
      },

      // HTTPS component URL that's not in the list of allowed ad component
      // URLs.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                            GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },

      // Negative time.
      {
          "Invalid bid duration",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::Milliseconds(-1),
      },
  };

  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    bidder1_worklet->InvokeGenerateBidCallback(
        test_case.bid, test_case.render_url, /*mojo_kanon_bid=*/nullptr,
        test_case.ad_component_urls, test_case.duration);
    // Bidder 2 doesn't bid.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_url);
    EXPECT_TRUE(result_.ad_component_urls.empty());
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
    EXPECT_TRUE(result_.private_aggregation_requests.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());
    EXPECT_EQ("", result_.winning_group_ad_metadata);
    CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2,
                    /*expected_sellers=*/1);
  }
}

// Test cases where a bad report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(AuctionRunnerTest, BadSellerReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // The seller provides a bad report URL.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(GURL("http://not.https.test/"));

  // The winning bidder still gets a chance to provide a report URL.
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("https://bidder.report.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://bidder.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test cases where a bad report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(AuctionRunnerTest, BadSellerBeaconUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // The seller provides a bad beacon map.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://seller.report.test/"),
      {{"click", GURL("http://not.https.test/")}});

  // The winning bidder still gets a chance to provide a report URL.
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("https://bidder.report.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller beacon URL for 'click'", TakeBadMessage());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://seller.report.test/"),
                                       GURL("https://bidder.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test cases where a bad report URL is received over Mojo from the winning
// component seller worklet. Bad report URLs should be rejected in the Mojo
// process, so this results in reporting a bad Mojo message, though the
// reporting phase is allowed to continue.
TEST_F(AuctionRunnerTest, BadComponentSellerReportUrl) {
  this->SetUpComponentAuctionAndResponses(
      /*bidder1_seller=*/kComponentSeller1,
      /*bidder2_seller=*/kComponentSeller1,
      /*bid_from_component_auction_wins=*/true);
  StartStandardAuctionWithMockService();

  auto component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  ASSERT_TRUE(component_seller_worklet);
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  // Component seller scores the bid.
  auto score_ad_params = component_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
              /*ad=*/"null",
              /*bid=*/0,
              /*has_bid=*/false),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // Top-level seller scores the bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // Top-level seller worklet returns a valid HTTPS report URL.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://seller.report.test/"));

  mock_auction_process_manager_->WaitForWinningSellerReload();
  component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet->WaitForReportResult();
  component_seller_worklet->InvokeReportResultCallback(GURL("Invalid URL"));

  // The winning bidder still gets a chance to provide a report URL.
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("https://bidder.report.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://seller.report.test/"),
                                       GURL("https://bidder.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/2);
}

// Test cases where a bad report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to complete.
TEST_F(AuctionRunnerTest, BadBidderReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://seller.report.test/"));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://seller.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Test cases where a bad URL is present in the beacon mapping received over
// Mojo from the bidder worklet. Bad report URLs should be rejected in the Mojo
// process, so this results in reporting a bad Mojo message, though the
// reporting phase is allowed to complete.
TEST_F(AuctionRunnerTest, BadBidderBeaconUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://seller.report.test/"));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(
      GURL("https://bidder.report.test/"),
      {{"click", GURL("http://not.https.test/")}});
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid bidder beacon URL for 'click'", TakeBadMessage());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre(
                                       GURL("https://seller.report.test/"),
                                       GURL("https://bidder.report.test/")));
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Check that BidderWorklets that don't make a bid are destroyed immediately.
TEST_F(AuctionRunnerTest, DestroyBidderWorkletWithoutBid) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the bid.
  mock_auction_process_manager_->Flush();
  // The AuctionRunner should have closed the pipe.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                             GURL("https://ad2.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(7, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/11,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*errors=*/{});

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_TRUE(result_.ad_component_urls.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
  EXPECT_TRUE(result_.private_aggregation_requests.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder2Key));
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2,
                  /*expected_sellers=*/1);
}

// Check that the winner of ties is randomized. Mock out bidders so can make
// sure that which bidder wins isn't changed just due to script execution order
// changing.
TEST_F(AuctionRunnerTest, Tie) {
  bool seen_bidder1_win = false;
  bool seen_bidder2_win = false;

  while (!seen_bidder1_win || !seen_bidder2_win) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Bidder1 returns a bid, which is then scored.
    bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                               GURL("https://ad1.com/"));
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/10,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});

    // Bidder2 returns a bid, which is then scored.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                               GURL("https://ad2.com/"));
    score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/10,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});
    // Need to flush the service pipe to make sure the AuctionRunner has
    // received the score.
    seller_worklet->Flush();

    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // Wait for a worklet to be reloaded, and try to get worklets for both
    // InterestGroups - only the InterestGroup that was picked as the winner
    // will be non-null.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);

    if (bidder1_worklet) {
      seen_bidder1_win = true;
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder1Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.winning_group_ad_metadata);
    } else {
      seen_bidder2_win = true;

      ASSERT_TRUE(bidder2_worklet);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder2Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_TRUE(result_.ad_component_urls.empty());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.winning_group_ad_metadata);
    }

    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_TRUE(result_.ad_beacon_map.metadata.empty());
    EXPECT_TRUE(result_.private_aggregation_requests.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
    CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2,
                    /*expected_sellers=*/1);
  }
}

// Test worklets completing in an order different from the one in which they're
// invoked.
TEST_F(AuctionRunnerTest, WorkletOrder) {
  // Events that can ordered differently for each loop iteration. All events
  // must happen, and a bid must be generated before it is scored.
  enum class Event {
    kBid1Generated,
    kBid2Generated,
    kBid1Scored,
    kBid2Scored,
  };

  // All possible orderings. This test assumes the order bidders are loaded in
  // is deterministic, which currently is the case (though that may change down
  // the line).
  const Event kTestCases[][4] = {
      {Event::kBid1Generated, Event::kBid1Scored, Event::kBid2Generated,
       Event::kBid2Scored},
      {Event::kBid1Generated, Event::kBid2Generated, Event::kBid1Scored,
       Event::kBid2Scored},
      {Event::kBid1Generated, Event::kBid2Generated, Event::kBid2Scored,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid2Scored, Event::kBid1Generated,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid1Generated, Event::kBid2Scored,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid1Generated, Event::kBid1Scored,
       Event::kBid2Scored},
  };

  for (const auto& test_case : kTestCases) {
    for (bool bidder1_wins : {false, true}) {
      StartStandardAuctionWithMockService();

      auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
      ASSERT_TRUE(seller_worklet);
      auto bidder1_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      ASSERT_TRUE(bidder1_worklet);
      auto bidder2_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
      ASSERT_TRUE(bidder2_worklet);

      MockSellerWorklet::ScoreAdParams score_ad_params1;
      MockSellerWorklet::ScoreAdParams score_ad_params2;

      for (Event event : test_case) {
        switch (event) {
          case Event::kBid1Generated:
            bidder1_worklet->InvokeGenerateBidCallback(
                /*bid=*/9, GURL("https://ad1.com/"));
            score_ad_params1 = seller_worklet->WaitForScoreAd();
            EXPECT_EQ(kBidder1, score_ad_params1.interest_group_owner);
            EXPECT_EQ(9, score_ad_params1.bid);
            break;
          case Event::kBid2Generated:
            bidder2_worklet->InvokeGenerateBidCallback(
                /*bid=*/10, GURL("https://ad2.com/"));
            score_ad_params2 = seller_worklet->WaitForScoreAd();
            EXPECT_EQ(kBidder2, score_ad_params2.interest_group_owner);
            EXPECT_EQ(10, score_ad_params2.bid);
            break;
          case Event::kBid1Scored:
            mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
                std::move(score_ad_params1.score_ad_client))
                ->OnScoreAdComplete(
                    /*score=*/bidder1_wins ? 11 : 9,
                    /*reject_reason=*/
                    auction_worklet::mojom::RejectReason::kNotAvailable,
                    auction_worklet::mojom::
                        ComponentAuctionModifiedBidParamsPtr(),
                    /*scoring_signals_data_version=*/0,
                    /*has_scoring_signals_data_version=*/false,
                    /*debug_loss_report_url=*/absl::nullopt,
                    /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
                    /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment_.RunUntilIdle();
            break;
          case Event::kBid2Scored:
            mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
                std::move(score_ad_params2.score_ad_client))
                ->OnScoreAdComplete(
                    /*score=*/10,
                    /*reject_reason=*/
                    auction_worklet::mojom::RejectReason::kNotAvailable,
                    auction_worklet::mojom::
                        ComponentAuctionModifiedBidParamsPtr(),
                    /*scoring_signals_data_version=*/0,
                    /*has_scoring_signals_data_version=*/false,
                    /*debug_loss_report_url=*/absl::nullopt,
                    /*debug_win_report_url=*/absl::nullopt,
                    /*pa_requests=*/{},
                    /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment_.RunUntilIdle();
            break;
        }
      }

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();

      mock_auction_process_manager_->WaitForWinningBidderReload();
      auto winning_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          bidder1_wins ? kBidder1Url : kBidder2Url);
      winning_worklet->WaitForReportWin();
      winning_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();
      EXPECT_THAT(result_.errors, testing::ElementsAre());

      if (bidder1_wins) {
        EXPECT_EQ(kBidder1Key, result_.winning_group_id);
        EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
        EXPECT_EQ(
            R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);
      } else {
        EXPECT_EQ(kBidder2Key, result_.winning_group_id);
        EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
        EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                  result_.winning_group_ad_metadata);
      }
    }
  }
}

// Check that the top bid and `highestScoringOtherBid` are randomized in a 3-way
// tie for the highest bid.
TEST_F(AuctionRunnerTest, ThreeWayTie) {
  bool seen_result[3][3] = {{false}};
  int total_seen_results = 0;

  const GURL kBidder3Url{"https://bidder3.test/bids.js"};
  const url::Origin kBidder3 = url::Origin::Create(kBidder3Url);
  interest_group_buyers_ = {{kBidder1, kBidder2, kBidder3}};

  while (total_seen_results < 6) {
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder3Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                           MakeAuctionScriptSupportsTie());

    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"1", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, /*name=*/"2", kBidder2Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
    // Use name "5" so that the IG bids "5", which is given the same score as
    // bids of "1" and "2" (A bid of "3" is given a different score).
    bidders.emplace_back(MakeInterestGroup(
        kBidder3, /*name=*/"5", kBidder3Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad3.com")));

    RunAuctionAndWait(kSellerUrl, std::move(bidders));
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    ASSERT_TRUE(result_.ad_url);

    int winner;
    if (result_.ad_url->spec() == "https://ad1.com/") {
      winner = 0;
    } else if (result_.ad_url->spec() == "https://ad2.com/") {
      winner = 1;
    } else {
      ASSERT_EQ(result_.ad_url->spec(), "https://ad3.com/");
      winner = 2;
    }

    int highest_other_bidder;
    ASSERT_EQ(2u, result_.report_urls.size());
    if (result_.report_urls[0].spec().find("highestScoringOtherBid=1") !=
        std::string::npos) {
      highest_other_bidder = 0;
    } else if (result_.report_urls[0].spec().find("highestScoringOtherBid=2") !=
               std::string::npos) {
      highest_other_bidder = 1;
    } else {
      ASSERT_NE(std::string::npos,
                result_.report_urls[0].spec().find("highestScoringOtherBid=5"));
      highest_other_bidder = 2;
    }

    ASSERT_NE(winner, highest_other_bidder);
    if (!seen_result[winner][highest_other_bidder]) {
      seen_result[winner][highest_other_bidder] = true;
      ++total_seen_results;
    }
  }
}

// Test the case where there's one IG with two groups, a size limit of 1, and
// the highest priority group has no bid script. The lower priority group should
// get a chance to bid, rather than being filtered out.
TEST_F(AuctionRunnerTest, SizeLimitHighestPriorityGroupHasNoBidScript) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  // Low priority group with a bidding URL.
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders.back().interest_group.priority = 0;

  // High priority group without a bidding URL.
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, "other-interest-group-name", /*bidding_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
  bidders.back().interest_group.priority = 10;

  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
}

TEST_F(AuctionRunnerTest, ExecutionModeGroupByOrigin) {
  // Test of GroupByOrigin execution mode at AuctionRunner level;
  // this primarily shows that the sorting actually groups things, and that
  // distinct groups are kept separate.
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 0;
    function generateBid() {
      ++count;
      return {ad: ["ad"], bid:count, render:"https://response.test/"};
    }
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://adplatform.com/metrics/" + browserSignals.bid);
    }
  )";

  const char kSellerScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      return {desirability: bid,
              ad: adMetadata};
    }
    function reportResult() {}
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  std::vector<StorageInterestGroup> bidders;
  // Add 5 groupByOrigin, 2 regular execution mode IGs.
  for (int i = 0; i < 7; ++i) {
    StorageInterestGroup ig = MakeInterestGroup(
        kBidder1, kBidder1Name + base::NumberToString(i), kBidder1Url,
        /* trusted_bidding_signals_url=*/absl::nullopt,
        /* trusted_bidding_signals_keys=*/{}, GURL("https://response.test/"));
    ig.joining_origin = url::Origin::Create(GURL("https://sports.example.org"));
    ig.interest_group.execution_mode =
        i < 5 ? blink::InterestGroup::ExecutionMode::kGroupedByOriginMode
              : blink::InterestGroup::ExecutionMode::kCompatibilityMode;
    bidders.push_back(std::move(ig));
  }

  // Add one with different join origin.
  StorageInterestGroup ig = MakeInterestGroup(
      kBidder1, kBidder1Name + std::string("8"), kBidder1Url,
      /* trusted_bidding_signals_url=*/absl::nullopt,
      /* trusted_bidding_signals_keys=*/{}, GURL("https://response.test/"));
  ig.joining_origin = url::Origin::Create(GURL("https://shopping.example.us"));
  ig.interest_group.execution_mode =
      blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  bidders.push_back(std::move(ig));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.winning_group_id);
  EXPECT_THAT(result_.report_urls,
              testing::ElementsAre(GURL("https://adplatform.com/metrics/5")));
}

// Auction with only one interest group participating. The priority calculated
// using its priority vector is negative, so it should be filtered out, and
// there should be no winner.
TEST_F(AuctionRunnerTest, PriorityVectorFiltersOnlyGroup) {
  // Only include bidder 1. Having a second bidder results in following a
  // slightly different path. With two bidders, the first bidder loads an
  // interest group, which is filtered, and then the bidder is deleted. Then the
  // second bidder loads no interest groups, and the auction is deleted. With a
  // single bidder, the auction is deleted immediately after filtering out the
  // bidders, which potentially affects the dangling pointer detection code,
  // since the discarded BuyerHelper must be deleted before the
  // InterestGroupAuction it has a pointer to.
  interest_group_buyers_ = {{kBidder1}};

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  // Priority should be 1 * -1 = -1.
  bidders.back().interest_group.priority_vector = {
      {{"browserSignals.one", -1}}};

  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(result_.winning_group_id, absl::nullopt);
  EXPECT_EQ(result_.ad_url, absl::nullopt);

  // No interest groups participated in the auction.
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/0);
}

// Check that when the priority vector calculation results in a zero priority,
// the interest group is not filtered.
TEST_F(AuctionRunnerTest, PriorityVectorZeroPriorityNotFiltered) {
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, /*bid=*/"1", "https://ad1.com/",
                    /*num_ad_components=*/0, kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  // Priority should be 0.
  bidders.back().interest_group.priority_vector = {{{"browserSignals.one", 0}}};

  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);

  // No interest groups participated in the auction.
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Check that both empty and null priority signals vectors are ignored.
TEST_F(AuctionRunnerTest, EmptyPriorityVector) {
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, /*bid=*/"1", "https://ad1.com/",
                    /*num_ad_components=*/0, kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  for (bool use_empty_priority_signals : {false, true}) {
    std::vector<StorageInterestGroup> bidders;
    // A higher priority interest group that has a null / empty priority vector.
    // The priority vector should be ignored, resulting in only this bidder
    // participating in the auction.
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders.back().interest_group.priority = 10;
    if (use_empty_priority_signals)
      bidders.back().interest_group.priority_vector = {};

    // A lower priority interest group with a priority greater than 0 (which
    // is what multiplying an empty priority vector would result in).
    const GURL kBidder1OtherUrl = GURL("https://adplatform.com/other_ad.js");
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, "other-bidder-1-group", kBidder1OtherUrl,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
    bidders.back().interest_group.priority = 1;

    all_buyers_group_limit_ = 1;

    RunAuctionAndWait(kSellerUrl, std::move(bidders));
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
    // No request should have been made for the other URL.
    EXPECT_FALSE(url_loader_factory_.IsPending(kBidder1OtherUrl.spec()));

    // The second interest group is not counted as having participated in the
    // auction.
    CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/1, /*expected_owners=*/1,
                    /*expected_sellers=*/1);
  }
}

// Run an auction where there are two interest groups with the same owner, and a
// limit of one interest group per buyer. One group has a higher base priority,
// but the other group has a higher priority after the priority vector is taken
// into account, so should be the only bidder to participate in the auction.
TEST_F(AuctionRunnerTest, PriorityVector) {
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, /*bid=*/"1", "https://ad1.com/",
                    /*num_ad_components=*/0, kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  std::vector<StorageInterestGroup> bidders;

  // A low priority interest group with a priority vector that results in a high
  // priority after multiplication.
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  // Priority should be -1 * -10 = 10.
  bidders.back().interest_group.priority = -1;
  bidders.back().interest_group.priority_vector = {
      {{"browserSignals.basePriority", -10}}};

  // A higher priority interest group that should end up being filtered out due
  // to having a lower (but non-negative) priority after the vector
  // multiplication.
  const GURL kBidder1OtherUrl = GURL("https://adplatform.com/other_ad.js");
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, "other-bidder-1-group", kBidder1OtherUrl,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
  // Priority should be 1 * 1 = 1.
  bidders.back().interest_group.priority = 1;
  bidders.back().interest_group.priority_vector = {
      {{"browserSignals.basePriority", 1}}};

  all_buyers_group_limit_ = 1;

  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  // No request should have been made for the other URL.
  EXPECT_FALSE(url_loader_factory_.IsPending(kBidder1OtherUrl.spec()));

  // The second interest group is not counted as having participated in the
  // auction.
  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with only one interest group participating. The priority calculated
// using the priority vector fetch in bidding signals is negative, so it should
// be filtered out after the bidding signals fetch, and there should be no
// winner.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorOnlyGroupFiltered) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullTrustedSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", -1}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);

  // The interest group is considered to have participated in the auction.
  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/1,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with only one interest group participating. The priority calculated
// using the priority vector fetch in bidding signals is zero, so it should
// not be filtered out.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorOnlyGroupNotFiltered) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullTrustedSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 0}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner. The
// priority calculated using the priority vector fetch in bidding signals is
// negative for both groups. The group limit is 1 and
// `enable_bidding_signals_prioritization` is set to true for one of the groups,
// so the auction should be set up to filter only after all priority vectors
// have been received, but then they eliminates both interest groups.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorBothGroupsFiltered) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullTrustedSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", -1}}}},
           {"2", {{{"browserSignals.one", -2}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// The priority calculated using the priority vector fetch in bidding signals is
// negative for the first group to receive trusted signals (which is group 2).
// The group limit is 1 and `enable_bidding_signals_prioritization` is set to
// true for one of the groups, so the auction should be set up to filter only
// after all priority vectors have been received.
//
// The two interest groups use different trusted signals URLs, so the order the
// responses are received in can be controlled.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorFirstGroupFiltered) {
  const GURL kFullTrustedSignalsUrl1 =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1");
  const GURL kBidder1TrustedSignalsUrl2 =
      GURL(kBidder1TrustedSignalsUrl.spec() + "2");
  const GURL kFullTrustedSignalsUrl2 =
      GURL(kBidder1TrustedSignalsUrl2.spec() +
           "?hostname=publisher1.com&interestGroupNames=2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl2,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(2, url_loader_factory_.NumPending());

  // Group 2 has a negative priority.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl2,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"2", {{{"browserSignals.one", -2}}}}}));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl1,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// The priority calculated using the priority vector fetch in bidding signals is
// negative for the second group to receive trusted signals (which is group 2).
// The group limit is 1 and `enable_bidding_signals_prioritization` is set to
// true for one of the groups, so the auction should be set up to filter only
// after all priority vectors have been received.
//
// The two interest groups use different trusted signals URLs, so the order the
// responses are received in can be controlled.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorSecondGroupFiltered) {
  const GURL kFullTrustedSignalsUrl1 =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1");
  const GURL kBidder1TrustedSignalsUrl2 =
      GURL(kBidder1TrustedSignalsUrl.spec() + "2");
  const GURL kFullTrustedSignalsUrl2 =
      GURL(kBidder1TrustedSignalsUrl2.spec() +
           "?hostname=publisher1.com&interestGroupNames=2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl2,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(2, url_loader_factory_.NumPending());

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl1,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}}}));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  // Group 2 has a negative priority.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl2,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"2", {{{"browserSignals.one", -2}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// The priority calculated using the priority vector fetch in bidding signals is
// negative for both groups. The group limit is 1 and
// `enable_bidding_signals_prioritization` is set to true for one of the groups,
// so the auction should be set up to filter only after all priority vectors
// have been received.
//
// In this test, the group with the lower priority is removed when enforcing the
// per-bidder size limit. The other interest group goes on to win the auction.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorSizeLimitFiltersOneGroup) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullTrustedSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);

  // Group 2 has a lower, but non-negative, priority.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}},
           {"2", {{{"browserSignals.one", 0.5}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// The priority calculated using the priority vector fetch in bidding signals is
// negative for both groups. The group limit is 1 and
// `enable_bidding_signals_prioritization` is set to true for one of the groups,
// so the auction should be set up to filter only after all priority vectors
// have been received.
//
// In this test, neither group is filtered due to having a negative priority,
// however, the group that would otherwise bid higher is filtered out due to the
// per buyer interest group limit.
TEST_F(AuctionRunnerTest, TrustedBiddingSignalsPriorityVectorNoGroupFiltered) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullTrustedSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 2}}}},
           {"2", {{{"browserSignals.one", 1}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Test that `basePriority` works as expected. Interest groups have one priority
// order with base priorities, another with the priority vectors that are part
// of the interest groups, and then the priority vectors downloaded as signals
// echo the base priority values, which should be the order that takes effect,
// when one group has `enable_bidding_signals_prioritization` set to true.
TEST_F(AuctionRunnerTest, TrustedBiddingSignalsPriorityVectorBasePriority) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.priority = 2;
  bidders[0].interest_group.priority_vector = {{"browserSignals.one", 1}};
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
  bidders[1].interest_group.priority = 1;
  bidders[1].interest_group.priority_vector = {{"browserSignals.one", 2}};
  bidders[1].interest_group.enable_bidding_signals_prioritization = true;

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.basePriority", 1}}}},
           {"2", {{{"browserSignals.basePriority", 1}}}}}));

  all_buyers_group_limit_ = 1;
  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Test that `firstDotProductPriority` works as expected. Interest groups have
// one priority order with base priorities, another with the priority vectors
// that are part of the interest groups, and then the priority vectors
// downloaded as signals echo the values of the previous priority vector dot
// product, which should be the order that takes effect, when one group has
// `enable_bidding_signals_prioritization` set to true.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorFirstDotProductPriority) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.priority = 1;
  bidders[0].interest_group.priority_vector = {{"browserSignals.one", 2}};
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
  bidders[1].interest_group.priority = 2;
  bidders[1].interest_group.priority_vector = {{"browserSignals.one", 1}};
  bidders[1].interest_group.enable_bidding_signals_prioritization = true;

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.firstDotProductPriority", 1}}}},
           {"2", {{{"browserSignals.firstDotProductPriority", 1}}}}}));

  all_buyers_group_limit_ = 1;
  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Test that when no priority vector is received, the result of the first
// priority calculation using the interest group's priority vector is used, if
// available, and if not, the base priority is used.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorNotreceivedMixPrioritySources) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");

  url_loader_factory_.ClearResponses();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeBidScriptSupportsTie());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptSupportsTie());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.priority = 0;
  bidders[0].interest_group.priority_vector = {{"browserSignals.one", 2}};
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
  bidders[1].interest_group.priority = 1;
  bidders[1].interest_group.enable_bidding_signals_prioritization = true;

  // Empty priority vector.
  auction_worklet::AddBidderJsonResponse(&url_loader_factory_,
                                         kFullTrustedSignalsUrl,
                                         /*content=*/"{}");

  all_buyers_group_limit_ = 1;
  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// `enable_bidding_signals_prioritization` is set to true and the size limit is
// one, so the worklets wait until all other worklets have received signals
// before proceeding. However, the worklets' Javascript fails to load before any
// signals are received, which should safely fail the auction. This follows the
// same path as if the worklet crashed, so no need to test crashing combined
// with `enable_bidding_signals_prioritization`.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorSharedScriptLoadErrorAfterSignals) {
  const GURL kFullTrustedSignalsUrl =
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&interestGroupNames=1,2");
  url_loader_factory_.ClearResponses();

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  // Seller script, bidder script, signals URL should all be pending.
  EXPECT_EQ(3, url_loader_factory_.NumPending());

  // Bidding signals received. Auction should still be pending.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}},
           {"2", {{{"browserSignals.one", 2}}}}}));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  // Seller script, bidder script should still be pending.
  EXPECT_EQ(2, url_loader_factory_.NumPending());

  // Script loads fail. The auction should safely fail.
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_run_loop_->Run();

  // Only get an error for one interest group - the other was filtered out due
  // to having a lower priority.
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          "Failed to load https://adplatform.com/offers.js HTTP status ="
          " 404 Not Found."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);
  EXPECT_EQ(absl::nullopt, result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

// Auction with two interest groups participating, both with the same owner.
// `enable_bidding_signals_prioritization` is set to true and the size limit is
// one, so the worklets wait until all other worklets have received signals
// before proceeding. However, the worklet's Javascript fails to load after
// signals are received, which should safely fail the auction. This follows the
// same path as if the worklet crashed, so no need to test crashing combined
// with `enable_bidding_signals_prioritization`.
TEST_F(AuctionRunnerTest,
       TrustedBiddingSignalsPriorityVectorSharedScriptLoadErrorBeforeSignals) {
  url_loader_factory_.ClearResponses();

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders[0].interest_group.enable_bidding_signals_prioritization = true;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"2", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  all_buyers_group_limit_ = 1;
  StartAuction(kSellerUrl, std::move(bidders));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  // Seller script, bidder script, signals URL should all be pending.
  EXPECT_EQ(3, url_loader_factory_.NumPending());

  // Script loads fail. The auction should safely fail.
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_run_loop_->Run();

  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          "Failed to load https://adplatform.com/offers.js HTTP status ="
          " 404 Not Found.",
          "Failed to load https://adplatform.com/offers.js HTTP status ="
          " 404 Not Found."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);
  EXPECT_EQ(absl::nullopt, result_.ad_url);

  CheckHistograms(InterestGroupAuction::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2,
                  /*expected_owners=*/1,
                  /*expected_sellers=*/1);
}

TEST_F(AuctionRunnerTest, SetPrioritySignalsOverride) {
  const char kBidderScript[] = R"(
    function generateBid() {
      setPrioritySignalsOverride("key", 3);
      return {bid:1, render:"https://ad1.com/"};
    }
    function reportWin() {}
  )";

  const char kSellerScript[] = R"(
    function scoreAd() {
      return {desirability: 1};
    }
    function reportResult() {}
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidderScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.winning_group_id);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);

  auto storage_interest_group = GetInterestGroup(kBidder1, kBidder1Name);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ((base::flat_map<std::string, double>{{"key", 3}}),
            storage_interest_group->interest_group.priority_signals_overrides);
}

// If there's no valid bid, setPrioritySignalsOverride() should still be
// respected.
TEST_F(AuctionRunnerTest, SetPrioritySignalsOverrideNoBid) {
  const char kBidderScript[] = R"(
    function generateBid() {
      setPrioritySignalsOverride("key", 3);
      return {bid:0, render:"https://ad1.com/"};
    }
    function reportWin() {}
  )";

  const char kSellerScript[] = R"(
    function scoreAd() {
      return {desirability: 1};
    }
    function reportResult() {}
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidderScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_url);

  auto storage_interest_group = GetInterestGroup(kBidder1, kBidder1Name);
  ASSERT_TRUE(storage_interest_group);
  EXPECT_EQ((base::flat_map<std::string, double>{{"key", 3}}),
            storage_interest_group->interest_group.priority_signals_overrides);
}

TEST_F(AuctionRunnerTest, Abort) {
  // Not adding kBidder1Url to block things in predictable spot.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true));

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));

  StartAuction(kSellerUrl, std::move(bidders));
  abortable_ad_auction_->Abort();
  auction_run_loop_->Run();
  EXPECT_TRUE(result_.manually_aborted);
  EXPECT_FALSE(result_.winning_group_id.has_value());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
}

// Testing what happens when Abort() is called after auction is done.
TEST_F(AuctionRunnerTest, AbortLate) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true));

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      /*ad_component_urls=*/absl::nullopt));

  // Want AuctionRunner still around to make sure that it handles Abort() OK
  // in that timing.
  dont_reset_auction_runner_ = true;
  const Result& result = RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_EQ(kBidder1Name, result.winning_group_id->name);
  EXPECT_FALSE(result.manually_aborted);
  EXPECT_THAT(result.errors, testing::ElementsAre());
  abortable_ad_auction_->Abort();
  task_environment_.RunUntilIdle();
  auction_runner_.reset();
}

// Enable and test forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() APIs.
class AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest
    : public AuctionRunnerTest {
 public:
  AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReporting) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    /*report_post_auction_signals=*/true,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true,
                        GURL("https://adstuff.publisher1.com/auction.js"),
                        kSellerDebugLossReportBaseUrl,
                        kSellerDebugWinReportBaseUrl));

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  EXPECT_EQ(2u, res.debug_loss_report_urls.size());
  // Sellers can get highest scoring other bid, but losing bidders can not.
  EXPECT_THAT(res.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(kBidder1DebugLossReportUrl,
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl(kSellerDebugLossReportBaseUrl,
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/true),
                                 /*bid=*/1)));

  EXPECT_EQ(2u, res.debug_win_report_urls.size());
  // Winning bidders can get highest scoring other bid.
  EXPECT_THAT(res.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(kBidder2DebugWinReportUrl,
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl(kSellerDebugWinReportBaseUrl,
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false),
                                 /*bid=*/2)));
}

// Post auction signals should only be reported through report URL's query
// string. Placeholder ${} in a debugging report URL's other parts such as path
// will be kept as it is without being replaced with actual signal.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       PostAuctionSignalsInQueryStringOnly) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(
          kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
          kBidder1Name,
          /*has_signals=*/false, "k1", "a",
          /*report_post_auction_signals=*/true,
          "https://bidder1-debug-loss-reporting.com/winningBid=${winningBid}",
          "https://bidder1-debug-win-reporting.com/winningBid=${winningBid}"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(
          kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
          kBidder2Name,
          /*has_signals=*/false, "l2", "b",
          /*report_post_auction_signals=*/true,
          "https://bidder2-debug-loss-reporting.com/winningBid=${winningBid}",
          "https://bidder2-debug-win-reporting.com/winningBid=${winningBid}"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(
          /*report_post_auction_signals=*/true,
          GURL("https://adstuff.publisher1.com/auction.js"),
          "https://seller-debug-loss-reporting.com/winningBid=${winningBid}",
          "https://seller-debug-win-reporting.com/winningBid=${winningBid}"));

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  // Placeholder ${winningBid} in a debugging report URL's path will not be
  // replaced with actual signal. Only those in a debugging report URL's query
  // param would be replaced.
  EXPECT_EQ(2u, res.debug_loss_report_urls.size());
  EXPECT_THAT(res.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl("https://bidder1-debug-loss-reporting.com/"
                                 "winningBid=${winningBid}",
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl("https://seller-debug-loss-reporting.com/"
                                 "winningBid=${winningBid}",
                                 PostAuctionSignals(
                                     /*winning_bid=*/2,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/true),
                                 /*bid=*/1)));

  EXPECT_EQ(2u, res.debug_win_report_urls.size());
  EXPECT_THAT(
      res.debug_win_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl("https://bidder2-debug-win-reporting.com/"
                         "winningBid=${winningBid}",
                         PostAuctionSignals(
                             /*winning_bid=*/2,
                             /*made_winning_bid=*/true,
                             /*highest_scoring_other_bid=*/1,
                             /*made_highest_scoring_other_bid=*/false)),
          DebugReportUrl(
              "https://seller-debug-win-reporting.com/winningBid=${winningBid}",
              PostAuctionSignals(
                  /*winning_bid=*/2,
                  /*made_winning_bid=*/true,
                  /*highest_scoring_other_bid=*/1,
                  /*made_highest_scoring_other_bid=*/false),
              /*bid=*/2)));
}

// When there are multiple bids getting the highest score, then highest scoring
// other bid will be one of them which didn't win the bid.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingMultipleTopBids) {
  bool seen_ad2_win = false;
  bool seen_ad3_win = false;

  while (!seen_ad2_win || !seen_ad3_win) {
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                           (MakeAuctionScriptSupportsTie()));

    std::vector<StorageInterestGroup> bidders;
    // Bid1 from kBidder1 gets second highest score. Bid2 from kBidder1 or bid3
    // from kBidder2 wins the auction. Integer values of interest group names
    // are used as their bid values.
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"1", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"3", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, /*name=*/"4", kBidder2Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad3.com")));

    const Result& res = RunAuctionAndWait(kSellerUrl, std::move(bidders));

    EXPECT_EQ(4u, res.debug_loss_report_urls.size());
    EXPECT_EQ(2u, res.debug_win_report_urls.size());
    EXPECT_EQ(2u, res.report_urls.size());

    // Winner has ad2 or ad3.
    if (res.ad_url == "https://ad2.com/") {
      seen_ad2_win = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/4,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/4,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/4,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/4,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=4&bid=3"),
                      ReportWinUrl(/*bid=*/3, /*highest_scoring_other_bid=*/4,
                                   /*made_highest_scoring_other_bid=*/false)));
    } else if (res.ad_url == "https://ad3.com/") {
      seen_ad3_win = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/3,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/3,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/3,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,

                  PostAuctionSignals(/*winning_bid=*/4,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/3,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=3&bid=4"),
                      ReportWinUrl(/*bid=*/4, /*highest_scoring_other_bid=*/3,
                                   /*made_highest_scoring_other_bid=*/false)));
    } else {
      NOTREACHED();
    }
  }
}

// This is used to test post auction signals when an auction where bidders are
// from the same interest group owner. All winning bid and highest scoring other
// bids come from the same interest group owner.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSameOwnerBidders) {
  // Seen bid1 or bid2 being picked as highest scoring other bid.
  bool seen_bid1 = false;
  bool seen_bid2 = false;
  // Adding these different bidder URLs so that the order of finishes fetch and
  // starts score is more arbitrary. Because highest scoring other bid picks
  // the one scored last when there's a tie, so it's more easily and faster to
  // reach both branches of the test.
  const GURL kBidder1Url2{"https://adplatform.com/offers2.js"};
  const GURL kBidder1Url3{"https://adplatform.com/offers3.js"};

  while (!seen_bid1 || !seen_bid2) {
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url2,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url3,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                           MakeAuctionScriptSupportsTie());

    std::vector<StorageInterestGroup> bidders;
    // Both bid1 and bid2 from kBidder1 get second highest score. Bid3 from
    // kBidder1 wins the auction.
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"1", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"2", kBidder1Url2,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"3", kBidder1Url3,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad3.com")));

    const Result& res = RunAuctionAndWait(kSellerUrl, std::move(bidders));

    double highest_scoring_other_bid = 0.0;
    if (base::Contains(
            res.report_urls,
            "https://reporting.example.com/?highestScoringOtherBid=1&bid=3",
            &GURL::spec)) {
      highest_scoring_other_bid = 1;
    } else if (base::Contains(res.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=2&bid=3",
                              &GURL::spec)) {
      highest_scoring_other_bid = 2;
    }

    EXPECT_EQ(GURL("https://ad3.com/"), res.ad_url);
    EXPECT_EQ(4u, res.debug_loss_report_urls.size());
    EXPECT_EQ(2u, res.debug_win_report_urls.size());
    EXPECT_EQ(2u, res.report_urls.size());

    if (highest_scoring_other_bid == 1) {
      seen_bid1 = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/true),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/true),
                  /*bid=*/2)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/3,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/1,
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/3,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/1,
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=1&bid=3"),
                      ReportWinUrl(/*bid=*/3, /*highest_scoring_other_bid=*/1,
                                   /*made_highest_scoring_other_bid=*/true)));
    } else if (highest_scoring_other_bid == 2) {
      seen_bid2 = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/true),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/true),
                  /*bid=*/2)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/3,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/2,
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/3,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/2,
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=2&bid=3"),
                      ReportWinUrl(/*bid=*/3, /*highest_scoring_other_bid=*/2,
                                   /*made_highest_scoring_other_bid=*/true)));
    } else {
      NOTREACHED();
    }
  }
}

// Multiple bids from different interest group owners get the second highest
// score, then `${madeHighestScoringOtherBid}` is always false.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingHighestScoringOtherBidFromDifferentOwners) {
  // Seen bid1 or bid2 being picked as highest scoring other bid.
  bool seen_bid1 = false;
  bool seen_bid2 = false;

  while (!seen_bid1 || !seen_bid2) {
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                           MakeBidScriptSupportsTie());
    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                           MakeAuctionScriptSupportsTie());

    std::vector<StorageInterestGroup> bidders;
    // Bidder1 and Bidder2 from different interest group owners both get second
    // highest score. Bidder3 got the highest score and won the auction.
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"1", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, /*name=*/"2", kBidder2Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, /*name=*/"3", kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad3.com")));

    const Result& res = RunAuctionAndWait(kSellerUrl, std::move(bidders));

    EXPECT_EQ(GURL("https://ad3.com/"), res.ad_url);
    EXPECT_EQ(4u, res.debug_loss_report_urls.size());
    EXPECT_EQ(2u, res.debug_win_report_urls.size());
    EXPECT_EQ(2u, res.report_urls.size());
    double highest_scoring_other_bid = 0.0;
    if (base::Contains(
            res.report_urls,
            "https://reporting.example.com/?highestScoringOtherBid=1&bid=3",
            &GURL::spec)) {
      highest_scoring_other_bid = 1;
    } else if (base::Contains(res.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=2&bid=3",
                              &GURL::spec)) {
      highest_scoring_other_bid = 2;
    }

    if (highest_scoring_other_bid == 1) {
      seen_bid1 = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/1,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=1&bid=3"),
                      ReportWinUrl(/*bid=*/3, /*highest_scoring_other_bid=*/1,
                                   /*made_highest_scoring_other_bid=*/false)));
    } else if (highest_scoring_other_bid == 2) {
      seen_bid2 = true;
      EXPECT_THAT(
          res.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2)));

      EXPECT_THAT(
          res.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/3,
                                     /*made_winning_bid=*/true,
                                     /*highest_scoring_other_bid=*/2,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(res.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/"
                           "?highestScoringOtherBid=2&bid=3"),
                      ReportWinUrl(/*bid=*/3, /*highest_scoring_other_bid=*/2,
                                   /*made_highest_scoring_other_bid=*/false)));
    } else {
      NOTREACHED();
    }
  }
}

// Should send loss report to seller and bidders when auction fails due to
// AllBidsRejected.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingAuctionFailAllBidsRejected) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl,
                    /*report_reject_reason=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    /*report_post_auction_signals=*/true,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl,
                    /*report_reject_reason=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScriptReject1And2WithDebugReporting(
          base::StrCat(
              {kSellerDebugLossReportBaseUrl, kPostAuctionSignalsPlaceholder}),
          base::StrCat(
              {kSellerDebugWinReportBaseUrl, kPostAuctionSignalsPlaceholder})));

  const Result& res = RunStandardAuction();
  // No winner since both bidders are rejected by seller.
  EXPECT_FALSE(res.winning_group_id);
  EXPECT_FALSE(res.ad_url);

  EXPECT_EQ(4u, res.debug_loss_report_urls.size());
  EXPECT_THAT(
      res.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(kBidder1DebugLossReportUrl, PostAuctionSignals(),
                         /*bid=*/absl::nullopt, "invalid-bid"),
          DebugReportUrl(kBidder2DebugLossReportUrl, PostAuctionSignals(),
                         /*bid=*/absl::nullopt, "bid-below-auction-floor"),
          DebugReportUrl(kSellerDebugLossReportBaseUrl, PostAuctionSignals(),
                         /*bid=*/1),
          DebugReportUrl(kSellerDebugLossReportBaseUrl, PostAuctionSignals(),
                         /*bid=*/2)));

  EXPECT_EQ(0u, res.debug_win_report_urls.size());
}

// Test win/loss reporting in a component auction with two components with one
// bidder each.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionTwoComponents) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeDecisionScript(
          kComponentSeller1Url,
          /*send_report_url=*/GURL("https://component1-report.test/"),
          /*bid_from_component_auction_wins=*/true,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://component1-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component1-win-reporting.test/",
          /*report_top_level_post_auction_signals*/ true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(
          kComponentSeller2Url,
          /*send_report_url=*/GURL("https://component2-report.test/"),
          /*bid_from_component_auction_wins=*/true,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://component2-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component2-win-reporting.test/",
          /*report_top_level_post_auction_signals*/ true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeDecisionScript(
          kSellerUrl,
          /*send_report_url=*/GURL("https://reporting.example.com"),
          /*bid_from_component_auction_wins=*/true,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://top-seller-loss-reporting.test/",
          /*debug_win_report_url=*/"https://top-seller-win-reporting.test/"));

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);

  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(kBidder1DebugLossReportUrl,
                         PostAuctionSignals(
                             /*winning_bid=*/1,
                             /*made_winning_bid=*/true,
                             /*highest_scoring_other_bid=*/0,
                             /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component1-loss-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/0,
                                 /*made_highest_scoring_other_bid=*/false),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/2,
                                 /*made_winning_bid=*/false),
              /*bid=*/1),
          DebugReportUrl("https://top-seller-loss-reporting.test/",
                         PostAuctionSignals(/*winning_bid=*/2,
                                            /*made_winning_bid=*/false),
                         /*bid=*/1)));

  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(
              kBidder2DebugWinReportUrl,
              PostAuctionSignals(/*winning_bid=*/2,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/0,
                                 /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component2-win-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/2,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/0,
                                 /*made_highest_scoring_other_bid=*/false),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/2,
                                 /*made_winning_bid=*/true),
              /*bid=*/2),
          DebugReportUrl("https://top-seller-win-reporting.test/",
                         PostAuctionSignals(/*winning_bid=*/2,
                                            /*made_winning_bid=*/true),
                         /*bid=*/2)));
}

// Test debug loss reporting in an auction with no winner. Component bidder 1 is
// rejected by component seller, and component bidder 2 is rejected by top-level
// seller. Component bidders get component auction's reject reason but not the
// top-level auction's.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionNoWinner) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeAuctionScriptReject1And2WithDebugReporting(
          "https://component1-loss-reporting.test/?",
          "https://component1-win-reporting.test/?"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl,
                    /*report_reject_reason=*/true));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(
          kComponentSeller2Url,
          /*send_report_url=*/GURL("https://component2-report.test/"),
          /*bid_from_component_auction_wins=*/false,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://component2-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component2-win-reporting.test/",
          /*report_top_level_post_auction_signals*/ true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl,
                    /*report_reject_reason=*/true));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      base::StringPrintf(R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  forDebuggingOnly.reportAdAuctionLoss(
      "https://top-seller-loss-reporting.test/%s&bid=" + bid);
  forDebuggingOnly.reportAdAuctionWin(
      "https://top-seller-win-reporting.test/%s&bid=" + bid);
  // While not setting `allowComponentAuction` will also reject the ad, it
  // also prevents loss reports and adds an error message, so need to set
  // it to true.
  return {
    desirability: 0,
    allowComponentAuction: true,
    rejectReason: "bid-below-auction-floor"
  };
}
  )",
                         kPostAuctionSignalsPlaceholder,
                         kPostAuctionSignalsPlaceholder));

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // No interest group won the auction.
  EXPECT_FALSE(result_.ad_url);

  // Component bidder 1 rejected by component auction gets its reject reason
  // "invalid-bid". Component bidders don't get the top-level auction's reject
  // reason.
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(kBidder1DebugLossReportUrl,
                         PostAuctionSignals(
                             /*winning_bid=*/0,
                             /*made_winning_bid=*/false,
                             /*highest_scoring_other_bid=*/0,
                             /*made_highest_scoring_other_bid=*/false),
                         /*bid=*/absl::nullopt, "invalid-bid"),
          GURL("https://component1-loss-reporting.test/?&bid=1"),
          DebugReportUrl(kBidder2DebugLossReportUrl,
                         PostAuctionSignals(
                             /*winning_bid=*/2,
                             /*made_winning_bid=*/true,
                             /*highest_scoring_other_bid=*/0,
                             /*made_highest_scoring_other_bid=*/false),
                         /*bid=*/absl::nullopt, "not-available"),
          ComponentSellerDebugReportUrl(
              "https://component2-loss-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/2,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/0,
                                 /*made_highest_scoring_other_bid=*/false),
              /*top_level_signals=*/
              PostAuctionSignals(),
              /*bid=*/2),
          DebugReportUrl("https://top-seller-loss-reporting.test/",
                         PostAuctionSignals(),
                         /*bid=*/2)));

  EXPECT_THAT(result_.debug_win_report_urls, testing::UnorderedElementsAre());
}

// Test win/loss reporting in a component auction with one component with two
// bidders.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionOneComponent) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1, kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeDecisionScript(
          kComponentSeller1Url,
          /*send_report_url=*/GURL("https://component-report.test/"),
          /*bid_from_component_auction_wins=*/true,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://component-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component-win-reporting.test/",
          /*report_top_level_post_auction_signals*/ true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"
           "&interestGroupNames=Ad+Platform"),
      kBidder1SignalsJson);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller1, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing"),
      kBidder2SignalsJson);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeDecisionScript(
          kSellerUrl,
          /*send_report_url=*/GURL("https://reporting.example.com"),
          /*bid_from_component_auction_wins=*/true,
          /*report_post_auction_signals=*/true,
          /*debug_loss_report_url=*/"https://top-seller-loss-reporting.test/",
          /*debug_win_report_url=*/"https://top-seller-win-reporting.test/"));

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 1 won the auction, since component auctions give lower bidders
  // higher desireability scores.
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);

  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(
              kBidder2DebugLossReportUrl,
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/false,
                                 /*highest_scoring_other_bid=*/0,
                                 /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component-loss-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/false,
                                 /*highest_scoring_other_bid=*/2,
                                 /*made_highest_scoring_other_bid=*/true),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/false),
              /*bid=*/2)));

  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(kBidder1DebugWinReportUrl,
                         PostAuctionSignals(
                             /*winning_bid=*/1,
                             /*made_winning_bid=*/true,
                             /*highest_scoring_other_bid=*/2,
                             /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component-win-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/2,
                                 /*made_highest_scoring_other_bid=*/false),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/1,
                                 /*made_winning_bid=*/true),
              /*bid=*/1),
          DebugReportUrl("https://top-seller-win-reporting.test/",
                         PostAuctionSignals(/*winning_bid=*/1,
                                            /*made_winning_bid=*/true),
                         /*bid=*/1)));
}

// Loss report URLs should be dropped when the seller worklet fails to load.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSellerWorkletFailToLoad) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    /*report_post_auction_signals*/ false,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    /*report_post_auction_signals*/ false,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));

  StartStandardAuction();
  // Wait for the bids to be generated.
  task_environment_.RunUntilIdle();
  // The seller script fails to load.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  // Wait for the auction to complete.
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));

  // There should be no debug win report URLs.
  EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  // Bidders' debug loss report URLs should be dropped as well.
  EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingBidderBadUrls) {
  const struct TestCase {
    const char* expected_error_message;
    absl::optional<GURL> bidder_debug_loss_report_url;
    absl::optional<GURL> bidder_debug_win_report_url;
  } kTestCases[] = {
      {
          "Invalid bidder debugging loss report URL",
          GURL("http://bidder-debug-loss-report.com/"),
          GURL("http://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging win report URL",
          GURL("https://bidder-debug-loss-report.com/"),
          GURL("http://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging loss report URL",
          GURL("file:///foo/"),
          GURL("https://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging loss report URL",
          GURL("Not a URL"),
          GURL("https://bidder-debug-win-report.com/"),
      },
  };
  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Only Bidder1 bids, to keep things simple.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, GURL("https://ad1.com/"),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        test_case.bidder_debug_loss_report_url,
        test_case.bidder_debug_win_report_url);
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();
    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_url);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());
    EXPECT_EQ("", result_.winning_group_ad_metadata);

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSellerBadUrls) {
  const struct TestCase {
    const char* expected_error_message;
    absl::optional<GURL> seller_debug_loss_report_url;
    absl::optional<GURL> seller_debug_win_report_url;
  } kTestCases[] = {
      {
          "Invalid seller debugging loss report URL",
          GURL("http://seller-debug-loss-report.com/"),
          GURL("http://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging win report URL",
          GURL("https://seller-debug-loss-report.com/"),
          GURL("http://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging loss report URL",
          GURL("file:///foo/"),
          GURL("https://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging loss report URL",
          GURL("Not a URL"),
          GURL("https://seller-debug-win-report.com/"),
      },
  };
  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Only Bidder1 bids, to keep things simple.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, GURL("https://ad1.com/"),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        GURL("https://bidder-debug-loss-report.com/"),
        GURL("https://bidder-debug-win-report.com/"));
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/10,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            test_case.seller_debug_loss_report_url,
            test_case.seller_debug_win_report_url, /*pa_requests=*/{},
            /*errors=*/{});
    auction_run_loop_->Run();
    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_url);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());
    EXPECT_EQ("", result_.winning_group_ad_metadata);

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingGoodAndBadUrl) {
  StartStandardAuctionWithMockService();
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Bidder1 returns a bid, which is then scored.
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/5, GURL("https://ad1.com/"),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt,
      GURL(kBidder1DebugLossReportUrl), GURL(kBidder1DebugWinReportUrl));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
  bidder1_worklet.reset();
  EXPECT_EQ("", TakeBadMessage());

  // Bidder2 returns a bid with an invalid debug report url. This could only
  // happen when the bidder worklet is compromised. It will be filtered out
  // and not be scored.
  bidder2_worklet->InvokeGenerateBidCallback(
      /*bid=*/10, GURL("https://ad2.com/"),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt,
      GURL("http://not-https.com/"), GURL(kBidder2DebugWinReportUrl));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
  bidder2_worklet.reset();
  EXPECT_EQ("Invalid bidder debugging loss report URL", TakeBadMessage());

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*scoring_signals_data_version=*/0,
          /*has_scoring_signals_data_version=*/false,
          GURL("https://seller-debug-loss-reporting.com/1"),
          GURL("https://seller-debug-win-reporting.com/1"), /*pa_requests=*/{},
          /*errors=*/{});

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder1 won. Bidder2 was filtered out as an invalid bid because its debug
  // loss report url is not a valid HTTPS URL.
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.winning_group_ad_metadata);

  // Bidder2 lost, but debug_loss_report_urls is empty because bidder2's
  // `debug_loss_report_url` is not a valid HTTPS URL. There's no seller debug
  // loss report url neither because bidder2 was filtered out and its bid was
  // not scored by seller.
  EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
  EXPECT_EQ(2u, result_.debug_win_report_urls.size());
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  kBidder1DebugWinReportUrl,
                  "https://seller-debug-win-reporting.com/1"));
}

// This tests the component auction state machine in the case of a large
// component auction. It uses the debug reporting API just to make sure all
// scripts were run to completion. The main thing this test serves to do is to
// validate the component auction state machinery works (Waits for all bids to
// be generated/scored, doesn't abort them early, doesn't wait for extra bids).
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       LargeComponentAuction) {
  const GURL kComponentSeller3Url{"https://component.seller3.test/baz.js"};

  // Seller URLs and number of bidders for each Auction.
  const struct {
    GURL seller_url;
    int num_bidders;
  } kSellerInfo[] = {
      {kSellerUrl, 2},
      {kComponentSeller1Url, 3},
      {kComponentSeller2Url, 5},
      {kComponentSeller3Url, 7},
  };

  // Set up auction, including bidder and seller Javascript responses,
  // AuctionConfig fields, etc.
  size_t bidder_index = 1;
  std::vector<StorageInterestGroup> all_bidders;
  for (size_t i = 0; i < std::size(kSellerInfo); ++i) {
    url::Origin seller = url::Origin::Create(kSellerInfo[i].seller_url);
    GURL send_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/report/", i));
    GURL debug_loss_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/loss/", i));
    GURL debug_win_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/win/", i));

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kSellerInfo[i].seller_url,
        MakeDecisionScript(kSellerInfo[i].seller_url, send_report_url,
                           /*bid_from_component_auction_wins=*/true,
                           /*report_post_auction_signals=*/false,
                           debug_loss_report_url.spec(),
                           debug_win_report_url.spec()));

    std::vector<url::Origin> bidders;
    for (int j = 0; j < kSellerInfo[i].num_bidders; ++j, ++bidder_index) {
      GURL bidder_url = GURL(
          base::StringPrintf("https://bidder%zu.test/script.js", bidder_index));
      url::Origin bidder = url::Origin::Create(bidder_url);
      GURL ad_url =
          GURL(base::StringPrintf("https://bidder%zu.ad.test/", bidder_index));
      GURL bidder_debug_loss_report_url = GURL(
          base::StringPrintf("https://bidder%zu.test/loss/", bidder_index));
      GURL bidder_debug_win_report_url =
          GURL(base::StringPrintf("https://bidder%zu.test/win/", bidder_index));

      all_bidders.emplace_back(MakeInterestGroup(
          bidder, /*name=*/base::NumberToString(bidder_index), bidder_url,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/{}, ad_url,
          /*ad_component_urls=*/absl::nullopt));

      auction_worklet::AddJavascriptResponse(
          &url_loader_factory_, bidder_url,
          MakeBidScript(
              seller, /*bid=*/base::NumberToString(bidder_index), ad_url.spec(),
              /*num_ad_components=*/0, bidder,
              /*interest_group_name=*/base::NumberToString(bidder_index),
              /*has_signals=*/false, /*signal_key=*/"", /*signal_val=*/"",
              /*report_post_auction_signals=*/false,
              bidder_debug_loss_report_url.spec(),
              bidder_debug_win_report_url.spec()));

      bidders.push_back(bidder);
    }

    // For the top-most auction, only need to set `interest_group_buyers_`. For
    // others, need to append to `component_auctions_`.
    if (kSellerInfo[i].seller_url == kSellerUrl) {
      interest_group_buyers_ = std::move(bidders);
    } else {
      component_auctions_.emplace_back(
          CreateAuctionConfig(kSellerInfo[i].seller_url, std::move(bidders)));
    }
  }

  StartAuction(kSellerInfo[0].seller_url, std::move(all_bidders));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 11 won - the first bidder for the third component auction. Higher
  // bidders bid more, but component sellers use a script that favors lower
  // bidders, while the top-level seller favors higher bidders.
  EXPECT_EQ(GURL("https://bidder11.ad.test/"), result_.ad_url);

  // Top seller doesn't report a loss, since it never saw the bid from the
  // second bidder.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  // kSeller's bidders.
                  GURL("https://bidder1.test/loss/"),
                  GURL("https://seller0.test/loss/1"),
                  GURL("https://bidder2.test/loss/"),
                  GURL("https://seller0.test/loss/2"),
                  // kComponentSeller1's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  GURL("https://bidder3.test/loss/"),
                  GURL("https://seller1.test/loss/3"),
                  GURL("https://seller0.test/loss/3"),
                  GURL("https://bidder4.test/loss/"),
                  GURL("https://seller1.test/loss/4"),
                  GURL("https://bidder5.test/loss/"),
                  GURL("https://seller1.test/loss/5"),
                  // kComponentSeller2's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  GURL("https://bidder6.test/loss/"),
                  GURL("https://seller2.test/loss/6"),
                  GURL("https://seller0.test/loss/6"),
                  GURL("https://bidder7.test/loss/"),
                  GURL("https://seller2.test/loss/7"),
                  GURL("https://bidder8.test/loss/"),
                  GURL("https://seller2.test/loss/8"),
                  GURL("https://bidder9.test/loss/"),
                  GURL("https://seller2.test/loss/9"),
                  GURL("https://bidder10.test/loss/"),
                  GURL("https://seller2.test/loss/10"),
                  // kComponentSeller3's bidders. Bidder 11 won the entire
                  // auction, all the others lose component seller 3's auction.
                  GURL("https://bidder12.test/loss/"),
                  GURL("https://seller3.test/loss/12"),
                  GURL("https://bidder13.test/loss/"),
                  GURL("https://seller3.test/loss/13"),
                  GURL("https://bidder14.test/loss/"),
                  GURL("https://seller3.test/loss/14"),
                  GURL("https://bidder15.test/loss/"),
                  GURL("https://seller3.test/loss/15"),
                  GURL("https://bidder16.test/loss/"),
                  GURL("https://seller3.test/loss/16"),
                  GURL("https://bidder17.test/loss/"),
                  GURL("https://seller3.test/loss/17")));

  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(GURL("https://bidder11.test/win/"),
                                    GURL("https://seller3.test/win/11"),
                                    GURL("https://seller0.test/win/11")));
}

// Reject reason returned by scoreAd() for a rejected bid can be reported to the
// bidder through its debug loss report URL.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       RejectedBidGetsRejectReason) {
  for (const std::string& reject_reason :
       {"not-available", "invalid-bid", "bid-below-auction-floor",
        "pending-approval-by-exchange", "disapproved-by-exchange",
        "blocked-by-publisher", "language-exclusions", "category-exclusions"}) {
    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder1Url,
        MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                      kBidder1, kBidder1Name,
                      /*has_signals=*/false, "k1", "a",
                      /*report_post_auction_signals=*/false,
                      kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl,
                      /*report_reject_reason=*/true));
    // Bidder 2 will get a negative score from scoreAd().
    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder2Url,
        MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                      kBidder2, kBidder2Name,
                      /*has_signals=*/false, "l2", "b",
                      /*report_post_auction_signals=*/false,
                      kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl,
                      /*report_reject_reason=*/true));
    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kSellerUrl,
        MakeAuctionScriptReject2(reject_reason));

    const Result& res = RunStandardAuction();
    // Bidder 1 won the auction.
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);

    EXPECT_EQ(1u, res.debug_loss_report_urls.size());
    // Seller rejected bidder 2 and returned the reject reason which were then
    // reported to bidder 2 through its loss report URL.
    EXPECT_THAT(res.debug_loss_report_urls,
                testing::UnorderedElementsAre(base::StringPrintf(
                    "https://bidder2-debug-loss-reporting.com/"
                    "?rejectReason=%s",
                    reject_reason.c_str())));

    EXPECT_EQ(1u, res.debug_win_report_urls.size());
    EXPECT_THAT(res.debug_win_report_urls,
                testing::UnorderedElementsAre(kBidder1DebugWinReportUrl));
  }
}

// Reject reason returned by scoreAd() for a bid whose score is positive is
// ignored and will not be reported to the bidder.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       rejectReasonIgnoredForPositiveBid) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    /*report_post_auction_signals=*/false,
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl,
                    /*report_reject_reason=*/true));

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "3", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    /*report_post_auction_signals=*/false,
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl,
                    /*report_reject_reason=*/true));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptReject2());

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  EXPECT_EQ(1u, res.debug_loss_report_urls.size());
  // Reject reason returned by scoreAd() for bidder 1 should be ignored and
  // reported as "not-available" in debug loss report URL, because the bid gets
  // a positive score thus not rejected by seller.
  EXPECT_THAT(
      res.debug_loss_report_urls,
      testing::UnorderedElementsAre("https://bidder1-debug-loss-reporting.com/"
                                    "?rejectReason=not-available"));

  EXPECT_EQ(1u, res.debug_win_report_urls.size());
  EXPECT_THAT(res.debug_win_report_urls,
              testing::UnorderedElementsAre(kBidder2DebugWinReportUrl));
}

// Only bidders' debug loss report URLs support macro ${rejectReason}.
// Bidders' debug win report URLs and sellers' debug loss/win report URLs does
// not.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       rejectReasonInBidderDebugLossReportOnly) {
  const char kBidder1Script[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://bidder1-debug-loss-reporting.com/?reason=${rejectReason}');
      forDebuggingOnly.reportAdAuctionWin(
          'https://bidder1-debug-win-reporting.com/?reason=${rejectReason}');
      return {
        bid: 1,
        render: interestGroup.ads[0].renderUrl
      };
    }

    // Prevent an error about this method not existing.
    function reportWin() {}
  )";

  const char kBidder2Script[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://bidder2-debug-loss-reporting.com/?reason=${rejectReason}');
      forDebuggingOnly.reportAdAuctionWin(
          'https://bidder2-debug-win-reporting.com/?reason=${rejectReason}');
      return {
        bid: 2,
        render: interestGroup.ads[0].renderUrl
      };
    }

    // Prevent an error about this method not existing.
    function reportWin() {}
  )";

  // Desirability is -1 if bid is 1, otherwise is bid.
  const char kSellerScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://seller-debug-loss-reporting.com/?reason=${rejectReason}');
      forDebuggingOnly.reportAdAuctionWin(
          'https://seller-debug-win-reporting.com/?reason=${rejectReason}');
      if (bid == 1) {
        return {desirability: -1, rejectReason: 'invalid-bid'}
      } else {
        return bid;
      }
    }

    // Prevent an error about this method not existing.
    function reportResult() {}
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidder1Script);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         kBidder2Script);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  // Only bidder's debug loss report supports macro ${rejectReason}.
  EXPECT_EQ(2u, res.debug_loss_report_urls.size());
  EXPECT_THAT(
      res.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          "https://bidder1-debug-loss-reporting.com/?reason=invalid-bid",
          "https://seller-debug-loss-reporting.com/"
          "?reason=${rejectReason}"));
  EXPECT_EQ(2u, res.debug_win_report_urls.size());
  EXPECT_THAT(
      res.debug_win_report_urls,
      testing::UnorderedElementsAre("https://bidder2-debug-win-reporting.com/"
                                    "?reason=${rejectReason}",
                                    "https://seller-debug-win-reporting.com/"
                                    "?reason=${rejectReason}"));
}

// When scoreAd() does not return a reject reason, report it as "not-available"
// in bidder's loss report URL as default.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       SellerNotReturningRejectReason) {
  const char kBidderScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://bidder-debug-loss-reporting.com/?reason=${rejectReason}');
      return {
        bid: 1,
        render: interestGroup.ads[0].renderUrl
      };
    }

    // Prevent an error about this method not existing.
    function reportWin() {}
  )";

  const char kSellerScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      return {desirability: -1};
    }

    // Prevent an error about this method not existing.
    function reportResult() {}
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidderScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  const Result& res = RunStandardAuction();

  EXPECT_EQ(1u, res.debug_loss_report_urls.size());
  EXPECT_THAT(
      res.debug_loss_report_urls,
      testing::UnorderedElementsAre("https://bidder-debug-loss-reporting.com/"
                                    "?reason=not-available"));
  EXPECT_EQ(0u, res.debug_win_report_urls.size());
}

// Disable private aggregation API.
class AuctionRunnerPrivateAggregationAPIDisabledTest
    : public AuctionRunnerTest {
 public:
  AuctionRunnerPrivateAggregationAPIDisabledTest()
      : AuctionRunnerTest(/*should_enable_private_aggregation=*/false) {}
};

TEST_F(AuctionRunnerPrivateAggregationAPIDisabledTest, ReportsNotSent) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(/*report_post_auction_signals=*/true));
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=k1,k2"),
      kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"),
      kBidder2SignalsJson);

  const Result& res = RunStandardAuction();
  EXPECT_TRUE(res.private_aggregation_requests.empty());
}

class AuctionRunnerKAnonTest : public AuctionRunnerTest,
                               public ::testing::WithParamInterface<
                                   auction_worklet::mojom::KAnonymityBidMode> {
 public:
  AuctionRunnerKAnonTest()
      : AuctionRunnerTest(/*should_enable_private_aggregation=*/true,
                          kanon_mode()) {}

  auction_worklet::mojom::KAnonymityBidMode kanon_mode() { return GetParam(); }
};

TEST_P(AuctionRunnerKAnonTest, SingleNonKAnon) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeConstBidScript(1, "https://ad1.com") + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  // No k-anon authorizations.
  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();
  EXPECT_THAT(
      result_.k_anon_keys_to_join,
      testing::UnorderedElementsAre(
          KAnonKeyForAdBid(bidders[0].interest_group,
                           bidders[0].interest_group.ads.value()[0].render_url),
          KAnonKeyForAdNameReporting(
              bidders[0].interest_group,
              bidders[0].interest_group.ads.value()[0])));
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);
  switch (kanon_mode()) {
    case auction_worklet::mojom::KAnonymityBidMode::kNone:
      ASSERT_TRUE(result_.ad_url.has_value());
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
      EXPECT_FALSE(result_.ad_url.has_value());
      EXPECT_THAT(
          result_.errors,
          testing::ElementsAre(
              "https://adplatform.com/offers.js generateBid() bid render URL "
              "'https://ad1.com/' isn't one of the registered creative URLs."));
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
      ASSERT_TRUE(result_.ad_url.has_value());
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      break;
  }
}

TEST_P(AuctionRunnerKAnonTest, SingleKAnon) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeConstBidScript(1, "https://ad1.com") + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

  // Authorize the ad.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_url.has_value());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_THAT(
      result_.k_anon_keys_to_join,
      testing::UnorderedElementsAre(
          KAnonKeyForAdBid(bidders[0].interest_group,
                           bidders[0].interest_group.ads.value()[0].render_url),
          KAnonKeyForAdNameReporting(
              bidders[0].interest_group,
              bidders[0].interest_group.ads.value()[0])));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon",
      kanon_mode() != auction_worklet::mojom::KAnonymityBidMode::kNone, 1);
}

// Test that k-anonymity for ads with ad components is handled correctly:
//  - All components must be k-anonymous to be eligible.
//  - All components of the winner will be reported as joined.
// Runs an auction with two groups where each gives a bid with two component ads
// and all ad URLs except one component ad URL of the second bidder are
// k-anonymous. When k-anonymity is enforced the first interest group should
// win, despite having a lower bid.
TEST_P(AuctionRunnerKAnonTest, ComponentURLs) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeFilteringBidScript(1) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeFilteringBidScript(2) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com"),
      absl::make_optional(std::vector<GURL>(
          {GURL("https://ad1.com/1"), GURL("https://ad1.com/2")}))));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com"),
      absl::make_optional(std::vector<GURL>(
          {GURL("https://ad2.com/1"), GURL("https://ad2.com/2")}))));

  // Authorize everything except for one of the components in ad2.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);
  AuthorizeKAnon(bidders[0].interest_group.ad_components.value()[0],
                 "https://ad1.com/1", bidders[0]);
  AuthorizeKAnon(bidders[0].interest_group.ad_components.value()[1],
                 "https://ad1.com/2", bidders[0]);
  AuthorizeKAnon(bidders[1].interest_group.ads.value()[0], "https://ad2.com",
                 bidders[1]);
  AuthorizeKAnon(bidders[1].interest_group.ad_components.value()[0],
                 "https://ad2.com/1", bidders[1]);

  std::vector<std::string> ad1_k_anon_keys = {
      KAnonKeyForAdBid(bidders[0].interest_group,
                       bidders[0].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[0].interest_group,
                                 bidders[0].interest_group.ads.value()[0]),
      KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ad_components.value()[0].render_url),
      KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ad_components.value()[1].render_url),
  };

  std::vector<std::string> ad2_k_anon_keys = {
      KAnonKeyForAdBid(bidders[1].interest_group,
                       bidders[1].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[1].interest_group,
                                 bidders[1].interest_group.ads.value()[0]),
      KAnonKeyForAdBid(
          bidders[1].interest_group,
          bidders[1].interest_group.ad_components.value()[0].render_url),
      KAnonKeyForAdBid(
          bidders[1].interest_group,
          bidders[1].interest_group.ad_components.value()[1].render_url),
  };

  for (bool run_as_component : {false, true}) {
    SCOPED_TRACE(run_as_component);

    if (run_as_component) {
      component_auctions_.emplace_back(
          CreateAuctionConfig(kSellerUrl, {{kBidder1, kBidder2}}));
      interest_group_buyers_->clear();
    } else {
      DCHECK(!interest_group_buyers_->empty());
    }

    StartAuction(kSellerUrl, bidders);
    auction_run_loop_->Run();
    ASSERT_TRUE(result_.ad_url.has_value());

    GURL expected_seller_report_url;
    std::vector<GURL> expected_report_urls;
    base::flat_set<std::string> expected_k_anon_keys_to_join;
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);
    switch (kanon_mode()) {
      case auction_worklet::mojom::KAnonymityBidMode::kNone:
        // k-anon support is turned off entirely, so ad2 wins, and no other URLs
        // are set.
        EXPECT_THAT(result_.errors, testing::ElementsAre());
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
        EXPECT_THAT(result_.ad_component_urls,
                    testing::UnorderedElementsAre(GURL("https://ad2.com/1"),
                                                  GURL("https://ad2.com/2")));
        // Only join for ad2
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());

        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                         /*made_highest_scoring_other_bid=*/false));
        break;

      case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
        // k-anon requirement meands ad1 wins, but we also report ad2 as what
        // would have won had it been authorized.
        EXPECT_THAT(result_.errors,
                    testing::ElementsAre(
                        "https://anotheradthing.com/bids.js generateBid() bid "
                        "adComponents URL 'https://ad2.com/2' isn't one of the "
                        "registered creative URLs."));
        EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
        EXPECT_THAT(result_.ad_component_urls,
                    testing::UnorderedElementsAre(GURL("https://ad1.com/1"),
                                                  GURL("https://ad1.com/2")));

        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/1");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/1, /*highest_scoring_other_bid=*/0,
                         /*made_highest_scoring_other_bid=*/false));
        break;

      case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
        // Winner is ad2.com, disregarding k-anonymity, but we also report that
        // if we did care about it, ad1.com would have won.
        EXPECT_THAT(result_.errors, testing::ElementsAre());
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
        EXPECT_THAT(result_.ad_component_urls,
                    testing::UnorderedElementsAre(GURL("https://ad2.com/1"),
                                                  GURL("https://ad2.com/2")));

        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                         /*made_highest_scoring_other_bid=*/false));
        break;
    }

    EXPECT_THAT(result_.k_anon_keys_to_join, testing::UnorderedElementsAreArray(
                                                 expected_k_anon_keys_to_join));

    expected_report_urls.push_back(expected_seller_report_url);
    if (run_as_component) {
      // Both top-level and component auction report this.
      expected_report_urls.push_back(expected_seller_report_url);
    }
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAreArray(expected_report_urls));
  }
}

// Test that if there are two ads, one k-anonymous and one not k-anonymous that
// the correct ad is the winner (depends on `kanon_mode()`). Note that the
// non-k-anonymous ad bids higher so that it wins when k-anonymity is not
// enforced.
TEST_P(AuctionRunnerKAnonTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeFilteringBidScript(1) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeFilteringBidScript(2) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  // Authorize only ad 1.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      KAnonKeyForAdBid(bidders[0].interest_group,
                       bidders[0].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[0].interest_group,
                                 bidders[0].interest_group.ads.value()[0]),
  };
  std::vector<std::string> ad2_k_anon_keys = {
      KAnonKeyForAdBid(bidders[1].interest_group,
                       bidders[1].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[1].interest_group,
                                 bidders[1].interest_group.ads.value()[0]),
  };

  for (bool run_as_component : {false, true}) {
    SCOPED_TRACE(run_as_component);

    if (run_as_component) {
      component_auctions_.emplace_back(
          CreateAuctionConfig(kSellerUrl, {{kBidder1, kBidder2}}));
      interest_group_buyers_->clear();
    } else {
      DCHECK(!interest_group_buyers_->empty());
    }

    StartAuction(kSellerUrl, bidders);
    auction_run_loop_->Run();
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    ASSERT_TRUE(result_.ad_url.has_value());
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);

    base::flat_set<std::string> expected_k_anon_keys_to_join;
    GURL expected_seller_report_url;
    std::vector<GURL> expected_report_urls;
    switch (kanon_mode()) {
      case auction_worklet::mojom::KAnonymityBidMode::kNone:
        // k-anon support is turned off entirely, so ad2 wins, and no other URLs
        // are set.
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                         /*made_highest_scoring_other_bid=*/false));
        break;

      case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
        // k-anon requirement meands ad1 wins, but we also report ad2 as what
        // would have won had it been authorized.
        EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/1");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/1, /*highest_scoring_other_bid=*/0,
                         /*made_highest_scoring_other_bid=*/false));
        break;

      case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
        // Winner is ad2.com, disregarding k-anonymity, but we also report that
        // if we did care about it, ad1.com would have won.
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(
            ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                         /*made_highest_scoring_other_bid=*/false));
        break;
    }
    EXPECT_THAT(result_.k_anon_keys_to_join, testing::UnorderedElementsAreArray(
                                                 expected_k_anon_keys_to_join));

    expected_report_urls.push_back(expected_seller_report_url);
    if (run_as_component) {
      // Both top-level and component auction report this.
      expected_report_urls.push_back(expected_seller_report_url);
    }
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAreArray(expected_report_urls));
  }
}

// Test where the k-anon ad has a higher bid.
TEST_P(AuctionRunnerKAnonTest, KAnonHigher) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeFilteringBidScript(2) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeFilteringBidScript(1) + kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad2.com")));

  // Authorize only ad 1.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      KAnonKeyForAdBid(bidders[0].interest_group,
                       bidders[0].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[0].interest_group,
                                 bidders[0].interest_group.ads.value()[0]),
  };

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_url.has_value());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
  EXPECT_THAT(result_.k_anon_keys_to_join,
              testing::UnorderedElementsAreArray(ad1_k_anon_keys));

  std::vector<GURL> expected_report_urls;
  expected_report_urls.emplace_back("https://reporting.example.com/2");
  switch (kanon_mode()) {
    case auction_worklet::mojom::KAnonymityBidMode::kNone:
      // k-anon support is turned off entirely, so no other URLs
      // are set.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);
      expected_report_urls.push_back(
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                       /*made_highest_scoring_other_bid=*/false));
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
      // The enforced winner is the same, but there is no runner-up.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", true, 1);
      expected_report_urls.push_back(
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/0,
                       /*made_highest_scoring_other_bid=*/false));
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
      // ad1.com also wins in the simulated mode.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", true, 1);
      expected_report_urls.push_back(
          ReportWinUrl(/*bid=*/2, /*highest_scoring_other_bid=*/1,
                       /*made_highest_scoring_other_bid=*/false));
      break;
  }
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAreArray(expected_report_urls));
}

// Test for where the same IG makes different bids based on k-anon enforcement,
// rather than potentially not bidding at all. The non-k-anon bid is higher.
TEST_P(AuctionRunnerKAnonTest, DifferentBids) {
  // A simple bid script that returns the last ad in the input and the length of
  // ads array as the bid.
  const char kAdsArraySensitiveBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                          trustedBiddingSignals, browserSignals) {
        return {ad: {},
                bid: interestGroup.ads.length,
                render: interestGroup.ads.pop().renderUrl,
                allowComponentAuction: true};
      }
  )";

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      std::string(kAdsArraySensitiveBidScript) + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders.back().interest_group.ads->emplace_back(GURL("https://ad2.com"),
                                                  /*metadata=*/absl::nullopt);

  // Authorize only ad 1.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      KAnonKeyForAdBid(bidders[0].interest_group,
                       bidders[0].interest_group.ads.value()[0].render_url),
      KAnonKeyForAdNameReporting(bidders[0].interest_group,
                                 bidders[0].interest_group.ads.value()[0]),
  };
  std::vector<std::string> ad2_k_anon_keys = {
      KAnonKeyForAdBid(bidders[0].interest_group,
                       bidders[0].interest_group.ads.value()[1].render_url),
      KAnonKeyForAdNameReporting(bidders[0].interest_group,
                                 bidders[0].interest_group.ads.value()[1]),
  };

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_url.has_value());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);

  base::flat_set<std::string> expected_k_anon_keys_to_join;
  switch (kanon_mode()) {
    case auction_worklet::mojom::KAnonymityBidMode::kNone:
      // Don't care about k-anonymity: ad2 wins, nothing else is reporter.
      EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/2"));
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
      // Ad 2 is what got blocked by enforcement --- if it were authorized, it
      // would win.
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_url.value());
      expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                          ad1_k_anon_keys.end());
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/1"));
      break;

    case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
      // Winner is ad2.com, disregarding k-anonymity, but we also report that
      // if we did care about it, ad1.com would have won.
      EXPECT_EQ(GURL("https://ad2.com"), result_.ad_url.value());
      expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                          ad1_k_anon_keys.end());
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/2"));
      break;
  }
  EXPECT_THAT(result_.k_anon_keys_to_join,
              testing::UnorderedElementsAreArray(expected_k_anon_keys_to_join));
}

// Test to make sure that k-anon info doesn't get incorrectly reported when
// an auction gets interrupted.
TEST_P(AuctionRunnerKAnonTest, FailureHandling) {
  // As in DifferentBids, this script produces different k-anon and n-k-anon
  // bids; it's helpful for this test since
  const char kAdsArraySensitiveBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {ad: {},
                bid: interestGroup.ads.length,
                render: interestGroup.ads.pop().renderUrl,
                allowComponentAuction: true};
      }
  )";

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      std::string(kAdsArraySensitiveBidScript) + kSimpleReportWin);
  // No script for bidder 2, so it never finishes.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
  bidders.back().interest_group.ads->emplace_back(GURL("https://ad2.com"),
                                                  /*metadata=*/absl::nullopt);
  bidders.emplace_back(MakeInterestGroup(
      kBidder2, kBidder2Name, kBidder2Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad3.com")));

  // Authorize only ad 1.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  // Run the auction, and simulate it being interrupted by navigating away.
  StartAuction(kSellerUrl, bidders);
  task_environment_.RunUntilIdle();
  auction_runner_->FailAuction(/*manually_aborted=*/false);

  EXPECT_THAT(result_.errors, testing::ElementsAre());

  // Should not have anything to report.
  EXPECT_FALSE(result_.ad_url.has_value());
  EXPECT_THAT(result_.k_anon_keys_to_join, testing::ElementsAre());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 0);
}

TEST_P(AuctionRunnerKAnonTest, MojoValidation) {
  const struct TestCase {
    std::set<auction_worklet::mojom::KAnonymityBidMode> run_in_modes;
    const char* expected_error_message;
    GURL render_url;
    auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_bid;
    bool expect_winner;
  } kTestCases[] = {
      // Sending a k-anon enforced bid when it should just match the
      // non-enforced bid.
      {{auction_worklet::mojom::KAnonymityBidMode::kEnforce,
        auction_worklet::mojom::KAnonymityBidMode::kSimulate},
       "Received different k-anon bid when unenforced bid already k-anon",
       GURL("https://ad1.com"),
       auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
           auction_worklet::mojom::BidderWorkletBid::New(
               "ad", 5.0, GURL("https://ad2.com"),
               /*ad_component_urls=*/absl::nullopt, base::TimeDelta())),
       /*expect_winner=*/true},
      // A non-k-anon bid as k-anon one. Enforced, so auction fails.
      {
          {auction_worklet::mojom::KAnonymityBidMode::kEnforce},
          "Bid render URL must be a valid ad URL",
          GURL("https://ad2.com"),
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
              auction_worklet::mojom::BidderWorkletBid::New(
                  "ad", 5.0, GURL("https://ad2.com"),
                  /*ad_component_urls=*/absl::nullopt, base::TimeDelta())),
          /*expect_winner=*/false,
      },
      // A non-k-anon bid as k-anon one. Simulate, so auction succeeds.
      {
          {auction_worklet::mojom::KAnonymityBidMode::kSimulate},
          "Bid render URL must be a valid ad URL",
          GURL("https://ad2.com"),
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
              auction_worklet::mojom::BidderWorkletBid::New(
                  "ad", 5.0, GURL("https://ad2.com"),
                  /*ad_component_urls=*/absl::nullopt, base::TimeDelta())),
          /*expect_winner=*/true,
      },
      // Sending k-anon data when it's not even on.
      {
          {auction_worklet::mojom::KAnonymityBidMode::kNone},
          "Received k-anon bid data when not considering k-anon",
          GURL("https://ad1.com"),
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::
              NewSameAsNonEnforced(nullptr),
          /*expect_winner=*/true,
      }};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{"k1", "k2"}, GURL("https://ad1.com")));
  bidders.back().interest_group.ads->emplace_back(GURL("https://ad2.com"),
                                                  /*metadata=*/absl::nullopt);
  // Authorize only ad 1.
  AuthorizeKAnon(bidders[0].interest_group.ads.value()[0], "https://ad1.com",
                 bidders[0]);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.expected_error_message);
    if (test_case.run_in_modes.find(kanon_mode()) ==
        test_case.run_in_modes.end())
      continue;

    UseMockWorkletService();
    StartAuction(kSellerUrl, bidders);
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/1, /*num_sellers=*/1);
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder1_worklet->InvokeGenerateBidCallback(1.0, test_case.render_url,
                                               test_case.mojo_bid.Clone());

    // All of these tests only get one scoreAd, since k-anon bid is invalid.
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(1, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/11,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*scoring_signals_data_version=*/0,
            /*has_scoring_signals_data_version=*/false,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*errors=*/{});

    // Finish the auction.
    if (test_case.expect_winner) {
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder1_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback();
    }
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());
    EXPECT_EQ(test_case.expect_winner, result_.ad_url.has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AuctionRunnerKAnonTest,
    ::testing::Values(auction_worklet::mojom::KAnonymityBidMode::kNone,
                      auction_worklet::mojom::KAnonymityBidMode::kEnforce,
                      auction_worklet::mojom::KAnonymityBidMode::kSimulate));

}  // namespace
}  // namespace content

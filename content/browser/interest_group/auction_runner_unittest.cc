// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/additional_bids_test_util.h"
#include "content/browser/interest_group/auction_nonce_manager.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_result.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/mock_auction_process_manager.h"
#include "content/browser/interest_group/test_interest_group_manager_impl.h"
#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/page.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/test/test_content_browser_client.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-shared.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"

using auction_worklet::TestDevToolsAgentClient;
using testing::HasSubstr;

namespace content {
namespace {

using InterestGroupKey = blink::InterestGroupKey;
using PostAuctionSignals = InterestGroupAuction::PostAuctionSignals;
using blink::FencedFrame::ReportingDestination;
using PrivateAggregationRequests = AuctionRunner::PrivateAggregationRequests;

// Same as the key in ad_auction_service_impl_unittest.cc.
// Randomly generated using EVP_HPKE_KEY_generate.
const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

// ED25519 key pairs used to test additional bids.
const char kBase64PublicKey1[] = "x9GNFldc5zosYCL7ROTIWrVB7vk07vgRAPf68H/6MG8=";

const uint8_t kPrivateKey1[] = {
    0x44, 0x0a, 0xb0, 0x1f, 0xec, 0x87, 0x57, 0xce, 0x42, 0x17, 0x91,
    0xa3, 0x67, 0xa4, 0x3f, 0x2b, 0xf6, 0x2c, 0xf5, 0xe3, 0x0a, 0x88,
    0x4e, 0x22, 0x94, 0xf0, 0x59, 0x01, 0x1d, 0x53, 0x2b, 0xa0, 0xc7,
    0xd1, 0x8d, 0x16, 0x57, 0x5c, 0xe7, 0x3a, 0x2c, 0x60, 0x22, 0xfb,
    0x44, 0xe4, 0xc8, 0x5a, 0xb5, 0x41, 0xee, 0xf9, 0x34, 0xee, 0xf8,
    0x11, 0x00, 0xf7, 0xfa, 0xf0, 0x7f, 0xfa, 0x30, 0x6f};

const char kBase64PublicKey2[] = "pjSy2WcByYsZ/Aqomo88tiqtFOpPpRd+4wTtjQ2vZhE=";

const uint8_t kPrivateKey2[] = {
    0x6b, 0x42, 0xed, 0x42, 0x2e, 0x8d, 0x01, 0xf7, 0x6a, 0xc5, 0xc6,
    0x26, 0x4a, 0xeb, 0xfb, 0xe5, 0xbc, 0x31, 0xd2, 0x49, 0x19, 0x92,
    0x63, 0x5e, 0x95, 0x1c, 0x99, 0x7f, 0xf2, 0xd2, 0x77, 0x87, 0xa6,
    0x34, 0xb2, 0xd9, 0x67, 0x01, 0xc9, 0x8b, 0x19, 0xfc, 0x0a, 0xa8,
    0x9a, 0x8f, 0x3c, 0xb6, 0x2a, 0xad, 0x14, 0xea, 0x4f, 0xa5, 0x17,
    0x7e, 0xe3, 0x04, 0xed, 0x8d, 0x0d, 0xaf, 0x66, 0x11};

std::string kBidder1Name{"Ad Platform"};
const std::string kBidder1NameAlt{"Ad Platform Alt"};
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
    "?winningBid=${winningBid}&"
    "winningBidCurrency=${winningBidCurrency}&madeWinningBid=${madeWinningBid}&"
    "highestScoringOtherBid=${highestScoringOtherBid}&"
    "highestScoringOtherBidCurrency=${highestScoringOtherBidCurrency}&"
    "madeHighestScoringOtherBid=${madeHighestScoringOtherBid}";

const char kTopLevelPostAuctionSignalsPlaceholder[] =
    "topLevelWinningBid=${topLevelWinningBid}&"
    "topLevelWinningBidCurrency=${topLevelWinningBidCurrency}&"
    "topLevelMadeWinningBid=${topLevelMadeWinningBid}";

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedGenerateBidPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/1,
                        /*value=*/2)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedKAnonFailureGenerateBidPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/static_cast<uint64_t>(
                            auction_worklet::mojom::RejectReason::
                                kBelowKAnonThreshold),
                        /*value=*/0)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedReportWinPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/3,
                        /*value=*/4)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedScoreAdPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/5,
                        /*value=*/6)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

const auction_worklet::mojom::PrivateAggregationRequestPtr
    kExpectedReportResultPrivateAggregationRequest =
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/7,
                        /*value=*/8)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New());

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
  constexpr char kBidScript[] = R"(
    const seller = "%s";
    const bid = %s;
    const renderURL = "%s";
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
                         "renderURL": "data for " + renderURL,
                         "seller": seller},
                    bid: bid,
                    bidCurrency: 'USD',
                    render: renderURL,
                    // Only need to allow component auction participation when
                    // `topLevelSeller` is populated.
                    allowComponentAuction: "topLevelSeller" in browserSignals};
      if (interestGroup.adComponents) {
        result.adComponents = [interestGroup.adComponents[0].renderURL];
        result.ad.adComponentsUrl = interestGroup.adComponents[0].renderURL;
      }

      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      // The actual priority should be hidden from the worklet.
      if (interestGroup.priority !== undefined)
        throw new Error("wrong priority: " + interestGroup.priority);
      // None of these tests set a updateURL. Non-empty values are tested by
      // browser tests.
      if ("updateURL" in interestGroup)
        throw new Error("Unexpected updateURL");
      if (interestGroup.ads.length != 1)
        throw new Error("wrong interestGroup.ads length");
      if (interestGroup.ads[0].renderURL != renderURL)
        throw new Error("wrong interestGroup.ads URL");
      if (numAdComponents == 0) {
        if (interestGroup.adComponents !== undefined)
          throw new Error("Non-empty adComponents");
      } else {
        if (interestGroup.adComponents.length !== numAdComponents)
          throw new Error("Wrong adComponents length");
        for (let i = 0; i < numAdComponents; ++i) {
          if (interestGroup.adComponents[i].renderURL !=
              renderURL.slice(0, -1) + "-component" + (i+1) + ".com/") {
            throw new Error("Wrong adComponents renderURL");
          }
        }
      }
      // Skip the `perBuyerSignals` check if the interest group name matches
      // the bid. This is for auctions that use more than the two standard
      // bidders, since there's currently no way to inject new perBuyerSignals
      // into the top-level auction.
      // TODO(mmenke): Worth fixing that?
      if (interestGroupName !== bid + '') {
        if (perBuyerSignals === null) {
          throw new Error("unexpectedly perBuyerSignals is null");
        }
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
      if (browserSignals.prevWinsMs.length !== 3)
        throw new Error("prevWinsMs");
      for (let i = 0; i < browserSignals.prevWinsMs.length; ++i) {
        if (!(browserSignals.prevWinsMs[i] instanceof Array))
          throw new Error("prevWinsMs entry not an array");
        if (typeof browserSignals.prevWinsMs[i][0] != "number")
          throw new Error("Not a Number in prevWin?");
        if (browserSignals.prevWinsMs[i][1].winner !== -i)
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
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      // Private aggregation requests with non-reserved event types will only be
      // reported for a winning bidder.
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 10n, value: 20 + bid});
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
      if (sellerSignals.renderURL !== renderURL)
        throw new Error("wrong renderURL");
      if (sellerSignals.bid !== bid) {
        throw new Error("wrong bid, bidder:" + bid +
                        " seller:" + sellerSignals.bid);
      }
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

      if (browserSignals.renderURL !== renderURL)
        throw new Error("wrong browserSignals.renderURL");
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
            '&highestScoringOtherBidCurrency=' +
            browserSignals.highestScoringOtherBidCurrency +
            '&madeHighestScoringOtherBid=' +
            browserSignals.madeHighestScoringOtherBid +
            '&bidCurrency=' + browserSignals.bidCurrency +
            '&bid=';
      }
      sendReportTo(sendReportUrl + bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*bid,
      });
      registerAdMacro("campaign", "" + 2*bid);
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 30n, value: 40 + bid});
    }

    reportAdditionalBidWin = reportWin;
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
        '&highestScoringOtherBidCurrency=' +
        browserSignals.highestScoringOtherBidCurrency +
        '&madeHighestScoringOtherBid=' +
        browserSignals.madeHighestScoringOtherBid +
        '&bidCurrency=' + browserSignals.bidCurrency +
        '&bid=' + browserSignals.bid);
  }
)";

// A bid script that returns either `bid` or nothing depending on whether all
// incoming ads got filtered. If the interestGroup has components, the ad URL
// with /1 and /2 generated will be returned as components in the bid. Records
// privateAggregation events for "reserved.loss" to enable checking for kanon
// failure reporting.
std::string MakeFilteringBidScript(int bid) {
  return base::StringPrintf(R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {

      privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
                bucket: {baseValue: "bid-reject-reason"},
                value: 0,
              });
      privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
                bucket: {baseValue: "winning-bid"},
                value: 2,
              });

      if (interestGroup.ads.length === 0)
        return;

      let result = {
        ad: {},
        bid: %d,
        render: interestGroup.ads[0].renderURL,
        allowComponentAuction: true
      };

      if (interestGroup.adComponents) {
        result.adComponents = [
          interestGroup.ads[0].renderURL + "1",
          interestGroup.ads[0].renderURL + "2",
        ];
      }

      return result;
    })",
                            bid);
}

// A bid script that always bids the same value + URL. If bid_currency is not
// an empty string, it will be returned, too.
std::string MakeConstBidScript(int bid,
                               const std::string& url,
                               const std::string& bid_currency = "USD") {
  return base::StringPrintf(R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
                bucket: {baseValue: "bid-reject-reason"},
                value: 123,
              });
      forDebuggingOnly.reportAdAuctionLoss(
            "https://loss.example.com/?rejectReason=${rejectReason}");
      const bidCurrency = "%s";
      let result = {
        ad: {},
        bid: %d,
        render: "%s",
        allowComponentAuction: true
      };
      if (bidCurrency.length !== 0) {
        result.bidCurrency = bidCurrency;
      }
      return result;
    })",
                            bid_currency.c_str(), bid, url.c_str());
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
    const decisionLogicURL = "%s";
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
      if (adMetadata.renderURL !== ("data for " + browserSignals.renderURL)) {
        throw new Error("wrong data for renderURL:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.renderURL);
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
      if (auctionConfig.decisionLogicURL !== decisionLogicURL)
        throw new Error("wrong decisionLogicURL in auctionConfig");
      // Check `perBuyerSignals` for the first bidder.
      let signals1 = auctionConfig.perBuyerSignals['https://adplatform.com'];
      if (signals1[auctionConfig.seller + 'Signals'] !== 'Ad PlatformSignals')
        throw new Error("Wrong perBuyerSignals in auctionConfig");
      if (typeof auctionConfig.perBuyerTimeouts['https://adplatform.com'] !==
              "number" ||
          typeof auctionConfig.perBuyerTimeouts['*'] !== "number") {
        throw new Error("timeout in auctionConfig.perBuyerTimeouts is not a " +
                        "number. huh");
      }
      if (auctionConfig.perBuyerCumulativeTimeouts['https://adplatform.com'] !==
              12345 ||
          auctionConfig.perBuyerCumulativeTimeouts['*'] !== 23456) {
        throw new Error("timeout in auctionConfig.perBuyerCumulativeTimeouts " +
                        "is the wrong value. huh");
      }
      if (auctionConfig.sellerSignals["url"] != decisionLogicURL)
        throw new Error("Wrong sellerSignals");
      if (typeof auctionConfig.sellerTimeout !== "number")
        throw new Error("auctionConfig.sellerTimeout is not a number. huh");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");

      if (decisionLogicURL.startsWith(topLevelSeller)) {
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
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      // Private aggregation requests with non-reserved event types in a seller
      // script will not be reported.
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 50n, value: 60});

      adMetadata.fromComponentAuction = true;

      let convertedBid = auctionConfig.sellerCurrency ? bid * 10 : undefined;
      // Currency-adjust the bidKey in metadata if needed.
      if (auctionConfig.sellerCurrency && bid !== 0.0)
        adMetadata.bidKey += "0";
      return {desirability: computeScore(bid),
              incomingBidInSellerCurrency: convertedBid,
              bid: convertedBid,
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
      if (auctionConfig.decisionLogicURL !== decisionLogicURL)
        throw new Error("wrong decisionLogicURL in auctionConfig");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname in browserSignals");

      if (decisionLogicURL.startsWith(topLevelSeller)) {
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
        // modifiedBid is for component sellers only.
        if ("modifiedBid" in browserSignals)
          throw new Error("modifiedBid unexpectedly in browserSignals");
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

        // scoreAd() does bid modification if sellerCurrency is on.
        if (auctionConfig.sellerCurrency) {
          if (!browserSignals.modifiedBid)
            throw new Error("modifiedBid missing in browserSignals");
        } else {
          if ("modifiedBid" in browserSignals)
            throw new Error("modifiedBid unexpectedly in browserSignals");
        }
      }

      if (browserSignals.desirability != computeScore(browserSignals.bid))
        throw new Error("wrong bid or desirability in browserSignals");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      if (sendReportUrl) {
        registerAdBeacon({
          "click": sendReportUrl + 2*browserSignals.bid,
        });
        if (reportPostAuctionSignals) {
          sendReportUrl += "?highestScoringOtherBid=" +
              browserSignals.highestScoringOtherBid +
              '&highestScoringOtherBidCurrency=' +
              browserSignals.highestScoringOtherBidCurrency +
              "&bidCurrency=" + browserSignals.bidCurrency +
              "&bid=";
        }
        sendReportTo(sendReportUrl + browserSignals.bid);
      }
      privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
      // Private aggregation requests with non-reserved event types in seller
      // script will not be reported.
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 70n, value: 80});

      return browserSignals;
    }

    // Use different scoring functions for the top-level seller and component
    // sellers, so can verify that each ReportResult() method gets the score
    // from the correct seller, and so that the the wrong bidder will win
    // in some tests if either component auction scores are used for the
    // top-level auction, or if all bidders from component auctions are passed
    // to the top-level auction.
    function computeScore(bid) {
      if (decisionLogicURL == "https://adstuff.publisher1.com/auction.js")
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
    privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
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
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      if (bid === 2)
        return {desirability: -1, rejectReason: '%s'};
      let convertedBid = auctionConfig.sellerCurrency ? bid * 10 : undefined;
      return {desirability: bid + 1, incomingBidInSellerCurrency: convertedBid};
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
      return {ad: [], bid: bid, bidCurrency: 'USD',
              render: interestGroup.ads[0].renderURL};
    }
    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo(
          'https://buyer-reporting.example.com/?highestScoringOtherBid=' +
          browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          '&madeHighestScoringOtherBid=' +
          browserSignals.madeHighestScoringOtherBid +
          '&bidCurrency=' + browserSignals.bidCurrency +
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
      let score = (bid == 3 || bid == 4) ? 3 : 1;
      let convertedBid = auctionConfig.sellerCurrency ? bid * 10 : undefined;
      return {desirability: score, incomingBidInSellerCurrency: convertedBid};
    }
    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(
          "https://reporting.example.com/?highestScoringOtherBid=" +
          browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          "&bidCurrency=" + browserSignals.bidCurrency +
          "&bid=" + browserSignals.bid);
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
    const absl::optional<blink::AdCurrency>& bid_currency,
    double highest_scoring_other_bid,
    const absl::optional<blink::AdCurrency>& highest_scoring_other_bid_currency,
    bool made_highest_scoring_other_bid,
    const std::string& url = "https://buyer-reporting.example.com/") {
  // Only keeps integer part of bid values for simplicity for now.
  return GURL(base::StringPrintf(
      "%s"
      "?highestScoringOtherBid=%.0f&highestScoringOtherBidCurrency=%s"
      "&madeHighestScoringOtherBid=%s&bidCurrency=%s&bid=%.0f",
      url.c_str(), highest_scoring_other_bid,
      blink::PrintableAdCurrency(highest_scoring_other_bid_currency).c_str(),
      made_highest_scoring_other_bid ? "true" : "false",
      blink::PrintableAdCurrency(bid_currency).c_str(), bid));
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
      "?winningBid=%.0f&winningBidCurrency=%s&"
      "madeWinningBid=%s&highestScoringOtherBid=%.0f&"
      "highestScoringOtherBidCurrency=%s&madeHighestScoringOtherBid=%s",
      url.c_str(), signals.winning_bid,
      blink::PrintableAdCurrency(signals.winning_bid_currency).c_str(),
      signals.made_winning_bid ? "true" : "false",
      signals.highest_scoring_other_bid,
      blink::PrintableAdCurrency(signals.highest_scoring_other_bid_currency)
          .c_str(),
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
      "?winningBid=%.0f&winningBidCurrency=%s&"
      "madeWinningBid=%s&highestScoringOtherBid=%.0f&"
      "highestScoringOtherBidCurrency=%s&madeHighestScoringOtherBid=%s"
      // Top level post auction signals.
      "&topLevelWinningBid=%.0f&topLevelWinningBidCurrency=%s&"
      "topLevelMadeWinningBid=%s"
      // Bid value.
      "&bid=%.0f",
      url.c_str(), signals.winning_bid,
      blink::PrintableAdCurrency(signals.winning_bid_currency).c_str(),
      signals.made_winning_bid ? "true" : "false",
      signals.highest_scoring_other_bid,
      blink::PrintableAdCurrency(signals.highest_scoring_other_bid_currency)
          .c_str(),
      signals.made_highest_scoring_other_bid ? "true" : "false",
      top_level_signals.winning_bid,
      blink::PrintableAdCurrency(top_level_signals.winning_bid_currency)
          .c_str(),
      top_level_signals.made_winning_bid ? "true" : "false", bid));
}

// Builds a PrivateAggregationRequest with histogram contribution using given
// `bucket` and `value`.
const auction_worklet::mojom::PrivateAggregationRequestPtr
BuildPrivateAggregationRequest(absl::uint128 bucket, int value) {
  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  bucket, value)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

const auction_worklet::mojom::PrivateAggregationRequestPtr
BuildPrivateAggregationForEventRequest(absl::uint128 bucket,
                                       int value,
                                       std::string event_type) {
  auction_worklet::mojom::AggregatableReportForEventContribution contribution(
      auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(bucket),
      auction_worklet::mojom::ForEventSignalValue::NewIntValue(value),
      event_type);

  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(contribution.Clone()),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

auction_worklet::mojom::PrivateAggregationRequestPtr
BuildPrivateAggregationForBaseValue(
    absl::uint128 bucket,
    auction_worklet::mojom::BaseValue base_value,
    std::string event_type) {
  auction_worklet::mojom::AggregatableReportForEventContribution contribution(
      auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(bucket),
      auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
          auction_worklet::mojom::SignalValue::New(base_value, /*scale=*/1.0,
                                                   /*offset=*/0)),
      event_type);

  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(contribution.Clone()),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

// Marks `ad` in `group` k-anonymous, double-checking that its url is `url`.
void AuthorizeKAnonAd(const blink::InterestGroup::Ad& ad,
                      const char* url,
                      StorageInterestGroup& group) {
  DCHECK_EQ(url, ad.render_url.spec());
  group.bidding_ads_kanon.emplace_back();
  group.bidding_ads_kanon.back().key =
      blink::KAnonKeyForAdBid(group.interest_group, ad.render_url);
  group.bidding_ads_kanon.back().is_k_anonymous = true;
  group.bidding_ads_kanon.back().last_updated = base::Time::Now();
}

void AuthorizeKAnonReporting(const blink::InterestGroup::Ad& ad,
                             const char* url,
                             StorageInterestGroup& group) {
  DCHECK_EQ(url, ad.render_url.spec());
  group.reporting_ads_kanon.emplace_back();
  group.reporting_ads_kanon.back().key =
      blink::KAnonKeyForAdNameReporting(group.interest_group, ad);
  group.reporting_ads_kanon.back().is_k_anonymous = true;
  group.reporting_ads_kanon.back().last_updated = base::Time::Now();
}

void AuthorizeKAnonAdComponent(const blink::InterestGroup::Ad& ad,
                               const char* url,
                               StorageInterestGroup& group) {
  DCHECK_EQ(url, ad.render_url.spec());
  group.component_ads_kanon.emplace_back();
  group.component_ads_kanon.back().key =
      blink::KAnonKeyForAdComponentBid(ad.render_url);
  group.component_ads_kanon.back().is_k_anonymous = true;
  group.component_ads_kanon.back().last_updated = base::Time::Now();
}

quiche::ObliviousHttpRequest::Context
CreateBiddingAndAuctionEncryptionContext() {
  const quiche::ObliviousHttpHeaderKeyConfig config =
      quiche::ObliviousHttpHeaderKeyConfig::Create(
          '0', EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_256_GCM)
          .value();
  quiche::ObliviousHttpRequest request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          "{}",
          std::string(reinterpret_cast<const char*>(kTestPublicKey),
                      sizeof(kTestPublicKey)),
          config)
          .value();
  return std::move(request).ReleaseContext();
}

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

  int NumBidderWorklets() const {
    int total = 0;
    for (const auto& svc : auction_worklet_services_) {
      total += svc->NumBidderWorkletsForTesting();
    }
    return total;
  }

  int NumSellerWorklets() const {
    int total = 0;
    for (const auto& svc : auction_worklet_services_) {
      total += svc->NumSellerWorkletsForTesting();
    }
    return total;
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

MATCHER_P2(HasMetricWithValue, key, matcher, "") {
  if (!arg.contains(key)) {
    *result_listener << "which does not contain " << key;
    return false;
  }
  return ExplainMatchResult(arg.at(key), matcher, result_listener);
}

MATCHER_P(HasMetric, key, "") {
  if (!arg.contains(key)) {
    *result_listener << "which does not contain " << key;
    return false;
  }
  return true;
}

MATCHER_P(DoesNotHaveMetric, key, "") {
  if (arg.contains(key)) {
    *result_listener << "which unexpectedly contains " << key;
    return false;
  }
  return true;
}

MATCHER_P2(OnlyHasMetricIf, key, condition, "") {
  if (condition) {
    if (!arg.contains(key)) {
      *result_listener << "which does not contain " << key << " and should";
      return false;
    }
  } else {
    if (arg.contains(key)) {
      *result_listener << "which unexpectedly contains " << key
                       << " and shouldn't";
      return false;
    }
  }
  return true;
}

MATCHER_P2(HasMetricWithValueOrNotIfNullOpt, key, matcher, "") {
  if (!matcher.has_value()) {
    if (arg.contains(key)) {
      *result_listener << "which unexpectedly contains " << key;
      return false;
    }
    // Metric has no value and none was expected, which is good.
    return true;
  }
  // Here, we expect a metric value.
  if (!arg.contains(key)) {
    *result_listener << "which does not contain " << key;
    return false;
  }
  return ExplainMatchResult(arg.at(key), matcher, result_listener);
}

class EventReportingAttestationBrowserClient : public TestContentBrowserClient {
 public:
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }
};

class AuctionRunnerTest : public RenderViewHostTestHarness,
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
    absl::optional<blink::AdDescriptor> ad_descriptor;
    std::vector<blink::AdDescriptor> ad_component_descriptors;
    std::string winning_group_ad_metadata;
    std::vector<GURL> report_urls;
    std::vector<GURL> debug_loss_report_urls;
    std::vector<GURL> debug_win_report_urls;
    base::flat_map<blink::FencedFrame::ReportingDestination,
                   FencedFrameReporter::ReportingUrlMap>
        ad_beacon_map;
    base::flat_map<blink::FencedFrame::ReportingDestination,
                   FencedFrameReporter::ReportingMacros>
        ad_macros;
    std::map<std::string, PrivateAggregationRequests>
        private_aggregation_event_map;
    std::vector<blink::InterestGroupKey> interest_groups_that_bid;

    std::vector<std::string> errors;
  };

  explicit AuctionRunnerTest(
      bool should_enable_private_aggregation = true,
      bool should_enable_private_aggregation_fledge_extension = true,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode =
          auction_worklet::mojom::KAnonymityBidMode::kNone)
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionRunnerTest::OnBadMessage, base::Unretained(this)));
    DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (should_enable_private_aggregation) {
      enabled_features.push_back(
          {blink::features::kPrivateAggregationApi,
           {{"fledge_extensions_enabled",
             should_enable_private_aggregation_fledge_extension ? "true"
                                                                : "false"}}});
    } else {
      disabled_features.push_back(blink::features::kPrivateAggregationApi);
    }

    enabled_features.push_back(
        {blink::features::kAdAuctionReportingWithMacroApi, {}});

    kanon_mode_ = kanon_mode;
    switch (kanon_mode) {
      case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
        enabled_features.push_back(
            {blink::features::kFledgeConsiderKAnonymity, {}});
        enabled_features.push_back(
            {blink::features::kFledgeEnforceKAnonymity, {}});
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
        enabled_features.push_back(
            {blink::features::kFledgeConsiderKAnonymity, {}});
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
      case auction_worklet::mojom::KAnonymityBidMode::kNone:
        disabled_features.push_back(blink::features::kFledgeConsiderKAnonymity);
        disabled_features.push_back(blink::features::kFledgeEnforceKAnonymity);
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auction_nonce_manager_ = std::make_unique<AuctionNonceManager>(GetFrame());
  }

  void TearDown() override {
    DebuggableAuctionWorkletTracker::GetInstance()->RemoveObserver(this);

    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    // Give off-thread things a chance to delete.
    task_environment()->RunUntilIdle();

    // `interest_group_manager_` and `auction_nonce_manager_` need to be reset
    // before the task environment is destroyed (in
    // `RenderViewHostTestHarness::TearDown()`).
    auction_runner_.reset();
    interest_group_manager_.reset();
    auction_nonce_manager_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  ~AuctionRunnerTest() override = default;

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
      return blink::AuctionConfig::MaybePromiseJson::FromValue(
          base::StringPrintf(R"({"url": "%s"})",
                             seller_decision_logic_url.spec().c_str()));
    }
  }

  blink::AuctionConfig::MaybePromiseJson MakeAuctionSignals(
      bool use_promise,
      const url::Origin& seller) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseJson::FromPromise();
    } else {
      return blink::AuctionConfig::MaybePromiseJson::FromValue(
          base::StringPrintf(R"("auctionSignalsFor %s")",
                             seller.Serialize().c_str()));
    }
  }

  blink::AuctionConfig::MaybePromisePerBuyerSignals MakePerBuyerSignals(
      bool use_promise,
      const url::Origin& seller) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromisePerBuyerSignals::FromPromise();
    } else {
      base::flat_map<url::Origin, std::string> per_buyer_signals;
      // Use a combination of bidder and seller values, so can make sure bidders
      // get the value from the correct seller script. Also append a fixed
      // string, as a defense against pulling the right values from the wrong
      // places.
      per_buyer_signals[kBidder1] =
          base::StringPrintf(R"({"%sSignals": "%sSignals"})",
                             seller.Serialize().c_str(), kBidder1Name.c_str());
      per_buyer_signals[kBidder2] =
          base::StringPrintf(R"({"%sSignals": "%sSignals"})",
                             seller.Serialize().c_str(), kBidder2Name.c_str());
      return blink::AuctionConfig::MaybePromisePerBuyerSignals::FromValue(
          std::move(per_buyer_signals));
    }
  }

  blink::AuctionConfig::MaybePromiseBuyerTimeouts MakeBuyerTimeouts(
      bool use_promise) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromPromise();
    } else {
      blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
      // Any per buyer timeout higher than 500 ms will be clamped to 500 ms by
      // the AuctionRunner.
      buyer_timeouts.per_buyer_timeouts.emplace();
      buyer_timeouts.per_buyer_timeouts.value()[kBidder1] =
          base::Milliseconds(1000);
      buyer_timeouts.all_buyers_timeout = base::Milliseconds(150);
      return blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_timeouts));
    }
  }

  blink::AuctionConfig::MaybePromiseBuyerTimeouts MakeBuyerCumulativeTimeouts(
      bool use_promise) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromPromise();
    } else {
      blink::AuctionConfig::BuyerTimeouts buyer_cumulative_timeouts;
      buyer_cumulative_timeouts.per_buyer_timeouts.emplace();
      buyer_cumulative_timeouts.per_buyer_timeouts.value()[kBidder1] =
          kBidder1CumulativeTimeout;
      buyer_cumulative_timeouts.all_buyers_timeout =
          kAllBuyersCumulativeTimeout;
      return blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_cumulative_timeouts));
    }
  }

  blink::AuctionConfig::MaybePromiseBuyerCurrencies MakeBuyerCurrencies(
      bool use_promise) {
    if (use_promise) {
      return blink::AuctionConfig::MaybePromiseBuyerCurrencies::FromPromise();
    } else {
      blink::AuctionConfig::BuyerCurrencies buyer_currencies;
      if (specify_all_buyer_currency_) {
        buyer_currencies.all_buyers_currency = blink::AdCurrency::From("USD");
      }
      return blink::AuctionConfig::MaybePromiseBuyerCurrencies::FromValue(
          std::move(buyer_currencies));
    }
  }

  // Helper to create an auction config with the specified values.
  blink::AuctionConfig CreateAuctionConfig(
      const GURL& seller_decision_logic_url,
      absl::optional<std::vector<url::Origin>> buyers) {
    blink::AuctionConfig auction_config;
    auction_config.seller = url::Origin::Create(seller_decision_logic_url);
    auction_config.decision_logic_url = seller_decision_logic_url;
    if (pass_promise_for_direct_from_seller_signals_) {
      auction_config.direct_from_seller_signals = blink::AuctionConfig::
          MaybePromiseDirectFromSellerSignals::FromPromise();
    }
    auction_config.expects_direct_from_seller_signals_header_ad_slot =
        pass_promise_for_direct_from_seller_signals_header_ad_slot_;

    if (server_response_request_id_) {
      auction_config.server_response.emplace();
      auction_config.server_response->request_id = *server_response_request_id_;
    }

    auction_config.non_shared_params.interest_group_buyers = std::move(buyers);

    auction_config.non_shared_params.seller_signals = MakeSellerSignals(
        use_promise_for_seller_signals_, seller_decision_logic_url);
    auction_config.non_shared_params.seller_timeout = base::Milliseconds(1000);
    auction_config.non_shared_params.per_buyer_signals = MakePerBuyerSignals(
        use_promise_for_per_buyer_signals_, auction_config.seller);
    auction_config.non_shared_params.buyer_timeouts =
        MakeBuyerTimeouts(use_promise_for_buyer_timeouts_);
    auction_config.non_shared_params.buyer_cumulative_timeouts =
        MakeBuyerCumulativeTimeouts(use_promise_for_buyer_cumulative_timeouts_);
    auction_config.non_shared_params.buyer_currencies =
        MakeBuyerCurrencies(use_promise_for_buyer_currencies_);
    auction_config.non_shared_params.seller_currency = seller_currency_;
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

    auction_config.non_shared_params.auction_report_buyer_keys =
        auction_report_buyer_keys_;
    auction_config.non_shared_params.auction_report_buyers =
        auction_report_buyers_;

    auction_config.non_shared_params.auction_nonce = auction_nonce_;

    auction_config.expects_additional_bids = pass_promise_for_additional_bids_;

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
    // Allows multiple auctions to be run in the same test.
    private_aggregation_manager_.Reset();

    auction_complete_ = false;

    auto auction_config =
        CreateAuctionConfig(seller_decision_logic_url, interest_group_buyers_);

    auction_config.trusted_scoring_signals_url = trusted_scoring_signals_url_;

    for (const auto& component_auction : component_auctions_) {
      auction_config.non_shared_params.component_auctions.push_back(
          component_auction);
    }

    // Need to clear `same_process_auction_process_manager_` since underlying
    // object may be owned by `interest_group_manager_`.
    same_process_auction_process_manager_ = nullptr;
    interest_group_manager_ = std::make_unique<TestInterestGroupManagerImpl>(
        frame_origin_, GetClientSecurityState(),
        dummy_report_shared_url_loader_factory_);
    if (!auction_process_manager_) {
      auto same_process_auction_process_manager =
          std::make_unique<SameProcessAuctionProcessManager>();
      same_process_auction_process_manager_ =
          same_process_auction_process_manager.get();
      auction_process_manager_ =
          std::move(same_process_auction_process_manager);
    }
    auction_worklet_manager_ = std::make_unique<AuctionWorkletManager>(
        auction_process_manager_.get(), top_frame_origin_, frame_origin_, this);
    interest_group_manager_->set_auction_process_manager_for_testing(
        std::move(auction_process_manager_));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

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
        task_environment()->FastForwardBy(base::Seconds(1));
      }

      for (const auto& kanon_data : bidder.bidding_ads_kanon) {
        interest_group_manager_->UpdateKAnonymity(kanon_data);
      }
      for (const auto& kanon_data : bidder.component_ads_kanon) {
        interest_group_manager_->UpdateKAnonymity(kanon_data);
      }
      for (const auto& kanon_data : bidder.reporting_ads_kanon) {
        interest_group_manager_->UpdateKAnonymity(kanon_data);
      }
    }

    interest_group_manager_->ClearLoggedData();
    source_id_ = ukm::AssignNewSourceId();

    task_environment()->FastForwardBy(between_join_run_auction_delay_);

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    abortable_ad_auction_.reset();

    // The callback to check attestation is not tested at this layer.
    // AdAuctionServiceImplTest tests it.
    base::RepeatingCallback<bool(const std::vector<url::Origin>&)>
        attestation_callback = base::BindRepeating(
            [](BrowserContext* browser_context,
               const std::vector<url::Origin>& origins) { return true; },
            base::Unretained(browser_context()));

    auction_runner_ = AuctionRunner::CreateAndStart(
        auction_worklet_manager_.get(), auction_nonce_manager_.get(),
        interest_group_manager_.get(), /*browser_context=*/browser_context(),
        &private_aggregation_manager_,
        private_aggregation_manager_.GetLogPrivateAggregationRequestsCallback(),
        std::move(auction_config), top_frame_origin_, frame_origin_, source_id_,
        GetClientSecurityState(), dummy_report_shared_url_loader_factory_,
        IsInterestGroupApiAllowedCallback(), base::BindLambdaForTesting([&]() {
          return ad_auction_page_data_.get();
        }),
        std::move(attestation_callback),
        abortable_ad_auction_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  void RunAuctionAndWait(const GURL& seller_decision_logic_url,
                         std::vector<StorageInterestGroup> bidders) {
    StartAuction(seller_decision_logic_url, std::move(bidders));
    auction_run_loop_->Run();
  }

  void OnAuctionComplete(
      AuctionRunner* auction_runner,
      bool manually_aborted,
      absl::optional<InterestGroupKey> winning_group_key,
      absl::optional<blink::AdSize> requested_ad_size,
      absl::optional<blink::AdDescriptor> ad_descriptor,
      std::vector<blink::AdDescriptor> ad_component_descriptors,
      std::vector<std::string> errors,
      std::unique_ptr<InterestGroupAuctionReporter> reporter) {
    DCHECK(auction_run_loop_);
    DCHECK(!auction_complete_);
    DCHECK_EQ(auction_runner, auction_runner_.get());

    // Delete the auction runner, which is needed to update histograms. Don't do
    // it immediately, so the Reporter is started before its destruction,
    // allowing reuse of the seller worklet, just as happens in production.
    std::unique_ptr<AuctionRunner> owned_auction_runner;
    if (!dont_reset_auction_runner_) {
      owned_auction_runner = std::move(auction_runner_);
    }

    auction_complete_ = true;
    result_.manually_aborted = manually_aborted;
    result_.winning_group_id = std::move(winning_group_key);
    result_.ad_descriptor = std::move(ad_descriptor);
    result_.ad_component_descriptors = std::move(ad_component_descriptors);
    result_.winning_group_ad_metadata.clear();
    result_.report_urls.clear();
    result_.errors = std::move(errors);
    result_.debug_loss_report_urls.clear();
    result_.debug_win_report_urls.clear();
    result_.ad_beacon_map.clear();
    result_.interest_groups_that_bid.clear();

    if (!reporter) {
      result_.debug_loss_report_urls =
          interest_group_manager_->TakeReportUrlsOfType(
              InterestGroupManagerImpl::ReportType::kDebugLoss);
      result_.interest_groups_that_bid =
          interest_group_manager_->TakeInterestGroupsThatBid();

      // There should be no reports of any other type queued.
      interest_group_manager_->ExpectReports({});

      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_descriptor);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      auction_run_loop_->Quit();
      return;
    }

    // No reports should have been queued yet, on success.
    interest_group_manager_->ExpectReports({});

    EXPECT_TRUE(result_.winning_group_id);
    EXPECT_TRUE(result_.ad_descriptor);

    // These are handled by the reporter, in the case an auction has a winner,
    // so they're only requested if the winning ad is used.
    interest_group_manager_->ExpectReports({});
    EXPECT_TRUE(result_.debug_loss_report_urls.empty());

    reporter_ = std::move(reporter);

    reporter_->Start(base::BindOnce(&AuctionRunnerTest::OnReportingComplete,
                                    base::Unretained(this)));
    // Invoke callback immediately, so as not to block reporter completion.
    reporter_->OnNavigateToWinningAdCallback().Run();
  }

  void OnReportingComplete() {
    DCHECK(reporter_);
    result_.report_urls = interest_group_manager_->TakeReportUrlsOfType(
        InterestGroupManagerImpl::ReportType::kSendReportTo);
    result_.ad_beacon_map =
        reporter_->fenced_frame_reporter()->GetAdBeaconMapForTesting();
    result_.ad_macros =
        reporter_->fenced_frame_reporter()->GetAdMacrosForTesting();
    result_.debug_loss_report_urls =
        interest_group_manager_->TakeReportUrlsOfType(
            InterestGroupManagerImpl::ReportType::kDebugLoss);
    result_.debug_win_report_urls =
        interest_group_manager_->TakeReportUrlsOfType(
            InterestGroupManagerImpl::ReportType::kDebugWin);
    result_.private_aggregation_event_map =
        reporter_->fenced_frame_reporter()
            ->GetPrivateAggregationEventMapForTesting();
    result_.interest_groups_that_bid =
        interest_group_manager_->TakeInterestGroupsThatBid();
    const auto& report_errors = reporter_->errors();
    result_.errors.insert(result_.errors.end(), report_errors.begin(),
                          report_errors.end());

    reporter_.reset();

    // Retrieve the winning interest group to extract the most recently added ad
    // metadata.
    interest_group_manager_->GetInterestGroup(
        InterestGroupKey(result_.winning_group_id->owner,
                         result_.winning_group_id->name),
        base::BindOnce(
            &AuctionRunnerTest::OnInterestGroupRetrievedAfterReporterDone,
            base::Unretained(this)));
  }

  void OnInterestGroupRetrievedAfterReporterDone(
      absl::optional<StorageInterestGroup> interest_group) {
    if (interest_group) {
      EXPECT_FALSE(interest_group->bidding_browser_signals->prev_wins.empty());
      base::Time most_recent_win_time;
      // Find the most recent win and write its metadata to
      // `winning_group_ad_metadata`.
      for (const auto& prev_win :
           interest_group->bidding_browser_signals->prev_wins) {
        if (prev_win->time > most_recent_win_time) {
          most_recent_win_time = prev_win->time;
          result_.winning_group_ad_metadata = prev_win->ad_json;
        }
      }
    } else {
      result_.winning_group_ad_metadata = "-no interest group-";
    }
    auction_run_loop_->Quit();
  }

  // Returns the specified interest group.
  absl::optional<StorageInterestGroup> GetInterestGroup(
      const url::Origin& owner,
      const std::string& name) {
    return interest_group_manager_->BlockingGetInterestGroup(owner, name);
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
        ads->emplace_back(*ad_url, R"({"ads": true})");
      } else {
        ads->emplace_back(*ad_url, absl::nullopt);
      }
    }

    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    if (ad_component_urls) {
      ad_components.emplace();
      for (const GURL& ad_component_url : *ad_component_urls) {
        ad_components->emplace_back(ad_component_url, absl::nullopt);
      }
    }

    return MakeInterestGroup(
        blink::TestInterestGroupBuilder(owner, name)
            .SetExpiry(base::Time::Max())
            .SetPriority(1.0)
            .SetBiddingUrl(bidding_url)
            .SetTrustedBiddingSignalsUrl(trusted_bidding_signals_url)
            .SetTrustedBiddingSignalsKeys({trusted_bidding_signals_keys})
            .SetAds(ads)
            .SetAdComponents(ad_components)
            .Build());
  }

  StorageInterestGroup MakeInterestGroup(blink::InterestGroup interest_group) {
    // Create fake previous wins. The time of these wins is ignored, since the
    // InterestGroupManager attaches the current time when logging a win.
    std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
    // Log a time that's before now, so that any new entry will have the largest
    // time.
    base::Time the_past = base::Time::Now() - base::Milliseconds(1);
    previous_wins.push_back(
        auction_worklet::mojom::PreviousWin::New(the_past, R"({"winner": 0})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        the_past, R"({"winner": -1})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        the_past, R"({"winner": -2})"));

    StorageInterestGroup storage_group;
    storage_group.interest_group = std::move(interest_group);
    storage_group.bidding_browser_signals =
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins));
    storage_group.joining_origin = storage_group.interest_group.owner;
    return storage_group;
  }

  void StartStandardAuction(bool request_trusted_bidding_signals = true) {
    std::vector<StorageInterestGroup> bidders;
    absl::optional<GURL> bidder1_signals_url;
    absl::optional<GURL> bidder2_signals_url;
    if (request_trusted_bidding_signals) {
      bidder1_signals_url = kBidder1TrustedSignalsUrl;
      bidder2_signals_url = kBidder2TrustedSignalsUrl;
    }
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, std::move(bidder1_signals_url),
        {"k1", "k2"}, GURL("https://ad1.com"),
        std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                          GURL("https://ad1.com-component2.com")}));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, kBidder2Name, kBidder2Url, std::move(bidder2_signals_url),
        {"l1", "l2"}, GURL("https://ad2.com"),
        std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                          GURL("https://ad2.com-component2.com")}));

    StartAuction(kSellerUrl, std::move(bidders));
  }

  void RunStandardAuction(bool request_trusted_bidding_signals = true) {
    StartStandardAuction(request_trusted_bidding_signals);
    auction_run_loop_->Run();
  }

  // Starts the standard auction with the mock worklet service, and waits for
  // the service to receive the worklet construction calls.
  //
  // `num_bidder_worklets` is the number of bidder worklets that are
  // expected to be created.
  void StartStandardAuctionWithMockService(int num_bidder_worklets = 2) {
    UseMockWorkletService();
    StartStandardAuction();
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/num_bidder_worklets,
        /*num_sellers=*/1 + component_auctions_.size());
  }

  // Runs an auction that exercises the extended private aggregation buyers
  // metrics. Uses a mock service to control the metrics returned from bidder
  // worklets.
  //
  // `bidders` All interest groups that will bid in the auction.
  //
  // `trusted_fetch_latency` For each interest group, the trusted fetch
  // duration that should be returned from the mock worklet.
  //
  // `bidding_latency` For each interest group, the bidding duration that
  // should be returned from the mock worklet.
  //
  // `should_bid` For each worklet, true indicates that a bid should be
  // generated, and false indicates that a null bid should be returned.
  //
  // NOTE: This method isn't compatible with auction limits like
  // `all_buyers_group_limit_`, since interest group load order isn't
  // deterministic -- instead, use a non-mock based runner like
  // RunAuctionAndWait().
  void RunExtendedPABuyersAuction(
      const std::vector<StorageInterestGroup>& bidders,
      const std::vector<base::TimeDelta> trusted_fetch_latency,
      const std::vector<base::TimeDelta> bidding_latency = {base::TimeDelta()},
      const std::vector<bool> should_bid = {true}) {
    ASSERT_EQ(bidders.size(), trusted_fetch_latency.size());
    ASSERT_EQ(bidders.size(), bidding_latency.size());
    ASSERT_EQ(bidders.size(), should_bid.size());
    UseMockWorkletService();
    StartAuction(kSellerUrl, bidders);
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/bidders.size(), /*num_sellers=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    for (size_t i = 0; i < bidders.size(); i++) {
      auto bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          *bidders[i].interest_group.bidding_url);
      ASSERT_TRUE(bidder_worklet);

      if (!trusted_fetch_latency[i].is_zero()) {
        bidder_worklet->SetBidderTrustedSignalsFetchLatency(
            trusted_fetch_latency[i]);
      }
      if (!bidding_latency[i].is_zero()) {
        bidder_worklet->SetBiddingLatency(bidding_latency[i]);
      }
      bidder_worklet->InvokeGenerateBidCallback(
          /*bid=*/should_bid[i] ? absl::make_optional(1) : absl::nullopt,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com/")));
      if (should_bid[i]) {
        auto score_ad_params = seller_worklet->WaitForScoreAd();
        mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
            std::move(score_ad_params.score_ad_client))
            ->OnScoreAdComplete(
                /*score=*/1,
                /*reject_reason=*/
                auction_worklet::mojom::RejectReason::kNotAvailable,
                auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
                /*bid_in_seller_currency=*/absl::nullopt,
                /*scoring_signals_data_version=*/absl::nullopt,
                /*debug_loss_report_url=*/absl::nullopt,
                /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
                /*scoring_latency=*/base::TimeDelta(),
                /*score_ad_dependency_latencies=*/
                auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                    /*code_ready_latency=*/absl::nullopt,
                    /*direct_from_seller_signals_latency=*/absl::nullopt,
                    /*trusted_scoring_signals_latency=*/absl::nullopt),
                /*errors=*/{});
      }
    }

    // Need to flush the service pipe to make sure the AuctionRunner has
    // received the score.
    seller_worklet->Flush();
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();
    mock_auction_process_manager_->WaitForWinningBidderReload();
    for (const StorageInterestGroup& bidder : bidders) {
      auto bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          *bidder.interest_group.bidding_url);
      if (bidder_worklet) {
        bidder_worklet->WaitForReportWin();
        bidder_worklet->InvokeReportWinCallback();
      }
    }

    auction_run_loop_->Run();
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
  RenderFrameHostImpl* GetFrame() override {
    return static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
  }
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
    // Any per buyer timeout in auction_config higher than 500 ms should be
    // clamped to 500 ms by the AuctionRunner before being passed to
    // GenerateBid(), and kBidder1's per buyer timeout is 1000 ms in
    // auction_config so it should be 500 ms here.
    mock_auction_process_manager->SetExpectedBuyerBidTimeout(
        kBidder1Name, base::Milliseconds(500));
    mock_auction_process_manager->SetExpectedBuyerBidTimeout(
        kBidder1NameAlt, base::Milliseconds(500));
    // Bidder 2's per buyer timeout should be 150 ms, since `auction_config's`
    // `all_buyers_timeout` is set to 150 ms in all tests.
    mock_auction_process_manager->SetExpectedBuyerBidTimeout(
        kBidder2Name, base::Milliseconds(150));
    same_process_auction_process_manager_ = nullptr;
    mock_auction_process_manager_ = mock_auction_process_manager.get();
    auction_process_manager_ = std::move(mock_auction_process_manager);
  }

  ukm::TestUkmRecorder::HumanReadableUkmMetrics GetUkmMetrics() const {
    using Entry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
        ukm_recorder_->GetEntries(
            Entry::kEntryName,
            {
                /* General metrics */
                Entry::kResultName,
                Entry::kEndToEndLatencyInMillisName,
                Entry::kLoadInterestGroupPhaseLatencyInMillisName,
                Entry::kNumInterestGroupsName,
                Entry::kNumOwnersWithInterestGroupsName,
                Entry::kNumDistinctOwnersWithInterestGroupsName,
                Entry::kNumSellersWithBiddersName,
                Entry::kNumBidderWorkletsName,
                /* Bid filtering metrics */
                Entry::kNumBidsAbortedByBuyerCumulativeTimeoutName,
                Entry::kNumBidsAbortedByBidderWorkletFatalErrorName,
                Entry::kNumBidsFilteredDuringInterestGroupLoadName,
                Entry::kNumBidsFilteredDuringReprioritizationName,
                Entry::kNumBidsFilteredByPerBuyerLimitsName,
                /* Properties of the auction metrics */
                Entry::kKAnonymityBidModeName,
                Entry::kNumConfigPromisesName,
                /* GenerateBid outcome metrics */
                Entry::kNumInterestGroupsWithNoBidsName,
                Entry::kNumInterestGroupsWithOnlyNonKAnonBidName,
                Entry::kNumInterestGroupsWithSameBidForKAnonAndNonKAnonName,
                Entry::
                    kNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnonName,
                /* ComponentAuction latency metrics */
                Entry::kMeanComponentAuctionLatencyInMillisName,
                Entry::kMaxComponentAuctionLatencyInMillisName,
                /* GenerateBid latency metrics */
                Entry::kMeanBidForOneInterestGroupLatencyInMillisName,
                Entry::kMaxBidForOneInterestGroupLatencyInMillisName,
                Entry::kMeanGenerateSingleBidLatencyInMillisName,
                Entry::kMaxGenerateSingleBidLatencyInMillisName,
                /* GenerateBid Dependency latency metrics */
                Entry::kMeanGenerateBidCodeReadyLatencyInMillisName,
                Entry::kMeanGenerateBidConfigPromisesLatencyInMillisName,
                Entry::
                    kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName,
                Entry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName,
                Entry::kMaxGenerateBidCodeReadyLatencyInMillisName,
                Entry::kMaxGenerateBidConfigPromisesLatencyInMillisName,
                Entry::
                    kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName,
                Entry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName,
                /* GenerateBid Dependency critical path metrics */
                Entry::kNumGenerateBidCodeReadyOnCriticalPathName,
                Entry::kNumGenerateBidConfigPromisesOnCriticalPathName,
                Entry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName,
                Entry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName,
                Entry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName,
                Entry::
                    kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName,
                Entry::
                    kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName,
                Entry::
                    kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName,
                /* Bids queued waiting to begin ScoreAd */
                Entry::kNumTopLevelBidsQueuedWaitingForConfigPromisesName,
                Entry::kNumTopLevelBidsQueuedWaitingForSellerWorkletName,
                Entry::
                    kMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillisName,
                Entry::
                    kMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillisName,
                Entry::kNumBidsQueuedWaitingForConfigPromisesName,
                Entry::kNumBidsQueuedWaitingForSellerWorkletName,
                Entry::kMeanTimeBidsQueuedWaitingForConfigPromisesInMillisName,
                Entry::kMeanTimeBidsQueuedWaitingForSellerWorkletInMillisName,
                /* ScoreAd latency metrics */
                Entry::kMeanScoreAdFlowLatencyInMillisName,
                Entry::kMaxScoreAdFlowLatencyInMillisName,
                Entry::kMeanScoreAdLatencyInMillisName,
                Entry::kMaxScoreAdLatencyInMillisName,
                /* ScoreAd Dependency latency metrics */
                Entry::kMeanScoreAdCodeReadyLatencyInMillisName,
                Entry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName,
                Entry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName,
                Entry::kMaxScoreAdCodeReadyLatencyInMillisName,
                Entry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName,
                Entry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName,
                /* ScoreAd Dependency critical path metrics */
                Entry::kNumScoreAdCodeReadyOnCriticalPathName,
                Entry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName,
                Entry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName,
                Entry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName,
                Entry::
                    kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName,
                Entry::
                    kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName,
            });

    EXPECT_THAT(ukm_entries, testing::SizeIs(1));
    if (ukm_entries.size() == 1) {
      EXPECT_EQ(ukm_entries.at(0).source_id, source_id_);
      return ukm_entries.at(0).metrics;
    }

    // Fallback to an empty metrics map
    return ukm::TestUkmRecorder::HumanReadableUkmMetrics();
  }

  struct MetricsExpectations {
    explicit MetricsExpectations(AuctionResult result) : result(result) {}

    MetricsExpectations& SetNumInterestGroups(int64_t value) {
      num_interest_groups = value;
      return *this;
    }

    MetricsExpectations& SetNumOwners(int64_t value) {
      num_owners = value;
      return *this;
    }

    MetricsExpectations& SetNumDistinctOwners(int64_t value) {
      num_distinct_owners = value;
      return *this;
    }

    // Shorthand for .SetNumOwners(owners).SetNumDistinctOwners(owners)
    MetricsExpectations& SetNumOwnersAndDistinctOwners(int64_t value) {
      num_owners = value;
      num_distinct_owners = value;
      return *this;
    }

    MetricsExpectations& SetNumSellers(int64_t value) {
      num_sellers = value;
      return *this;
    }

    MetricsExpectations& SetNumBidderWorklets(int64_t value) {
      num_bidder_worklets = value;
      return *this;
    }

    MetricsExpectations& SetNumConfigPromises(int64_t value) {
      num_config_promises = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsAbortedByBuyerCumulativeTimeout(
        int64_t value) {
      num_bids_aborted_by_buyer_cumulative_timeout = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsAbortedByBidderWorkletFatalError(
        int64_t value) {
      num_bids_aborted_by_bidder_worklet_fatal_error = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsFilteredDuringInterestGroupLoad(
        int64_t value) {
      num_bids_filtered_during_interest_group_load = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsFilteredDuringReprioritization(
        int64_t value) {
      num_bids_filtered_during_reprioritization = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsFilteredByPerBuyerLimits(int64_t value) {
      num_bids_filtered_by_per_buyer_limits = value;
      return *this;
    }

    MetricsExpectations& SetNumInterestGroupsWithNoBids(int64_t value) {
      num_interest_groups_with_no_bids = value;
      return *this;
    }

    MetricsExpectations& SetNumInterestGroupsWithOnlyNonKAnonBid(
        int64_t value) {
      num_interest_groups_with_only_non_k_anon_bid = value;
      InferHasBidRelatedLatencyMetrics();
      return *this;
    }

    MetricsExpectations& SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(
        int64_t value) {
      num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon = value;
      InferHasBidRelatedLatencyMetrics();
      return *this;
    }

    MetricsExpectations&
    SetNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon(int64_t value) {
      num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon = value;
      InferHasBidRelatedLatencyMetrics();
      return *this;
    }

    // This should be called after calls to any of the SetNumInterestGroups*
    // methods above, as those override the value of this expectation.
    MetricsExpectations& SetHasBidForOneInterestGroupLatencyMetrics(
        bool value) {
      has_bid_for_one_interest_group_latency_metrics = value;
      return *this;
    }

    // This should be called after calls to any of the SetNumInterestGroups*
    // methods above, as those override the value of this expectation.
    MetricsExpectations& SetHasGenerateSingleBidLatencyMetrics(bool value) {
      has_generate_single_bid_latency_metrics = value;
      return *this;
    }

    // This should be called after calls to any of the SetNumInterestGroups*
    // methods above, as those override the value of this expectation.
    MetricsExpectations& SetHasScoreAdFlowLatencyMetrics(bool value) {
      has_score_ad_flow_latency_metrics = value;
      return *this;
    }

    // This should be called after calls to any of the SetNumInterestGroups*
    // methods above, as those override the value of this expectation.
    MetricsExpectations& SetHasScoreAdLatencyMetrics(bool value) {
      has_score_ad_latency_metrics = value;
      return *this;
    }

    void InferHasBidRelatedLatencyMetrics() {
      bool generated_bids =
          num_interest_groups_with_only_non_k_anon_bid +
              num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon +
              num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon >
          0;
      SetHasBidForOneInterestGroupLatencyMetrics(generated_bids);
      SetHasGenerateSingleBidLatencyMetrics(generated_bids);
      SetHasScoreAdFlowLatencyMetrics(generated_bids);
      SetHasScoreAdLatencyMetrics(generated_bids);
    }

    MetricsExpectations& SetNumTopLevelBidsQueuedWaitingForConfigPromises(
        int64_t value) {
      num_top_level_bids_queued_waiting_for_config_promises = value;
      return *this;
    }

    MetricsExpectations& SetNumTopLevelBidsQueuedWaitingForSellerWorklet(
        int64_t value) {
      num_top_level_bids_queued_waiting_for_seller_worklet = value;
      return *this;
    }

    MetricsExpectations& SetNumBidsQueuedWaitingForConfigPromises(
        int64_t value) {
      num_bids_queued_waiting_for_config_promises = value;
      return *this;
    }

    // If set to -1, these metrics are not checked.
    MetricsExpectations& SetNumBidsQueuedWaitingForSellerWorklet(
        int64_t value) {
      num_bids_queued_waiting_for_seller_worklet = value;
      return *this;
    }

    AuctionResult result;
    absl::optional<int64_t> num_interest_groups;
    absl::optional<int64_t> num_owners;
    absl::optional<int64_t> num_sellers;
    int64_t num_distinct_owners = 0;
    int64_t num_bidder_worklets = 0;
    int64_t num_config_promises = 0;
    int64_t num_bids_aborted_by_buyer_cumulative_timeout = 0;
    int64_t num_bids_aborted_by_bidder_worklet_fatal_error = 0;
    int64_t num_bids_filtered_during_interest_group_load = 0;
    int64_t num_bids_filtered_during_reprioritization = 0;
    int64_t num_bids_filtered_by_per_buyer_limits = 0;
    int64_t num_interest_groups_with_no_bids = 0;
    int64_t num_interest_groups_with_only_non_k_anon_bid = 0;
    int64_t num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon = 0;
    int64_t num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon =
        0;

    bool has_bid_for_one_interest_group_latency_metrics = false;
    bool has_generate_single_bid_latency_metrics = false;
    bool has_score_ad_flow_latency_metrics = false;
    bool has_score_ad_latency_metrics = false;

    int64_t num_top_level_bids_queued_waiting_for_config_promises = 0;
    int64_t num_top_level_bids_queued_waiting_for_seller_worklet = 0;
    int64_t num_bids_queued_waiting_for_config_promises = 0;
    int64_t num_bids_queued_waiting_for_seller_worklet = 0;
  };

  // Check histogram values and UKMs.
  // If `num_interest_groups` or `num_owners` is null, expect the auction to be
  // aborted before the corresponding histograms or UKMs are recorded.
  void CheckMetrics(MetricsExpectations expectations) {
    using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;
    ukm::TestUkmRecorder::HumanReadableUkmMetrics ukm_metrics = GetUkmMetrics();
    histogram_tester_->ExpectUniqueSample("Ads.InterestGroup.Auction.Result",
                                          expectations.result, 1);
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(UkmEntry::kResultName,
                                   static_cast<int64_t>(expectations.result)));

    if (expectations.num_interest_groups.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumInterestGroups",
          *expectations.num_interest_groups, 1);
      EXPECT_THAT(ukm_metrics,
                  HasMetricWithValue(UkmEntry::kNumInterestGroupsName,
                                     expectations.num_interest_groups));
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumInterestGroups", 0);
      EXPECT_THAT(ukm_metrics,
                  DoesNotHaveMetric(UkmEntry::kNumInterestGroupsName));
    }

    if (expectations.num_owners.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          *expectations.num_owners, 1);
      EXPECT_THAT(ukm_metrics,
                  HasMetricWithValue(UkmEntry::kNumOwnersWithInterestGroupsName,
                                     expectations.num_owners));
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups", 0);
      EXPECT_THAT(ukm_metrics, DoesNotHaveMetric(
                                   UkmEntry::kNumOwnersWithInterestGroupsName));
    }

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(UkmEntry::kNumDistinctOwnersWithInterestGroupsName,
                           expectations.num_distinct_owners));

    if (expectations.num_sellers.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumSellersWithBidders",
          *expectations.num_sellers, 1);
      EXPECT_THAT(ukm_metrics,
                  HasMetricWithValue(UkmEntry::kNumSellersWithBiddersName,
                                     expectations.num_sellers));
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumSellersWithBidders", 0);
      EXPECT_THAT(ukm_metrics,
                  DoesNotHaveMetric(UkmEntry::kNumSellersWithBiddersName));
    }

    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(UkmEntry::kNumBidderWorkletsName,
                                   expectations.num_bidder_worklets));

    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AbortTime",
        expectations.result == AuctionResult::kAborted);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
        expectations.result == AuctionResult::kNoBids ||
            expectations.result == AuctionResult::kAllBidsRejected);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
        expectations.result == AuctionResult::kSuccess);

    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(UkmEntry::kKAnonymityBidModeName,
                                   static_cast<int64_t>(kanon_mode_)));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(UkmEntry::kNumConfigPromisesName,
                                   expectations.num_config_promises));
    EXPECT_THAT(
        ukm_metrics,
        HasMetric(UkmEntry::kLoadInterestGroupPhaseLatencyInMillisName));
    EXPECT_THAT(ukm_metrics, HasMetric(UkmEntry::kEndToEndLatencyInMillisName));

    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumBidsAbortedByBuyerCumulativeTimeoutName,
                    expectations.num_bids_aborted_by_buyer_cumulative_timeout));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumBidsAbortedByBidderWorkletFatalErrorName,
            expectations.num_bids_aborted_by_bidder_worklet_fatal_error));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumBidsFilteredDuringInterestGroupLoadName,
                    expectations.num_bids_filtered_during_interest_group_load));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumBidsFilteredDuringReprioritizationName,
                    expectations.num_bids_filtered_during_reprioritization));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(UkmEntry::kNumBidsFilteredByPerBuyerLimitsName,
                           expectations.num_bids_filtered_by_per_buyer_limits));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(UkmEntry::kNumInterestGroupsWithNoBidsName,
                           expectations.num_interest_groups_with_no_bids));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumInterestGroupsWithOnlyNonKAnonBidName,
                    expectations.num_interest_groups_with_only_non_k_anon_bid));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumInterestGroupsWithSameBidForKAnonAndNonKAnonName,
            expectations
                .num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnonName,
            expectations
                .num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon));

    bool is_multi_seller_auction =
        expectations.num_sellers && *expectations.num_sellers >= 2;
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMeanComponentAuctionLatencyInMillisName,
                        is_multi_seller_auction));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMaxComponentAuctionLatencyInMillisName,
                        is_multi_seller_auction));

    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(
            UkmEntry::kMeanBidForOneInterestGroupLatencyInMillisName,
            expectations.has_bid_for_one_interest_group_latency_metrics));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(
            UkmEntry::kMaxBidForOneInterestGroupLatencyInMillisName,
            expectations.has_bid_for_one_interest_group_latency_metrics));

    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMeanGenerateSingleBidLatencyInMillisName,
                        expectations.has_generate_single_bid_latency_metrics));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMaxGenerateSingleBidLatencyInMillisName,
                        expectations.has_generate_single_bid_latency_metrics));

    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMeanScoreAdFlowLatencyInMillisName,
                        expectations.has_score_ad_flow_latency_metrics));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(UkmEntry::kMaxScoreAdFlowLatencyInMillisName,
                        expectations.has_score_ad_flow_latency_metrics));

    EXPECT_THAT(ukm_metrics,
                OnlyHasMetricIf(UkmEntry::kMeanScoreAdLatencyInMillisName,
                                expectations.has_score_ad_latency_metrics));
    EXPECT_THAT(ukm_metrics,
                OnlyHasMetricIf(UkmEntry::kMaxScoreAdLatencyInMillisName,
                                expectations.has_score_ad_latency_metrics));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumTopLevelBidsQueuedWaitingForConfigPromisesName,
            expectations
                .num_top_level_bids_queued_waiting_for_config_promises));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(
            UkmEntry::
                kMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillisName,
            expectations.num_top_level_bids_queued_waiting_for_config_promises >
                0));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumTopLevelBidsQueuedWaitingForSellerWorkletName,
            expectations.num_top_level_bids_queued_waiting_for_seller_worklet));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(
            UkmEntry::
                kMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillisName,
            expectations.num_top_level_bids_queued_waiting_for_seller_worklet >
                0));

    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumBidsQueuedWaitingForConfigPromisesName,
                    expectations.num_bids_queued_waiting_for_config_promises));
    EXPECT_THAT(
        ukm_metrics,
        OnlyHasMetricIf(
            UkmEntry::kMeanTimeBidsQueuedWaitingForConfigPromisesInMillisName,
            expectations.num_bids_queued_waiting_for_config_promises > 0));

    if (expectations.num_bids_queued_waiting_for_seller_worklet >= 0) {
      EXPECT_THAT(ukm_metrics,
                  HasMetricWithValue(
                      UkmEntry::kNumBidsQueuedWaitingForSellerWorkletName,
                      expectations.num_bids_queued_waiting_for_seller_worklet));
      EXPECT_THAT(
          ukm_metrics,
          OnlyHasMetricIf(
              UkmEntry::kMeanTimeBidsQueuedWaitingForSellerWorkletInMillisName,
              expectations.num_bids_queued_waiting_for_seller_worklet > 0));
    }
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
  // auction each bidder is in, and must be either kComponentSeller1 or
  // kComponentSeller2. kComponentSeller1 is always added to the auction,
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

    if (bidder1_seller == kComponentSeller1) {
      component1_buyers.push_back(kBidder1);
    } else if (bidder1_seller == kComponentSeller2) {
      component2_buyers.push_back(kBidder1);
    } else {
      NOTREACHED();
    }

    if (bidder2_seller == kComponentSeller1) {
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

  data_decoder::test::InProcessDataDecoder data_decoder_;
  EventReportingAttestationBrowserClient browser_client_;
  ScopedContentBrowserClientSetting browser_client_setting_{&browser_client_};

  // This is what an "empty" FLEDGE ad beacon map looks like.
  const base::flat_map<blink::FencedFrame::ReportingDestination,
                       FencedFrameReporter::ReportingUrlMap>
      kEmptyAdBeaconMap = {
          {blink::FencedFrame::ReportingDestination::kSeller, {}},
          {blink::FencedFrame::ReportingDestination::kComponentSeller, {}},
          {blink::FencedFrame::ReportingDestination::kBuyer, {}},
      };

  auction_worklet::mojom::KAnonymityBidMode kanon_mode_;

  bool use_promise_for_seller_signals_ = false;
  bool use_promise_for_auction_signals_ = false;
  bool use_promise_for_per_buyer_signals_ = false;
  bool use_promise_for_buyer_timeouts_ = false;
  bool use_promise_for_buyer_cumulative_timeouts_ = false;
  bool use_promise_for_buyer_currencies_ = false;
  bool specify_all_buyer_currency_ = true;
  absl::optional<blink::AdCurrency> seller_currency_;

  // Unlike others, this is only test with promises at this level.
  bool pass_promise_for_direct_from_seller_signals_ = false;
  bool pass_promise_for_direct_from_seller_signals_header_ad_slot_ = false;
  absl::optional<uint16_t> seller_experiment_group_id_;
  absl::optional<uint16_t> all_buyer_experiment_group_id_;
  std::map<url::Origin, uint16_t> per_buyer_experiment_group_id_;
  uint16_t all_buyers_group_limit_ = std::numeric_limits<std::uint16_t>::max();
  absl::optional<base::flat_map<std::string, double>>
      all_buyers_priority_signals_;

  // This is also tested only with promises at this level.
  absl::optional<base::Uuid> server_response_request_id_;
  raw_ptr<AdAuctionPageData> ad_auction_page_data_ = nullptr;

  bool pass_promise_for_additional_bids_ = false;

  absl::optional<base::Uuid> auction_nonce_;

  absl::optional<std::vector<absl::uint128>> auction_report_buyer_keys_;
  absl::optional<base::flat_map<
      blink::AuctionConfig::NonSharedParams::BuyerReportType,
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>>
      auction_report_buyers_;

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
  const GURL kBidder1UrlAlt{"https://adplatform.com/offers_alt.js"};
  const url::Origin kBidder1 = url::Origin::Create(kBidder1Url);
  const InterestGroupKey kBidder1Key{kBidder1, kBidder1Name};
  const GURL kBidder1TrustedSignalsUrl{"https://adplatform.com/signals1"};
  const base::TimeDelta kBidder1CumulativeTimeout = base::Milliseconds(12345);

  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 = url::Origin::Create(kBidder2Url);
  const std::string kBidder2Name{"Another Ad Thing"};
  const InterestGroupKey kBidder2Key{kBidder2, kBidder2Name};
  const GURL kBidder2TrustedSignalsUrl{"https://anotheradthing.com/signals2"};

  const base::TimeDelta kAllBuyersCumulativeTimeout = base::Milliseconds(23456);

  // Timeout tests can wait until this amount before a timeout, make sure
  // nothing has happened, and then wait this amount, and check the timeout
  // happened.
  const base::TimeDelta kTinyTime = base::Milliseconds(1);

  absl::optional<std::vector<url::Origin>> interest_group_buyers_ = {
      {kBidder1, kBidder2}};

  std::vector<blink::AuctionConfig> component_auctions_;

  // Origins which are not allowed to take part in auctions, as the
  // corresponding participant types.
  std::set<url::Origin> disallowed_sellers_;
  std::set<url::Origin> disallowed_buyers_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  // Delay between joining interest groups and starting the auction.
  base::TimeDelta between_join_run_auction_delay_;

  network::TestURLLoaderFactory url_loader_factory_;

  // ScopedURLLoaderFactory used for reports. The FencedFrameReporter is never
  // told to send any reports in these tests, and reports sent directly through
  // the InterestGroupManager are short-circuited by the
  // TestInterestGroupManagerImpl before they make it over the network, so this
  // is only used for equality checks around making sure the right factory is
  // passed to it.
  scoped_refptr<network::SharedURLLoaderFactory>
      dummy_report_shared_url_loader_factory_ =
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              nullptr);

  std::unique_ptr<AuctionWorkletManager> auction_worklet_manager_;
  std::unique_ptr<AuctionNonceManager> auction_nonce_manager_;

  TestInterestGroupPrivateAggregationManager private_aggregation_manager_{
      top_frame_origin_};

  // This is used (and consumed) when starting an auction, if non-null. Allows
  // either using a MockAuctionProcessManager instead of a
  // SameProcessAuctionProcessManager, or using a
  // SameProcessAuctionProcessManager that has already vended processes. If
  // nullptr, a new SameProcessAuctionProcessManager() is created when an
  // auction is started.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Set by UseMockWorkletService(). Non-owning reference to the
  // AuctionProcessManager that will be / has been passed to the
  // InterestGroupManager.
  raw_ptr<MockAuctionProcessManager, DanglingUntriaged>
      mock_auction_process_manager_ = nullptr;

  // If StartAuction() created a SameProcessAuctionProcessManager for
  // `auction_process_manager_`, this alises it.
  // Reset by other things that set `auction_process_manager_`.
  raw_ptr<SameProcessAuctionProcessManager, DanglingUntriaged>
      same_process_auction_process_manager_ = nullptr;

  // The TestInterestGroupManager is recreated and repopulated for each auction.
  std::unique_ptr<TestInterestGroupManagerImpl> interest_group_manager_;

  ukm::SourceId source_id_;

  std::unique_ptr<AuctionRunner> auction_runner_;
  std::unique_ptr<InterestGroupAuctionReporter> reporter_;
  bool dont_reset_auction_runner_ = false;
  // This should be inspected using TakeBadMessage(), which also clears it.
  std::string bad_message_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));
}

// Runs a component auction with all buyers fields null.
TEST_F(AuctionRunnerTest, ComponentAuctionNullBuyers) {
  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/absl::nullopt));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));
}

// Runs an auction with an empty buyers field.
TEST_F(AuctionRunnerTest, EmptyBuyers) {
  interest_group_buyers_->clear();
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));
}

// Runs a component auction with all buyers fields empty.
TEST_F(AuctionRunnerTest, ComponentAuctionEmptyBuyers) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/{}));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));
}

// Runs the standard auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, NoInterestGroups) {
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(0)
                   .SetNumSellers(0));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(0)
                   .SetNumSellers(0));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(0)
                   .SetNumSellers(0));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(0)
                   .SetNumSellers(0));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(0)
                   .SetNumSellers(0));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=0&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=1"),
          ReportWinUrl(/*bid=*/1,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/0,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
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

  RunStandardAuction();
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=1&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=2"),
          ReportWinUrl(/*bid=*/2,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/1,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));

  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "4")))));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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

  EXPECT_THAT(
      private_aggregation_manager_.TakeLoggedPrivateAggregationRequests(),
      ElementsAreRequests(
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedReportWinPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedReportResultPrivateAggregationRequest,
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/21,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/22,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/30, /*value=*/42,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/70, /*value=*/80,
                                                 /*event_type=*/"click")));

  EXPECT_THAT(result_.private_aggregation_event_map,
              testing::UnorderedElementsAre(testing::Pair(
                  "click", ElementsAreRequests(
                               BuildPrivateAggregationRequest(/*bucket=*/10,
                                                              /*value=*/22),
                               BuildPrivateAggregationRequest(/*bucket=*/30,
                                                              /*value=*/42)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_TRUE(result_.errors.empty());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

TEST_F(AuctionRunnerTest, BasicCurrencyCheck) {
  // This feature only relevant for the debug_loss_report_urls portion of the
  // test.
  base::test::ScopedFeatureList debug_features;
  debug_features.InitAndEnableFeature(
      blink::features::kBiddingAndScoringDebugReportingAPI);

  // Test with bidder 2 making a bid with CAD when the fixture expects USD.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeConstBidScript(1, "https://ad1.com/", "USD") + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeConstBidScript(2, "https://ad2.com/", "CAD") + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  // Only bidder 1 is considered to have actually bid, as bid2 failed currency
  // check and was therefore dropped.
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_THAT(result_.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "generateBid() bidCurrency mismatch;"
                                   " returned 'CAD', expected 'USD'."));
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::ElementsAre("https://loss.example.com/"
                           "?rejectReason=wrong-generate-bid-currency"));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(kBidder2,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                            /*bucket=*/static_cast<uint64_t>(
                                auction_worklet::mojom::RejectReason::
                                    kWrongGenerateBidCurrency),
                            /*value=*/123))),
          testing::Pair(
              // kBasicReportResult reports (7,8)
              kSeller, ElementsAreRequests(
                           BuildPrivateAggregationRequest(/*bucket=*/7,
                                                          /*value=*/8)))));
}

TEST_F(AuctionRunnerTest, BasicCurrencyRedact) {
  // Make sure that if buyer currency isn't specified in AuctionConfig it's
  // redacted in reportWin()
  specify_all_buyer_currency_ = false;

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

  RunStandardAuction();
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/"
                       "?highestScoringOtherBid=1&"
                       "highestScoringOtherBidCurrency=???&"
                       "bidCurrency=???&bid=2"),
                  ReportWinUrl(
                      /*bid=*/2, /*bid_currency=*/absl::nullopt,
                      /*highest_scoring_other_bid=*/1,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false)));
}

TEST_F(AuctionRunnerTest, BasicCurrencyRedact2) {
  // Make sure that if buyer currency is specified in AuctionConfig but not
  // given by generateBid, the config currency is given to reportWin().
  specify_all_buyer_currency_ = true;

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeConstBidScript(/*bid=*/1, /*url=*/"https://ad1.com",
                         /*bid_currency=*/"") +
          kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeConstBidScript(/*bid=*/2, /*url=*/"https://ad2.com",
                         /*bid_currency=*/"") +
          kSimpleReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/2"),
          ReportWinUrl(
              /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
              /*highest_scoring_other_bid=*/1,
              /*highest_scoring_other_bid_currency=*/absl::nullopt,
              /*made_highest_scoring_other_bid=*/false)));
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
    task_environment()->RunUntilIdle();

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
      task_environment()->RunUntilIdle();
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
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAre(
                    GURL("https://reporting.example.com/2"),
                    GURL("https://buyer-reporting.example.com/2")));
    EXPECT_THAT(
        result_.ad_beacon_map,
        testing::UnorderedElementsAre(
            testing::Pair(
                ReportingDestination::kSeller,
                testing::ElementsAre(testing::Pair(
                    "click", GURL("https://reporting.example.com/4")))),
            testing::Pair(ReportingDestination::kComponentSeller,
                          testing::ElementsAre()),
            testing::Pair(
                ReportingDestination::kBuyer,
                testing::ElementsAre(testing::Pair(
                    "click", GURL("https://buyer-reporting.example.com/4"))))));
    EXPECT_THAT(result_.ad_macros,
                testing::UnorderedElementsAre(testing::Pair(
                    ReportingDestination::kBuyer,
                    testing::ElementsAre(testing::Pair("${campaign}", "4")))));
    EXPECT_THAT(
        private_aggregation_manager_.TakePrivateAggregationRequests(),
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
    EXPECT_THAT(
        result_.private_aggregation_event_map,
        testing::UnorderedElementsAre(testing::Pair(
            "click",
            ElementsAreRequests(
                BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
                BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));
  }
}

TEST_F(AuctionRunnerTest, PauseBidder) {
  pause_worklet_url_ = kBidder2Url;

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
  task_environment()->RunUntilIdle();
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  same_process_auction_process_manager_->ResumeAllPaused();

  // Need to resume a second time, when the script is re-loaded to run
  // ReportWin().
  task_environment()->RunUntilIdle();
  same_process_auction_process_manager_->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "4")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));
}

TEST_F(AuctionRunnerTest, PauseSeller) {
  pause_worklet_url_ = kSellerUrl;

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
  task_environment()->RunUntilIdle();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  same_process_auction_process_manager_->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "4")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));
}

// A component auction with two successful bids from different components.
TEST_F(AuctionRunnerTest, ComponentAuction) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          // top-level doesn't get highestScoringOtherBid.
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=0&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=2"),
          GURL("https://component2-report.test/"
               "?highestScoringOtherBid=0&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=2"),
          ReportWinUrl(/*bid=*/2,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/0,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "4")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
}

// Test of a component auction where top-level seller and intermediate one use
// different currencies.
TEST_F(AuctionRunnerTest, ComponentAuctionMixedCurrency) {
  const char kBidScript[] = R"(
    const inBid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: 1 + 1000 * inBid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 2 + 1000 * inBid,
      });
      return {bid: inBid, render: interestGroup.ads[0].renderURL,
              allowComponentAuction: true};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      let sendReportUrl = "https://buyer-reporting.example.com/";
      sendReportUrl +=
          '?highestScoringOtherBid=' + browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          '&bidCurrency=' + browserSignals.bidCurrency +
          '&bid=' + browserSignals.bid;
      sendReportTo(sendReportUrl);

      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 11 + 1000 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 12 + 1000 * browserSignals.bid,
      });
    }
  )";

  const char kDecisionScript[] = R"(
    const inOffset = %d;
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                      browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: inOffset + 1 + 1000 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: inOffset + 2 + 1000 * bid,
      });
      // Pass along a bit less than our conversion.
      return {desirability: bid,
              incomingBidInSellerCurrency: bid * 10,
              bid: bid * 9,
              allowComponentAuction: true,
              ad: adMetadata};
    }

    function reportResult(auctionConfig, browserSignals) {
      let sendReportUrl = "https://seller-reporting-" + inOffset +
                          ".example.com/";
      sendReportUrl +=
          '?highestScoringOtherBid=' + browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          '&bidCurrency=' + browserSignals.bidCurrency +
          '&bid=' + browserSignals.bid;
      sendReportTo(sendReportUrl);

      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: inOffset + 11 + 1000 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: inOffset + 12 + 1000 * browserSignals.bid,
      });
     }
  )";

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      base::StringPrintf(kDecisionScript, 20));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      base::StringPrintf(kDecisionScript, 50));
  ASSERT_EQ(component_auctions_.size(), 1u);
  component_auctions_[0].non_shared_params.seller_currency =
      blink::AdCurrency::From("USD");
  seller_currency_ = blink::AdCurrency::From("CAD");

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          // Top-level gets a bid of 18USD from the component.
          GURL("https://seller-reporting-50.example.com/"
               "?highestScoringOtherBid=0&highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=18"),
          // Bid in component is 2USD.
          GURL("https://seller-reporting-20.example.com/"
               "?highestScoringOtherBid=10&highestScoringOtherBidCurrency=USD&"
               "bidCurrency=USD&bid=2"),
          GURL("https://buyer-reporting.example.com/"
               "?highestScoringOtherBid=10&highestScoringOtherBidCurrency=USD&"
               "bidCurrency=USD&bid=2")));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  // generateBid(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/1001),
                  // generateBid(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/10,
                                                 /*value=*/1002))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(
                  // generateBid(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2001),
                  // generateBid(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/2002),
                  // reportWin(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2011),
                  // reportWin(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/10,
                                                 /*value=*/2012))),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(
                  // scoreAd(), winning-bid, incoming bid 1
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/1021),
                  // scoreAd(), highest-scoring-other-bid, incoming bid 1
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/1022),
                  // scoreAd(), winning-bid, incoming bid 2
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2021),
                  // scoreAd(), highest-scoring-other-bid, incoming bid 2
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/2022),
                  // reportResult(), winning-bid, browserSignals.bid is 2
                  BuildPrivateAggregationRequest(/*bucket=*/20,
                                                 /*value=*/2031),
                  // reportResult(), highest-scoring-other-bid,
                  // browserSignals.bid is 2
                  BuildPrivateAggregationRequest(/*bucket=*/10,
                                                 /*value=*/2032))),
          testing::Pair(
              kSeller,
              ElementsAreRequests(
                  // scoreAd(), winning-bid, browserSignals.bid is 18
                  BuildPrivateAggregationRequest(/*bucket=*/180,
                                                 /*value=*/18051),
                  // scoreAd(), highest-scoring-other-bid is 0.
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/18052),
                  // reportResult(), winning-bid, browserSignals.bid is 18.
                  BuildPrivateAggregationRequest(/*bucket=*/180,
                                                 /*value=*/18061),
                  // reportResult() highest-scoring-other-bid is 0.
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/18062)))));
}

// Test of a component auction where top-level seller and intermediate one use
// different currencies, with two components.
TEST_F(AuctionRunnerTest, ComponentAuctionMixedCurrency2) {
  const char kBidScript[] = R"(
    const inBid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: 1 + 1000 * inBid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 2 + 1000 * inBid,
      });
      return {bid: inBid, render: interestGroup.ads[0].renderURL,
              allowComponentAuction: true};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      let sendReportUrl = "https://buyer-reporting.example.com/";
      sendReportUrl +=
          '?highestScoringOtherBid=' + browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          '&bidCurrency=' + browserSignals.bidCurrency +
          '&bid=' + browserSignals.bid;
      sendReportTo(sendReportUrl);

      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 11 + 1000 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 12 + 1000 * browserSignals.bid,
      });
    }
  )";

  const char kDecisionScript[] = R"(
    const inOffset = %d;
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                      browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: inOffset + 1 + 1000 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: inOffset + 2 + 1000 * bid,
      });
      // Pass along a bit less than our conversion.
      return {desirability: bid,
              incomingBidInSellerCurrency: bid * 10,
              bid: bid * 9,
              allowComponentAuction: true,
              ad: adMetadata};
    }

    function reportResult(auctionConfig, browserSignals) {
      let sendReportUrl = "https://seller-reporting-" + inOffset +
                          ".example.com/";
      sendReportUrl +=
          '?highestScoringOtherBid=' + browserSignals.highestScoringOtherBid +
          '&highestScoringOtherBidCurrency=' +
          browserSignals.highestScoringOtherBidCurrency +
          '&bidCurrency=' + browserSignals.bidCurrency +
          '&bid=' + browserSignals.bid;
      sendReportTo(sendReportUrl);

      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: inOffset + 11 + 1000 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: inOffset + 12 + 1000 * browserSignals.bid,
      });
     }
  )";

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      base::StringPrintf(kDecisionScript, 20));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      base::StringPrintf(kDecisionScript, 40));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      base::StringPrintf(kDecisionScript, 60));
  ASSERT_EQ(component_auctions_.size(), 2u);
  component_auctions_[0].non_shared_params.seller_currency =
      blink::AdCurrency::From("USD");
  component_auctions_[1].non_shared_params.seller_currency =
      blink::AdCurrency::From("MXN");
  seller_currency_ = blink::AdCurrency::From("CAD");

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          // Component's bid in top-level is 18MXN.
          GURL("https://seller-reporting-60.example.com/"
               "?highestScoringOtherBid=0&highestScoringOtherBidCurrency=???&"
               "bidCurrency=MXN&bid=18"),
          // Buyer's bid in component is 2USD.
          GURL("https://seller-reporting-40.example.com/"
               "?highestScoringOtherBid=0&highestScoringOtherBidCurrency=MXN&"
               "bidCurrency=USD&bid=2"),
          // Winning buyer's bid is 2USD.
          GURL("https://buyer-reporting.example.com/"
               "?highestScoringOtherBid=0&highestScoringOtherBidCurrency=MXN&"
               "bidCurrency=USD&bid=2")));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  // generateBid(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/1001),
                  // generateBid(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/1002))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(
                  // generateBid(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2001),
                  // generateBid(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/2002),
                  // reportWin(), winning-bid
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2011),
                  // reportWin(), highest-scoring-other-bid
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/2012))),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(
                  // scoreAd(), winning-bid, incoming bid 1
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/1021),
                  // scoreAd(), highest-scoring-other-bid, incoming bid 1
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/1022))),
          testing::Pair(
              kComponentSeller2,
              ElementsAreRequests(
                  // scoreAd(), winning-bid, incoming bid 2
                  BuildPrivateAggregationRequest(/*bucket=*/20, /*value=*/2041),
                  // scoreAd(), highest-scoring-other-bid, incoming bid 2
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/2042),
                  // reportResult(), winning-bid, browserSignals.bid is 2
                  BuildPrivateAggregationRequest(/*bucket=*/20,
                                                 /*value=*/2051),
                  // reportResult(), highest-scoring-other-bid,
                  // browserSignals.bid is 2
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/2052))),
          testing::Pair(
              kSeller,
              ElementsAreRequests(
                  // scoreAd(), winning-bid, browserSignals.bid is 9
                  BuildPrivateAggregationRequest(/*bucket=*/180,
                                                 /*value=*/9061),
                  // scoreAd(), highest-scoring-other-bid is 0.
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/9062),

                  // scoreAd(), winning-bid, browserSignals.bid is 18
                  BuildPrivateAggregationRequest(/*bucket=*/180,
                                                 /*value=*/18061),
                  // scoreAd(), highest-scoring-other-bid is 0, since not set
                  // on top-level.
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/18062),
                  // reportResult(), winning-bid, browserSignals.bid is 18.
                  BuildPrivateAggregationRequest(/*bucket=*/180,
                                                 /*value=*/18071),
                  // reportResult() highest-scoring-other-bid is 0, since not
                  // set on top-level.
                  BuildPrivateAggregationRequest(/*bucket=*/0,
                                                 /*value=*/18072)))));
}

// Test of currency handling in a component auction where bid is passed
// straight through by the component seller.
TEST_F(AuctionRunnerTest, ComponentAuctionCurrencyPassThrough) {
  const char kBidScript[] = R"(
    const inBid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: inBid, bidCurrency: 'USD',
              render: interestGroup.ads[0].renderURL,
              allowComponentAuction: true};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
    }
  )";

  const char kDecisionScript[] = R"(
    const suffix = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                      browserSignals) {
      if (browserSignals.bidCurrency !== 'USD') {
        throw 'Wrong bidCurrency in scoreAd() for ' + suffix;
      }
      return {desirability: bid,
              allowComponentAuction: true,
              ad: adMetadata};
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      base::StringPrintf(kDecisionScript, "component"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      base::StringPrintf(kDecisionScript, "top-level"));
  ASSERT_EQ(component_auctions_.size(), 1u);

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
}

// Test of currency handling in a component auction where bid is passed
// straight through by the component seller --- verifying it's checked against
// sellerCurrency of the component auction.
TEST_F(AuctionRunnerTest, ComponentAuctionCurrencyPassThroughCheck) {
  // This feature only relevant for the debug_loss_report_urls portion of the
  // test.
  base::test::ScopedFeatureList debug_features;
  debug_features.InitAndEnableFeature(
      blink::features::kBiddingAndScoringDebugReportingAPI);

  const char kBidScript[] = R"(
    const inBid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
            "https://loss.example.com/?rejectReason=${rejectReason}&bid=" +
            inBid);
      return {bid: inBid, bidCurrency: 'USD',
              render: interestGroup.ads[0].renderURL,
              allowComponentAuction: true};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
    }
  )";

  const char kDecisionScript[] = R"(
    const suffix = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                      browserSignals) {
      privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
        bucket: {baseValue: "bid-reject-reason"},
        value: 123 + bid,
      });
      if (browserSignals.bidCurrency !== 'USD') {
        throw 'Wrong bidCurrency in scoreAd() for ' + suffix;
      }
      return {desirability: bid,
              allowComponentAuction: true,
              ad: adMetadata};
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/true);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      base::StringPrintf(kDecisionScript, "component"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      base::StringPrintf(kDecisionScript, "top-level"));
  ASSERT_EQ(component_auctions_.size(), 1u);
  component_auctions_[0].non_shared_params.seller_currency =
      blink::AdCurrency::From("CAD");

  RunStandardAuction();
  const char kError[] =
      "https://component.seller1.test/foo.js scoreAd() bid passthrough "
      "mismatch vs own sellerCurrency, expected 'CAD' got 'USD'.";

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          kComponentSeller1,
          ElementsAreRequests(BuildPrivateAggregationRequest(
                                  /*bucket=*/static_cast<uint64_t>(
                                      auction_worklet::mojom::RejectReason::
                                          kWrongScoreAdCurrency),
                                  /*value=*/124),
                              BuildPrivateAggregationRequest(
                                  /*bucket=*/static_cast<uint64_t>(
                                      auction_worklet::mojom::RejectReason::
                                          kWrongScoreAdCurrency),
                                  /*value=*/125)))));

  // Error is seen twice since it's relevant to two bids.
  EXPECT_THAT(result_.errors, testing::ElementsAre(kError, kError));
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::ElementsAre("https://loss.example.com/"
                           "?rejectReason=wrong-score-ad-currency&bid=1",
                           "https://loss.example.com/"
                           "?rejectReason=wrong-score-ad-currency&bid=2"));
  EXPECT_FALSE(result_.winning_group_id.has_value());
  EXPECT_FALSE(result_.ad_descriptor.has_value());
}

// Test a component auction where the top level seller rejects all bids. This
// should fail with kAllBidsRejected instead of kNoBids.
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
  EXPECT_EQ(absl::nullopt, result_.ad_descriptor);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  CheckMetrics(MetricsExpectations(AuctionResult::kAllBidsRejected)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
}

// Test case where the two components have the same buyer, which makes different
// bids for both auctions.
//
// This tests that parameters are separated, that bid counts are updated
// correctly, and how histograms are updated in these cases.
TEST_F(AuctionRunnerTest, ComponentAuctionSharedBuyer) {
  const GURL kComponent1BidUrl = GURL("https://component1-bid.test/");
  const GURL kComponent2BidUrl = GURL("https://component2-bid.test/");

  // Bid script used in both auctions. The bid amount is based on the seller:
  // It bids the most in auctions run by kComponentSeller2Url, and the least in
  // auctions run by kComponentSeller1Url.
  const char kBidScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                          trustedBiddingSignals, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      if (browserSignals.seller == "https://component.seller1.test") {
        privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 10n, value: 21});
        return {ad: [], bid: 1, render: "https://component1-bid.test/",
                allowComponentAuction: true};
      }
      if (browserSignals.seller == "https://component.seller2.test") {
        privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 10n, value: 23});
        return {ad: [], bid: 3, render: "https://component2-bid.test/",
                allowComponentAuction: true};
      }
      return 0;
    }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*browserSignals.bid,
      });
      registerAdMacro("campaign", "" + 2*browserSignals.bid);
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 30n, value: 40 + browserSignals.bid});
    }
  )";

  // Script used for both sellers. Return different desireability scores based
  // on bid and seller, to make sure correct values are plumbed through.
  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      if (auctionConfig.seller == "https://adstuff.publisher1.com")
        return {desirability: 20 + bid, allowComponentAuction: true};
      if (auctionConfig.seller == "https://component.seller2.test")
        return {desirability: 30 + bid, allowComponentAuction: true};
      return {desirability: 10 + bid, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" +
                   browserSignals.desirability);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2*browserSignals.desirability,
      });
      privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
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

  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder1}}));

  // Custom interest group with two ads, so both bid URLs are valid.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, kComponent1BidUrl));
  bidders[0].interest_group.ads->emplace_back(kComponent2BidUrl, absl::nullopt);

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ(kComponent2BidUrl, result_.ad_descriptor->url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://adstuff.publisher1.com/23"),
                  GURL("https://component.seller2.test/33"),
                  GURL("https://buyer-reporting.example.com/3")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://adstuff.publisher1.com/46")))),
          testing::Pair(
              ReportingDestination::kComponentSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://component.seller2.test/66")))),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/6"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "6")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
              ElementsAreRequests(kExpectedScoreAdPrivateAggregationRequest)),
          testing::Pair(kComponentSeller2,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/23),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/43)))));
  // Bid count should only be incremented by 1.
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"renderURL":"https://component2-bid.test/"})",
            result_.winning_group_ad_metadata);
  // Currently an interest group participating twice in an auction is counted
  // twice.
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwners(2)
                   .SetNumDistinctOwners(1)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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
        return {bid: 1, render: interestGroup.ads[0].renderURL,
                allowComponentAuction: true};
      }

    function reportWin() {}
  )";

  // Script used by the losing bidder. It makes the higher bid.
  const char kBidder2Script[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {bid: 2, render: interestGroup.ads[0].renderURL,
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

  EXPECT_EQ("https://ad1.com/", result_.ad_descriptor->url);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=0&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=1"),
          GURL("https://component1-report.test/"
               "?highestScoringOtherBid=2&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=1"),
          ReportWinUrl(/*bid=*/1,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/2,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
}

// Test the case a top-level seller returns no signals in its reportResult
// method. The default scripts return signals, so only need to individually test
// the no-value case.
TEST_F(AuctionRunnerTest, ComponentAuctionNoTopLevelReportResultSignals) {
  // Basic bid script.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
        return {ad: [], bid: 2, render: interestGroup.ads[0].renderURL,
                allowComponentAuction: true};
      }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
      registerAdBeacon({
        "click": "https://buyer-reporting.example.com/" + 2*browserSignals.bid,
      });
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
    }
  )";

  // Component seller script that makes a report to a URL based on whether the
  // top-level seller signals are null.
  const std::string kComponentSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      return {desirability: 10, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" +
                   (browserSignals.topLevelSellerSignals === null));
      registerAdBeacon({
        "click": auctionConfig.seller + "/" +
                   (browserSignals.topLevelSellerSignals === null),
      });
      privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
    }
  )";

  // Top-level seller script with a reportResult method that has no return
  // value.
  const std::string kTopLevelSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      return {desirability: 10, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" + browserSignals.bid);
      registerAdBeacon({
        "click": auctionConfig.seller + "/" + 2 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://buyer-reporting.example.com/2"),
                  GURL("https://component.seller1.test/true"),
                  GURL("https://adstuff.publisher1.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
}

TEST_F(AuctionRunnerTest, ComponentAuctionModifiesBid) {
  // Basic bid script.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        return {ad: [], bid: 2, render: interestGroup.ads[0].renderURL,
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

  // Component seller script that modifies the bid to 3 USD; also notes that
  // it interpreted the incoming bid as 20 in sellerCurrency.
  const std::string kComponentSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
              bucket: {baseValue: "bid-reject-reason"},
              value: 123,
      });
      return {desirability: 10, allowComponentAuction: true, bid: 3,
              bidCurrency: 'USD', incomingBidInSellerCurrency: 20};
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
  for (bool match_currency : {true, false}) {
    SCOPED_TRACE(match_currency);
    component_auctions_.clear();
    component_auctions_.emplace_back(
        CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
    component_auctions_[0].non_shared_params.seller_currency =
        blink::AdCurrency::From(match_currency ? "USD" : "CAD");

    // Basic interest group.
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));

    StartAuction(kSellerUrl, std::move(bidders));
    auction_run_loop_->Run();
    if (match_currency) {
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
      // The reporting URLs contain the bids - the top-level seller report
      // should see the modified bid, the other worklets see the original bid
      // (modulo currency conversion).
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://buyer-reporting.example.com/2"),
                      GURL("https://component.seller1.test/2_3"),
                      GURL("https://adstuff.publisher1.com/3")));
      EXPECT_THAT(
          result_.ad_beacon_map,
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
                      "click",
                      GURL("https://buyer-reporting.example.com/4"))))));
      EXPECT_TRUE(private_aggregation_manager_.TakePrivateAggregationRequests()
                      .empty());
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(2)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
    } else {
      EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
                  testing::UnorderedElementsAre(testing::Pair(
                      kComponentSeller1,
                      ElementsAreRequests(BuildPrivateAggregationRequest(
                          /*bucket=*/static_cast<uint64_t>(
                              auction_worklet::mojom::RejectReason::
                                  kWrongScoreAdCurrency),
                          /*value=*/123)))));

      EXPECT_THAT(result_.errors,
                  testing::ElementsAre(
                      "https://component.seller1.test/foo.js scoreAd() "
                      "bidCurrency mismatch vs own sellerCurrency, "
                      "expected 'CAD' got 'USD'."));
      EXPECT_FALSE(result_.ad_descriptor);
    }
  }
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSellerRejected));

  // No requests for the bidder worklet URLs should be made.
  task_environment()->RunUntilIdle();
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));

  // No requests for the bidder worklet URLs should be made.
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));

  // No requests for the seller worklet URL should be made.
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));

  // No requests for bidder2's worklet URL should be made.
  task_environment()->RunUntilIdle();
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups));

  // No requests for the bidder worklet URLs should be made.
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(
      result_.errors,
      testing::ElementsAre("Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsAbortedByBidderWorkletFatalError(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));

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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  // The bid send to the failing component seller worklet isn't counted,
  // regardless of whether the bid completed before the worklet failed to load.
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "`generateBid` is not a function."));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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

  RunStandardAuction();
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://adplatform.com/offers.js "
                  "HTTP status = 404 Not Found.",
                  "Failed to load https://anotheradthing.com/bids.js "
                  "HTTP status = 404 Not Found."));
  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsAbortedByBidderWorkletFatalError(2)
                   .SetNumInterestGroupsWithNoBids(2));
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

  RunStandardAuction();
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          "https://adplatform.com/offers.js `generateBid` is not a function.",
          "https://anotheradthing.com/bids.js `generateBid` is not a "
          "function."));
  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithNoBids(2)
                   // We successfully generated no bids, so record that latency
                   .SetHasBidForOneInterestGroupLatencyMetrics(true));
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

  RunStandardAuction();
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(
                                  "https://adstuff.publisher1.com/auction.js "
                                  "`scoreAd` is not a function.",
                                  "https://adstuff.publisher1.com/auction.js "
                                  "`scoreAd` is not a function."));
  CheckMetrics(MetricsExpectations(AuctionResult::kAllBidsRejected)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "2")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  RunStandardAuction();
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());

  EXPECT_EQ(0, url_loader_factory_.NumPending());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  CheckMetrics(MetricsExpectations(AuctionResult::kSellerWorkletLoadFailed)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2));
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

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(
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
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller, testing::ElementsAre()),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/4"))))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(GURL("https://reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/4")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(ReportingDestination::kBuyer, testing::ElementsAre())));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer, testing::ElementsAre())));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(result_.private_aggregation_event_map,
              testing::UnorderedElementsAre(testing::Pair(
                  "click", ElementsAreRequests(BuildPrivateAggregationRequest(
                               /*bucket=*/10, /*value=*/22)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.ad_beacon_map,
              testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer, testing::ElementsAre())));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(result_.private_aggregation_event_map,
              testing::UnorderedElementsAre(testing::Pair(
                  "click", ElementsAreRequests(BuildPrivateAggregationRequest(
                               /*bucket=*/10, /*value=*/22)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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

  RunStandardAuction();
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://seller.signals.were.null.test/")));
  EXPECT_THAT(result_.ad_beacon_map,
              testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(kBidder1,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            // ReportWin script override doesn't send a report.
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              // ReportWin script override doesn't send a report.
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                  "%s `reportResult` is not a function.",
                                  kSellerUrl.spec().c_str())));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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
  // `renderURL` is "accept".
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         std::string(R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  let signal = trustedScoringSignals.renderUrl[browserSignals.renderURL];
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad1.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(ReportingDestination::kSeller,
                        testing::ElementsAre(testing::Pair(
                            "click", GURL("https://reporting.example.com/2")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/2"))))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      // Overridden script functions don't send reports
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(kExpectedGenerateBidPrivateAggregationRequest,
                                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kBidder2,
                        ElementsAreRequests(
                            kExpectedGenerateBidPrivateAggregationRequest))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/21),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/41)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
}

// Test that shows we reliably wait for promises to resolve even if nothing
// participates in the IG.
TEST_F(AuctionRunnerTest, PromiseCheckNoBidders) {
  use_promise_for_auction_signals_ = true;

  StartAuction(kSellerUrl, /*bidders=*/{});
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Emulate the renderer aborting things.
  abortable_ad_auction_->Abort();
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.manually_aborted);
}

// An auction that passes auctionSignals via promises. This makes sure to
// order worklet process creation before promise delivery (compare to
// PromiseAuctionSignalsDeliveredBeforeWorklet).
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

  // Make sure the worklet processes are created. Since promises haven't
  // resolved yet, the auction should not complete.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
  EXPECT_EQ(same_process_auction_process_manager_->NumBidderWorklets(), 2);

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .value());

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumBidderWorklets(2)
                   .SetNumConfigPromises(1)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumInterestGroups(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2)
                   .SetNumSellers(1));
}

// Checks case where auction signals promises resolves before the bidder worklet
// process is ready.
TEST_F(AuctionRunnerTest, PromiseAuctionSignalsDeliveredBeforeWorklet) {
  use_promise_for_auction_signals_ = true;

  // Create AuctionProcessManager in advance of starting the auction so can
  // create worklets before the auction starts.
  auction_process_manager_ =
      std::make_unique<SameProcessAuctionProcessManager>();

  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      busy_processes;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    busy_processes.push_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin,
        scoped_refptr<SiteInstance>(), &*busy_processes.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }
  task_environment()->RunUntilIdle();

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .value());

  // Can't complete yet since there is no process slot.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Free up process space to permit this to proceed.
  busy_processes.clear();

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .value());

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);

  auction_run_loop_->Run();

  EXPECT_FALSE(result_.winning_group_id.has_value());
  EXPECT_FALSE(result_.ad_descriptor.has_value());
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          testing::AllOf(HasSubstr("https://adplatform.com/offers.js"),
                         HasSubstr("Uncaught Error: wrong auctionSignals.")),
          testing::AllOf(HasSubstr("https://anotheradthing.com/bids.js"),
                         HasSubstr("Uncaught Error: wrong auctionSignals."))));
}

// An auction that passes perBuyerSignals, perBuyerTimeouts,
// perBuyerCumulativeTimeouts, and perBuyerCurrencies via promises.
TEST_F(AuctionRunnerTest, PromiseSignals3) {
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_timeouts_ = true;
  use_promise_for_buyer_cumulative_timeouts_ = true;
  use_promise_for_buyer_currencies_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerSignals.
  abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      MakePerBuyerSignals(/*use_promise=*/false,
                          url::Origin::Create(kSellerUrl))
          .value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerTimeouts.
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      MakeBuyerTimeouts(/*use_promise=*/false).value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerCumulativeTimeouts.
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::
          kPerBuyerCumulativeTimeouts,
      MakeBuyerCumulativeTimeouts(/*use_promise=*/false).value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerCurrencies.
  abortable_ad_auction_->ResolvedBuyerCurrenciesPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      MakeBuyerCurrencies(/*use_promise=*/false).value());
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// An auction that passes perBuyerSignals and perBuyerTimeouts via promises.
// Empty values are provided, which causes the validation scripts to complain.
TEST_F(AuctionRunnerTest, PromiseSignals4) {
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerSignals.
  abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerTimeouts.
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      blink::AuctionConfig::BuyerTimeouts());

  auction_run_loop_->Run();

  EXPECT_FALSE(result_.winning_group_id.has_value());
  EXPECT_FALSE(result_.ad_descriptor.has_value());
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          testing::AllOf(
              HasSubstr("https://adplatform.com/offers.js"),
              HasSubstr(
                  "Uncaught Error: unexpectedly perBuyerSignals is null.")),
          testing::AllOf(
              HasSubstr("https://anotheradthing.com/bids.js"),
              HasSubstr(
                  "Uncaught Error: unexpectedly perBuyerSignals is null."))));
}

// An auction that passes empty additionalBids via a promise (in that headers
// are never observed carrying them).
TEST_F(AuctionRunnerTest, PromiseSignalsAdditionalBids) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitAndEnableFeature(
      blink::features::kFledgeNegativeTargeting);
  auction_nonce_ =
      static_cast<base::Uuid>(auction_nonce_manager_->CreateAuctionNonce());
  pass_promise_for_additional_bids_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in empty additionalBids.
  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// Runs an auction that passes auctionSignals via a promise, and makes sure that
// URL fetches begin, and worklet processes are launched, before the promise is
// resolved.
TEST_F(AuctionRunnerTest, PromiseSignalsParallelism) {
  use_promise_for_seller_signals_ = true;

  StartStandardAuction();

  // Various scripts and trusted signals should be pending, and worklets should
  // be created.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1, same_process_auction_process_manager_->NumSellerWorklets());
  EXPECT_EQ(2, same_process_auction_process_manager_->NumBidderWorklets());
  GURL trusted_bidding_signals_url1 = GURL(kBidder1TrustedSignalsUrl.spec() +
                                           "?hostname=publisher1.com&keys=k1,k2"
                                           "&interestGroupNames=Ad+Platform");
  GURL trusted_bidding_signals_url2 =
      GURL(kBidder2TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=l1,l2"
           "&interestGroupNames=Another+Ad+Thing");
  EXPECT_TRUE(url_loader_factory_.IsPending(kBidder1Url.spec()));
  EXPECT_TRUE(url_loader_factory_.IsPending(kBidder2Url.spec()));
  EXPECT_TRUE(url_loader_factory_.IsPending(kSellerUrl.spec()));
  EXPECT_TRUE(
      url_loader_factory_.IsPending(trusted_bidding_signals_url1.spec()));
  EXPECT_TRUE(
      url_loader_factory_.IsPending(trusted_bidding_signals_url2.spec()));

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
      &url_loader_factory_, trusted_bidding_signals_url1, kBidder1SignalsJson);
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, trusted_bidding_signals_url2, kBidder2SignalsJson);
  // Feed in the sources.
  task_environment()->RunUntilIdle();

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
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
  task_environment()->RunUntilIdle();
  ;
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->Abort();
  auction_run_loop_->Run();
  EXPECT_TRUE(result_.manually_aborted);

  // Feed in sellerSignals. Nothing weird should happen.
  auction_run_loop_ = std::make_unique<base::RunLoop>();
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in the signals. Updating main before component auctions is significant
  // because it makes sure main auction is notified of config readiness
  // triggered by component config updates, too.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals,
      MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(kSellerUrl))
          .value());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  for (int component = 0; component < 2; ++component) {
    const GURL& url =
        (component == 0) ? kComponentSeller1Url : kComponentSeller2Url;
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(component),
        blink::mojom::AuctionAdConfigField::kSellerSignals,
        MakeSellerSignals(/*use_promise=*/false, url).value());
    task_environment()->RunUntilIdle();
    EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(component),
        blink::mojom::AuctionAdConfigField::kAuctionSignals,
        MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(url))
            .value());
    if (component != 1) {
      task_environment()->RunUntilIdle();
      EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
    }
  }

  auction_run_loop_->Run();

  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// Coverage of what happens when promises come in for a component auction that
// got dropped at the database load stage, due to not having anything to bid,
// including that it still gets error-checked.
TEST_F(AuctionRunnerTest, PromiseSignalsComponentAuctionRejected) {
  use_promise_for_auction_signals_ = true;

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/false);

  for (bool inject_incorrect_call : {false, true}) {
    SCOPED_TRACE(inject_incorrect_call);
    std::vector<StorageInterestGroup> bidders;
    // Only component 2's bidder added.
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
        {"l1", "l2"}, GURL("https://ad2.com"),
        std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                          GURL("https://ad2.com-component2.com")}));
    StartAuction(kSellerUrl, std::move(bidders));

    // Can't complete yet.
    task_environment()->RunUntilIdle();
    EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

    // Feed in the signals.
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
        blink::mojom::AuctionAdConfigField::kAuctionSignals,
        MakeAuctionSignals(/*use_promise=*/false,
                           url::Origin::Create(kSellerUrl))
            .value());
    task_environment()->RunUntilIdle();
    EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

    for (int component = 0; component < 2; ++component) {
      const GURL& url =
          (component == 0) ? kComponentSeller1Url : kComponentSeller2Url;
      abortable_ad_auction_->ResolvedPromiseParam(
          blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(
              component),
          blink::mojom::AuctionAdConfigField::kAuctionSignals,
          MakeAuctionSignals(/*use_promise=*/false, url::Origin::Create(url))
              .value());

      if (component == 0) {
        if (inject_incorrect_call) {
          abortable_ad_auction_->ResolvedPromiseParam(
              blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(
                  component),
              blink::mojom::AuctionAdConfigField::kAuctionSignals,
              MakeAuctionSignals(/*use_promise=*/false,
                                 url::Origin::Create(url))
                  .value());
        }
        task_environment()->RunUntilIdle();
        EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
      }
    }

    if (inject_incorrect_call) {
      EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
    }

    // TODO(morlovich): This should eventually abort rather than succeed in the
    // `inject_incorrect_call` case.
    auction_run_loop_->Run();

    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
    EXPECT_THAT(result_.errors, testing::ElementsAre());
  }
}

// Make sure the scoring portion of the auction waits to have promises resolved.
// Checking at bidding time is not enough since a top-level auction can receive
// bids to score from component auctions, and those complete their configuration
// independently.
TEST_F(AuctionRunnerTest, PromiseSignalsSellerDependency) {
  use_promise_for_seller_signals_ = true;

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/false);
  // Make sure there is nothing at top-level.
  interest_group_buyers_->clear();

  StartStandardAuction();

  // Can't complete yet.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in the signals for component auctions (but not top-level auction)
  for (int component = 0; component < 2; ++component) {
    const GURL& url =
        (component == 0) ? kComponentSeller1Url : kComponentSeller2Url;
    abortable_ad_auction_->ResolvedPromiseParam(
        blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(component),
        blink::mojom::AuctionAdConfigField::kSellerSignals,
        MakeSellerSignals(/*use_promise=*/false, url).value());

    task_environment()->RunUntilIdle();
    EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());
  }

  // Now finally pass in the main auction param, unblocking the seller worklet.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());

  auction_run_loop_->Run();
  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals with wrong component ID.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedPromiseParam", TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId2) {
  use_promise_for_per_buyer_signals_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals with wrong component ID.
  abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedPerBuyerSignalsPromise",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId3) {
  use_promise_for_buyer_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerSignals with wrong component ID.
  blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      buyer_timeouts);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedBuyerTimeoutsPromise",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId4) {
  use_promise_for_buyer_cumulative_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerSignals with wrong component ID.
  blink::AuctionConfig::BuyerTimeouts buyer_cumulative_timeouts;
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::
          kPerBuyerCumulativeTimeouts,
      buyer_cumulative_timeouts);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedBuyerTimeoutsPromise",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId5) {
  pass_promise_for_direct_from_seller_signals_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignals with wrong component ID.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      absl::nullopt);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedDirectFromSellerSignalsPromise",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionId6) {
  use_promise_for_buyer_currencies_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerCurrencies with wrong component ID.
  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  abortable_ad_auction_->ResolvedBuyerCurrenciesPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0),
      buyer_currencies);
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedBuyerCurrenciesPromise",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseSignalsBadAuctionIdAdditionalBids) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitAndEnableFeature(
      blink::features::kFledgeNegativeTargeting);
  pass_promise_for_additional_bids_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in additionalBids with wrong component ID.
  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0));
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedAdditionalBids", TakeBadMessage());
}

// An auction where the winning additional bid claims to be from an IG the user
// is already in.
TEST_F(AuctionRunnerTest, AdditionalBidAliasesInterestGroup) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitWithFeatures(
      {blink::features::kFledgeNegativeTargeting,
       blink::features::kBiddingAndScoringDebugReportingAPI},
      {});

  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  const char kAdditionalBidUrl[] =
      "https://adplatform.com/offers-contextual.js";

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, GURL(kAdditionalBidUrl),
      MakeBidScript(kSeller, "40", "https://additional.test/",
                    /*num_ad_components=*/0, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    /*debug_loss_report_url=*/"",
                    /*debug_win_report_url=*/"https://buyer-win.example.com/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(
          /*report_post_auction_signals=*/true, kSellerUrl,
          /*debug_loss_report_url=*/"",
          /*debug_win_report_url=*/"https://seller-win.example.com/"));
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

  auction_nonce_ =
      static_cast<base::Uuid>(auction_nonce_manager_->CreateAuctionNonce());
  pass_promise_for_additional_bids_ = true;

  StartStandardAuction();

  const char kBidJsonTemplate[] = R"({
    "interestGroup": {
      "name": "%s",
      "biddingLogicURL": "%s",
      "owner": "%s"
    },
    "bid": {
      "ad": {
        "bidKey": "data for 40",
        "renderURL": "data for https://additional.test/",
        "seller": "%s"
      },
      "bid": 40,
      "render": "https://additional.test"
    },
    "auctionNonce": "%s",
    "seller": "%s"
  })";

  std::string bid_json = base::StringPrintf(
      kBidJsonTemplate, kBidder1Name.c_str(), kAdditionalBidUrl,
      kBidder1.Serialize().c_str(), kSeller.Serialize().c_str(),
      auction_nonce_->AsLowercaseString().c_str(), kSeller.Serialize().c_str());

  std::map<std::string, std::vector<std::string>> additional_bids_for_nonce;
  additional_bids_for_nonce[auction_nonce_->AsLowercaseString()].push_back(
      GenerateSignedAdditionalBidHeaderPayloadPortion(
          SignedAdditionalBidFault::kNone, bid_json,
          {kPrivateKey1, kPrivateKey2},
          {kBase64PublicKey1, kBase64PublicKey2}));
  ad_auction_page_data_->AddAuctionAdditionalBidsWitnessForOrigin(
      kSeller, additional_bids_for_nonce);

  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  auction_run_loop_->Run();

  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://additional.test/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=2&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=40"),
          ReportWinUrl(/*bid=*/40,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/2,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));

  // scoreAd() does win reporting.
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  "https://seller-win.example.com/"
                  "?winningBid=40&winningBidCurrency=???&madeWinningBid=true&"
                  "highestScoringOtherBid=2&highestScoringOtherBidCurrency=???&"
                  "madeHighestScoringOtherBid=false&bid=40"));
  EXPECT_THAT(result_.debug_loss_report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://reporting.example.com/80")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/80"))))));

  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "80")))));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));

  EXPECT_THAT(
      private_aggregation_manager_.TakeLoggedPrivateAggregationRequests(),
      ElementsAreRequests(
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedReportWinPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedReportResultPrivateAggregationRequest,
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/21,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/22,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/30, /*value=*/80,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/70, /*value=*/80,
                                                 /*event_type=*/"click")));

  EXPECT_THAT(result_.private_aggregation_event_map,
              testing::UnorderedElementsAre(testing::Pair(
                  "click", ElementsAreRequests(
                               BuildPrivateAggregationRequest(/*bucket=*/30,
                                                              /*value=*/80)))));

  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumConfigPromises(1)
                   // Timing of what waits for seller worklet can vary, with
                   // execution time and platform.
                   .SetNumBidsQueuedWaitingForSellerWorklet(-1)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers-contextual.js",
                  "Destroy https://adplatform.com/offers-contextual.js"));
  EXPECT_THAT(
      title_log_,
      testing::UnorderedElementsAre(
          "FLEDGE seller worklet for https://adstuff.publisher1.com/auction.js",
          "FLEDGE bidder worklet for https://adplatform.com/offers.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js",
          "FLEDGE bidder worklet for "
          "https://adplatform.com/offers-contextual.js"));

  // Check what's in the database for the winning owner/name.
  // `winning_group_ad_metadata` checked for `prev_wins` update, but we also
  // should check the `bid_count`.
  base::RunLoop run_loop;
  EXPECT_EQ(R"({"winner": -2})", result_.winning_group_ad_metadata);
  interest_group_manager_->GetInterestGroup(
      kBidder1Key,
      base::BindLambdaForTesting(
          [&](absl::optional<StorageInterestGroup> interest_group) {
            ASSERT_TRUE(interest_group);
            // MakeInterestGroup() set `bid_count` to 5, so it should be 6
            // (not 7).
            EXPECT_EQ(6, interest_group->bidding_browser_signals->bid_count);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Should also not have a k-anon key for additionalBid.
  base::RunLoop run_loop2;
  std::string kanon_key = blink::KAnonKeyForAdBid(
      kBidder1, GURL(kAdditionalBidUrl), GURL("https://additional.test"));
  interest_group_manager_->GetLastKAnonymityReported(
      kanon_key,
      base::BindLambdaForTesting([&](absl::optional<base::Time> reported) {
        EXPECT_EQ(reported.value_or(base::Time::Min()), base::Time::Min());
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// An auction where the winning additional bid claims to be from an IG the user
// is not in.
TEST_F(AuctionRunnerTest, AdditionalBidDistinctFromInterestGroup) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitWithFeatures(
      {blink::features::kFledgeNegativeTargeting,
       blink::features::kBiddingAndScoringDebugReportingAPI},
      {});

  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  const char kAdditionalBidUrl[] =
      "https://adplatform.com/offers-contextual.js";
  const char kAdditionalBidName[] = "40";  // avoids per-buyerSignals check.

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, GURL(kAdditionalBidUrl),
      MakeBidScript(kSeller, "40", "https://additional.test/",
                    /*num_ad_components=*/0, kBidder1, kAdditionalBidName,
                    /*has_signals=*/true, "k1", "a",
                    /*report_post_auction_signals=*/true,
                    /*debug_loss_report_url=*/"",
                    /*debug_win_report_url=*/"https://buyer-win.example.com/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b",
                    /*report_post_auction_signals=*/true));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(
          /*report_post_auction_signals=*/true, kSellerUrl,
          /*debug_loss_report_url=*/"",
          /*debug_win_report_url=*/"https://seller-win.example.com/"));
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

  auction_nonce_ =
      static_cast<base::Uuid>(auction_nonce_manager_->CreateAuctionNonce());
  pass_promise_for_additional_bids_ = true;

  StartStandardAuction();

  const char kBidJsonTemplate[] = R"({
    "interestGroup": {
      "name": "%s",
      "biddingLogicURL": "%s",
      "owner": "%s"
    },
    "bid": {
      "ad": {
        "bidKey": "data for 40",
        "renderURL": "data for https://additional.test/",
        "seller": "%s"
      },
      "bid": 40,
      "render": "https://additional.test"
    },
    "auctionNonce": "%s",
    "seller": "%s"
  })";

  std::string bid_json = base::StringPrintf(
      kBidJsonTemplate, kAdditionalBidName, kAdditionalBidUrl,
      kBidder1.Serialize().c_str(), kSeller.Serialize().c_str(),
      auction_nonce_->AsLowercaseString().c_str(), kSeller.Serialize().c_str());

  std::map<std::string, std::vector<std::string>> additional_bids_for_nonce;
  additional_bids_for_nonce[auction_nonce_->AsLowercaseString()].push_back(
      GenerateSignedAdditionalBidHeaderPayloadPortion(
          SignedAdditionalBidFault::kNone, bid_json,
          {kPrivateKey1, kPrivateKey2},
          {kBase64PublicKey1, kBase64PublicKey2}));
  ad_auction_page_data_->AddAuctionAdditionalBidsWitnessForOrigin(
      kSeller, additional_bids_for_nonce);

  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  auction_run_loop_->Run();

  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, kAdditionalBidName),
            result_.winning_group_id);
  EXPECT_EQ(GURL("https://additional.test/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(
      result_.report_urls,
      testing::UnorderedElementsAre(
          GURL("https://reporting.example.com/"
               "?highestScoringOtherBid=2&"
               "highestScoringOtherBidCurrency=???&"
               "bidCurrency=USD&bid=40"),
          ReportWinUrl(/*bid=*/40,
                       /*bid_currency=*/blink::AdCurrency::From("USD"),
                       /*highest_scoring_other_bid=*/2,
                       /*highest_scoring_other_bid_currency=*/absl::nullopt,
                       /*made_highest_scoring_other_bid=*/false)));
  // scoreAd() does win reporting.
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  "https://seller-win.example.com/"
                  "?winningBid=40&winningBidCurrency=???&madeWinningBid=true&"
                  "highestScoringOtherBid=2&highestScoringOtherBidCurrency=???&"
                  "madeHighestScoringOtherBid=false&bid=40"));
  EXPECT_THAT(result_.debug_loss_report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(
      result_.ad_beacon_map,
      testing::UnorderedElementsAre(
          testing::Pair(
              ReportingDestination::kSeller,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://reporting.example.com/80")))),
          testing::Pair(ReportingDestination::kComponentSeller,
                        testing::ElementsAre()),
          testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair(
                  "click", GURL("https://buyer-reporting.example.com/80"))))));

  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "80")))));

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));

  EXPECT_THAT(
      private_aggregation_manager_.TakeLoggedPrivateAggregationRequests(),
      ElementsAreRequests(
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedGenerateBidPrivateAggregationRequest,
          kExpectedReportWinPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedScoreAdPrivateAggregationRequest,
          kExpectedReportResultPrivateAggregationRequest,
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/21,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/10, /*value=*/22,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/30, /*value=*/80,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/50, /*value=*/60,
                                                 /*event_type=*/"click"),
          BuildPrivateAggregationForEventRequest(/*bucket=*/70, /*value=*/80,
                                                 /*event_type=*/"click")));

  EXPECT_THAT(result_.private_aggregation_event_map,
              testing::UnorderedElementsAre(testing::Pair(
                  "click", ElementsAreRequests(
                               BuildPrivateAggregationRequest(/*bucket=*/30,
                                                              /*value=*/80)))));

  // Note that this doesn't include the additional bid. This is correct for
  // database updates, but might not be right for the observer (e.g. devtools).
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumConfigPromises(1)
                   // Timing of what waits for seller worklet can vary, with
                   // execution time and platform.
                   .SetNumBidsQueuedWaitingForSellerWorklet(-1)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers-contextual.js",
                  "Destroy https://adplatform.com/offers-contextual.js"));
  EXPECT_THAT(
      title_log_,
      testing::UnorderedElementsAre(
          "FLEDGE seller worklet for https://adstuff.publisher1.com/auction.js",
          "FLEDGE bidder worklet for https://adplatform.com/offers.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js",
          "FLEDGE bidder worklet for "
          "https://adplatform.com/offers-contextual.js"));

  EXPECT_EQ("-no interest group-", result_.winning_group_ad_metadata);

  // Should also not have a k-anon key for additionalBid.
  base::RunLoop run_loop2;
  std::string kanon_key = blink::KAnonKeyForAdBid(
      kBidder1, GURL(kAdditionalBidUrl), GURL("https://additional.test"));
  interest_group_manager_->GetLastKAnonymityReported(
      kanon_key,
      base::BindLambdaForTesting([&](absl::optional<base::Time> reported) {
        EXPECT_EQ(reported.value_or(base::Time::Min()), base::Time::Min());
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

class AuctionRunnerDfssAdSlotTest : public AuctionRunnerTest {
 protected:
  AuctionRunnerDfssAdSlotTest() {
    direct_from_seller_signals_header_ad_slot_on_.InitAndEnableFeature(
        blink::features::kFledgeDirectFromSellerSignalsHeaderAdSlot);
  }

  base::test::ScopedFeatureList direct_from_seller_signals_header_ad_slot_on_;
};

TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseSignalsBadAuctionIdDirectFromSellerSignalsHeaderAdSlot) {
  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot with wrong component ID.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewComponentAuction(0), "slot1");
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("Invalid auction ID in ResolvedDirectFromSellerSignalsHeaderAdSlot",
            TakeBadMessage());
}

TEST_F(AuctionRunnerTest, PromiseInvalidDirectFromSellerSignals) {
  pass_promise_for_direct_from_seller_signals_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  blink::DirectFromSellerSignals direct_from_seller_signals;
  direct_from_seller_signals.prefix =
      GURL("https://seller.test/?query_invalid");

  // Feed in directFromSellerSignals with wrong component ID.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      std::move(direct_from_seller_signals));
  auction_run_loop_->RunUntilIdle();
  EXPECT_EQ("ResolvedDirectFromSellerSignalsPromise with invalid signals",
            TakeBadMessage());
}

// An auction that passes directFromSellerSignalsHeaderAdSlot via a promise.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseDirectFromSellerSignalsHeaderAdSlot) {
  const char kSignals[] =
      R"([{
  "adSlot": "adSlot1",
  "sellerSignals": 3
}])";
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ad_auction_page_data_->AddAuctionSignalsWitnessForOrigin(kSeller, kSignals);

  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// An auction that passes nullopt directFromSellerSignalsHeaderAdSlot via a
// promise.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseNulloptDirectFromSellerSignalsHeaderAdSlot) {
  const char kSignals[] =
      R"([{
  "adSlot": "adSlot1",
  "sellerSignals": 3
}])";
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ad_auction_page_data_->AddAuctionSignalsWitnessForOrigin(kSeller, kSignals);

  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// An auction that passes directFromSellerSignalsHeaderAdSlot via a promise.
// JSON parsing completes before other promises resolve.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseDirectFromSellerSignalsHeaderAdSlotResolvesBeforePromises) {
  const char kSignals[] =
      R"([{
  "adSlot": "adSlot1",
  "sellerSignals": 3
}])";
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ad_auction_page_data_->AddAuctionSignalsWitnessForOrigin(kSeller, kSignals);

  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;
  use_promise_for_seller_signals_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");

  // Can't complete yet.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals,
      MakeSellerSignals(/*use_promise=*/false, kSellerUrl).value());

  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// An auction that passes directFromSellerSignalsHeaderAdSlot via a promise --
// encountered errors are reported.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseDirectFromSellerSignalsHeaderAdSlotReportsErrors) {
  const char kSignals[] =
      R"({
  "adSlot": "adSlot1",
  "sellerSignals": 3
})";
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ad_auction_page_data_->AddAuctionSignalsWitnessForOrigin(kSeller, kSignals);

  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  auction_run_loop_->Run();

  EXPECT_EQ(InterestGroupKey(kBidder2, kBidder2Name), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          testing::Eq(
              "When looking for directFromSellerSignalsHeaderAdSlot adSlot1, "
              "encountered response where top-level JSON value isn't an array: "
              "Ad-Auction-Signals={\n  \"adSlot\": \"adSlot1\",\n  "
              "\"sellerSignals\": 3\n}"),
          testing::Eq("When looking for directFromSellerSignalsHeaderAdSlot "
                      "adSlot1, failed to find a matching response.")));

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// An auction that passes directFromSellerSignalsHeaderAdSlot via a promise, but
// the auction fails since there's no AdAuctionPageData.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseDirectFromSellerSignalsHeaderAdSlotNoPageData) {
  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  auction_run_loop_->Run();

  EXPECT_EQ(absl::nullopt, result_.winning_group_id);
  EXPECT_EQ(absl::nullopt, result_.ad_descriptor);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
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
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in auctionSignals twice.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kAuctionSignals, absl::nullopt);
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in sellerSignals twice.
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  abortable_ad_auction_->ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigField::kSellerSignals, absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedPromiseParam updating non-promise", TakeBadMessage());
}

// Trying to update perBuyerSignals twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise5) {
  // Have two kind of promises so we don't just finish after first
  // perBuyerSignals update
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerSignals twice.
  abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedPerBuyerSignalsPromise updating non-promise",
            TakeBadMessage());
}

// Trying to update buyer timeouts twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise6) {
  // Have two kind of promises so we don't just finish after first update.
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in buyer timeouts twice.
  blink::AuctionConfig::BuyerTimeouts timeouts;
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      timeouts);
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      timeouts);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedBuyerTimeoutsPromise updating non-promise",
            TakeBadMessage());
}

// Trying to update buyer cumulative timeouts twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise7) {
  // Have two kind of promises so we don't just finish after first update.
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_cumulative_timeouts_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in buyer timeouts twice.
  blink::AuctionConfig::BuyerTimeouts timeouts;
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
      timeouts);
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::
          kPerBuyerCumulativeTimeouts,
      timeouts);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedBuyerTimeoutsPromise updating non-promise",
            TakeBadMessage());
}

// Trying to update direct from seller signals twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise8) {
  // Have two kind of promises so we don't just finish after first update.
  use_promise_for_per_buyer_signals_ = true;
  pass_promise_for_direct_from_seller_signals_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in direct from seller signals twice.
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedDirectFromSellerSignalsPromise updating non-promise",
            TakeBadMessage());
}

// Server response was not passed in, but promise response called.
TEST_F(AuctionRunnerTest, PromiseServerResponseUpdateNonPromise) {
  server_response_request_id_ = absl::nullopt;

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

  abortable_ad_auction_->ResolvedAuctionAdResponsePromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      mojo_base::BigBuffer());

  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedAuctionAdResponsePromise updating non-promise",
            TakeBadMessage());
}

// Server response passed in, but promise response called twice.
TEST_F(AuctionRunnerTest, PromiseServerResponseResolveTwice) {
  const base::Uuid request_id = base::Uuid();
  server_response_request_id_ = request_id;

  const char kResponse[] = {0x02, 0x00, 0x00, 0x00, 0x02, '{', '}'};

  quiche::ObliviousHttpRequest::Context client_context =
      CreateBiddingAndAuctionEncryptionContext();
  std::string encrypted_response =
      quiche::ObliviousHttpResponse::CreateServerObliviousResponse(
          std::string(kResponse, sizeof(kResponse)), client_context)
          ->EncapsulateAndSerialize();

  std::string witnessed_hash = crypto::SHA256HashString(encrypted_response);
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  ad_auction_page_data_->AddAuctionResultWitnessForOrigin(kSeller,
                                                          witnessed_hash);

  AdAuctionRequestContext context(kSeller, {}, std::move(client_context));
  ad_auction_page_data_->RegisterAdAuctionRequestContext(request_id,
                                                         std::move(context));

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

  abortable_ad_auction_->ResolvedAuctionAdResponsePromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      mojo_base::BigBuffer(
          base::as_bytes(base::make_span(encrypted_response))));

  abortable_ad_auction_->ResolvedAuctionAdResponsePromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      mojo_base::BigBuffer(
          base::as_bytes(base::make_span(encrypted_response))));

  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedAuctionAdResponsePromise updating non-promise",
            TakeBadMessage());

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

// Trying to update perBuyerCurrencies twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromise9) {
  // Have two kind of promises so we don't just finish after first
  // perBuyerCurrencies update
  use_promise_for_per_buyer_signals_ = true;
  use_promise_for_buyer_currencies_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in perBuyerCurrencies twice.
  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  abortable_ad_auction_->ResolvedBuyerCurrenciesPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      buyer_currencies);
  abortable_ad_auction_->ResolvedBuyerCurrenciesPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      buyer_currencies);
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedBuyerCurrenciesPromise updating non-promise",
            TakeBadMessage());
}

// Trying to update additionalBids twice.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateNonPromiseAdditionalBids) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitAndEnableFeature(
      blink::features::kFledgeNegativeTargeting);

  // Have two kind of promises so we don't just finish after first
  // additionalBids update
  use_promise_for_per_buyer_signals_ = true;
  auction_nonce_ =
      static_cast<base::Uuid>(auction_nonce_manager_->CreateAuctionNonce());
  pass_promise_for_additional_bids_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in additional bids twice.
  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedAdditionalBids updating non-promise", TakeBadMessage());
}

// Trying to update additionalBids when the negative targeting feature is off.
TEST_F(AuctionRunnerTest, PromiseSignalsUpdateAdditionalBidsFeatureOff) {
  base::test::ScopedFeatureList additional_bids_off;
  additional_bids_off.InitAndDisableFeature(
      blink::features::kFledgeNegativeTargeting);

  pass_promise_for_additional_bids_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedAdditionalBids with FledgeNegativeTargeting off",
            TakeBadMessage());
}

// Trying to update directFromSellerSignalsHeaderAdSlot twice.
TEST_F(AuctionRunnerDfssAdSlotTest,
       PromiseSignalsUpdateNonPromiseDirectFromSellerSignalsHeaderAdSlot) {
  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  // Have two kind of promises so we don't just finish after first
  // directFromSellerSignalsHeaderAdSlot update
  use_promise_for_per_buyer_signals_ = true;
  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  // Feed in directFromSellerSignalsHeaderAdSlot bids twice.
  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  task_environment()->RunUntilIdle();
  EXPECT_EQ("ResolvedDirectFromSellerSignalsHeaderAdSlot updating non-promise",
            TakeBadMessage());

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

class AuctionRunnerDfssAdSlotDisabledTest : public AuctionRunnerTest {
 protected:
  AuctionRunnerDfssAdSlotDisabledTest() {
    direct_from_seller_signals_header_ad_slot_off_.InitAndDisableFeature(
        blink::features::kFledgeDirectFromSellerSignalsHeaderAdSlot);
  }

  base::test::ScopedFeatureList direct_from_seller_signals_header_ad_slot_off_;
};

// Trying to pass directFromSellerSignalsHeaderAdSlot when the
// directFromSellerSignalsHeaderAdSlot feature is off.
TEST_F(AuctionRunnerDfssAdSlotDisabledTest,
       PromiseDirectFromSellerSignalsHeaderAdSlotFeatureOff) {
  pass_promise_for_direct_from_seller_signals_header_ad_slot_ = true;

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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_run_loop_->AnyQuitCalled());

  abortable_ad_auction_->ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0), "adSlot1");
  task_environment()->RunUntilIdle();
  EXPECT_EQ(
      "ResolvedDirectFromSellerSignalsHeaderAdSlot with "
      "FledgeDirectFromSellerSignalsHeaderAdSlot off",
      TakeBadMessage());
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
        task_environment()->RunUntilIdle();

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
          task_environment()->RunUntilIdle();
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
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
      EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                    GURL("https://ad2.com-component1.com"))},
                result_.ad_component_descriptors);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_THAT(
          result_.ad_beacon_map,
          testing::UnorderedElementsAre(
              testing::Pair(
                  ReportingDestination::kSeller,
                  testing::ElementsAre(testing::Pair(
                      "click", GURL("https://reporting.example.com/4")))),
              testing::Pair(ReportingDestination::kComponentSeller,
                            testing::ElementsAre()),
              testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair(
                      "click",
                      GURL("https://buyer-reporting.example.com/4"))))));
      EXPECT_THAT(
          result_.ad_macros,
          testing::UnorderedElementsAre(testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair("${campaign}", "4")))));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
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
      EXPECT_THAT(
          result_.private_aggregation_event_map,
          testing::UnorderedElementsAre(testing::Pair(
              "click", ElementsAreRequests(BuildPrivateAggregationRequest(
                                               /*bucket=*/10, /*value=*/22),
                                           BuildPrivateAggregationRequest(
                                               /*bucket=*/30, /*value=*/42)))));

      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
      EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
                result_.winning_group_ad_metadata);
      // In the case where no SellerWorklet is available at the start of the
      // auction, the auction is blocked on scoring the ad until the
      // SellerWorklet is later released by the test. This doesn't happen if
      // there are no BidderWorklets available at the start of the auction,
      // because a BidderWorklet is released by the test after a SellerWorklet
      // is released.
      int64_t num_bids_queued = 0;
      if (seller_worklet_creation_delayed &&
          num_used_bidder_worklet_processes !=
              AuctionProcessManager::kMaxBidderProcesses) {
        num_bids_queued = 2;
      }
      CheckMetrics(
          MetricsExpectations(AuctionResult::kSuccess)
              .SetNumInterestGroups(2)
              .SetNumOwnersAndDistinctOwners(2)
              .SetNumSellers(1)
              .SetNumBidderWorklets(2)
              .SetNumInterestGroupsWithOnlyNonKAnonBid(2)
              .SetNumBidsQueuedWaitingForSellerWorklet(num_bids_queued));
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
        task_environment()->RunUntilIdle();

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
          task_environment()->RunUntilIdle();
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
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
      EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                    GURL("https://ad2.com-component1.com"))},
                result_.ad_component_descriptors);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://component2-report.test/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_THAT(
          result_.ad_beacon_map,
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
          result_.ad_macros,
          testing::UnorderedElementsAre(testing::Pair(
              ReportingDestination::kBuyer,
              testing::ElementsAre(testing::Pair("${campaign}", "4")))));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
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
      EXPECT_THAT(
          result_.private_aggregation_event_map,
          testing::UnorderedElementsAre(testing::Pair(
              "click", ElementsAreRequests(BuildPrivateAggregationRequest(
                                               /*bucket=*/10, /*value=*/22),
                                           BuildPrivateAggregationRequest(
                                               /*bucket=*/30, /*value=*/42)))));

      int64_t num_top_level_bids_queued = 0;
      int64_t num_component_bids_queued = 0;

      // The component auction needs three SellerWorklets in total. In all of
      // the cases where we're making an insufficient number of these available,
      // the top-level InterestGroupAuction will need to wait for the
      // SellerWorklet to become available to rescore the bids.
      //
      // Only a much weaker expectation can be applied to the number of
      // component bids queued. In all cases where we limit the number of
      // available SellerWorklets, it's possible that the component bids are
      // generated before a SellerWorklet is available. Because the ordering of
      // these events is non-deterministic, we can only make lower-bound
      // assumptions on how many of the bids will be queued as a result. As
      // such, we simply don't check these metrics in this case. We still check
      // that no bids are queued when all SellerWorklets are available.
      if (num_used_seller_worklet_processes > 0) {
        num_top_level_bids_queued = 2;
        num_component_bids_queued = -1;
      }
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(2)
                       .SetNumOwnersAndDistinctOwners(2)
                       .SetNumSellers(3)
                       .SetNumBidderWorklets(2)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(2)
                       .SetNumTopLevelBidsQueuedWaitingForSellerWorklet(
                           num_top_level_bids_queued)
                       .SetNumBidsQueuedWaitingForSellerWorklet(
                           num_component_bids_queued));
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kSellerWorkletLoadFailed)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2));
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_EQ(std::vector<blink::AdDescriptor>{blink::AdDescriptor(
                GURL("https://ad2.com-component1.com"))},
            result_.ad_component_descriptors);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://component2-report.test/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(
      result_.ad_beacon_map,
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
  EXPECT_THAT(result_.ad_macros,
              testing::UnorderedElementsAre(testing::Pair(
                  ReportingDestination::kBuyer,
                  testing::ElementsAre(testing::Pair("${campaign}", "4")))));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
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
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(testing::Pair(
          "click",
          ElementsAreRequests(
              BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/22),
              BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/42)))));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1)
                   .SetNumTopLevelBidsQueuedWaitingForSellerWorklet(1));
}

// Test to make sure SendPendingSignalsRequests is called on a seller worklet
// if the worklet becomes available only after everything is queued.
TEST_F(AuctionRunnerTest, LateSellerWorkletSendPendingSignalsRequestsCalled) {
  UseMockWorkletService();

  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  // Make kMaxSellerProcesses seller worklet requests for other origins so
  // seller worklet creation will be blocked by the process limit.
  for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses; ++i) {
    sellers.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    EXPECT_TRUE(mock_auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kSeller, origin,
        scoped_refptr<SiteInstance>(), &*sellers.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  StartStandardAuction();
  mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/2,
                                                 /*num_sellers=*/0);

  // Let bidder worklets finish all the work while seller worklet is not
  // available yet.
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/6, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad1.com/")));
  bidder1_worklet.reset();
  bidder2_worklet->InvokeGenerateBidCallback(
      /*bid=*/7, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad2.com/")));
  bidder2_worklet.reset();

  mock_auction_process_manager_->Flush();

  // Make seller worklet available and finish the auction.
  sellers.clear();
  mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/0,
                                                 /*num_sellers=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(6, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
          /*errors=*/{});

  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(7, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/11,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
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

  // Bidder2 won.
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  // ~MockSellerWorklet verifies whether SendPendingSignalsRequests() was
  // called.
  seller_worklet->set_expect_send_pending_signals_requests_called(true);
  seller_worklet.reset();
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
        render: interestGroup.ads[0].renderURL
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
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=key0,key1&interestGroupNames=0,1"),
      R"({"keys":{"key0":2, "key1": 1}})", /*data_version=*/4);

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_TRUE(auction_complete_);

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "0"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.ad_beacon_map,
              testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
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

  task_environment()->RunUntilIdle();

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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsAbortedByBidderWorkletFatalError(2)
                   .SetNumInterestGroupsWithNoBids(2));
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
      bidder2_worklet->InvokeGenerateBidCallback(
          /*bid=*/7, /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad2.com/")));
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
    task_environment()->RunUntilIdle();

    if (!other_bidder_finishes_first) {
      bidder2_worklet->InvokeGenerateBidCallback(
          /*bid=*/7, /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad2.com/")));
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
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
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
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
    EXPECT_TRUE(result_.ad_component_descriptors.empty());
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_THAT(result_.ad_beacon_map,
                testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
    EXPECT_TRUE(
        private_aggregation_manager_.TakePrivateAggregationRequests().empty());
    EXPECT_TRUE(result_.private_aggregation_event_map.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre(kBidder2Key));
    EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
              result_.winning_group_ad_metadata);
    CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                     .SetNumInterestGroups(2)
                     .SetNumOwnersAndDistinctOwners(2)
                     .SetNumSellers(1)
                     .SetNumBidderWorklets(2)
                     .SetNumBidsAbortedByBidderWorkletFatalError(1)
                     .SetNumInterestGroupsWithNoBids(1)
                     .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
  }
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
  EXPECT_EQ(0u, result_.debug_win_report_urls.size());
}

// If the seller crashes before all bids are scored, the auction fails. Seller
// load failures look the same to auctions, so this test also covers load
// failures in the same places. Note that a seller worklet load error while
// waiting for bidder worklet processes is covered in another test, and looks
// exactly like a crash at the same point to the AuctionRunner.
TEST_F(AuctionRunnerTest, SellerCrash) {
  enum class CrashPhase {
    kLoad,
    kScoreBid,
  };
  for (CrashPhase crash_phase : {CrashPhase::kLoad, CrashPhase::kScoreBid}) {
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

    if (crash_phase == CrashPhase::kLoad) {
      seller_worklet->set_expect_send_pending_signals_requests_called(false);
    } else {
      // Generate both bids, wait for seller to receive them..
      bidder1_worklet->InvokeGenerateBidCallback(
          /*bid=*/5, /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com/")));
      bidder2_worklet->InvokeGenerateBidCallback(
          /*bid=*/7, /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad2.com/")));
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      auto score_ad_params2 = seller_worklet->WaitForScoreAd();

      // Wait for SendPendingSignalsRequests() invocation.
      task_environment()->RunUntilIdle();
    }

    // Simulate seller crash.
    seller_worklet.reset();

    // Wait for auction to complete.
    auction_run_loop_->Run();

    EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                    "%s crashed.", kSellerUrl.spec().c_str())));
    // No bidder won, seller crashed.
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_descriptor);
    EXPECT_TRUE(result_.ad_component_descriptors.empty());
    EXPECT_TRUE(
        private_aggregation_manager_.TakePrivateAggregationRequests().empty());
    EXPECT_TRUE(result_.private_aggregation_event_map.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());
    MetricsExpectations expectations(AuctionResult::kSellerWorkletCrashed);
    expectations.SetNumInterestGroups(2)
        .SetNumOwnersAndDistinctOwners(2)
        .SetNumSellers(1)
        .SetNumBidderWorklets(2);
    if (crash_phase == CrashPhase::kScoreBid) {
      expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(2);
    }
    expectations.SetHasScoreAdFlowLatencyMetrics(false)
        .SetHasScoreAdLatencyMetrics(false);
    CheckMetrics(expectations);
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
  EXPECT_FALSE(result_.ad_descriptor);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsAbortedByBidderWorkletFatalError(2)
                   .SetNumInterestGroupsWithNoBids(2));
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
  task_environment()->RunUntilIdle();

  // The second bidder worklet bids.
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet->InvokeGenerateBidCallback(
      2, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad2.com/")));

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
              /*bid_currency=*/absl::nullopt,
              /*has_bid=*/false),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
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
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                            GURL("https://report2.test/"),
                                            GURL("https://report3.test/")));
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder2Key));
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(2)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsAbortedByBidderWorkletFatalError(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());
  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(3)
                   .SetNumBidderWorklets(2));
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
           /*bid=*/0, /*bid_currency=*/absl::nullopt,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-1, /*bid_currency=*/absl::nullopt,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/std::numeric_limits<double>::infinity(),
           /*bid_currency=*/absl::nullopt,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-std::numeric_limits<double>::infinity(),
           /*bid_currency=*/absl::nullopt,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/-std::numeric_limits<double>::quiet_NaN(),
           /*bid_currency=*/absl::nullopt,
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid"},

      // Bad currencies.
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/2,
           /*bid_currency=*/blink::AdCurrency::From("USD"),
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid_currency"},
      {auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
           /*ad=*/"null",
           /*bid=*/2,
           /*bid_currency=*/blink::AdCurrency::From("CAD"),
           /*has_bid=*/true),
       "Invalid component_auction_modified_bid_params bid_currency"}};

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true);
  // The default top-level config requires USD, so one of the two currency
  // checks will always fail here.
  component_auctions_[0].non_shared_params.seller_currency =
      blink::AdCurrency::From("CAD");

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
    bidder2_worklet->InvokeGenerateBidCallback(
        2, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad2.com/")));

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
        ->OnScoreAdComplete(
            /*score=*/3,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            test_case.params.Clone(),
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt,
            /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
            /*errors=*/{});

    // The auction fails, because of the bad ComponentAuctionModifiedBidParams.
    auction_run_loop_->Run();
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_FALSE(result_.ad_descriptor);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());

    // Since these are security errors rather than script errors, they're
    // reported as bad Mojo messages, instead of in the return error list.
    EXPECT_EQ(test_case.expected_error, TakeBadMessage());

    // The component auction failed with a Mojo error, but the top-level auction
    // sees that as no bids.
    CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                     .SetNumInterestGroups(2)
                     .SetNumOwnersAndDistinctOwners(2)
                     .SetNumSellers(2)
                     .SetNumBidderWorklets(2)
                     .SetNumInterestGroupsWithOnlyNonKAnonBid(1)
                     .SetHasScoreAdFlowLatencyMetrics(false)
                     .SetHasScoreAdLatencyMetrics(false));
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
  bidder2_worklet->InvokeGenerateBidCallback(
      2, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad2.com/")));

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
              /*bid_currency=*/absl::nullopt,
              /*has_bid=*/false),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
          /*errors=*/{});

  auction_run_loop_->Run();

  // The auction fails, because of the unexpected
  // ComponentAuctionModifiedBidParams.
  //
  // Since these are security errors rather than script errors, they're
  // reported as bad Mojo messages, instead of in the return error list.
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ("Invalid component_auction_modified_bid_params", TakeBadMessage());
  EXPECT_FALSE(result_.ad_descriptor);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre());

  CheckMetrics(MetricsExpectations(AuctionResult::kBadMojoMessage)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1)
                   .SetHasScoreAdFlowLatencyMetrics(false)
                   .SetHasScoreAdLatencyMetrics(false));
}

TEST_F(AuctionRunnerTest, NullAdComponents) {
  const GURL kRenderUrl = GURL("https://ad1.com");
  const struct {
    absl::optional<std::vector<blink::AdDescriptor>> bid_ad_component_urls;
    bool expect_successful_bid;
  } kTestCases[] = {
      {absl::nullopt, true},
      {std::vector<blink::AdDescriptor>{}, false},
      {std::vector<blink::AdDescriptor>{
           blink::AdDescriptor(GURL("https://ad1.com-component1.com"))},
       false},
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
        /*bid=*/1, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(kRenderUrl),
        /*mojo_kanon_bid=*/nullptr, test_case.bid_ad_component_urls,
        base::TimeDelta());

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
              /*bid_in_seller_currency=*/absl::nullopt,
              /*scoring_signals_data_version=*/absl::nullopt,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*scoring_latency=*/base::TimeDelta(),
              /*score_ad_dependency_latencies=*/
              auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                  /*code_ready_latency=*/absl::nullopt,
                  /*direct_from_seller_signals_latency=*/absl::nullopt,
                  /*trusted_scoring_signals_latency=*/absl::nullopt),
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
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_THAT(result_.ad_beacon_map,
                  testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
      EXPECT_TRUE(private_aggregation_manager_.TakePrivateAggregationRequests()
                      .empty());
      EXPECT_TRUE(result_.private_aggregation_event_map.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key));
      EXPECT_EQ(
          R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
          result_.winning_group_ad_metadata);
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Unexpected non-null ad component list", TakeBadMessage());

      // No bidder won.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_descriptor);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      EXPECT_TRUE(private_aggregation_manager_.TakePrivateAggregationRequests()
                      .empty());
      EXPECT_TRUE(result_.private_aggregation_event_map.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre());
      CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1)
                       .SetHasScoreAdFlowLatencyMetrics(false)
                       .SetHasScoreAdLatencyMetrics(false));
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
    std::vector<blink::AdDescriptor> ad_component_descriptors;
    for (size_t i = 0; i < num_components; ++i) {
      ad_component_urls.emplace_back(base::StringPrintf("https://%zu.com", i));
      ad_component_descriptors.emplace_back(
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
        /*bid=*/1, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(kRenderUrl),
        /*mojo_kanon_bid=*/nullptr, ad_component_descriptors,
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
              /*bid_in_seller_currency=*/absl::nullopt,
              /*scoring_signals_data_version=*/absl::nullopt,
              /*debug_loss_report_url=*/absl::nullopt,
              /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
              /*scoring_latency=*/base::TimeDelta(),
              /*score_ad_dependency_latencies=*/
              auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                  /*code_ready_latency=*/absl::nullopt,
                  /*direct_from_seller_signals_latency=*/absl::nullopt,
                  /*trusted_scoring_signals_latency=*/absl::nullopt),
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
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
      EXPECT_EQ(ad_component_descriptors, result_.ad_component_descriptors);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_THAT(result_.ad_beacon_map,
                  testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
      EXPECT_TRUE(private_aggregation_manager_.TakePrivateAggregationRequests()
                      .empty());
      EXPECT_TRUE(result_.private_aggregation_event_map.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre(kBidder1Key));
      EXPECT_EQ(
          R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
          result_.winning_group_ad_metadata);
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Too many ad component URLs", TakeBadMessage());

      // No bidder won.
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_FALSE(result_.winning_group_id);
      EXPECT_FALSE(result_.ad_descriptor);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      EXPECT_TRUE(private_aggregation_manager_.TakePrivateAggregationRequests()
                      .empty());
      EXPECT_TRUE(result_.private_aggregation_event_map.empty());
      EXPECT_THAT(result_.interest_groups_that_bid,
                  testing::UnorderedElementsAre());
      CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1)
                       .SetHasScoreAdFlowLatencyMetrics(false)
                       .SetHasScoreAdLatencyMetrics(false));
    }
  }
}

// Test cases where a bad bid is received over Mojo. Bad bids should be rejected
// in the Mojo process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadBid) {
  const struct TestCase {
    const char* expected_error_message;
    double bid;
    absl::optional<blink::AdCurrency> bid_currency;
    blink::AdDescriptor ad_descriptor;
    absl::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors;
    base::TimeDelta duration;
    bool is_invalid_size_test = false;
    auction_worklet::mojom::RejectReason reject_reason =
        auction_worklet::mojom::RejectReason::kNotAvailable;
  } kTestCases[] = {
      // Bids that aren't positive integers.
      {
          "Invalid bid value",
          -10,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          0,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::infinity(),
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::quiet_NaN(),
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid currencies.
      {
          "Invalid bid currency",
          // This is syntactically valid but test fixture says USD.
          1.0,
          blink::AdCurrency::From("CAD"),
          blink::AdDescriptor(GURL("https://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid render URL.
      {
          "Bid render ad must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL(":")),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Non-HTTPS render URLs.
      {
          "Bid render ad must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("data:,foo")),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Bid render ad must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("http://ad1.com")),
          absl::nullopt,
          base::TimeDelta(),
      },

      // HTTPS render URL that's not in the list of allowed renderURLs.
      {
          "Bid render ad must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad2.com")),
          absl::nullopt,
          base::TimeDelta(),
      },

      // HTTPS render URL with an invalid size value.
      {"Validation failed for auction_worklet.mojom.GenerateBidClient.1  "
       "[VALIDATION_ERROR_DESERIALIZATION_FAILED]",
       1,
       /*bid_currency=*/absl::nullopt,
       blink::AdDescriptor(
           GURL("https://ad1.com"),
           blink::AdSize(0, blink::AdSize::LengthUnit::kPixels, 100,
                         blink::AdSize::LengthUnit::kPixels)),
       absl::nullopt, base::TimeDelta(), true},

      // HTTPS render URL with an invalid size unit.
      {
          "Validation failed for auction_worklet.mojom.GenerateBidClient.1  "
          "[VALIDATION_ERROR_DESERIALIZATION_FAILED]",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(
              GURL("https://ad1.com"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kInvalid, 100,
                            blink::AdSize::LengthUnit::kPixels)),
          absl::nullopt,
          base::TimeDelta(),
          true,
      },

      // Invalid component URL.
      {
          "Bid ad component must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          std::vector<blink::AdDescriptor>{blink::AdDescriptor(GURL(":"))},
          base::TimeDelta(),
      },

      // HTTPS component URL that's not in the list of allowed ad component
      // URLs.
      {
          "Bid ad component must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad2.com-component1.com"))},
          base::TimeDelta(),
      },
      {
          "Bid ad component must have a valid URL and size (if specified)",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad1.com")),
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad1.com-component1.com")),
              blink::AdDescriptor(GURL("https://ad2.com-component1.com"))},
          base::TimeDelta(),
      },

      // HTTPS component URL with an invalid size value.
      {"Validation failed for auction_worklet.mojom.GenerateBidClient.1  "
       "[VALIDATION_ERROR_DESERIALIZATION_FAILED]",
       1,
       /*bid_currency=*/absl::nullopt,
       blink::AdDescriptor(GURL("https://ad1.com")),
       std::vector<blink::AdDescriptor>{blink::AdDescriptor(
           GURL("https://ad1.com-component1.com"),
           blink::AdSize(0, blink::AdSize::LengthUnit::kPixels, 100,
                         blink::AdSize::LengthUnit::kPixels))},
       base::TimeDelta(), true},
      // HTTPS component URL with an invalid size unit.
      {"Validation failed for auction_worklet.mojom.GenerateBidClient.1  "
       "[VALIDATION_ERROR_DESERIALIZATION_FAILED]",
       1,
       /*bid_currency=*/absl::nullopt,
       blink::AdDescriptor(GURL("https://ad1.com")),
       std::vector<blink::AdDescriptor>{blink::AdDescriptor(
           GURL("https://ad1.com-component1.com"),
           blink::AdSize(100, blink::AdSize::LengthUnit::kInvalid, 100,
                         blink::AdSize::LengthUnit::kPixels))},
       base::TimeDelta(), true},

      // Negative time.
      {
          "Invalid bid duration",
          1,
          /*bid_currency=*/absl::nullopt,
          blink::AdDescriptor(GURL("https://ad2.com")),
          absl::nullopt,
          base::Milliseconds(-1),
      },

      // Reject reason validation.
      {"Invalid bid reject_reason", 1,
       /*bid_currency=*/absl::nullopt,
       blink::AdDescriptor(GURL("https://ad2.com")), absl::nullopt,
       base::Milliseconds(10), false,
       auction_worklet::mojom::RejectReason::kCategoryExclusions},
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
        test_case.bid, test_case.bid_currency, test_case.ad_descriptor,
        /*mojo_kanon_bid=*/nullptr, test_case.ad_component_descriptors,
        test_case.duration, /*bidding_signals_data_version=*/
        absl::nullopt, /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*pa_requests=*/{},
        /*dependency_latencies=*/
        auction_worklet::mojom::GenerateBidDependencyLatenciesPtr(),
        test_case.reject_reason);
    // Bidder 2 doesn't bid.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    // Invalid ad sizes fail in Mojom deserialization.
    if (test_case.is_invalid_size_test) {
      EXPECT_THAT(
          result_.errors,
          testing::ElementsAre(
              "https://adplatform.com/offers.js crashed while trying to run "
              "generateBid()."));
    } else {
      EXPECT_THAT(result_.errors, testing::ElementsAre());
    }
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_descriptor);
    EXPECT_TRUE(result_.ad_component_descriptors.empty());
    EXPECT_TRUE(
        private_aggregation_manager_.TakePrivateAggregationRequests().empty());
    EXPECT_TRUE(result_.private_aggregation_event_map.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());
  }
}

// Testcase for mojo errors in ScoreAd result's bid_in_seller_currency; note
// that problems with ComponentAuctionModifiedBidParams are covered in
// *BadBidParams* tests and those with debug URLs in
// ForDebuggingOnlyReportingSellerBadUrls.
TEST_F(AuctionRunnerTest, BadScoreAdBidInSellerCurrency) {
  const struct TestCase {
    const char* test_name;
    absl::optional<blink::AdCurrency> seller_currency;
    absl::optional<double> bid_in_seller_currency;
  } kTestCases[] = {
      {
          "Should not specify when no seller_currency",
          absl::nullopt,
          5,
      },
      {
          "Must be a valid bid",
          blink::AdCurrency::From("CAD"),
          -5,
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.test_name);
    seller_currency_ = test_case.seller_currency;

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
        /*bid=*/5, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad1.com/")));
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
            /*bid_in_seller_currency=*/test_case.bid_in_seller_currency,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
            /*errors=*/{});
    auction_run_loop_->Run();
    EXPECT_EQ("Invalid bid_in_seller_currency", TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.winning_group_id);
  }
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
  bidder2_worklet->InvokeGenerateBidCallback(
      /*bid=*/7, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad2.com/")));
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
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
  EXPECT_TRUE(result_.ad_component_descriptors.empty());
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.ad_beacon_map,
              testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder2Key));
  EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
            result_.winning_group_ad_metadata);
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(2)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad1.com/")));
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
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
            /*errors=*/{});

    // Bidder2 returns a bid, which is then scored.
    bidder2_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad2.com/")));
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
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
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
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      EXPECT_EQ(
          R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
          result_.winning_group_ad_metadata);
    } else {
      seen_bidder2_win = true;

      ASSERT_TRUE(bidder2_worklet);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_THAT(result_.errors, testing::ElementsAre());
      EXPECT_EQ(kBidder2Key, result_.winning_group_id);
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
      EXPECT_TRUE(result_.ad_component_descriptors.empty());
      EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
                result_.winning_group_ad_metadata);
    }

    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_THAT(result_.ad_beacon_map,
                testing::UnorderedElementsAreArray(kEmptyAdBeaconMap));
    EXPECT_TRUE(
        private_aggregation_manager_.TakePrivateAggregationRequests().empty());
    EXPECT_TRUE(result_.private_aggregation_event_map.empty());
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre(kBidder1Key, kBidder2Key));
    CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                     .SetNumInterestGroups(2)
                     .SetNumOwnersAndDistinctOwners(2)
                     .SetNumSellers(1)
                     .SetNumBidderWorklets(2)
                     .SetNumInterestGroupsWithOnlyNonKAnonBid(2));
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
                /*bid=*/9, /*bid_currency=*/absl::nullopt,
                blink::AdDescriptor(GURL("https://ad1.com/")));
            score_ad_params1 = seller_worklet->WaitForScoreAd();
            EXPECT_EQ(kBidder1, score_ad_params1.interest_group_owner);
            EXPECT_EQ(9, score_ad_params1.bid);
            break;
          case Event::kBid2Generated:
            bidder2_worklet->InvokeGenerateBidCallback(
                /*bid=*/10, /*bid_currency=*/absl::nullopt,
                blink::AdDescriptor(GURL("https://ad2.com/")));
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
                    /*bid_in_seller_currency=*/absl::nullopt,
                    /*scoring_signals_data_version=*/absl::nullopt,
                    /*debug_loss_report_url=*/absl::nullopt,
                    /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
                    /*scoring_latency=*/base::TimeDelta(),
                    /*score_ad_dependency_latencies=*/
                    auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                        /*code_ready_latency=*/absl::nullopt,
                        /*direct_from_seller_signals_latency=*/absl::nullopt,
                        /*trusted_scoring_signals_latency=*/absl::nullopt),
                    /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment()->RunUntilIdle();
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
                    /*bid_in_seller_currency=*/absl::nullopt,
                    /*scoring_signals_data_version=*/absl::nullopt,
                    /*debug_loss_report_url=*/absl::nullopt,
                    /*debug_win_report_url=*/absl::nullopt,
                    /*pa_requests=*/{},
                    /*scoring_latency=*/base::TimeDelta(),
                    /*score_ad_dependency_latencies=*/
                    auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                        /*code_ready_latency=*/absl::nullopt,
                        /*direct_from_seller_signals_latency=*/absl::nullopt,
                        /*trusted_scoring_signals_latency=*/absl::nullopt),
                    /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment()->RunUntilIdle();
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
        EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
        EXPECT_EQ(
            R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);
      } else {
        EXPECT_EQ(kBidder2Key, result_.winning_group_id);
        EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);
        EXPECT_EQ(R"({"renderURL":"https://ad2.com/"})",
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
    ASSERT_TRUE(result_.ad_descriptor);

    int winner;
    if (result_.ad_descriptor->url.spec() == "https://ad1.com/") {
      winner = 0;
    } else if (result_.ad_descriptor->url.spec() == "https://ad2.com/") {
      winner = 1;
    } else {
      ASSERT_EQ(result_.ad_descriptor->url.spec(), "https://ad3.com/");
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
}

TEST_F(AuctionRunnerTest, ExecutionModeGroupByOrigin) {
  // Test of group-by-origin execution mode at AuctionRunner level;
  // this primarily shows that the sorting actually groups things, and that
  // distinct groups are kept separate.
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 1;
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
  // Add 5 group-by-origin, 2 frozen, 2 regular execution mode IGs.
  for (int i = 0; i < 9; ++i) {
    StorageInterestGroup ig = MakeInterestGroup(
        kBidder1, kBidder1Name + base::NumberToString(i), kBidder1Url,
        /* trusted_bidding_signals_url=*/absl::nullopt,
        /* trusted_bidding_signals_keys=*/{}, GURL("https://response.test/"));
    ig.joining_origin = url::Origin::Create(GURL("https://sports.example.org"));
    if (i < 5) {
      ig.interest_group.execution_mode =
          blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    } else if (i < 7) {
      ig.interest_group.execution_mode =
          blink::InterestGroup::ExecutionMode::kFrozenContext;
    } else {
      ig.interest_group.execution_mode =
          blink::InterestGroup::ExecutionMode::kCompatibilityMode;
    }
    bidders.push_back(std::move(ig));
  }

  // Add one with different join origin.
  StorageInterestGroup ig = MakeInterestGroup(
      kBidder1, kBidder1Name + std::string("10"), kBidder1Url,
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
              testing::ElementsAre(GURL("https://adplatform.com/metrics/6")));
}

// Test the case where the only bidder times out due to the
// perBuyerCumulativeTimeouts.
TEST_F(AuctionRunnerTest, PerBuyerCumulativeTimeouts) {
  interest_group_buyers_ = {{kBidder1}};
  StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  task_environment()->FastForwardBy(kBidder1CumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);
  task_environment()->FastForwardBy(kTinyTime);
  EXPECT_TRUE(auction_complete_);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "https://adplatform.com/offers.js perBuyerCumulativeTimeout "
                  "exceeded during bid generation."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsAbortedByBuyerCumulativeTimeout(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   // We explicitly include timeouts in these metrics.
                   .SetHasBidForOneInterestGroupLatencyMetrics(true));
}

// Test the case where the perBuyerCumulativeTimeout expires during the
// scoreAd() call. The bid should not be timed out.
TEST_F(AuctionRunnerTest,
       PerBuyerCumulativeTimeoutsTimeoutPassesDuringScoreAd) {
  interest_group_buyers_ = {{kBidder1}};
  StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  // The timeout isn't quite hit.
  task_environment()->FastForwardBy(kBidder1CumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);

  // Bid generation completes.
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/2, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad1.com/")));

  // More than the timeout time passes, but since the bid is being blocked on
  // the seller, there should be no timeout.
  task_environment()->FastForwardBy(2 * kBidder1CumulativeTimeout);
  EXPECT_FALSE(auction_complete_);

  // Score the ad.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
          /*errors=*/{});

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
}

// Test the case where a pending promise delays the start of the
// perBuyerCumulativeTimeout, but generating a bid still times out since
// perBuyerCumulativeTimeout passes after promise resolution.
TEST_F(AuctionRunnerTest,
       PerBuyerCumulativeTimeoutsPromiseDelaysTimeoutButStillTimesOut) {
  use_promise_for_buyer_cumulative_timeouts_ = true;
  interest_group_buyers_ = {{kBidder1}};
  StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  // The timeout duration passes, but since the seller is being waited on, too,
  // this doesn't count towards the timeout.
  task_environment()->FastForwardBy(2 * kBidder1CumulativeTimeout);
  EXPECT_FALSE(auction_complete_);

  // Feed in perBuyerCumulativeTimeouts.
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::
          kPerBuyerCumulativeTimeouts,
      MakeBuyerCumulativeTimeouts(/*use_promise=*/false).value());

  // The timeout passes again, but this time, it counts towards the cumulative
  // timeout.
  task_environment()->FastForwardBy(kBidder1CumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);
  task_environment()->FastForwardBy(kTinyTime);
  EXPECT_TRUE(auction_complete_);

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "https://adplatform.com/offers.js perBuyerCumulativeTimeout "
                  "exceeded during bid generation."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);
}

// Test the case where a pending promise delays the start of the
// perBuyerCumulativeTimeout, and a bid is ultimately generated successfully
// because of the delayed promise resolution.
TEST_F(AuctionRunnerTest,
       PerBuyerCumulativeTimeoutsPromiseDelaysTimeoutAndNoTimeout) {
  use_promise_for_buyer_cumulative_timeouts_ = true;
  interest_group_buyers_ = {{kBidder1}};
  StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  // The timeout duration passes, but since the seller is being waited on, too,
  // this doesn't count towards the timeout.
  task_environment()->FastForwardBy(2 * kBidder1CumulativeTimeout);
  EXPECT_FALSE(auction_complete_);

  // Feed in perBuyerCumulativeTimeouts.
  abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
      blink::mojom::AuctionAdConfigBuyerTimeoutField::
          kPerBuyerCumulativeTimeouts,
      MakeBuyerCumulativeTimeouts(/*use_promise=*/false).value());

  // The timeout doesn't quite pass after the promise is resolved.
  task_environment()->FastForwardBy(kBidder1CumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);

  // Bid generation completes.
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/2, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad1.com/")));

  // Score the ad.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
          /*errors=*/{});

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
}

// Test that the cumulative timeout only starts once a process is assigned.
TEST_F(AuctionRunnerTest, PerBuyerCumulativeTimeoutsWaitForProcess) {
  // Create AuctionProcessManager in advance of starting the auction so can
  // create worklets before the auction starts.
  UseMockWorkletService();

  // Fill up all bidding process slots.
  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      busy_processes;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    busy_processes.push_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin,
        scoped_refptr<SiteInstance>(), &*busy_processes.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }
  task_environment()->RunUntilIdle();

  // Start a 1-bidder auction.
  interest_group_buyers_ = {{kBidder1}};
  StartStandardAuction();

  // The timeout should not have started, since the bidder is still waiting on a
  // process slot.
  task_environment()->FastForwardBy(2 * kBidder1CumulativeTimeout);
  EXPECT_FALSE(auction_complete_);

  // Free up a process slot.
  busy_processes.erase(busy_processes.begin());

  // Wait for all worklet requests.
  mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1,
                                                 /*num_sellers=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);

  // Wait for the timeout to pass again. This time, it should result in the
  // bidder timing out.
  task_environment()->FastForwardBy(kBidder1CumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);
  task_environment()->FastForwardBy(kTinyTime);
  EXPECT_TRUE(auction_complete_);

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "https://adplatform.com/offers.js perBuyerCumulativeTimeout "
                  "exceeded during bid generation."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);
}

// Test the case where the only bidder times out due to the
// perBuyerCumulativeTimeout's "*" field.
TEST_F(AuctionRunnerTest, PerBuyerCumulativeTimeoutsAllBuyersTimeout) {
  interest_group_buyers_ = {{kBidder2}};
  StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  task_environment()->FastForwardBy(kAllBuyersCumulativeTimeout - kTinyTime);
  EXPECT_FALSE(auction_complete_);
  task_environment()->FastForwardBy(kTinyTime);
  EXPECT_TRUE(auction_complete_);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "https://anotheradthing.com/bids.js "
                  "perBuyerCumulativeTimeout exceeded during bid generation."));
  EXPECT_EQ(absl::nullopt, result_.winning_group_id);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsAbortedByBuyerCumulativeTimeout(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   // We explicitly include timeouts in these metrics.
                   .SetHasBidForOneInterestGroupLatencyMetrics(true));
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
  EXPECT_EQ(result_.ad_descriptor, absl::nullopt);

  // No interest groups participated in the auction.
  CheckMetrics(MetricsExpectations(AuctionResult::kNoInterestGroups)
                   .SetNumInterestGroups(0)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(0)
                   .SetNumBidderWorklets(0)
                   .SetNumBidsFilteredDuringInterestGroupLoad(1));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // No interest groups participated in the auction.
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
    if (use_empty_priority_signals) {
      bidders.back().interest_group.priority_vector = {};
    }

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
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
    // No request should have been made for the other URL.
    EXPECT_FALSE(url_loader_factory_.IsPending(kBidder1OtherUrl.spec()));

    // The second interest group is not counted as having participated in the
    // auction.
    CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                     .SetNumInterestGroups(1)
                     .SetNumOwnersAndDistinctOwners(1)
                     .SetNumSellers(1)
                     .SetNumBidderWorklets(1)
                     .SetNumBidsFilteredByPerBuyerLimits(1)
                     .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  // No request should have been made for the other URL.
  EXPECT_FALSE(url_loader_factory_.IsPending(kBidder1OtherUrl.spec()));

  // The second interest group is not counted as having participated in the
  // auction.
  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_FALSE(result_.ad_descriptor);

  // The interest group is considered to have participated in the auction.
  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredDuringReprioritization(1)
                   .SetNumInterestGroupsWithNoBids(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(1)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_FALSE(result_.ad_descriptor);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredDuringReprioritization(2)
                   .SetNumInterestGroupsWithNoBids(2));
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(2, url_loader_factory_.NumPending());

  // Group 2 has a negative priority.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl2,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"2", {{{"browserSignals.one", -2}}}}}));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl1,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsFilteredDuringReprioritization(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  ASSERT_EQ(2, url_loader_factory_.NumPending());

  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl1,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}}}));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  // Group 2 has a negative priority.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl2,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"2", {{{"browserSignals.one", -2}}}}}));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_EQ(InterestGroupKey(kBidder1, "1"), result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(2)
                   .SetNumBidsFilteredDuringReprioritization(1)
                   .SetNumInterestGroupsWithNoBids(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);

  CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(auction_complete_);
  // Seller script, bidder script, signals URL should all be pending.
  EXPECT_EQ(3, url_loader_factory_.NumPending());

  // Bidding signals received. Auction should still be pending.
  auction_worklet::AddBidderJsonResponse(
      &url_loader_factory_, kFullTrustedSignalsUrl,
      MakeBiddingSignalsWithPerInterestGroupData(
          {{"1", {{{"browserSignals.one", 1}}}},
           {"2", {{{"browserSignals.one", 2}}}}}));
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(absl::nullopt, result_.ad_descriptor);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsAbortedByBidderWorkletFatalError(1)
                   .SetNumBidsFilteredByPerBuyerLimits(1)
                   .SetNumInterestGroupsWithNoBids(1));
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
  task_environment()->RunUntilIdle();
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
  EXPECT_EQ(absl::nullopt, result_.ad_descriptor);

  CheckMetrics(MetricsExpectations(AuctionResult::kNoBids)
                   .SetNumInterestGroups(2)
                   .SetNumOwnersAndDistinctOwners(1)
                   .SetNumSellers(1)
                   .SetNumBidderWorklets(1)
                   .SetNumBidsAbortedByBidderWorkletFatalError(2)
                   .SetNumInterestGroupsWithNoBids(2));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

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
  EXPECT_FALSE(result_.ad_descriptor);

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
  RunAuctionAndWait(kSellerUrl, std::move(bidders));
  EXPECT_EQ(kBidder1Name, result_.winning_group_id->name);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  abortable_ad_auction_->Abort();
  task_environment()->RunUntilIdle();
  auction_runner_.reset();
}

// Test the critical path latency computation for GenerateBid dependencies.
TEST_F(
    AuctionRunnerTest,
    CriticalPathIsComputedFromDependencyLatenciesPassedToGenerateBidCallback) {
  using absl::nullopt;
  using auction_worklet::mojom::GenerateBidDependencyLatencies;
  using auction_worklet::mojom::GenerateBidDependencyLatenciesPtr;
  const struct TestCase {
    std::string description;
    GenerateBidDependencyLatencies dependency_latencies;
    // All remaining values are bucketed values.
    absl::optional<int64_t> code_ready_latency_in_millis;
    int64_t num_bidder_worklet_on_critical_path;
    absl::optional<int64_t> bidder_worklet_critical_path_latency_in_millis;
    absl::optional<int64_t> config_promises_latency_in_millis;
    int64_t num_config_promises_on_critical_path;
    absl::optional<int64_t> config_promises_critical_path_latency_in_millis;
    absl::optional<int64_t> direct_from_seller_signals_latency_in_millis;
    int64_t num_direct_from_seller_signals_on_critical_path;
    absl::optional<int64_t>
        direct_from_seller_signals_critical_path_latency_in_millis;
    absl::optional<int64_t> trusted_bidding_signals_latency_in_millis;
    int64_t num_trusted_bidding_signals_on_critical_path;
    absl::optional<int64_t>
        trusted_bidding_signals_critical_path_latency_in_millis;
  } kTestCases[] = {
      {/*description=*/"bidder_worklet on critical path",
       /*dependency_latencies=*/
       GenerateBidDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(550),
            /*config_promises_latency=*/nullopt,
            /*direct_from_seller_signals_latency=*/base::Milliseconds(400),
            /*trusted_bidding_signals_latency=*/nullopt}),
       /*code_ready_latency_in_millis=*/500,
       /*num_bidder_worklet_on_critical_path=*/1,
       /*bidder_worklet_critical_path_latency_in_millis=*/100,
       /*config_promises_latency_in_millis=*/nullopt,
       /*num_config_promises_on_critical_path=*/0,
       /*config_promises_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/400,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_bidding_signals_latency_in_millis=*/nullopt,
       /*num_trusted_bidding_signals_on_critical_path=*/0,
       /*trusted_bidding_signals_critical_path_latency_in_millis=*/nullopt},
      {/*description=*/"config_promises on critical path",
       /*dependency_latencies=*/
       GenerateBidDependencyLatencies(
           {/*code_ready_latency=*/nullopt,
            /*config_promises_latency=*/base::Milliseconds(550),
            /*direct_from_seller_signals_latency=*/nullopt,
            /*trusted_bidding_signals_latency=*/base::Milliseconds(300)}),
       /*code_ready_latency_in_millis=*/nullopt,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*config_promises_latency_in_millis=*/500,
       /*num_config_promises_on_critical_path=*/1,
       /*config_promises_critical_path_latency_in_millis=*/200,
       /*direct_from_seller_signals_latency_in_millis=*/nullopt,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_bidding_signals_latency_in_millis=*/300,
       /*num_trusted_bidding_signals_on_critical_path=*/0,
       /*trusted_bidding_signals_critical_path_latency_in_millis=*/nullopt},
      {/*description=*/"direct_from_seller_signals on critical path",
       /*dependency_latencies=*/
       GenerateBidDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(50),
            /*config_promises_latency=*/base::Milliseconds(200),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(550),
            /*trusted_bidding_signals_latency=*/nullopt}),
       /*code_ready_latency_in_millis=*/50,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*config_promises_latency_in_millis=*/200,
       /*num_config_promises_on_critical_path=*/0,
       /*config_promises_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/500,
       /*num_direct_from_seller_signals_on_critical_path=*/1,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/300,
       /*trusted_bidding_signals_latency_in_millis=*/nullopt,
       /*num_trusted_bidding_signals_on_critical_path=*/0,
       /*trusted_bidding_signals_critical_path_latency_in_millis=*/nullopt},
      {/*description=*/"trusted_bidding_signals on critical path",
       /*dependency_latencies=*/
       GenerateBidDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(100),
            /*config_promises_latency=*/base::Milliseconds(51),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(100),
            /*trusted_bidding_signals_latency=*/base::Milliseconds(550)}),
       /*code_ready_latency_in_millis=*/100,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*config_promises_latency_in_millis=*/50,
       /*num_config_promises_on_critical_path=*/0,
       /*config_promises_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/100,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_bidding_signals_latency_in_millis=*/500,
       /*num_trusted_bidding_signals_on_critical_path=*/1,
       /*trusted_bidding_signals_critical_path_latency_in_millis=*/400},
      {/*description=*/"a tie: last-listed dependency  wins, 0 critical path",
       /*dependency_latencies=*/
       GenerateBidDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(100),
            /*config_promises_latency=*/base::Milliseconds(550),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(100),
            /*trusted_bidding_signals_latency=*/base::Milliseconds(550)}),
       /*code_ready_latency_in_millis=*/100,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*config_promises_latency_in_millis=*/500,
       /*num_config_promises_on_critical_path=*/0,
       /*config_promises_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/100,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_bidding_signals_latency_in_millis=*/500,
       /*num_trusted_bidding_signals_on_critical_path=*/1,
       /*trusted_bidding_signals_critical_path_latency_in_millis=*/0},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    interest_group_buyers_ = {{kBidder1}};
    StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);

    // Bid generation completes.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/2, /*bid_currency=*/absl::nullopt,
        /*ad_descriptor=*/blink::AdDescriptor(GURL("https://ad1.com/")),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_descriptors=*/absl::nullopt,
        /*duration=*/base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*pa_requests=*/{},
        /*dependency_latencies=*/
        GenerateBidDependencyLatencies::New(test_case.dependency_latencies));

    // Score the ad.
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/10,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
            /*errors=*/{});

    // Finish the auction.
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder1_worklet->WaitForReportWin();
    bidder1_worklet->InvokeReportWinCallback();
    auction_run_loop_->Run();

    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    ukm::TestUkmRecorder::HumanReadableUkmMetrics ukm_metrics = GetUkmMetrics();

    using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName,
                    test_case.code_ready_latency_in_millis));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName,
                    test_case.code_ready_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(UkmEntry::kNumGenerateBidCodeReadyOnCriticalPathName,
                           test_case.num_bidder_worklet_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName,
            test_case.bidder_worklet_critical_path_latency_in_millis));

    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName,
                    test_case.config_promises_latency_in_millis));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName,
                    test_case.config_promises_latency_in_millis));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValue(
                    UkmEntry::kNumGenerateBidConfigPromisesOnCriticalPathName,
                    test_case.num_config_promises_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName,
            test_case.config_promises_critical_path_latency_in_millis));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName,
            test_case.direct_from_seller_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName,
            test_case.direct_from_seller_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName,
            test_case.num_direct_from_seller_signals_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName,
            test_case
                .direct_from_seller_signals_critical_path_latency_in_millis));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName,
            test_case.trusted_bidding_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName,
            test_case.trusted_bidding_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName,
            test_case.num_trusted_bidding_signals_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName,
            test_case.trusted_bidding_signals_critical_path_latency_in_millis));
  }
}

// Test the critical path latency computation for ScoreAd dependencies.
TEST_F(AuctionRunnerTest,
       CriticalPathIsComputedFromDependencyLatenciesPassedToScoreAdCallback) {
  using absl::nullopt;
  using auction_worklet::mojom::ScoreAdDependencyLatencies;
  using auction_worklet::mojom::ScoreAdDependencyLatenciesPtr;
  const struct TestCase {
    std::string description;
    ScoreAdDependencyLatencies dependency_latencies;
    // All remaining values are bucketed values.
    absl::optional<int64_t> code_ready_latency_in_millis;
    int64_t num_bidder_worklet_on_critical_path;
    absl::optional<int64_t> bidder_worklet_critical_path_latency_in_millis;
    absl::optional<int64_t> direct_from_seller_signals_latency_in_millis;
    int64_t num_direct_from_seller_signals_on_critical_path;
    absl::optional<int64_t>
        direct_from_seller_signals_critical_path_latency_in_millis;
    absl::optional<int64_t> trusted_scoring_signals_latency_in_millis;
    int64_t num_trusted_scoring_signals_on_critical_path;
    absl::optional<int64_t>
        trusted_scoring_signals_critical_path_latency_in_millis;
  } kTestCases[] = {
      {/*description=*/"bidder_worklet on critical path",
       /*dependency_latencies=*/
       ScoreAdDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(550),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(400),
            /*trusted_scoring_signals_latency=*/nullopt}),
       /*code_ready_latency_in_millis=*/500,
       /*num_bidder_worklet_on_critical_path=*/1,
       /*bidder_worklet_critical_path_latency_in_millis=*/100,
       /*direct_from_seller_signals_latency_in_millis=*/400,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_scoring_signals_latency_in_millis=*/nullopt,
       /*num_trusted_scoring_signals_on_critical_path=*/0,
       /*trusted_scoring_signals_critical_path_latency_in_millis=*/nullopt},
      {/*description=*/"direct_from_seller_signals on critical path",
       /*dependency_latencies=*/
       ScoreAdDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(200),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(550),
            /*trusted_scoring_signals_latency=*/nullopt}),
       /*code_ready_latency_in_millis=*/200,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/500,
       /*num_direct_from_seller_signals_on_critical_path=*/1,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/300,
       /*trusted_scoring_signals_latency_in_millis=*/nullopt,
       /*num_trusted_scoring_signals_on_critical_path=*/0,
       /*trusted_scoring_signals_critical_path_latency_in_millis=*/nullopt},
      {/*description=*/"trusted_scoring_signals on critical path",
       /*dependency_latencies=*/
       ScoreAdDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(100),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(100),
            /*trusted_scoring_signals_latency=*/base::Milliseconds(550)}),
       /*code_ready_latency_in_millis=*/100,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/100,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_scoring_signals_latency_in_millis=*/500,
       /*num_trusted_scoring_signals_on_critical_path=*/1,
       /*trusted_scoring_signals_critical_path_latency_in_millis=*/400},
      {/*description=*/"a tie: last-listed dependency  wins, 0 critical path",
       /*dependency_latencies=*/
       ScoreAdDependencyLatencies(
           {/*code_ready_latency=*/base::Milliseconds(550),
            /*direct_from_seller_signals_latency=*/base::Milliseconds(100),
            /*trusted_scoring_signals_latency=*/base::Milliseconds(550)}),
       /*code_ready_latency_in_millis=*/500,
       /*num_bidder_worklet_on_critical_path=*/0,
       /*bidder_worklet_critical_path_latency_in_millis=*/nullopt,
       /*direct_from_seller_signals_latency_in_millis=*/100,
       /*num_direct_from_seller_signals_on_critical_path=*/0,
       /*direct_from_seller_signals_critical_path_latency_in_millis=*/nullopt,
       /*trusted_scoring_signals_latency_in_millis=*/500,
       /*num_trusted_scoring_signals_on_critical_path=*/1,
       /*trusted_scoring_signals_critical_path_latency_in_millis=*/0},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    interest_group_buyers_ = {{kBidder1}};
    StartStandardAuctionWithMockService(/*num_bidder_worklets=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);

    // Bid generation completes.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/2, /*bid_currency=*/absl::nullopt,
        /*ad_descriptor=*/blink::AdDescriptor(GURL("https://ad1.com/")),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_descriptors=*/absl::nullopt,
        /*duration=*/base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*pa_requests=*/{},
        /*dependency_latencies=*/
        auction_worklet::mojom::GenerateBidDependencyLatencies::New(
            /*code_ready_latency=*/absl::nullopt,
            /*config_promises_latency=*/absl::nullopt,
            /*direct_from_seller_signals_latency=*/absl::nullopt,
            /*trusted_bidding_signals_latency=*/absl::nullopt));

    // Score the ad.
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/10,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*score_ad_latency=*/base::Milliseconds(0),
            /*score_ad_dependency_latencies=*/
            ScoreAdDependencyLatencies::New(test_case.dependency_latencies),
            /*errors=*/{});

    // Finish the auction.
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder1_worklet->WaitForReportWin();
    bidder1_worklet->InvokeReportWinCallback();
    auction_run_loop_->Run();

    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    ukm::TestUkmRecorder::HumanReadableUkmMetrics ukm_metrics = GetUkmMetrics();

    using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName,
                    test_case.code_ready_latency_in_millis));
    EXPECT_THAT(ukm_metrics,
                HasMetricWithValueOrNotIfNullOpt(
                    UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName,
                    test_case.code_ready_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(UkmEntry::kNumScoreAdCodeReadyOnCriticalPathName,
                           test_case.num_bidder_worklet_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName,
            test_case.bidder_worklet_critical_path_latency_in_millis));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName,
            test_case.direct_from_seller_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName,
            test_case.direct_from_seller_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName,
            test_case.num_direct_from_seller_signals_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName,
            test_case
                .direct_from_seller_signals_critical_path_latency_in_millis));

    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName,
            test_case.trusted_scoring_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName,
            test_case.trusted_scoring_signals_latency_in_millis));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValue(
            UkmEntry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName,
            test_case.num_trusted_scoring_signals_on_critical_path));
    EXPECT_THAT(
        ukm_metrics,
        HasMetricWithValueOrNotIfNullOpt(
            UkmEntry::
                kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName,
            test_case.trusted_scoring_signals_critical_path_latency_in_millis));
  }
}

// An auction with two successful bids. contributeToHistogram() and
// contributeToHistogramOnEvent() are both called in all of generateBid(),
// scoreAd(), reportWin() and reportResult().
TEST_F(AuctionRunnerTest, PrivateAggregationRequestForEventContributionEvents) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always', {bucket: 10n, value: 20});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win', {bucket: 11n, value: 21});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss', {bucket: 12n, value: 22});
      privateAggregation.contributeToHistogramOnEvent(
          'arbitrary', {bucket: 100n, value: 200});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 101n, value: 201});
      return {bid: bid, render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 3n, value: 4});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always', {bucket: 30n, value: 40});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win', {bucket: 31n, value: 41});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss', {bucket: 32n, value: 42});
      privateAggregation.contributeToHistogramOnEvent(
          'arbitrary', {bucket: 300n, value: 400});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 301n, value: 401});
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 5n, value: 6});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always', {bucket: 50n, value: 60});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win', {bucket: 51n, value: 61});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss', {bucket: 52n, value: 62});
      privateAggregation.contributeToHistogramOnEvent(
          'arbitrary', {bucket: 500n, value: 600});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 501n, value: 601});
      return {desirability: 2 * bid, allowComponentAuction: true};
    }

    function reportResult(auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogram({bucket: 7n, value: 8});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.always', {bucket: 70n, value: 80});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.win', {bucket: 71n, value: 81});
      privateAggregation.contributeToHistogramOnEvent(
          'reserved.loss', {bucket: 72n, value: 82});
      privateAggregation.contributeToHistogramOnEvent(
          'arbitrary', {bucket: 700n, value: 800});
      privateAggregation.contributeToHistogramOnEvent(
          'click', {bucket: 701n, value: 801});
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Bidder 2 won the auction.
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  kExpectedGenerateBidPrivateAggregationRequest,
                  // generateBid() "reserved.always".
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  // generateBid() "reserved.loss".
                  BuildPrivateAggregationRequest(/*bucket=*/12, /*value=*/22))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(
                  kExpectedGenerateBidPrivateAggregationRequest,
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  // generateBid() "reserved.win".
                  BuildPrivateAggregationRequest(/*bucket=*/11, /*value=*/21),
                  kExpectedReportWinPrivateAggregationRequest,
                  // reportWin() "reserved.always".
                  BuildPrivateAggregationRequest(/*bucket=*/30, /*value=*/40),
                  // reportWin() "reserved.win".
                  BuildPrivateAggregationRequest(/*bucket=*/31, /*value=*/41))),
          testing::Pair(
              kSeller,
              ElementsAreRequests(
                  kExpectedScoreAdPrivateAggregationRequest,
                  // scoreAd() "reserved.always".
                  BuildPrivateAggregationRequest(/*bucket=*/50, /*value=*/60),
                  // scoreAd() "reserved.loss".
                  BuildPrivateAggregationRequest(/*bucket=*/52, /*value=*/62),
                  kExpectedScoreAdPrivateAggregationRequest,
                  BuildPrivateAggregationRequest(/*bucket=*/50, /*value=*/60),
                  // scoreAd() "reserved.win".
                  BuildPrivateAggregationRequest(/*bucket=*/51, /*value=*/61),
                  kExpectedReportResultPrivateAggregationRequest,
                  // reportResult() "reserved.always".
                  BuildPrivateAggregationRequest(/*bucket=*/70, /*value=*/80),
                  // reportResult() "reserved.win".
                  BuildPrivateAggregationRequest(/*bucket=*/71,
                                                 /*value=*/81)))));
  EXPECT_THAT(
      result_.private_aggregation_event_map,
      testing::UnorderedElementsAre(
          testing::Pair("arbitrary", ElementsAreRequests(
                                         BuildPrivateAggregationRequest(
                                             /*bucket=*/100, /*value=*/200),
                                         BuildPrivateAggregationRequest(
                                             /*bucket=*/300, /*value=*/400))),
          testing::Pair("click", ElementsAreRequests(
                                     BuildPrivateAggregationRequest(
                                         /*bucket=*/101, /*value=*/201),
                                     BuildPrivateAggregationRequest(
                                         /*bucket=*/301, /*value=*/401)))));
}

// Base values in contribution's bucket.
TEST_F(AuctionRunnerTest,
       PrivateAggregationRequestForEventContributionBucketBaseValue) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 1.0, offset: 0n},
        value: 1 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid', scale: 1.0},
        value: 2 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason', offset: 0n},
        value: 3 + 100 * bid,
      });
      return {bid: bid, render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 11 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid', scale: 1.0,
                 offset: 0n},
        value: 12 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 13 + 100 * browserSignals.bid,
      });
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      let convertedBid;
      if (auctionConfig.sellerCurrency) {
        convertedBid = 10*bid;
      }
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: 21 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 22 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 23 + 100 * bid,
      });
      if (bid === 2) return {
        desirability: -1,
        incomingBidInSellerCurrency: convertedBid,
        rejectReason: 'invalid-bid'
      };
      return {desirability: bid, incomingBidInSellerCurrency: convertedBid};
    }

    function reportResult(auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 31 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 32 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 33 + 100 * browserSignals.bid,
      });
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);
  for (bool use_seller_currency : {false, true}) {
    SCOPED_TRACE(use_seller_currency);
    if (use_seller_currency) {
      seller_currency_ = blink::AdCurrency::From("CAD");
    }

    // kBidder2 was rejected by seller, so kBidder1 won the auction.
    RunStandardAuction(/*request_trusted_bidding_signals=*/false);
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_FALSE(result_.manually_aborted);
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    // Post auction signals of this auction:
    // If sellerCurrency is off:
    //   winning-bid is 1, highest-scoring-other-bid is 0, bid-reject-reason for
    //   kBidder1 is kNotAvailable (0), and bid-reject-reason for kBidder2 is
    //   kInvalidBid (1).
    // If sellerCurrency is on:
    //   winning-bid is 10, everything else is the same.
    int winning_bid = use_seller_currency ? 10 : 1;

    EXPECT_THAT(
        private_aggregation_manager_.TakePrivateAggregationRequests(),
        testing::UnorderedElementsAre(
            testing::Pair(
                kBidder1,
                ElementsAreRequests(
                    // generateBid().
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/101),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/102),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/103),
                    // reportWin(). No request for 'bid-reject-reason' whose
                    // value is 113, because it's not a supported base value in
                    // reportWin().
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/111),
                    BuildPrivateAggregationRequest(/*bucket=*/0,
                                                   /*value=*/112))),
            testing::Pair(
                kBidder2,
                ElementsAreRequests(
                    // generateBid().
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/201),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/202),
                    BuildPrivateAggregationRequest(/*bucket=*/1,
                                                   /*value=*/203))),
            testing::Pair(
                kSeller,
                ElementsAreRequests(
                    // scoreAd() for kBidder1.
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/121),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/122),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/123),
                    // scoreAd() for kBidder2.
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/221),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/222),
                    BuildPrivateAggregationRequest(/*bucket=*/1, /*value=*/223),
                    // reportResult() for kBidder1. No request for
                    // 'bid-reject-reason' whose value is 133, because it's not
                    // a supported base value in reportResult().
                    BuildPrivateAggregationRequest(
                        /*bucket=*/winning_bid,
                        /*value=*/131),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/0,
                        /*value=*/132)))));
    EXPECT_TRUE(result_.private_aggregation_event_map.empty());
  }
}

// Similar to `PrivateAggregationRequestForEventContributionBucketBaseValue()`
// above, but no bid is rejected.
TEST_F(AuctionRunnerTest,
       PrivateAggregationRequestForEventContributionTwoBidsNotRejected) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 1.0, offset: 0n},
        value: 1 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid', scale: 1.0},
        value: 2 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason', offset: 0n},
        value: 3 + 100 * bid,
      });
      return {bid: bid, render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 11 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid', scale: 1.0,
                 offset: 0n},
        value: 12 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 13 + 100 * browserSignals.bid,
      });
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      let convertedBid;
      if (auctionConfig.sellerCurrency) {
        convertedBid = 10*bid;
      }
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid'},
        value: 21 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 22 + 100 * bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 23 + 100 * bid,
      });
      return {desirability: bid, incomingBidInSellerCurrency: convertedBid};
    }

    function reportResult(auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'winning-bid'},
        value: 31 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: {baseValue: 'highest-scoring-other-bid'},
        value: 32 + 100 * browserSignals.bid,
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'bid-reject-reason'},
        value: 33 + 100 * browserSignals.bid,
      });
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  for (bool use_seller_currency : {false, true}) {
    SCOPED_TRACE(use_seller_currency);
    if (use_seller_currency) {
      seller_currency_ = blink::AdCurrency::From("CAD");
    }

    // kBidder2 won the auction.
    RunStandardAuction(/*request_trusted_bidding_signals=*/false);
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
    EXPECT_FALSE(result_.manually_aborted);
    EXPECT_EQ(kBidder2Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

    // Post auction signals of this auction:
    // If sellerCurrency is off:
    //   winning-bid is 2, highest-scoring-other-bid is 1, bid-reject-reason for
    //   both bidders are kNotAvailable (0).
    // If sellerCurrency is on:
    //   winning-bid is 20, highest_scoring_other_bid is 10, everything else is
    // same.
    int winning_bid = use_seller_currency ? 20 : 2;
    int highest_scoring_other_bid = use_seller_currency ? 10 : 1;

    EXPECT_THAT(
        private_aggregation_manager_.TakePrivateAggregationRequests(),
        testing::UnorderedElementsAre(
            testing::Pair(
                kBidder1,
                ElementsAreRequests(
                    // generateBid().
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/101),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid, /*value=*/102),
                    BuildPrivateAggregationRequest(/*bucket=*/0,
                                                   /*value=*/103))),
            testing::Pair(
                kBidder2,
                ElementsAreRequests(
                    // generateBid().
                    BuildPrivateAggregationRequest(
                        /*bucket=*/winning_bid, /*value=*/201),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid, /*value=*/202),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/0, /*value=*/203),
                    // reportWin(). No request for 'bid-reject-reason' whose
                    // value is 213, because it's not a supported base value in
                    // reportWin().
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/211),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid, /*value=*/212))),
            testing::Pair(
                kSeller,
                ElementsAreRequests(
                    // scoreAd() for kBidder1.
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/121),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid, /*value=*/122),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/123),
                    // scoreAd() for kBidder2.
                    BuildPrivateAggregationRequest(/*bucket=*/winning_bid,
                                                   /*value=*/221),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid, /*value=*/222),
                    BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/223),
                    // reportResult() for kBidder2. No request for
                    // 'bid-reject-reason' whose value is 233, because it's not
                    // a supported base value in reportResult().
                    BuildPrivateAggregationRequest(
                        /*bucket=*/winning_bid,
                        /*value=*/231),
                    BuildPrivateAggregationRequest(
                        /*bucket=*/highest_scoring_other_bid,
                        /*value=*/232)))));
  }
}

// Similar to PrivateAggregationRequestForEventContributionBucketBaseValue,
// but with contribution's value field as object.
TEST_F(AuctionRunnerTest,
       PrivateAggregationRequestForEventContributionValueBaseValue) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(1 + 100 * bid),
        value: {baseValue: 'winning-bid', scale: 1.0, offset: 0},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(2 + 100 * bid),
        value: {baseValue: 'highest-scoring-other-bid', scale: 1.0},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(3 + 100 * bid),
        value: {baseValue: 'bid-reject-reason', offset: 0},
      });
      return {bid: bid, render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: BigInt(11 + 100 * browserSignals.bid),
        value: {baseValue: 'winning-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: BigInt(12 + 100 * browserSignals.bid),
        value: {baseValue: 'highest-scoring-other-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(13 + 100 * browserSignals.bid),
        value: {baseValue: 'bid-reject-reason'},
      });
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(21 + 100 * bid),
        value: {baseValue: 'winning-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(22 + 100 * bid),
        value: {baseValue: 'highest-scoring-other-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(23 + 100 * bid),
        value: {baseValue: 'bid-reject-reason'},
      });
      if (bid === 2) return {desirability: -1, rejectReason: 'invalid-bid'};
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: BigInt(31 + 100 * browserSignals.bid),
        value: {baseValue: 'winning-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.win', {
        bucket: BigInt(32 + 100 * browserSignals.bid),
        value: {baseValue: 'highest-scoring-other-bid'},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: BigInt(33 + 100 * browserSignals.bid),
        value: {baseValue: 'bid-reject-reason'},
      });
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 2));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // kBidder2 was rejected by seller, so kBidder1 won the auction.
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // Post auction signals of this auction:
  // winning-bid is 1, highest-scoring-other-bid is 0, bid-reject-reason for
  // kBidder1 is kNotAvailable (0), and bid-reject-reason for kBidder2 is
  // kInvalidBid (1).
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  // generateBid().
                  BuildPrivateAggregationRequest(/*bucket=*/101, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/102, /*value=*/0),
                  BuildPrivateAggregationRequest(/*bucket=*/103, /*value=*/0),
                  // reportWin().
                  BuildPrivateAggregationRequest(/*bucket=*/111, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/112, /*value=*/0))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(
                  // generateBid().
                  BuildPrivateAggregationRequest(/*bucket=*/201, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/202, /*value=*/0),
                  BuildPrivateAggregationRequest(/*bucket=*/203, /*value=*/1))),
          testing::Pair(
              kSeller,
              ElementsAreRequests(
                  // scoreAd() for kBidder1.
                  BuildPrivateAggregationRequest(/*bucket=*/121, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/122, /*value=*/0),
                  BuildPrivateAggregationRequest(/*bucket=*/123, /*value=*/0),
                  // scoreAd() for kBidder2.
                  BuildPrivateAggregationRequest(/*bucket=*/221, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/222, /*value=*/0),
                  BuildPrivateAggregationRequest(/*bucket=*/223, /*value=*/1),
                  // reportResult() for kBidder1.
                  BuildPrivateAggregationRequest(/*bucket=*/131, /*value=*/1),
                  BuildPrivateAggregationRequest(/*bucket=*/132,
                                                 /*value=*/0)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationRequestForEventContributionScaleAndOffset) {
  // Only one bidder participating the auction, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};

  const char kBidScript[] = R"(
    const bid = %d;
    function contributeToHistogramOnEvent() {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 2.1, offset: 10n},
        value: {baseValue: 'winning-bid', scale: 2.1, offset: 20},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 0, offset: 10n},
        value: {baseValue: 'winning-bid', scale: 0, offset: 20},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: -1, offset: 10n},
        value: {baseValue: 'winning-bid', scale: -1, offset: 20},
      });
      // Bucket overflows due to being negative, so will be clamped to 0.
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: -200, offset: 10n},
        value: 1,
      });
    }

    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      contributeToHistogramOnEvent();
      return {bid: bid, render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      contributeToHistogramOnEvent();
    }
  )";

  const std::string kSellerScript = R"(
    function contributeToHistogramOnEvent() {
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 2.1, offset: 10n},
        value: {baseValue: 'winning-bid', scale: 2.1, offset: 20},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: 0, offset: 10n},
        value: {baseValue: 'winning-bid', scale: 0, offset: 20},
      });
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: -1, offset: 10n},
        value: {baseValue: 'winning-bid', scale: -1, offset: 20},
      });
      // Bucket overflows due to being negative, so will be clamped to 0.
      privateAggregation.contributeToHistogramOnEvent('reserved.always', {
        bucket: {baseValue: 'winning-bid', scale: -200, offset: 10n},
        value: 1,
      });
    }

    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      contributeToHistogramOnEvent();
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
      contributeToHistogramOnEvent();
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // winning-bid is 1.
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  // generateBid().
                  BuildPrivateAggregationRequest(/*bucket=*/12, /*value=*/22),
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  BuildPrivateAggregationRequest(/*bucket=*/9, /*value=*/19),
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/1),
                  // reportWin().
                  BuildPrivateAggregationRequest(/*bucket=*/12, /*value=*/22),
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  BuildPrivateAggregationRequest(/*bucket=*/9, /*value=*/19),
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/1))),

          testing::Pair(
              kSeller,
              ElementsAreRequests(
                  // scoreAd().
                  BuildPrivateAggregationRequest(/*bucket=*/12, /*value=*/22),
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  BuildPrivateAggregationRequest(/*bucket=*/9, /*value=*/19),
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/1),
                  // reportResult().
                  BuildPrivateAggregationRequest(/*bucket=*/12, /*value=*/22),
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  BuildPrivateAggregationRequest(/*bucket=*/9, /*value=*/19),
                  BuildPrivateAggregationRequest(/*bucket=*/0, /*value=*/1)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationReportGenerateBidInvalidReservedEventType) {
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
  PrivateAggregationRequests bidder_1_pa_requests;
  bidder_1_pa_requests.push_back(
      BuildPrivateAggregationForEventRequest(
          /*bucket=*/10, /*value=*/20, /*event_type=*/"reserved.always")
          .Clone());
  bidder_1_pa_requests.push_back(
      BuildPrivateAggregationForEventRequest(
          /*bucket=*/11, /*value=*/21, /*event_type=*/"reserved.not-supported")
          .Clone());

  // Bidder1 returns a bid with a private aggregation request whose reserved
  // event type is not a supported one. This could only happen when the bidder
  // worklet is compromised.
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/5, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad1.com")),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_descriptors=*/absl::nullopt,
      /*duration=*/base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt,
      /*debug_loss_report_url=*/absl::nullopt,
      /*debug_win_report_url=*/absl::nullopt,
      /*pa_requests=*/
      std::move(bidder_1_pa_requests));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  PrivateAggregationRequests score_ad_1_pa_requests;
  score_ad_1_pa_requests.push_back(
      kExpectedScoreAdPrivateAggregationRequest.Clone());
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/10,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          std::move(score_ad_1_pa_requests),
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
          /*errors=*/{});

  PrivateAggregationRequests report_win_pa_requests;
  report_win_pa_requests.push_back(
      kExpectedReportWinPrivateAggregationRequest.Clone());
  PrivateAggregationRequests report_result_pa_requests;
  report_result_pa_requests.push_back(
      kExpectedReportResultPrivateAggregationRequest.Clone());

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, std::move(report_result_pa_requests));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      /*pa_requests=*/std::move(report_win_pa_requests));
  auction_run_loop_->Run();

  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
            result_.winning_group_ad_metadata);

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  BuildPrivateAggregationRequest(/*bucket=*/10, /*value=*/20),
                  kExpectedReportWinPrivateAggregationRequest)),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            kExpectedScoreAdPrivateAggregationRequest,
                            kExpectedReportResultPrivateAggregationRequest))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationTimeMetrics) {
  const int kNumBidders = 2;
  UseMockWorkletService();
  StartStandardAuction();
  mock_auction_process_manager_->WaitForWorklets(kNumBidders,
                                                 /*num_sellers=*/1);

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);

  std::unique_ptr<MockBidderWorklet> bidder_worklets[2];
  bidder_worklets[0] =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder_worklets[1] =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  for (int i = 0; i < kNumBidders; ++i) {
    ASSERT_TRUE(bidder_worklets[i]);
    bidder_worklets[i]->SetBidderTrustedSignalsFetchLatency(
        base::Milliseconds(100 * i + 1));
    bidder_worklets[i]->SetBiddingLatency(base::Milliseconds(100 * i + 10));

    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        bidder_pa_requests, seller_pa_requests;
    bidder_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i, auction_worklet::mojom::BaseValue::kScriptRunTime,
        "reserved.always"));
    seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 10,
        auction_worklet::mojom::BaseValue::kScriptRunTime, "reserved.always"));
    bidder_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 1,
        auction_worklet::mojom::BaseValue::kSignalsFetchTime,
        "reserved.always"));
    seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 11,
        auction_worklet::mojom::BaseValue::kSignalsFetchTime,
        "reserved.always"));
    bidder_worklets[i]->InvokeGenerateBidCallback(
        i + 1, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(
            GURL(base::StringPrintf("https://ad%d.com/", i + 1))),
        auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
        /*ad_component_descriptors=*/absl::nullopt,
        // Note that duration here is the duration of non-kanon-enforced run;
        // if there is a k-anon-enforced one as well it will have a duration
        // inside the BidderWorkletKAnonEnforcedBid. The overall bidding_latency
        // passed to OnGenerateBidComplete (here configured via
        // SetBiddingLatency()) encompasses all the runs combined.
        // This test just passes a silly value to make sure we use the right
        // (combined) value.
        /*duration=*/base::Seconds(5),
        /*bidding_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt, std::move(bidder_pa_requests));
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/i + 1,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt,
            /*pa_requests=*/std::move(seller_pa_requests),
            /*scoring_latency=*/base::Milliseconds(100 * i + 20),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/
                base::Milliseconds(100 * i + 21)),
            /*errors=*/{});
  }

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      bidder_report_pa_requests, seller_report_pa_requests;
  bidder_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/50, auction_worklet::mojom::BaseValue::kScriptRunTime,
      "reserved.always"));
  seller_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/60, auction_worklet::mojom::BaseValue::kScriptRunTime,
      "reserved.always"));
  bidder_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/51, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
      "reserved.always"));
  seller_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/61, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
      "reserved.always"));

  // Need to flush the service pipe to make sure the AuctionRunner has
  // received the score.
  seller_worklet->Flush();
  seller_worklet->WaitForReportResult();
  seller_worklet->SetReportingLatency(base::Milliseconds(200));
  seller_worklet->InvokeReportResultCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, std::move(seller_report_pa_requests));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  auto winning_bidder_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(winning_bidder_worklet);
  winning_bidder_worklet->WaitForReportWin();
  winning_bidder_worklet->SetReportingLatency(base::Milliseconds(250));
  winning_bidder_worklet->InvokeReportWinCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      std::move(bidder_report_pa_requests));
  auction_run_loop_->Run();

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(BuildPrivateAggregationRequest(/*bucket=*/0,
                                                                 /*value=*/10),
                                  BuildPrivateAggregationRequest(/*bucket=*/1,
                                                                 /*value=*/1))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(BuildPrivateAggregationRequest(/*bucket=*/100,
                                                                 /*value=*/110),
                                  BuildPrivateAggregationRequest(/*bucket=*/101,
                                                                 /*value=*/101),
                                  BuildPrivateAggregationRequest(/*bucket=*/50,
                                                                 /*value=*/250),
                                  // No signals fetch for reporting, so
                                  // value = 0.
                                  BuildPrivateAggregationRequest(/*bucket=*/51,
                                                                 /*value=*/0))),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(/*bucket=*/10,
                                                           /*value=*/20),
                            BuildPrivateAggregationRequest(/*bucket=*/11,
                                                           /*value=*/21),
                            BuildPrivateAggregationRequest(/*bucket=*/110,
                                                           /*value=*/120),
                            BuildPrivateAggregationRequest(/*bucket=*/111,
                                                           /*value=*/121),
                            BuildPrivateAggregationRequest(/*bucket=*/60,
                                                           /*value=*/200),
                            // No signals fetch for reporting, so value = 0.
                            BuildPrivateAggregationRequest(/*bucket=*/61,
                                                           /*value=*/0)))));
}

TEST_F(AuctionRunnerTest, ComponentAuctionPrivateAggregationTimeMetrics) {
  const int kNumBidders = 2;
  UseMockWorkletService();

  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1,
                                    /*bid_from_component_auction_wins=*/true,
                                    /*report_post_auction_signals=*/false);

  StartStandardAuction();
  mock_auction_process_manager_->WaitForWorklets(kNumBidders,
                                                 /*num_sellers=*/2);

  auto component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  ASSERT_TRUE(component_seller_worklet);

  auto top_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
  ASSERT_TRUE(top_seller_worklet);

  std::unique_ptr<MockBidderWorklet> bidder_worklets[2];
  bidder_worklets[0] =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder_worklets[1] =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);

  for (int i = 0; i < kNumBidders; ++i) {
    ASSERT_TRUE(bidder_worklets[i]);
    bidder_worklets[i]->SetBidderTrustedSignalsFetchLatency(
        base::Milliseconds(100 * i + 1));
    bidder_worklets[i]->SetBiddingLatency(base::Milliseconds(100 * i + 10));

    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        bidder_pa_requests, component_seller_pa_requests;
    bidder_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i, auction_worklet::mojom::BaseValue::kScriptRunTime,
        "reserved.always"));
    component_seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 10,
        auction_worklet::mojom::BaseValue::kScriptRunTime, "reserved.always"));
    bidder_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 1,
        auction_worklet::mojom::BaseValue::kSignalsFetchTime,
        "reserved.always"));
    component_seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
        /*bucket=*/100 * i + 11,
        auction_worklet::mojom::BaseValue::kSignalsFetchTime,
        "reserved.always"));
    bidder_worklets[i]->InvokeGenerateBidCallback(
        i + 1, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(
            GURL(base::StringPrintf("https://ad%d.com/", i + 1))),
        auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
        /*ad_component_descriptors=*/absl::nullopt,
        /*duration=*/base::Seconds(5),
        /*bidding_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt, std::move(bidder_pa_requests));

    auto component_score_ad_params = component_seller_worklet->WaitForScoreAd();
    mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
        std::move(component_score_ad_params.score_ad_client))
        ->OnScoreAdComplete(
            /*score=*/i + 1,
            /*reject_reason=*/
            auction_worklet::mojom::RejectReason::kNotAvailable,
            auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(),
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt,
            /*pa_requests=*/std::move(component_seller_pa_requests),
            /*scoring_latency=*/base::Milliseconds(100 * i + 20),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/
                base::Milliseconds(100 * i + 21)),
            /*errors=*/{});
  }

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      top_seller_pa_requests;
  top_seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/20, auction_worklet::mojom::BaseValue::kScriptRunTime,
      "reserved.always"));
  top_seller_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/21, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
      "reserved.always"));

  auto top_score_ad_params = top_seller_worklet->WaitForScoreAd();
  mojo::Remote<auction_worklet::mojom::ScoreAdClient>(
      std::move(top_score_ad_params.score_ad_client))
      ->OnScoreAdComplete(
          /*score=*/1,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kNotAvailable,
          auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr(),
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          /*pa_requests=*/std::move(top_seller_pa_requests),
          /*scoring_latency=*/base::Milliseconds(30),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/base::Milliseconds(31)),
          /*errors=*/{});

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      bidder_report_pa_requests, component_seller_report_pa_requests,
      top_seller_report_pa_requests;
  bidder_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/50, auction_worklet::mojom::BaseValue::kScriptRunTime,
      "reserved.always"));
  component_seller_report_pa_requests.push_back(
      BuildPrivateAggregationForBaseValue(
          /*bucket=*/60, auction_worklet::mojom::BaseValue::kScriptRunTime,
          "reserved.always"));
  top_seller_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/70, auction_worklet::mojom::BaseValue::kScriptRunTime,
      "reserved.always"));
  bidder_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/51, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
      "reserved.always"));
  component_seller_report_pa_requests.push_back(
      BuildPrivateAggregationForBaseValue(
          /*bucket=*/61, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
          "reserved.always"));
  top_seller_report_pa_requests.push_back(BuildPrivateAggregationForBaseValue(
      /*bucket=*/71, auction_worklet::mojom::BaseValue::kSignalsFetchTime,
      "reserved.always"));

  top_seller_worklet->WaitForReportResult();
  top_seller_worklet->SetReportingLatency(base::Milliseconds(200));
  top_seller_worklet->InvokeReportResultCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, std::move(top_seller_report_pa_requests));

  mock_auction_process_manager_->WaitForWinningSellerReload();
  component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet->WaitForReportResult();
  component_seller_worklet->SetReportingLatency(base::Milliseconds(250));
  component_seller_worklet->InvokeReportResultCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, std::move(component_seller_report_pa_requests));

  mock_auction_process_manager_->WaitForWinningBidderReload();
  auto winning_bidder_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(winning_bidder_worklet);
  winning_bidder_worklet->WaitForReportWin();
  winning_bidder_worklet->SetReportingLatency(base::Milliseconds(300));
  winning_bidder_worklet->InvokeReportWinCallback(
      /*report_url=*/absl::nullopt,
      /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      std::move(bidder_report_pa_requests));
  auction_run_loop_->Run();

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kBidder1,
              ElementsAreRequests(BuildPrivateAggregationRequest(/*bucket=*/0,
                                                                 /*value=*/10),
                                  BuildPrivateAggregationRequest(/*bucket=*/1,
                                                                 /*value=*/1))),
          testing::Pair(
              kBidder2,
              ElementsAreRequests(BuildPrivateAggregationRequest(/*bucket=*/100,
                                                                 /*value=*/110),
                                  BuildPrivateAggregationRequest(/*bucket=*/101,
                                                                 /*value=*/101),
                                  BuildPrivateAggregationRequest(/*bucket=*/50,
                                                                 /*value=*/300),
                                  // No signals fetch for reporting,
                                  // so value = 0.
                                  BuildPrivateAggregationRequest(/*bucket=*/51,
                                                                 /*value=*/0))),
          testing::Pair(
              kComponentSeller1,
              ElementsAreRequests(BuildPrivateAggregationRequest(/*bucket=*/10,
                                                                 /*value=*/20),
                                  BuildPrivateAggregationRequest(/*bucket=*/11,
                                                                 /*value=*/21),
                                  BuildPrivateAggregationRequest(/*bucket=*/110,
                                                                 /*value=*/120),
                                  BuildPrivateAggregationRequest(/*bucket=*/111,
                                                                 /*value=*/121),
                                  BuildPrivateAggregationRequest(/*bucket=*/60,
                                                                 /*value=*/250),
                                  // No signals fetch for reporting,
                                  // so value = 0.
                                  BuildPrivateAggregationRequest(/*bucket=*/61,
                                                                 /*value=*/0))),
          testing::Pair(kSeller,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(/*bucket=*/20,
                                                           /*value=*/30),
                            BuildPrivateAggregationRequest(/*bucket=*/21,
                                                           /*value=*/31),
                            BuildPrivateAggregationRequest(/*bucket=*/70,
                                                           /*value=*/200),
                            // No signals fetch for reporting, so value = 0.
                            BuildPrivateAggregationRequest(/*bucket=*/71,
                                                           /*value=*/0)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportTrustedSignalsFetchLatency) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/kTrustedSignalsFetchLatency.InMilliseconds() *
                          kScale)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportBiddingDuration) {
  constexpr base::TimeDelta kBiddingDuration = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalGenerateBidLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders,
                             /*trusted_fetch_latency=*/{base::TimeDelta()},
                             /*bidding_latency=*/{kBiddingDuration});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/kBiddingDuration.InMilliseconds() * kScale)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportAllSellersCapabilities) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetAllSellerCapabilities({blink::SellerCapabilities::kLatencyStats})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/kTrustedSignalsFetchLatency.InMilliseconds() *
                          kScale)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportDifferentDurationScaleAndOffset) {
  // Use a non-whole number of milliseconds to check truncation.
  constexpr base::TimeDelta kTrustedSignalsFetchLatency =
      base::Microseconds(2500);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 2l;
  constexpr double kScale = 0.5;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      // 2500 microseconds = 2.5 milliseconds,
                      // 2.5 * 0.5 = 1.25
                      // 1.25 gets truncated to 1, since an integer is required
                      /*value=*/1)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportInfiniteScale) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 2l;
  constexpr double kScale = std::numeric_limits<double>::infinity();

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/std::numeric_limits<int32_t>::max())))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportNaNScale) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 2l;
  constexpr double kScale = std::numeric_limits<double>::quiet_NaN();

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/0)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportNegativeScale) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 2l;
  constexpr double kScale = -1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/0)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportBucketOverflowDoesntCrash) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = absl::Uint128Max();
  constexpr absl::uint128 kOffset = 1l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  // The actual report doesn't matter for this test -- we just want to ensure
  // that no crash occurs.
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersNotAuthorized) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersNoReportBuyerKeys) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = absl::nullopt;
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersNoReportBuyers) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = absl::nullopt;

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportBuyersDoesntMatchCapabilities) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kInterestGroupCount,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, {kTrustedSignalsFetchLatency});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportMultipleBidders) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency1 =
      base::Milliseconds(2);
  constexpr base::TimeDelta kTrustedSignalsFetchLatency2 =
      base::Milliseconds(4);
  constexpr absl::uint128 kBaseBucket1 = 100l;
  constexpr absl::uint128 kBaseBucket2 = 105l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket1, kBaseBucket2}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder2, kBidder2Name)
          .SetBiddingUrl(kBidder2Url)
          .SetTrustedBiddingSignalsUrl(kBidder2TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {kTrustedSignalsFetchLatency1, kTrustedSignalsFetchLatency2},
      /*bidding_latency=*/{base::TimeDelta(), base::TimeDelta()},
      /*should_bid=*/{true, true});

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          url::Origin::Create(kSellerUrl),
          ElementsAreRequests(
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket1 + kOffset,
                  /*value=*/kTrustedSignalsFetchLatency1.InMilliseconds() *
                      kScale),
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket2 + kOffset,
                  /*value=*/kTrustedSignalsFetchLatency2.InMilliseconds() *
                      kScale)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersReportMultipleBiddersIncompleteBuyerKeys) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency1 =
      base::Milliseconds(2);
  constexpr base::TimeDelta kTrustedSignalsFetchLatency2 =
      base::Milliseconds(4);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder2, kBidder2Name)
          .SetBiddingUrl(kBidder2Url)
          .SetTrustedBiddingSignalsUrl(kBidder2TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {kTrustedSignalsFetchLatency1, kTrustedSignalsFetchLatency2},
      /*bidding_latency=*/{base::TimeDelta(), base::TimeDelta()},
      /*should_bid=*/{true, true});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/kTrustedSignalsFetchLatency1.InMilliseconds() *
                          kScale)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersMultipleBiddersSameOwnerTrustedSignalsLatency) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency1 =
      base::Milliseconds(2);
  constexpr base::TimeDelta kTrustedSignalsFetchLatency2 =
      base::Milliseconds(3);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};
  // Both interest groups belong to the same bidder.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1NameAlt)
          .SetBiddingUrl(kBidder1UrlAlt)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {kTrustedSignalsFetchLatency1, kTrustedSignalsFetchLatency2},
      /*bidding_latency=*/{base::TimeDelta(), base::TimeDelta()},
      /*should_bid=*/{true, true});

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          url::Origin::Create(kSellerUrl),
          ElementsAreRequests(
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset,
                  /*value=*/kTrustedSignalsFetchLatency1.InMilliseconds() *
                      kScale),
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset,
                  /*value=*/kTrustedSignalsFetchLatency2.InMilliseconds() *
                      kScale)))));
}

TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersMultipleBiddersSameOwnerBiddingLatency) {
  constexpr base::TimeDelta kBiddingFetchLatency1 = base::Milliseconds(2);
  constexpr base::TimeDelta kBiddingFetchLatency2 = base::Milliseconds(3);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalGenerateBidLatency,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};
  // Both interest groups belong to the same bidder.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1NameAlt)
          .SetBiddingUrl(kBidder1UrlAlt)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {base::TimeDelta(), base::TimeDelta()},
      /*bidding_latency=*/{kBiddingFetchLatency1, kBiddingFetchLatency2},
      /*should_bid=*/{true, true});

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          url::Origin::Create(kSellerUrl),
          ElementsAreRequests(
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset,
                  /*value=*/kBiddingFetchLatency1.InMilliseconds() * kScale),
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset,
                  /*value=*/kBiddingFetchLatency2.InMilliseconds() *
                      kScale)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersMultipleStats) {
  constexpr base::TimeDelta kTrustedSignalsFetchLatency = base::Milliseconds(2);
  constexpr base::TimeDelta kBiddingFetchLatency = base::Milliseconds(3);
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset1 = 0l;
  constexpr absl::uint128 kOffset2 = 1l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalSignalsFetchLatency,
                              {/*bucket=*/kOffset1,
                               /*scale=*/kScale}},
                             {blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kTotalGenerateBidLatency,
                              {/*bucket=*/kOffset2,
                               /*scale=*/kScale}}}};
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kLatencyStats}}}})
          .Build()));

  RunExtendedPABuyersAuction(bidders, /*trusted_fetch_latency=*/
                             {kTrustedSignalsFetchLatency},
                             /*bidding_latency=*/{kBiddingFetchLatency},
                             /*should_bid=*/{true});

  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          url::Origin::Create(kSellerUrl),
          ElementsAreRequests(
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset1,
                  /*value=*/kTrustedSignalsFetchLatency.InMilliseconds() *
                      kScale),
              BuildPrivateAggregationRequest(
                  /*bucket=*/kBaseBucket + kOffset2,
                  /*value=*/kBiddingFetchLatency.InMilliseconds() * kScale)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportBidCount) {
  constexpr absl::uint128 kBaseBucket1 = 100l;
  constexpr absl::uint128 kBaseBucket2 = 105l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket1, kBaseBucket2}};
  auction_report_buyers_ = {
      {{blink::AuctionConfig::NonSharedParams::BuyerReportType::kBidCount,
        {/*bucket=*/kOffset,
         /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder2, kBidder2Name)
          .SetBiddingUrl(kBidder2Url)
          .SetTrustedBiddingSignalsUrl(kBidder2TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {base::TimeDelta(), base::TimeDelta()},
      /*bidding_latency=*/{base::TimeDelta(), base::TimeDelta()},
      /*should_bid=*/{true, false});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                                          /*bucket=*/kBaseBucket1 + kOffset,
                                          /*value=*/1 * kScale),
                                      BuildPrivateAggregationRequest(
                                          /*bucket=*/kBaseBucket2 + kOffset,
                                          /*value=*/0 * kScale)))));
}

TEST_F(AuctionRunnerTest, PrivateAggregationBuyersReportInterestGroupCount) {
  constexpr absl::uint128 kBaseBucket1 = 100l;
  constexpr absl::uint128 kBaseBucket2 = 105l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket1, kBaseBucket2}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kInterestGroupCount,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder2, kBidder2Name)
          .SetBiddingUrl(kBidder2Url)
          .SetTrustedBiddingSignalsUrl(kBidder2TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));

  RunExtendedPABuyersAuction(
      bidders, /*trusted_fetch_latency=*/
      {base::TimeDelta(), base::TimeDelta()},
      /*bidding_latency=*/{base::TimeDelta(), base::TimeDelta()},
      /*should_bid=*/{true, false});

  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                                          /*bucket=*/kBaseBucket1 + kOffset,
                                          /*value=*/1 * kScale),
                                      BuildPrivateAggregationRequest(
                                          /*bucket=*/kBaseBucket2 + kOffset,
                                          /*value=*/1 * kScale)))));
}

// Reported InterestGroupCount is unaffected by the group limit.
TEST_F(AuctionRunnerTest,
       PrivateAggregationBuyersInterestGroupCountUnconstrainedByLimits) {
  constexpr absl::uint128 kBaseBucket = 100l;
  constexpr absl::uint128 kOffset = 0l;
  constexpr double kScale = 1.0;

  auction_report_buyer_keys_ = {{kBaseBucket}};
  auction_report_buyers_ = {{{blink::AuctionConfig::NonSharedParams::
                                  BuyerReportType::kInterestGroupCount,
                              {/*bucket=*/kOffset,
                               /*scale=*/kScale}}}};
  all_buyers_group_limit_ = 1;

  // Both interest groups belong to the same bidder.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1Name)
          .SetBiddingUrl(kBidder1Url)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));
  bidders.emplace_back(MakeInterestGroup(
      blink::TestInterestGroupBuilder(kBidder1, kBidder1NameAlt)
          .SetBiddingUrl(kBidder1UrlAlt)
          .SetTrustedBiddingSignalsUrl(kBidder1TrustedSignalsUrl)
          .SetTrustedBiddingSignalsKeys({{"k1", "k2"}})
          .SetAds({{blink::InterestGroup::Ad(GURL("https://ad1.com"),
                                             absl::nullopt)}})
          .SetSellerCapabilities(
              {{{url::Origin::Create(kSellerUrl),
                 {blink::SellerCapabilities::kInterestGroupCounts}}}})
          .Build()));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  // Only one report comes in, and it indicates there are 2 groups, even though
  // we apply a group limit of 1.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre(testing::Pair(
                  url::Origin::Create(kSellerUrl),
                  ElementsAreRequests(BuildPrivateAggregationRequest(
                      /*bucket=*/kBaseBucket + kOffset,
                      /*value=*/2 * kScale)))));
}

class RoundingTest : public AuctionRunnerTest,
                     public ::testing::WithParamInterface<size_t> {
 public:
  RoundingTest(size_t bid_bits, size_t score_bits, size_t cost_bits)
      : bid_bits_(bid_bits), score_bits_(score_bits), cost_bits_(cost_bits) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kFledgeRounding,
          {{kFledgeBidReportingBits.name, base::NumberToString(bid_bits_)},
           {kFledgeScoreReportingBits.name, base::NumberToString(score_bits_)},
           {kFledgeAdCostReportingBits.name,
            base::NumberToString(cost_bits_)}}}},
        {});
  }

  size_t bid_bits() { return bid_bits_; }
  size_t score_bits() { return score_bits_; }
  size_t cost_bits() { return cost_bits_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  size_t bid_bits_, score_bits_, cost_bits_;
};

class BidRoundingTest : public RoundingTest {
 public:
  BidRoundingTest() : RoundingTest(GetParam(), 8, 8) {}
};

class ScoreRoundingTest : public RoundingTest {
 public:
  ScoreRoundingTest() : RoundingTest(8, GetParam(), 8) {}
};

class CostRoundingTest : public RoundingTest {
 public:
  CostRoundingTest() : RoundingTest(8, 8, GetParam()) {}
};

TEST_P(CostRoundingTest, AdCostPassed) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: bid,
              render: interestGroup.ads[0].renderURL,
              adCost: bid + 1};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?adCost=" +
                   browserSignals.adCost);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Only one bidder, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // adCost should be 2.
  EXPECT_THAT(result_.report_urls,
              testing::ElementsAre(
                  GURL("https://buyer-reporting.example.com/?adCost=2")));
}

TEST_P(CostRoundingTest, AdCostRounded) {
  const char kBidScript[] = R"(
    const bid = %f;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // Return an adCost that requires more bits of precision than allowed.
      return {bid: bid,
              render: interestGroup.ads[0].renderURL,
              adCost: bid};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?adCost=" +
                   browserSignals.adCost);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1.99));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Only one bidder, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  switch (GetParam()) {
    case 8:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(testing::AnyOf(
              GURL("https://buyer-reporting.example.com/?adCost=1.9921875"),
              GURL("https://buyer-reporting.example.com/?adCost=1.984375"))));
      break;
    case 16:
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre(
                      testing::AnyOf(GURL("https://buyer-reporting.example.com/"
                                          "?adCost=1.990020751953125"),
                                     GURL("https://buyer-reporting.example.com/"
                                          "?adCost=1.989990234375"))));
      break;
    case 53:
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre(GURL(
                      "https://buyer-reporting.example.com/?adCost=1.99")));
      break;
    default:
      // Not a supported test case.
      ASSERT_TRUE(false);
  }
}

TEST_P(CostRoundingTest, AdCostExponentTruncated) {
  const char kBidScript[] = R"(
    const bid = %d;
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // Return an adCost that requires more bits of exponent than allowed.
      return {bid: bid, render: interestGroup.ads[0].renderURL, adCost: 2**256};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?adCost=" +
                   browserSignals.adCost);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Only one bidder, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // adCost should be (Infinity).
  EXPECT_THAT(result_.report_urls,
              testing::ElementsAre(GURL(
                  "https://buyer-reporting.example.com/?adCost=Infinity")));
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    CostRoundingTest,
    testing::Values(8, 16, 53));

TEST_F(AuctionRunnerTest, ModelingSignalsPassed) {
  // Due to noising, modelingSignals is only correctly passed 99% of the time.
  //
  // Since the noising pseudorandom number generator is uniform, in 30 runs, the
  // probability that at least 15 of those get the correct answer is
  //
  // CDF[BinomialDistribution[30, 0.99], 30] -
  // CDF[BinomialDistribution[30, 0.99], 15]
  //
  // Which is *extremely* close to 1 (within 10^(-20)):
  // https://www.wolframalpha.com/input?i=N%5B%5BCDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+30%5D+-+CDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+15%5D%5D%2C+30%5D
  //
  // The chosen constants below balance having an extremely low (basically 0)
  // probability of flakes, shorter runtime of the test (auctions are slow), and
  // having a substantial number of the runs return the correct value.
  constexpr int kNumRuns = 30;
  constexpr int kNumCorrectAtLeast = 15;

  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: 1,
              render: interestGroup.ads[0].renderURL,
              modelingSignals: 0xF23};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?modelingSignals=" +
                   browserSignals.modelingSignals);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  int num_correct = 0;
  for (int i = 0; i < kNumRuns; i++) {
    RunStandardAuction(/*request_trusted_bidding_signals=*/false);
    EXPECT_FALSE(result_.manually_aborted);
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    ASSERT_EQ(result_.report_urls.size(), 1u);
    base::StringPiece query = result_.report_urls[0].query_piece();
    std::vector<base::StringPiece> split = base::SplitStringPiece(
        query, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(split.size(), 2u);
    int reported_modeling_signals;
    EXPECT_TRUE(base::StringToInt(split[1], &reported_modeling_signals));
    // Even noised results should be in the modeling signals range.
    EXPECT_GE(reported_modeling_signals, 0);
    EXPECT_LE(reported_modeling_signals, 0x0FFF);
    if (reported_modeling_signals == 0xF23) {
      num_correct++;
    }
  }
  EXPECT_GE(num_correct, kNumCorrectAtLeast);
}

TEST_F(AuctionRunnerTest, ModelingSignalsNotPresent) {
  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: 1,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?modelingSignals=" +
                   browserSignals.modelingSignals);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  // modelingSignals should be undefined.
  EXPECT_THAT(
      result_.report_urls,
      testing::ElementsAre(GURL(
          "https://buyer-reporting.example.com/?modelingSignals=undefined")));
}

TEST_F(AuctionRunnerTest, JoinCountPassedToReportWin) {
  // Due to noising, joinCount is only correctly passed 99% of the time.
  //
  // Since the noising pseudorandom number generator is uniform, in 30 runs, the
  // probability that at least 15 of those get the correct answer is
  //
  // CDF[BinomialDistribution[30, 0.99], 30] -
  // CDF[BinomialDistribution[30, 0.99], 15]
  //
  // Which is *extremely* close to 1 (within 10^(-20)):
  // https://www.wolframalpha.com/input?i=N%5B%5BCDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+30%5D+-+CDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+15%5D%5D%2C+30%5D
  //
  // The chosen constants below balance having an extremely low (basically 0)
  // probability of flakes, shorter runtime of the test (auctions are slow), and
  // having a substantial number of the runs return the correct value.
  constexpr int kNumRuns = 30;
  constexpr int kNumCorrectAtLeast = 15;

  // Chosen arbitrarily to exercise bucketing.
  constexpr int kJoinCount = 13;
  constexpr int kJoinCountBucketed = 11;

  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: 1,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?joinCount=" +
                   browserSignals.joinCount);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  int num_correct = 0;
  for (int i = 0; i < kNumRuns; i++) {
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
    bidders[0].bidding_browser_signals->join_count = kJoinCount;
    StartAuction(kSellerUrl, std::move(bidders));
    auction_run_loop_->Run();

    EXPECT_FALSE(result_.manually_aborted);
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    ASSERT_EQ(result_.report_urls.size(), 1u);
    base::StringPiece query = result_.report_urls[0].query_piece();
    std::vector<base::StringPiece> split = base::SplitStringPiece(
        query, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(split.size(), 2u);
    int reported_join_count;
    EXPECT_TRUE(base::StringToInt(split[1], &reported_join_count));
    // Even noised results should be in the join count range.
    EXPECT_GE(reported_join_count, 1);
    EXPECT_LE(reported_join_count, 16);
    if (reported_join_count == kJoinCountBucketed) {
      num_correct++;
    }
  }
  EXPECT_GE(num_correct, kNumCorrectAtLeast);
}

TEST_F(AuctionRunnerTest, RecencyPassedReportWin) {
  // Due to noising, recency is only correctly passed 99% of the time.
  //
  // Since the noising pseudorandom number generator is uniform, in 30 runs, the
  // probability that at least 15 of those get the correct answer is
  //
  // CDF[BinomialDistribution[30, 0.99], 30] -
  // CDF[BinomialDistribution[30, 0.99], 15]
  //
  // Which is *extremely* close to 1 (within 10^(-20)):
  // https://www.wolframalpha.com/input?i=N%5B%5BCDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+30%5D+-+CDF%5BBinomialDistribution%5B30%2C+0.99%5D%2C+15%5D%5D%2C+30%5D
  //
  // The chosen constants below balance having an extremely low (basically 0)
  // probability of flakes, shorter runtime of the test (auctions are slow), and
  // having a substantial number of the runs return the correct value.
  constexpr int kNumRuns = 30;
  constexpr int kNumCorrectAtLeast = 15;

  // Chosen arbitrarily to exercise bucketing.
  constexpr base::TimeDelta kRecency = base::Minutes(70);
  constexpr int kRecencyBucketed = 16;

  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      return {bid: 1,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?recency=" +
                   browserSignals.recency);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  int num_correct = 0;
  for (int i = 0; i < kNumRuns; i++) {
    between_join_run_auction_delay_ = kRecency;
    RunStandardAuction(/*request_trusted_bidding_signals=*/false);
    EXPECT_FALSE(result_.manually_aborted);
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    ASSERT_EQ(result_.report_urls.size(), 1u);
    base::StringPiece query = result_.report_urls[0].query_piece();
    std::vector<base::StringPiece> split = base::SplitStringPiece(
        query, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(split.size(), 2u);
    int reported_recency;
    EXPECT_TRUE(base::StringToInt(split[1], &reported_recency));
    // Even noised results should be in the recency range.
    EXPECT_GE(reported_recency, 0);
    EXPECT_LE(reported_recency, 31);
    if (reported_recency == kRecencyBucketed) {
      num_correct++;
    }
  }
  EXPECT_GE(num_correct, kNumCorrectAtLeast);
}

TEST_F(AuctionRunnerTest, RecencyPassedGenerateBid) {
  // Add on some number of milliseconds that aren't a multiple of 100ms to test
  // coarsening to 100ms.
  constexpr base::TimeDelta kRecency = base::Hours(1) + base::Milliseconds(105);

  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // RunStandardAuction adds an additional 1 second for each previous win,
      // so we expect to see 6 seconds more than kRecency (2 interest groups
      // with 3 previous wins each), truncated to the next multiple of 100
      // milliseconds. Hence, the delay is 3606100 and not 3600100 or 3600105.
      if (browserSignals.recency !== 3606100) {
        throw "Wrong recency " + browserSignals.recency;
      }
      return {bid: 1,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  between_join_run_auction_delay_ = kRecency;
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
}

class AuctionRunnerPassRecencyToGenerateBidDisabledTest
    : public AuctionRunnerTest {
 public:
  AuctionRunnerPassRecencyToGenerateBidDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kFledgePassRecencyToGenerateBid);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuctionRunnerPassRecencyToGenerateBidDisabledTest, NotPassed) {
  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // PassRecencyToGenerateBid is disabled, so recency shouldn't be set.
      if (typeof browserSignals.recency !== "undefined") {
        throw "Wrong recency " + browserSignals.recency;
      }
      return {bid: 1,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
}

TEST_P(BidRoundingTest, BidRounded) {
  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // Return a bid that requires more bits of precision than allowed.
      return {bid: %f,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?bid=" +
                   browserSignals.bid);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1.99));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Only one bidder, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  switch (GetParam()) {
    case 8:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(testing::AnyOf(
              GURL("https://buyer-reporting.example.com/?bid=1.9921875"),
              GURL("https://buyer-reporting.example.com/?bid=1.984375"))));
      break;
    case 16:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(testing::AnyOf(
              GURL(
                  "https://buyer-reporting.example.com/?bid=1.990020751953125"),
              GURL(
                  "https://buyer-reporting.example.com/?bid=1.989990234375"))));
      break;
    case 53:
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre(
                      GURL("https://buyer-reporting.example.com/?bid=1.99")));
      break;
    default:
      // Not a supported test case.
      ASSERT_TRUE(false);
  }
}

TEST_P(BidRoundingTest, HighestScoringOtherBidRounded) {
  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // Return a bid that requires more bits of precision than allowed.
      return {bid: %f,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/?"
        + "highestScoringOtherBid=" + browserSignals.highestScoringOtherBid);
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return bid;
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo("https://seller-reporting.example.com/?"
        + "highestScoringOtherBid="
        + browserSignals.highestScoringOtherBid);
    }
  )";
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         base::StringPrintf(kBidScript, 3.0));

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1.99));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  switch (GetParam()) {
    case 8:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(
              testing::AnyOf(GURL("https://seller-reporting.example.com/"
                                  "?highestScoringOtherBid=1.9921875"),
                             GURL("https://seller-reporting.example.com/"
                                  "?highestScoringOtherBid=1.984375")),
              testing::AnyOf(GURL("https://buyer-reporting.example.com/"
                                  "?highestScoringOtherBid=1.9921875"),
                             GURL("https://buyer-reporting.example.com/"
                                  "?highestScoringOtherBid=1.984375"))));
      break;
    case 16:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(
              testing::AnyOf(GURL("https://seller-reporting.example.com/"
                                  "?highestScoringOtherBid=1.990020751953125"),
                             GURL("https://seller-reporting.example.com/"
                                  "?highestScoringOtherBid=1.989990234375")),
              testing::AnyOf(GURL("https://buyer-reporting.example.com/"
                                  "?highestScoringOtherBid=1.990020751953125"),
                             GURL("https://buyer-reporting.example.com/"
                                  "?highestScoringOtherBid=1.989990234375"))));
      break;
    case 53:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(GURL("https://seller-reporting.example.com/"
                                    "?highestScoringOtherBid=1.99"),
                               GURL("https://buyer-reporting.example.com/"
                                    "?highestScoringOtherBid=1.99")));
      break;
    default:
      // Not a supported test case.
      ASSERT_TRUE(false);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    BidRoundingTest,
    ::testing::Values(8, 16, 53));

TEST_P(ScoreRoundingTest, ScoreRounded) {
  const char kBidScript[] = R"(
    function generateBid(
        interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
        browserSignals) {
      // Return an score that requires more bits of precision than allowed.
      return {bid: %f,
              render: interestGroup.ads[0].renderURL};
    }

    function reportWin(
        auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
    }
  )";

  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      return 1.99;
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo("https://seller-reporting.example.com/?score=" + browserSignals.desirability);
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         base::StringPrintf(kBidScript, 1.99));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  // Only one bidder, to keep things simple.
  interest_group_buyers_ = {{kBidder1}};
  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_FALSE(result_.manually_aborted);
  EXPECT_EQ(kBidder1Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  switch (GetParam()) {
    case 8:
      EXPECT_THAT(
          result_.report_urls,
          testing::ElementsAre(testing::AnyOf(
              GURL("https://seller-reporting.example.com/?score=1.9921875"),
              GURL("https://seller-reporting.example.com/?score=1.984375"))));
      break;
    case 16:
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre(testing::AnyOf(
                      GURL("https://seller-reporting.example.com/"
                           "?score=1.990020751953125"),
                      GURL("https://seller-reporting.example.com/"
                           "?score=1.989990234375"))));
      break;
    case 53:
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre(GURL(
                      "https://seller-reporting.example.com/?score=1.99")));
      break;
    default:
      // Not a supported test case.
      ASSERT_TRUE(false);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ScoreRoundingTest,
    ::testing::Values(8, 16, 53));

// Enable and test forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() APIs.
// IMPORTANT: These tests do a better job of covering second-highest-scoring
// bid ties and other such tricky cases than elsewhere[1], so if the
// forDebuggingOnly functionality is removed they should be adjusted to only
// look at `report_urls` rather than entirely removed.
//
// [1] And are therefore the only place for some currency testcases.
class AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest
    : public AuctionRunnerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
    if (SellerCurrencyOn()) {
      seller_currency_ = blink::AdCurrency::From("EUR");
    }
  }

  bool SellerCurrencyOn() const { return GetParam(); }

  absl::optional<blink::AdCurrency> ModeCurrency() {
    return SellerCurrencyOn()
               ? absl::make_optional(blink::AdCurrency::From("EUR"))
               : absl::nullopt;
  }

  double ModeBid(double in) { return SellerCurrencyOn() ? in * 10.0 : in; }

  double TopLevelModeBid(double in) {
    // Since the component auction doesn't tag its bid as EUR, we end up doing
    // the conversion twice (which is useful for tracking things).
    return SellerCurrencyOn() ? in * 100.0 : in;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_EQ(2u, result_.debug_loss_report_urls.size());
  // Sellers can get highest scoring other bid, but losing bidders can not.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(
                      kBidder1DebugLossReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/false,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/absl::nullopt,
                          /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl(kSellerDebugLossReportBaseUrl,
                                 PostAuctionSignals(
                                     /*winning_bid=*/ModeBid(2),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/ModeBid(1),
                                     /*highest_scoring_other_bid_currency=*/
                                     ModeCurrency(),
                                     /*made_highest_scoring_other_bid=*/true),
                                 /*bid=*/1)));

  EXPECT_EQ(2u, result_.debug_win_report_urls.size());
  // Winning bidders can get highest scoring other bid.
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(
                      kBidder2DebugWinReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/ModeBid(1),
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl(
                      kSellerDebugWinReportBaseUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/ModeBid(1),
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false),
                      /*bid=*/2)));
}

// Post auction signals should only be reported through report URL's query
// string. Placeholder ${} in a debugging report URL's other parts such as path
// will be kept as it is without being replaced with actual signal.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  // Placeholder ${winningBid} in a debugging report URL's path will not be
  // replaced with actual signal. Only those in a debugging report URL's query
  // param would be replaced.
  EXPECT_EQ(2u, result_.debug_loss_report_urls.size());
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl("https://bidder1-debug-loss-reporting.com/"
                                 "winningBid=${winningBid}",
                                 PostAuctionSignals(
                                     /*winning_bid=*/ModeBid(2),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*highest_scoring_other_bid_currency=*/
                                     absl::nullopt,
                                     /*made_highest_scoring_other_bid=*/false)),
                  DebugReportUrl("https://seller-debug-loss-reporting.com/"
                                 "winningBid=${winningBid}",
                                 PostAuctionSignals(
                                     /*winning_bid=*/ModeBid(2),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/ModeBid(1),
                                     /*highest_scoring_other_bid_currency=*/
                                     ModeCurrency(),
                                     /*made_highest_scoring_other_bid=*/true),
                                 /*bid=*/1)));

  EXPECT_EQ(2u, result_.debug_win_report_urls.size());
  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl("https://bidder2-debug-win-reporting.com/"
                         "winningBid=${winningBid}",
                         PostAuctionSignals(
                             /*winning_bid=*/ModeBid(2),
                             /*winning_bid_currency=*/ModeCurrency(),
                             /*made_winning_bid=*/true,
                             /*highest_scoring_other_bid=*/ModeBid(1),
                             /*highest_scoring_other_bid_currency=*/
                             ModeCurrency(),
                             /*made_highest_scoring_other_bid=*/false)),
          DebugReportUrl(
              "https://seller-debug-win-reporting.com/winningBid=${winningBid}",
              PostAuctionSignals(
                  /*winning_bid=*/ModeBid(2),
                  /*winning_bid_currency=*/ModeCurrency(),
                  /*made_winning_bid=*/true,
                  /*highest_scoring_other_bid=*/ModeBid(1),
                  /*highest_scoring_other_bid_currency=*/
                  ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false),
              /*bid=*/2)));
}

// When there are multiple bids getting the highest score, then highest scoring
// other bid will be one of them which didn't win the bid.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

    RunAuctionAndWait(kSellerUrl, std::move(bidders));

    EXPECT_EQ(4u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(2u, result_.debug_win_report_urls.size());
    EXPECT_EQ(2u, result_.report_urls.size());

    // Winner has ad2 or ad3.
    if (result_.ad_descriptor->url == "https://ad2.com/") {
      seen_ad2_win = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/ModeBid(4),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/ModeBid(4),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/ModeBid(4),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/ModeBid(4),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=40&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=3")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=4&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=3"),
              ReportWinUrl(
                  /*bid=*/3, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(4), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false)));
    } else if (result_.ad_descriptor->url == "https://ad3.com/") {
      seen_ad3_win = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/ModeBid(3),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/false,
                      /*highest_scoring_other_bid=*/ModeBid(3),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/3)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugWinReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/ModeBid(3),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4),
              DebugReportUrl(
                  kSellerDebugWinReportBaseUrl,

                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(4),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/ModeBid(3),
                      /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/4)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=30&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=4")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=3&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=4"),
              ReportWinUrl(
                  /*bid=*/4, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(3), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false)));
    } else {
      NOTREACHED();
    }
  }
}

// This is used to test post auction signals when an auction where bidders are
// from the same interest group owner. All winning bid and highest scoring other
// bids come from the same interest group owner.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

    RunAuctionAndWait(kSellerUrl, std::move(bidders));

    double highest_scoring_other_bid = 0.0;
    if (base::Contains(result_.report_urls,
                       "https://reporting.example.com/"
                       "?highestScoringOtherBid=1&"
                       "highestScoringOtherBidCurrency=???&"
                       "bidCurrency=USD&bid=3",
                       &GURL::spec) ||
        base::Contains(result_.report_urls,
                       "https://reporting.example.com/"
                       "?highestScoringOtherBid=10&"
                       "highestScoringOtherBidCurrency=EUR&"
                       "bidCurrency=USD&bid=3",
                       &GURL::spec)) {
      highest_scoring_other_bid = 1;
    } else if (base::Contains(result_.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=2&"
                              "highestScoringOtherBidCurrency=???&"
                              "bidCurrency=USD&bid=3",
                              &GURL::spec) ||
               base::Contains(result_.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=20&"
                              "highestScoringOtherBidCurrency=EUR&"
                              "bidCurrency=USD&bid=3",
                              &GURL::spec)) {
      highest_scoring_other_bid = 2;
    }

    EXPECT_EQ(GURL("https://ad3.com/"), result_.ad_descriptor->url);
    EXPECT_EQ(4u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(2u, result_.debug_win_report_urls.size());
    EXPECT_EQ(2u, result_.report_urls.size());

    if (highest_scoring_other_bid == 1) {
      seen_bid1 = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/1),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/2)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=10&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=3")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=1&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=3"),
              ReportWinUrl(
                  /*bid=*/3, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(1), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/true)));
    } else if (highest_scoring_other_bid == 2) {
      seen_bid2 = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/1),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/2)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
                             /*bid=*/3)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=20&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=3")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=2&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=3"),
              ReportWinUrl(
                  /*bid=*/3, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(2), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/true)));
    } else {
      NOTREACHED();
    }
  }
}

// Multiple bids from different interest group owners get the second highest
// score, then `${madeHighestScoringOtherBid}` is always false.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

    RunAuctionAndWait(kSellerUrl, std::move(bidders));

    EXPECT_EQ(GURL("https://ad3.com/"), result_.ad_descriptor->url);
    EXPECT_EQ(4u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(2u, result_.debug_win_report_urls.size());
    EXPECT_EQ(2u, result_.report_urls.size());
    double highest_scoring_other_bid = 0.0;
    if (base::Contains(result_.report_urls,
                       "https://reporting.example.com/"
                       "?highestScoringOtherBid=1&"
                       "highestScoringOtherBidCurrency=???&"
                       "bidCurrency=USD&bid=3",
                       &GURL::spec) ||
        base::Contains(result_.report_urls,
                       "https://reporting.example.com/"
                       "?highestScoringOtherBid=10&"
                       "highestScoringOtherBidCurrency=EUR&"
                       "bidCurrency=USD&bid=3",
                       &GURL::spec)) {
      highest_scoring_other_bid = 1;
    } else if (base::Contains(result_.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=2&"
                              "highestScoringOtherBidCurrency=???&"
                              "bidCurrency=USD&bid=3",
                              &GURL::spec) ||
               base::Contains(result_.report_urls,
                              "https://reporting.example.com/"
                              "?highestScoringOtherBid=20&"
                              "highestScoringOtherBidCurrency=EUR&"
                              "bidCurrency=USD&bid=3",
                              &GURL::spec)) {
      highest_scoring_other_bid = 2;
    }

    if (highest_scoring_other_bid == 1) {
      seen_bid1 = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(
                      /*winning_bid=*/ModeBid(3),
                      /*winning_bid_currency=*/ModeCurrency(),
                      /*made_winning_bid=*/true,
                      /*highest_scoring_other_bid=*/0,
                      /*highest_scoring_other_bid_currency=*/absl::nullopt,
                      /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/ModeBid(3),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*highest_scoring_other_bid_currency=*/
                                     absl::nullopt,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/ModeBid(3),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/ModeBid(1),
                                     /*highest_scoring_other_bid_currency=*/
                                     ModeCurrency(),
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(1),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/3)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=10&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=3")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=1&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=3"),
              ReportWinUrl(
                  /*bid=*/3, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(1), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false)));
    } else if (highest_scoring_other_bid == 2) {
      seen_bid2 = true;
      EXPECT_THAT(
          result_.debug_loss_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/0,
                                 /*highest_scoring_other_bid_currency=*/
                                 absl::nullopt,
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/1),
              DebugReportUrl(
                  kBidderDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/ModeBid(3),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/0,
                                     /*highest_scoring_other_bid_currency=*/
                                     absl::nullopt,
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2),
              DebugReportUrl(kSellerDebugLossReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/1),
              DebugReportUrl(
                  kSellerDebugLossReportBaseUrl,
                  PostAuctionSignals(/*winning_bid=*/ModeBid(3),
                                     /*winning_bid_currency=*/ModeCurrency(),
                                     /*made_winning_bid=*/false,
                                     /*highest_scoring_other_bid=*/ModeBid(2),
                                     /*highest_scoring_other_bid_currency=*/
                                     ModeCurrency(),
                                     /*made_highest_scoring_other_bid=*/false),
                  /*bid=*/2)));

      EXPECT_THAT(
          result_.debug_win_report_urls,
          testing::UnorderedElementsAre(
              DebugReportUrl(kBidderDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/3),
              DebugReportUrl(kSellerDebugWinReportBaseUrl,
                             PostAuctionSignals(
                                 /*winning_bid=*/ModeBid(3),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/true,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/false),
                             /*bid=*/3)));

      EXPECT_THAT(
          result_.report_urls,
          testing::UnorderedElementsAre(
              SellerCurrencyOn() ? GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=20&"
                                        "highestScoringOtherBidCurrency=EUR&"
                                        "bidCurrency=USD&bid=3")
                                 : GURL("https://reporting.example.com/"
                                        "?highestScoringOtherBid=2&"
                                        "highestScoringOtherBidCurrency=???&"
                                        "bidCurrency=USD&bid=3"),
              ReportWinUrl(
                  /*bid=*/3, /*bid_currency=*/blink::AdCurrency::From("USD"),
                  /*highest_scoring_other_bid=*/ModeBid(2), ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false)));
    } else {
      NOTREACHED();
    }
  }
}

// Should send loss report to seller and bidders when auction fails due to
// AllBidsRejected.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // No winner since both bidders are rejected by seller.
  EXPECT_FALSE(result_.winning_group_id);
  EXPECT_FALSE(result_.ad_descriptor);

  EXPECT_EQ(4u, result_.debug_loss_report_urls.size());
  PostAuctionSignals empty_signals_with_currency;
  empty_signals_with_currency.highest_scoring_other_bid_currency =
      ModeCurrency();
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(kBidder1DebugLossReportUrl, PostAuctionSignals(),
                         /*bid=*/absl::nullopt, "invalid-bid"),
          DebugReportUrl(kBidder2DebugLossReportUrl, PostAuctionSignals(),
                         /*bid=*/absl::nullopt, "bid-below-auction-floor"),
          DebugReportUrl(kSellerDebugLossReportBaseUrl,
                         empty_signals_with_currency,
                         /*bid=*/1),
          DebugReportUrl(kSellerDebugLossReportBaseUrl,
                         empty_signals_with_currency,
                         /*bid=*/2)));

  EXPECT_EQ(0u, result_.debug_win_report_urls.size());
}

// Test win/loss reporting in a component auction with two components with one
// bidder each.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
          /*report_top_level_post_auction_signals=*/true));
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
          /*report_top_level_post_auction_signals=*/true));
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
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(
              kBidder1DebugLossReportUrl,
              PostAuctionSignals(
                  /*winning_bid=*/ModeBid(1),
                  /*winning_bid_currency=*/ModeCurrency(),
                  /*made_winning_bid=*/true,
                  /*highest_scoring_other_bid=*/0,
                  /*highest_scoring_other_bid_currency=*/absl::nullopt,
                  /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component1-loss-reporting.test/",
              /*signals=*/
              PostAuctionSignals(
                  /*winning_bid=*/ModeBid(1),
                  /*winning_bid_currency=*/ModeCurrency(),
                  /*made_winning_bid=*/true,
                  /*highest_scoring_other_bid=*/0,
                  /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                  /*made_highest_scoring_other_bid=*/false),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/TopLevelModeBid(2),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/false),
              /*bid=*/1),
          DebugReportUrl(
              "https://top-seller-loss-reporting.test/",
              PostAuctionSignals(
                  /*winning_bid=*/TopLevelModeBid(2),
                  /*winning_bid_currency=*/ModeCurrency(),
                  /*made_winning_bid=*/false,
                  /*highest_scoring_other_bid=*/0.0,
                  /*highest_scoring_other_bid_currency=*/absl::nullopt,
                  /*made_highest_scoring_other_bid=*/false),
              /*bid=*/ModeBid(1))));

  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(
                      kBidder2DebugWinReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false)),
                  ComponentSellerDebugReportUrl(
                      "https://component2-win-reporting.test/",
                      /*signals=*/
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false),
                      /*top_level_signals=*/
                      PostAuctionSignals(
                          /*winning_bid=*/TopLevelModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true),
                      /*bid=*/2),
                  DebugReportUrl(
                      "https://top-seller-win-reporting.test/",
                      PostAuctionSignals(
                          /*winning_bid=*/TopLevelModeBid(2), ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0.0,
                          /*highest_scoring_other_bid_currency=*/absl::nullopt,
                          /*made_highest_scoring_other_bid=*/false),
                      /*bid=*/ModeBid(2))));
}

// Test debug loss reporting in an auction with no winner. Component bidder 1 is
// rejected by component seller, and component bidder 2 is rejected by top-level
// seller. Component bidders get component auction's reject reason but not the
// top-level auction's.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
          /*report_top_level_post_auction_signals=*/true));
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
  EXPECT_FALSE(result_.ad_descriptor);

  // Component bidder 1 rejected by component auction gets its reject reason
  // "invalid-bid". Component bidders don't get the top-level auction's reject
  // reason.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(
                      kBidder1DebugLossReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/0,
                          /*winning_bid_currency=*/absl::nullopt,
                          /*made_winning_bid=*/false,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/absl::nullopt,
                          /*made_highest_scoring_other_bid=*/false),
                      /*bid=*/absl::nullopt, "invalid-bid"),
                  GURL("https://component1-loss-reporting.test/?&bid=1"),
                  DebugReportUrl(
                      kBidder2DebugLossReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/absl::nullopt,
                          /*made_highest_scoring_other_bid=*/false),
                      /*bid=*/absl::nullopt, "not-available"),
                  ComponentSellerDebugReportUrl(
                      "https://component2-loss-reporting.test/",
                      /*signals=*/
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(2),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0,
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false),
                      /*top_level_signals=*/
                      PostAuctionSignals(),
                      /*bid=*/2),
                  DebugReportUrl("https://top-seller-loss-reporting.test/",
                                 PostAuctionSignals(),
                                 /*bid=*/ModeBid(2))));

  EXPECT_THAT(result_.debug_win_report_urls, testing::UnorderedElementsAre());
}

// Test win/loss reporting in a component auction with one component with two
// bidders.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
          /*report_top_level_post_auction_signals=*/true));
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          DebugReportUrl(
              kBidder2DebugLossReportUrl,
              PostAuctionSignals(
                  /*winning_bid=*/ModeBid(1),
                  /*winning_bid_currency=*/ModeCurrency(),
                  /*made_winning_bid=*/false,
                  /*highest_scoring_other_bid=*/0,
                  /*highest_scoring_other_bid_currency=*/absl::nullopt,
                  /*made_highest_scoring_other_bid=*/false)),
          ComponentSellerDebugReportUrl(
              "https://component-loss-reporting.test/",
              /*signals=*/
              PostAuctionSignals(/*winning_bid=*/ModeBid(1),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/false,
                                 /*highest_scoring_other_bid=*/ModeBid(2),
                                 /*highest_scoring_other_bid_currency=*/
                                 ModeCurrency(),
                                 /*made_highest_scoring_other_bid=*/true),
              /*top_level_signals=*/
              PostAuctionSignals(/*winning_bid=*/TopLevelModeBid(1),
                                 /*winning_bid_currency=*/ModeCurrency(),
                                 /*made_winning_bid=*/false),
              /*bid=*/2)));

  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  DebugReportUrl(
                      kBidder1DebugWinReportUrl,
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(1),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/ModeBid(2),
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false)),
                  ComponentSellerDebugReportUrl(
                      "https://component-win-reporting.test/",
                      /*signals=*/
                      PostAuctionSignals(
                          /*winning_bid=*/ModeBid(1),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/ModeBid(2),
                          /*highest_scoring_other_bid_currency=*/ModeCurrency(),
                          /*made_highest_scoring_other_bid=*/false),
                      /*top_level_signals=*/
                      PostAuctionSignals(
                          /*winning_bid=*/TopLevelModeBid(1),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true),
                      /*bid=*/1),
                  DebugReportUrl(
                      "https://top-seller-win-reporting.test/",
                      PostAuctionSignals(
                          /*winning_bid=*/TopLevelModeBid(1),
                          /*winning_bid_currency=*/ModeCurrency(),
                          /*made_winning_bid=*/true,
                          /*highest_scoring_other_bid=*/0.0,
                          /*highest_scoring_other_bid_currency=*/absl::nullopt,
                          /*made_highest_scoring_other_bid=*/false),
                      /*bid=*/ModeBid(1))));
}

// Loss report URLs should be dropped when the seller worklet fails to load.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSellerWorkletFailToLoad) {
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

  StartStandardAuction();
  // Wait for the bids to be generated.
  task_environment()->RunUntilIdle();
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

TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
        /*bid=*/5, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad1.com/")),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_descriptors=*/absl::nullopt, base::TimeDelta(),
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
    EXPECT_FALSE(result_.ad_descriptor);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
        /*bid=*/5, /*bid_currency=*/absl::nullopt,
        blink::AdDescriptor(GURL("https://ad1.com/")),
        /*mojo_kanon_bid=*/nullptr,
        /*ad_component_descriptors=*/absl::nullopt, base::TimeDelta(),
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
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            test_case.seller_debug_loss_report_url,
            test_case.seller_debug_win_report_url, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
            /*errors=*/{});
    auction_run_loop_->Run();
    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.winning_group_id);
    EXPECT_FALSE(result_.ad_descriptor);
    EXPECT_THAT(result_.interest_groups_that_bid,
                testing::UnorderedElementsAre());

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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
      /*bid=*/5, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad1.com/")),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_descriptors=*/absl::nullopt, base::TimeDelta(),
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
      /*bid=*/10, /*bid_currency=*/absl::nullopt,
      blink::AdDescriptor(GURL("https://ad2.com/")),
      /*mojo_kanon_bid=*/nullptr,
      /*ad_component_descriptors=*/absl::nullopt, base::TimeDelta(),
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
          /*bid_in_seller_currency=*/absl::nullopt,
          /*scoring_signals_data_version=*/absl::nullopt,
          GURL("https://seller-debug-loss-reporting.com/1"),
          GURL("https://seller-debug-win-reporting.com/1"), /*pa_requests=*/{},
          /*scoring_latency=*/base::TimeDelta(),
          /*score_ad_dependency_latencies=*/
          auction_worklet::mojom::ScoreAdDependencyLatencies::New(
              /*code_ready_latency=*/absl::nullopt,
              /*direct_from_seller_signals_latency=*/absl::nullopt,
              /*trusted_scoring_signals_latency=*/absl::nullopt),
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
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.interest_groups_that_bid,
              testing::UnorderedElementsAre(kBidder1Key));
  EXPECT_EQ(R"({"metadata":"{\"ads\": true}","renderURL":"https://ad1.com/"})",
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
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       LargeComponentAuction) {
  const GURL kComponentSeller3Url{"https://component.seller3.test/baz.js"};

  // Seller URLs and number of bidders for each Auction.
  const struct {
    GURL seller_url;
    int num_bidders;
  } kSellerInfo[] = {
      // Top-level seller can't have any bidders.
      {kSellerUrl, 0},
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

  // Bidder 9 won - the first bidder for the third component auction. Higher
  // bidders bid more, but component sellers use a script that favors lower
  // bidders, while the top-level seller favors higher bidders.
  EXPECT_EQ(GURL("https://bidder9.ad.test/"), result_.ad_descriptor->url);

  // Only the bidder that wins the overall auction gets win report. All other
  // bidders get loss reports, even the component auction winners.
  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(
          GURL("https://bidder9.test/win/"), GURL("https://seller3.test/win/9"),
          GURL(SellerCurrencyOn() ? "https://seller0.test/win/90"
                                  : "https://seller0.test/win/9")));

  // Top-level seller doesn't report a loss for losing bidders of component
  // auctions, since the top-level seller never saw them.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  // kComponentSeller1's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  //
                  // Note that seller 0 here is the top-level seller, so it gets
                  // currency adjusted (*10) numbers if SellerCurrencyOn
                  GURL("https://bidder1.test/loss/"),
                  GURL("https://seller1.test/loss/1"),
                  GURL(SellerCurrencyOn() ? "https://seller0.test/loss/10"
                                          : "https://seller0.test/loss/1"),
                  GURL("https://bidder2.test/loss/"),
                  GURL("https://seller1.test/loss/2"),
                  GURL("https://bidder3.test/loss/"),
                  GURL("https://seller1.test/loss/3"),
                  // kComponentSeller2's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  GURL("https://bidder4.test/loss/"),
                  GURL("https://seller2.test/loss/4"),
                  GURL(SellerCurrencyOn() ? "https://seller0.test/loss/40"
                                          : "https://seller0.test/loss/4"),
                  GURL("https://bidder5.test/loss/"),
                  GURL("https://seller2.test/loss/5"),
                  GURL("https://bidder6.test/loss/"),
                  GURL("https://seller2.test/loss/6"),
                  GURL("https://bidder7.test/loss/"),
                  GURL("https://seller2.test/loss/7"),
                  GURL("https://bidder8.test/loss/"),
                  GURL("https://seller2.test/loss/8"),
                  // kComponentSeller3's bidders. Bidder 9 won the entire
                  // auction, all the others lose component seller 3's auction.
                  GURL("https://bidder10.test/loss/"),
                  GURL("https://seller3.test/loss/10"),
                  GURL("https://bidder11.test/loss/"),
                  GURL("https://seller3.test/loss/11"),
                  GURL("https://bidder12.test/loss/"),
                  GURL("https://seller3.test/loss/12"),
                  GURL("https://bidder13.test/loss/"),
                  GURL("https://seller3.test/loss/13"),
                  GURL("https://bidder14.test/loss/"),
                  GURL("https://seller3.test/loss/14"),
                  GURL("https://bidder15.test/loss/"),
                  GURL("https://seller3.test/loss/15")));
}

// Reject reason returned by scoreAd() for a rejected bid can be reported to the
// bidder through its debug loss report URL.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
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

    RunStandardAuction(/*request_trusted_bidding_signals=*/false);
    EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

    // Bidder 1 won the auction.
    EXPECT_EQ(kBidder1Key, result_.winning_group_id);
    EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_descriptor->url);

    EXPECT_EQ(1u, result_.debug_loss_report_urls.size());
    // Seller rejected bidder 2 and returned the reject reason which were then
    // reported to bidder 2 through its loss report URL.
    EXPECT_THAT(result_.debug_loss_report_urls,
                testing::UnorderedElementsAre(base::StringPrintf(
                    "https://bidder2-debug-loss-reporting.com/"
                    "?rejectReason=%s",
                    reject_reason.c_str())));

    EXPECT_EQ(1u, result_.debug_win_report_urls.size());
    EXPECT_THAT(result_.debug_win_report_urls,
                testing::UnorderedElementsAre(kBidder1DebugWinReportUrl));
  }
}

// Reject reason returned by scoreAd() for a bid whose score is positive is
// ignored and will not be reported to the bidder.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       RejectReasonIgnoredForPositiveBid) {
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  EXPECT_EQ(1u, result_.debug_loss_report_urls.size());
  // Reject reason returned by scoreAd() for bidder 1 should be ignored and
  // reported as "not-available" in debug loss report URL, because the bid gets
  // a positive score thus not rejected by seller.
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre("https://bidder1-debug-loss-reporting.com/"
                                    "?rejectReason=not-available"));

  EXPECT_EQ(1u, result_.debug_win_report_urls.size());
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(kBidder2DebugWinReportUrl));
}

// Only bidders' debug loss report URLs support macro ${rejectReason}.
// Bidders' debug win report URLs and sellers' debug loss/win report URLs does
// not.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       RejectReasonInBidderDebugLossReportOnly) {
  const char kBidder1Script[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://bidder1-debug-loss-reporting.com/?reason=${rejectReason}');
      forDebuggingOnly.reportAdAuctionWin(
          'https://bidder1-debug-win-reporting.com/?reason=${rejectReason}');
      return {
        bid: 1,
        render: interestGroup.ads[0].renderURL
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
        render: interestGroup.ads[0].renderURL
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(kBidder2Key, result_.winning_group_id);
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_descriptor->url);

  // Only bidder's debug loss report supports macro ${rejectReason}.
  EXPECT_EQ(2u, result_.debug_loss_report_urls.size());
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          "https://bidder1-debug-loss-reporting.com/?reason=invalid-bid",
          "https://seller-debug-loss-reporting.com/"
          "?reason=${rejectReason}"));
  EXPECT_EQ(2u, result_.debug_win_report_urls.size());
  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre("https://bidder2-debug-win-reporting.com/"
                                    "?reason=${rejectReason}",
                                    "https://seller-debug-win-reporting.com/"
                                    "?reason=${rejectReason}"));
}

// When scoreAd() does not return a reject reason, report it as "not-available"
// in bidder's loss report URL as default.
TEST_P(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       SellerNotReturningRejectReason) {
  interest_group_buyers_ = {{kBidder1}};

  const char kBidderScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      forDebuggingOnly.reportAdAuctionLoss(
          'https://bidder-debug-loss-reporting.com/?reason=${rejectReason}');
      return {
        bid: 1,
        render: interestGroup.ads[0].renderURL
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

  RunStandardAuction(/*request_trusted_bidding_signals=*/false);
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  EXPECT_EQ(1u, result_.debug_loss_report_urls.size());
  EXPECT_THAT(
      result_.debug_loss_report_urls,
      testing::UnorderedElementsAre("https://bidder-debug-loss-reporting.com/"
                                    "?reason=not-available"));
  EXPECT_EQ(0u, result_.debug_win_report_urls.size());
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
    ::testing::Bool());

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

  RunStandardAuction();
  EXPECT_TRUE(
      private_aggregation_manager_.TakePrivateAggregationRequests().empty());
  EXPECT_TRUE(result_.private_aggregation_event_map.empty());
}

class AuctionRunnerKAnonTest : public AuctionRunnerTest,
                               public ::testing::WithParamInterface<
                                   auction_worklet::mojom::KAnonymityBidMode> {
 public:
  AuctionRunnerKAnonTest()
      : AuctionRunnerTest(
            /*should_enable_private_aggregation=*/true,
            /*should_enable_private_aggregation_fledge_extension=*/true,
            kanon_mode()) {}

  using KAnonMode = auction_worklet::mojom::KAnonymityBidMode;
  KAnonMode kanon_mode() { return GetParam(); }
};

TEST_P(AuctionRunnerKAnonTest, SingleNonKAnon) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      // bidding script tries to bid with ad that is not k-anonymous.
      std::string(R"(
        function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
          privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
              bucket: {baseValue: "bid-reject-reason"},
              value: 0,
            });
          privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
              bucket: {baseValue: "winning-bid"},
              value: 2,
            });
          return {ad: {},
              bid: 1,
              render: "https://ad1.com",
              allowComponentAuction: true};
        })") +
          kReportWinNoUrl);
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
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre(
                  blink::KAnonKeyForAdBid(
                      bidders[0].interest_group,
                      bidders[0].interest_group.ads.value()[0].render_url),
                  blink::KAnonKeyForAdNameReporting(
                      bidders[0].interest_group,
                      bidders[0].interest_group.ads.value()[0])));
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);
  switch (kanon_mode()) {
    case KAnonMode::kNone:
      ASSERT_TRUE(result_.ad_descriptor.has_value());
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kSeller, ElementsAreRequests(
                           kExpectedReportResultPrivateAggregationRequest))));
      break;

    case KAnonMode::kEnforce:
      EXPECT_FALSE(result_.ad_descriptor.has_value());
      EXPECT_THAT(
          result_.errors,
          testing::ElementsAre(
              "https://adplatform.com/offers.js generateBid() bid render URL "
              "'https://ad1.com/' isn't one of the registered creative URLs."));
      CheckMetrics(MetricsExpectations(AuctionResult::kAllBidsRejected)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kBidder1,
              ElementsAreRequests(
                  BuildPrivateAggregationRequest(0, 0),
                  BuildPrivateAggregationRequest(0, 2),
                  kExpectedKAnonFailureGenerateBidPrivateAggregationRequest))));
      break;

    case KAnonMode::kSimulate:
      ASSERT_TRUE(result_.ad_descriptor.has_value());
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckMetrics(MetricsExpectations(AuctionResult::kSuccess)
                       .SetNumInterestGroups(1)
                       .SetNumOwnersAndDistinctOwners(1)
                       .SetNumSellers(1)
                       .SetNumBidderWorklets(1)
                       .SetNumInterestGroupsWithOnlyNonKAnonBid(1));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kSeller, ElementsAreRequests(
                           kExpectedReportResultPrivateAggregationRequest))));
      break;
  }
}

TEST_P(AuctionRunnerKAnonTest, SingleKAnon) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeFilteringBidScript(1) + kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com/")));

  // Authorize the ad.
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_descriptor.has_value());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre(
                  blink::KAnonKeyForAdBid(
                      bidders[0].interest_group,
                      bidders[0].interest_group.ads.value()[0].render_url),
                  blink::KAnonKeyForAdNameReporting(
                      bidders[0].interest_group,
                      bidders[0].interest_group.ads.value()[0])));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon",
      kanon_mode() != KAnonMode::kNone, 1);
  MetricsExpectations expectations(AuctionResult::kSuccess);
  expectations.SetNumInterestGroups(1)
      .SetNumOwnersAndDistinctOwners(1)
      .SetNumSellers(1)
      .SetNumBidderWorklets(1);
  if (kanon_mode() == KAnonMode::kNone) {
    expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1);
  } else {
    expectations.SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
  }
  CheckMetrics(expectations);
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(testing::Pair(
          kSeller, ElementsAreRequests(
                       kExpectedReportResultPrivateAggregationRequest))));
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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);
  AuthorizeKAnonAdComponent(bidders[0].interest_group.ad_components.value()[0],
                            "https://ad1.com/1", bidders[0]);
  AuthorizeKAnonAdComponent(bidders[0].interest_group.ad_components.value()[1],
                            "https://ad1.com/2", bidders[0]);
  AuthorizeKAnonAd(bidders[1].interest_group.ads.value()[0], "https://ad2.com/",
                   bidders[1]);
  AuthorizeKAnonAdComponent(bidders[1].interest_group.ad_components.value()[0],
                            "https://ad2.com/1", bidders[1]);

  std::vector<std::string> ad1_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[0].interest_group, bidders[0].interest_group.ads.value()[0]),
      blink::KAnonKeyForAdComponentBid(
          bidders[0].interest_group.ad_components.value()[0].render_url),
      blink::KAnonKeyForAdComponentBid(
          bidders[0].interest_group.ad_components.value()[1].render_url),
  };

  std::vector<std::string> ad2_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[1].interest_group,
          bidders[1].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[1].interest_group, bidders[1].interest_group.ads.value()[0]),
      blink::KAnonKeyForAdComponentBid(
          bidders[1].interest_group.ad_components.value()[0].render_url),
      blink::KAnonKeyForAdComponentBid(
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
    ASSERT_TRUE(result_.ad_descriptor.has_value());

    GURL expected_seller_report_url;
    std::vector<GURL> expected_report_urls;
    base::flat_set<std::string> expected_k_anon_keys_to_join;
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);

    MetricsExpectations expectations(AuctionResult::kSuccess);
    expectations.SetNumInterestGroups(2)
        .SetNumOwnersAndDistinctOwners(2)
        .SetNumSellers(run_as_component ? 2 : 1)
        .SetNumBidderWorklets(2);

    switch (kanon_mode()) {
      case KAnonMode::kNone:
        // k-anon support is turned off entirely, so ad2 wins, and no other URLs
        // are set.
        EXPECT_THAT(result_.errors, testing::ElementsAre());
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
        EXPECT_THAT(result_.ad_component_descriptors,
                    testing::UnorderedElementsAre(
                        blink::AdDescriptor(GURL("https://ad2.com/1")),
                        blink::AdDescriptor(GURL("https://ad2.com/2"))));
        // Only join for ad2
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());

        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/1,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(2);
        break;

      case KAnonMode::kEnforce:
        // k-anon requirement means ad1 wins, but we also report ad2 as what
        // would have won had it been authorized.
        EXPECT_THAT(result_.errors,
                    testing::ElementsAre(
                        "https://anotheradthing.com/bids.js generateBid() bid "
                        "adComponents URL 'https://ad2.com/2' isn't one of the "
                        "registered creative URLs."));
        EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
        EXPECT_THAT(result_.ad_component_descriptors,
                    testing::UnorderedElementsAre(
                        blink::AdDescriptor(GURL("https://ad1.com/1")),
                        blink::AdDescriptor(GURL("https://ad1.com/2"))));

        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/1");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/1, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/0,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder2,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(
                                0, 0),  // reason not available
                            kExpectedKAnonFailureGenerateBidPrivateAggregationRequest,
                            BuildPrivateAggregationRequest(
                                1, 2)))));  // bid was 1.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder2,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(
                                0, 0),  // reason not available
                            kExpectedKAnonFailureGenerateBidPrivateAggregationRequest,
                            BuildPrivateAggregationRequest(
                                1, 2)))));  // bid was 1.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
            .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
        break;

      case KAnonMode::kSimulate:
        // Winner is ad2.com, disregarding k-anonymity, but we also report that
        // if we did care about it, ad1.com would have won.
        EXPECT_THAT(result_.errors, testing::ElementsAre());
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
        EXPECT_THAT(result_.ad_component_descriptors,
                    testing::UnorderedElementsAre(
                        blink::AdDescriptor(GURL("https://ad2.com/1")),
                        blink::AdDescriptor(GURL("https://ad2.com/2"))));

        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/1,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
            .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
        break;
    }
    CheckMetrics(expectations);

    // Have to spin all message loops to flush any k-anon set join events.
    task_environment()->RunUntilIdle();
    EXPECT_THAT(
        interest_group_manager_->TakeJoinedKAnonSets(),
        testing::UnorderedElementsAreArray(expected_k_anon_keys_to_join));

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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[0].interest_group, bidders[0].interest_group.ads.value()[0]),
  };
  std::vector<std::string> ad2_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[1].interest_group,
          bidders[1].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[1].interest_group, bidders[1].interest_group.ads.value()[0]),
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
    ASSERT_TRUE(result_.ad_descriptor.has_value());
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);

    base::flat_set<std::string> expected_k_anon_keys_to_join;
    GURL expected_seller_report_url;
    std::vector<GURL> expected_report_urls;

    MetricsExpectations expectations(AuctionResult::kSuccess);
    expectations.SetNumInterestGroups(2)
        .SetNumOwnersAndDistinctOwners(2)
        .SetNumSellers(run_as_component ? 2 : 1)
        .SetNumBidderWorklets(2);

    switch (kanon_mode()) {
      case KAnonMode::kNone:
        // k-anon support is turned off entirely, so ad2 wins, and no other URLs
        // are set.
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/1,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(2);
        break;

      case KAnonMode::kEnforce:
        // k-anon requirement means ad1 wins, but we also report ad2 as what
        // would have won had it been authorized.
        EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/1");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/1, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/0,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder2,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(
                                0, 0),  // reason not available
                            kExpectedKAnonFailureGenerateBidPrivateAggregationRequest,
                            BuildPrivateAggregationRequest(
                                1, 2)))));  // bid was 1.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder2,
                        ElementsAreRequests(
                            BuildPrivateAggregationRequest(
                                0, 0),  // reason not available
                            kExpectedKAnonFailureGenerateBidPrivateAggregationRequest,
                            BuildPrivateAggregationRequest(
                                1, 2)))));  // bid was 1.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
            .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
        break;

      case KAnonMode::kSimulate:
        // Winner is ad2.com, disregarding k-anonymity, but we also report that
        // if we did care about it, ad1.com would have won.
        EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
        expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                            ad1_k_anon_keys.end());
        expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                            ad2_k_anon_keys.end());
        expected_seller_report_url = GURL("https://reporting.example.com/2");
        expected_report_urls.push_back(ReportWinUrl(
            /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
            /*highest_scoring_other_bid=*/1,
            /*highest_scoring_other_bid_currency=*/absl::nullopt,
            /*made_highest_scoring_other_bid=*/false));
        {
          auto requests =
              private_aggregation_manager_.TakePrivateAggregationRequests();
          if (!run_as_component) {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          } else {
            EXPECT_THAT(
                requests,
                testing::UnorderedElementsAre(
                    testing::Pair(
                        kSeller,
                        ElementsAreRequests(
                            kExpectedReportResultPrivateAggregationRequest,
                            // extra report for component auction.
                            kExpectedReportResultPrivateAggregationRequest)),
                    testing::Pair(
                        kBidder1,
                        ElementsAreRequests(BuildPrivateAggregationRequest(
                                                0, 0),  // reason not available
                                            BuildPrivateAggregationRequest(
                                                2, 2)))));  // bid was 2.
          }
        }
        expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
            .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
        break;
    }

    CheckMetrics(expectations);

    // Have to spin all message loops to flush any k-anon set join events.
    task_environment()->RunUntilIdle();
    EXPECT_THAT(
        interest_group_manager_->TakeJoinedKAnonSets(),
        testing::UnorderedElementsAreArray(expected_k_anon_keys_to_join));

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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[0].interest_group, bidders[0].interest_group.ads.value()[0]),
  };

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_descriptor.has_value());
  EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(ad1_k_anon_keys));

  std::vector<GURL> expected_report_urls;
  expected_report_urls.emplace_back("https://reporting.example.com/2");
  MetricsExpectations expectations(AuctionResult::kSuccess);
  expectations.SetNumInterestGroups(2)
      .SetNumOwnersAndDistinctOwners(2)
      .SetNumSellers(1)
      .SetNumBidderWorklets(2);

  switch (kanon_mode()) {
    case KAnonMode::kNone:
      // k-anon support is turned off entirely, so no other URLs
      // are set.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);
      expected_report_urls.push_back(ReportWinUrl(
          /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
          /*highest_scoring_other_bid=*/1,
          /*highest_scoring_other_bid_currency=*/absl::nullopt,
          /*made_highest_scoring_other_bid=*/false));
      {
        auto requests =
            private_aggregation_manager_.TakePrivateAggregationRequests();
        EXPECT_THAT(
            requests,
            testing::UnorderedElementsAre(
                testing::Pair(
                    kSeller,
                    ElementsAreRequests(
                        kExpectedReportResultPrivateAggregationRequest)),
                testing::Pair(kBidder2,
                              ElementsAreRequests(
                                  BuildPrivateAggregationRequest(0, 0),
                                  BuildPrivateAggregationRequest(2, 2)))));
      }
      expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(2);
      break;

    case KAnonMode::kEnforce:
      // The enforced winner is the same, but there is no runner-up.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", true, 1);
      expected_report_urls.push_back(ReportWinUrl(
          /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
          /*highest_scoring_other_bid=*/0,
          /*highest_scoring_other_bid_currency=*/absl::nullopt,
          /*made_highest_scoring_other_bid=*/false));
      {
        auto requests =
            private_aggregation_manager_.TakePrivateAggregationRequests();
        EXPECT_THAT(
            requests,
            testing::UnorderedElementsAre(
                testing::Pair(
                    kSeller,
                    ElementsAreRequests(
                        kExpectedReportResultPrivateAggregationRequest)),
                testing::Pair(kBidder2,
                              ElementsAreRequests(
                                  BuildPrivateAggregationRequest(0, 0),
                                  BuildPrivateAggregationRequest(2, 2)))));
      }
      expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
          .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
      break;

    case KAnonMode::kSimulate:
      // ad1.com also wins in the simulated mode.
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", true, 1);
      expected_report_urls.push_back(ReportWinUrl(
          /*bid=*/2, /*bid_currency=*/blink::AdCurrency::From("USD"),
          /*highest_scoring_other_bid=*/1,
          /*highest_scoring_other_bid_currency=*/absl::nullopt,
          /*made_highest_scoring_other_bid=*/false));
      {
        auto requests =
            private_aggregation_manager_.TakePrivateAggregationRequests();
        EXPECT_THAT(
            requests,
            testing::UnorderedElementsAre(
                testing::Pair(
                    kSeller,
                    ElementsAreRequests(
                        kExpectedReportResultPrivateAggregationRequest)),
                testing::Pair(kBidder2,
                              ElementsAreRequests(
                                  BuildPrivateAggregationRequest(0, 0),
                                  BuildPrivateAggregationRequest(2, 2)))));
      }
      expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1)
          .SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(1);
      break;
  }
  CheckMetrics(expectations);
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
                render: interestGroup.ads.pop().renderURL,
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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  std::vector<std::string> ad1_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ads.value()[0].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[0].interest_group, bidders[0].interest_group.ads.value()[0]),
  };
  std::vector<std::string> ad2_k_anon_keys = {
      blink::KAnonKeyForAdBid(
          bidders[0].interest_group,
          bidders[0].interest_group.ads.value()[1].render_url),
      blink::KAnonKeyForAdNameReporting(
          bidders[0].interest_group, bidders[0].interest_group.ads.value()[1]),
  };

  StartAuction(kSellerUrl, bidders);
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  ASSERT_TRUE(result_.ad_descriptor.has_value());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 1);

  base::flat_set<std::string> expected_k_anon_keys_to_join;

  MetricsExpectations expectations(AuctionResult::kSuccess);
  expectations.SetNumInterestGroups(1)
      .SetNumOwnersAndDistinctOwners(1)
      .SetNumSellers(1)
      .SetNumBidderWorklets(1);

  switch (kanon_mode()) {
    case KAnonMode::kNone:
      // Don't care about k-anonymity: ad2 wins, nothing else is reporter.
      EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/2"));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kSeller, ElementsAreRequests(
                           kExpectedReportResultPrivateAggregationRequest))));
      expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1);
      break;

    case KAnonMode::kEnforce:
      // Ad 2 is what got blocked by enforcement --- if it were authorized, it
      // would win.
      EXPECT_EQ(GURL("https://ad1.com"), result_.ad_descriptor->url);
      expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                          ad1_k_anon_keys.end());
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/1"));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kSeller, ElementsAreRequests(
                           kExpectedReportResultPrivateAggregationRequest))));
      expectations.SetNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon(1);
      break;

    case KAnonMode::kSimulate:
      // Winner is ad2.com, disregarding k-anonymity, but we also report that
      // if we did care about it, ad1.com would have won.
      EXPECT_EQ(GURL("https://ad2.com"), result_.ad_descriptor->url);
      expected_k_anon_keys_to_join.insert(ad1_k_anon_keys.begin(),
                                          ad1_k_anon_keys.end());
      expected_k_anon_keys_to_join.insert(ad2_k_anon_keys.begin(),
                                          ad2_k_anon_keys.end());
      EXPECT_THAT(result_.report_urls,
                  testing::ElementsAre("https://reporting.example.com/2"));
      EXPECT_THAT(
          private_aggregation_manager_.TakePrivateAggregationRequests(),
          testing::UnorderedElementsAre(testing::Pair(
              kSeller, ElementsAreRequests(
                           kExpectedReportResultPrivateAggregationRequest))));
      expectations.SetNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon(1);
      break;
  }

  CheckMetrics(expectations);

  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_->TakeJoinedKAnonSets(),
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
                render: interestGroup.ads.pop().renderURL,
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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  // Run the auction, and simulate it being interrupted by navigating away.
  StartAuction(kSellerUrl, bidders);
  task_environment()->RunUntilIdle();
  auction_runner_->FailAuction(/*manually_aborted=*/false);

  EXPECT_THAT(result_.errors, testing::ElementsAre());

  // Should not have anything to report.
  EXPECT_FALSE(result_.ad_descriptor.has_value());
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_->TakeJoinedKAnonSets(),
              testing::ElementsAre());
  histogram_tester_->ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon", false, 0);
  MetricsExpectations expectations(AuctionResult::kAborted);
  expectations.SetNumInterestGroups(2)
      .SetNumOwnersAndDistinctOwners(2)
      .SetNumSellers(1)
      .SetNumBidderWorklets(2);
  if (kanon_mode() == KAnonMode::kNone) {
    expectations.SetNumInterestGroupsWithOnlyNonKAnonBid(1);
  } else {
    expectations.SetNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon(1);
  }
  CheckMetrics(expectations);
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
}

TEST_P(AuctionRunnerKAnonTest, MojoValidation) {
  const struct TestCase {
    std::set<KAnonMode> run_in_modes;
    const char* expected_error_message;
    blink::AdDescriptor ad_descriptor;
    auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_bid;
    bool expect_winner;
  } kTestCases[] = {
      // Sending a k-anon enforced bid when it should just match the
      // non-enforced bid.
      {{KAnonMode::kEnforce, KAnonMode::kSimulate},
       "Received different k-anon bid when unenforced bid already k-anon",
       blink::AdDescriptor(GURL("https://ad1.com")),
       auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
           auction_worklet::mojom::BidderWorkletBid::New(
               "ad", 5.0, /*bid_currency=*/absl::nullopt,
               /*ad_cost=*/absl::nullopt,
               blink::AdDescriptor(GURL("https://ad2.com")),
               /*ad_component_urls=*/absl::nullopt,
               /*modeling_signals=*/absl::nullopt, base::TimeDelta())),
       /*expect_winner=*/true},
      // A non-k-anon bid as k-anon one. Enforced, so auction fails.
      {
          {KAnonMode::kEnforce},
          "Bid render ad must have a valid URL and size (if specified)",
          blink::AdDescriptor(GURL("https://ad2.com")),
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
              auction_worklet::mojom::BidderWorkletBid::New(
                  "ad", 5.0, /*bid_currency=*/absl::nullopt,
                  /*ad_cost=*/absl::nullopt,
                  blink::AdDescriptor(GURL("https://ad2.com")),
                  /*ad_component_urls=*/absl::nullopt,
                  /*modeling_signals=*/absl::nullopt, base::TimeDelta())),
          /*expect_winner=*/false,
      },
      // A non-k-anon bid as k-anon one. Simulate, so auction succeeds.
      {
          {KAnonMode::kSimulate},
          "Bid render ad must have a valid URL and size (if specified)",
          blink::AdDescriptor(GURL("https://ad2.com")),
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBid::NewBid(
              auction_worklet::mojom::BidderWorkletBid::New(
                  "ad", 5.0, /*bid_currency=*/absl::nullopt,
                  /*ad_cost=*/absl::nullopt,
                  blink::AdDescriptor(GURL("https://ad2.com")),
                  /*ad_component_urls=*/absl::nullopt,
                  /*modeling_signals=*/absl::nullopt, base::TimeDelta())),
          /*expect_winner=*/true,
      },
      // Sending k-anon data when it's not even on.
      {
          {KAnonMode::kNone},
          "Received k-anon bid data when not considering k-anon",
          blink::AdDescriptor(GURL("https://ad1.com")),
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
  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0], "https://ad1.com/",
                   bidders[0]);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.expected_error_message);
    if (test_case.run_in_modes.find(kanon_mode()) ==
        test_case.run_in_modes.end()) {
      continue;
    }

    UseMockWorkletService();
    StartAuction(kSellerUrl, bidders);
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/1, /*num_sellers=*/1);
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder1_worklet->InvokeGenerateBidCallback(
        1.0, /*bid_currency=*/absl::nullopt, test_case.ad_descriptor,
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
            /*bid_in_seller_currency=*/absl::nullopt,
            /*scoring_signals_data_version=*/absl::nullopt,
            /*debug_loss_report_url=*/absl::nullopt,
            /*debug_win_report_url=*/absl::nullopt, /*pa_requests=*/{},
            /*scoring_latency=*/base::TimeDelta(),
            /*score_ad_dependency_latencies=*/
            auction_worklet::mojom::ScoreAdDependencyLatencies::New(
                /*code_ready_latency=*/absl::nullopt,
                /*direct_from_seller_signals_latency=*/absl::nullopt,
                /*trusted_scoring_signals_latency=*/absl::nullopt),
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
    EXPECT_EQ(test_case.expect_winner, result_.ad_descriptor.has_value());
  }
}

TEST_P(AuctionRunnerKAnonTest, ReportingId) {
  const char kReportingIdReportWin[] = R"(
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://example.org/?" +
                   browserSignals.interestGroupName + "/" +
                   ("interestGroupName" in browserSignals) + "/" +
                   browserSignals.buyerReportingId + "/" +
                   ("buyerReportingId" in browserSignals) + "/" +
                   browserSignals.buyerAndSellerReportingId + "/" +
                   ("buyerAndSellerReportingId" in browserSignals));
    }
  )";

  const char kReportingIdReportResult[] = R"(
    function reportResult(auctionConfig, browserSignals) {
      sendReportTo("https://seller.example.org/?" +
                   browserSignals.buyerAndSellerReportingId + "/" +
                   ("buyerAndSellerReportingId" in browserSignals));
    }
  )";

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeFilteringBidScript(1) + kReportingIdReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kReportingIdReportResult);

  for (bool authorize_reporting_kanon : {false, true}) {
    SCOPED_TRACE(authorize_reporting_kanon);
    for (auto field_to_test :
         {auction_worklet::mojom::ReportingIdField::kInterestGroupName,
          auction_worklet::mojom::ReportingIdField::kBuyerReportingId,
          auction_worklet::mojom::ReportingIdField::
              kBuyerAndSellerReportingId}) {
      SCOPED_TRACE(field_to_test);
      std::vector<StorageInterestGroup> bidders;
      bidders.emplace_back(MakeInterestGroup(
          kBidder1, kBidder1Name, kBidder1Url,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/{}, GURL("https://ad1.com")));
      if (field_to_test ==
          auction_worklet::mojom::ReportingIdField::kBuyerReportingId) {
        bidders[0].interest_group.ads.value()[0].buyer_reporting_id = "buyid1";
      }
      if (field_to_test == auction_worklet::mojom::ReportingIdField::
                               kBuyerAndSellerReportingId) {
        // buyer-only ID should be ignored.
        bidders[0].interest_group.ads.value()[0].buyer_reporting_id = "buyid1";
        bidders[0].interest_group.ads.value()[0].buyer_and_seller_reporting_id =
            "commonid1";
      }

      AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0],
                       "https://ad1.com/", bidders[0]);
      if (authorize_reporting_kanon) {
        AuthorizeKAnonReporting(bidders[0].interest_group.ads.value()[0],
                                "https://ad1.com/", bidders[0]);
      }

      StartAuction(kSellerUrl, bidders);
      auction_run_loop_->Run();
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      ASSERT_TRUE(result_.ad_descriptor.has_value());
      if (kanon_mode() == KAnonMode::kEnforce && !authorize_reporting_kanon) {
        // In case the k-anon check fails, seller worklet gets nothing, and
        // bidder worklet gets empty group name (regardless of which field is
        // actually set in the ad).
        EXPECT_THAT(result_.report_urls,
                    testing::UnorderedElementsAre(
                        "https://seller.example.org/?undefined/false",
                        "https://example.org/?/true/"
                        "undefined/false/undefined/false"));
      } else {
        switch (field_to_test) {
          case auction_worklet::mojom::ReportingIdField::kInterestGroupName:
            // IG name in bidder, nothing in seller.
            EXPECT_THAT(result_.report_urls,
                        testing::UnorderedElementsAre(
                            "https://seller.example.org/?undefined/false",
                            "https://example.org/?Ad%20Platform/true/"
                            "undefined/false/undefined/false"));
            break;
          case auction_worklet::mojom::ReportingIdField::kBuyerReportingId:
            // Buyer ID in bidder, nothing in seller.
            EXPECT_THAT(result_.report_urls,
                        testing::UnorderedElementsAre(
                            "https://seller.example.org/?undefined/false",
                            "https://example.org/?undefined/false/buyid1/true/"
                            "undefined/false"));
            break;

          case auction_worklet::mojom::ReportingIdField::
              kBuyerAndSellerReportingId:
            // Shared ID in both.
            EXPECT_THAT(
                result_.report_urls,
                testing::UnorderedElementsAre(
                    "https://seller.example.org/?commonid1/true",
                    "https://example.org/?undefined/false/undefined/false/"
                    "commonid1/true"));
        }
      }
    }
  }
}

TEST_P(AuctionRunnerKAnonTest, AdditionalBidBuyerReporting) {
  base::test::ScopedFeatureList additional_bids_on;
  additional_bids_on.InitAndEnableFeature(
      blink::features::kFledgeNegativeTargeting);

  ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());

  const char kAdditionalBidUrl[] =
      "https://adplatform.com/offers-contextual.js";

  const char kReportWin[] = R"(
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://example.org/?" +
                   browserSignals.interestGroupName);
    }

    function reportAdditionalBidWin(auctionSignals, perBuyerSignals,
                                    sellerSignals, browserSignals) {
      sendReportTo("https://contextual.test/?" +
                   browserSignals.interestGroupName);
    }
  )";

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeConstBidScript(1, "https://regular.test") + kReportWin);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         GURL(kAdditionalBidUrl), kReportWin);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      std::string(kMinimumDecisionScript) + kBasicReportResult);

  // We don't authorize anything for reporting k-anon, but do authorize the
  // regular ad to participate.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/{}, GURL("https://regular.test")));

  AuthorizeKAnonAd(bidders[0].interest_group.ads.value()[0],
                   "https://regular.test/", bidders[0]);

  auction_nonce_ =
      static_cast<base::Uuid>(auction_nonce_manager_->CreateAuctionNonce());
  pass_promise_for_additional_bids_ = true;
  StartAuction(kSellerUrl, bidders);

  const char kBidJsonTemplate[] = R"({
    "interestGroup": {
      "name": "additionalPseudoIG",
      "biddingLogicURL": "%s",
      "owner": "%s"
    },
    "bid": {
      "bid": 40,
      "render": "https://additional.test"
    },
    "auctionNonce": "%s",
    "seller": "%s"
  })";

  std::string bid_json = base::StringPrintf(
      kBidJsonTemplate, kAdditionalBidUrl, kBidder1.Serialize().c_str(),
      auction_nonce_->AsLowercaseString().c_str(), kSeller.Serialize().c_str());

  std::map<std::string, std::vector<std::string>> additional_bids_for_nonce;
  additional_bids_for_nonce[auction_nonce_->AsLowercaseString()].push_back(
      GenerateSignedAdditionalBidHeaderPayloadPortion(
          SignedAdditionalBidFault::kNone, bid_json,
          {kPrivateKey1, kPrivateKey2},
          {kBase64PublicKey1, kBase64PublicKey2}));
  ad_auction_page_data_->AddAuctionAdditionalBidsWitnessForOrigin(
      kSeller, additional_bids_for_nonce);

  abortable_ad_auction_->ResolvedAdditionalBids(
      blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0));

  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  "https://reporting.example.com/40",
                  "https://contextual.test/?additionalPseudoIG"));

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AuctionRunnerKAnonTest,
    ::testing::Values(auction_worklet::mojom::KAnonymityBidMode::kNone,
                      auction_worklet::mojom::KAnonymityBidMode::kEnforce,
                      auction_worklet::mojom::KAnonymityBidMode::kSimulate));

// Server response passed in, but promise response called twice.
TEST_F(AuctionRunnerTest, ServerResponseLogsErrors) {
  const base::Uuid bad_request_id = base::Uuid();

  std::string bad_framing_response;
  // CBOR {isChaff: true} | xxd -r -p | gzip | base64 (no framing)
  ASSERT_TRUE(base::Base64Decode("H4sIAAAAAAAAAwMAAAAAAAAAAAA=",
                                 &bad_framing_response));
  std::string not_zipped_response;
  // CBOR {isChaff: true} | xxd -r -p | sed 's/^/02<size>/' | xxd -p -c 0|
  // base64 (no compression)
  ASSERT_TRUE(
      base::Base64Decode("AgAAAA6hZ2lzQ2hhZmb1AAAAAA==", &not_zipped_response));
  std::string not_cbor_response;
  // "{isChaff: true}" | xxd -p | xxd -p -r | gzip | xxd -p -c 0 | sed
  // 's/^/02<size>/' | xxd -r -p | base64
  ASSERT_TRUE(base::Base64Decode(
      "AgAAACQfiwgAAAAAAAADq84sds5ITEuzUigpKk2t5QIAAAgvJxAAAAA=",
      &not_cbor_response));
  std::string missing_fields_response;
  // CBOR {isChaff: false} | xxd -r -p | gzip | xxd -p -c 0 | sed
  // 's/^/02<size>/' | xxd -r -p | base64
  ASSERT_TRUE(
      base::Base64Decode("AgAAAB4fiwgAAAAAAAADW5ieWeyckZiW9gUATA0P6QoAAAA=",
                         &missing_fields_response));
  std::string chaff_response;
  // CBOR {isChaff: true} | xxd -r -p | gzip | xxd -p -c 0 | sed 's/^/02<size>/'
  // | xxd -r -p | base64
  ASSERT_TRUE(base::Base64Decode(
      "AgAAAB4fiwgAAAAAAAADW5ieWeyckZiW9hUA2j0IngoAAAA=", &chaff_response));

  std::string response_with_error;
  // CBOR {isChaff: true, error: { message: "Error on server"}} | xxd -r -p |
  // gzip | xxd -p -c 0 | sed 's/^/02<size>/' | xxd -r -p | base64
  ASSERT_TRUE(
      base::Base64Decode("AgAAADsfiwgAAAAAAAADW5SaWlSUX7QwPTe1uDgxPTXfFcRVyM9TK"
                         "E4tKkstSs8sds5ITEv7CgD8"
                         "/h5bKQAAAA==",
                         &response_with_error));

  const struct {
    std::string test_name;
    std::string response;
    bool witnessed;
    bool use_correct_request_id;
    GURL seller_decision_logic_url;
    bool encrypt;
    std::vector<std::string> errors;
  } kTestCases[] = {
      {"Bad framing",
       bad_framing_response,
       true,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Could not parse response framing"}},
      {"not zipped",
       not_zipped_response,
       true,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Could not decompress server response"}},
      {"not CBOR",
       not_cbor_response,
       true,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Could not parse server response"}},
      {"missing fields",
       missing_fields_response,
       true,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Could not parse server response"}},
      {"not witnessed",
       chaff_response,
       false,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Server response was not witnessed from " +
        kSeller.Serialize()}},
      {"wrong request id",
       chaff_response,
       true,
       false,
       kSellerUrl,
       true,
       {"runAdAuction(): No corresponding request with ID: " +
        bad_request_id.AsLowercaseString()}},
      {"wrong seller",
       chaff_response,
       true,
       true,
       kBidder1Url,  // Not the seller
       true,
       {"runAdAuction(): Seller in response doesn't match request"}},
      {"not encrypted",
       chaff_response,
       true,
       true,
       kSellerUrl,
       false,
       {"runAdAuction(): Could not decrypt server response"}},
      {"error on server",
       response_with_error,
       true,
       true,
       kSellerUrl,
       true,
       {"runAdAuction(): Error on server"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.test_name);

    const base::Uuid request_id = base::Uuid::GenerateRandomV4();
    server_response_request_id_ =
        (test_case.use_correct_request_id) ? request_id : bad_request_id;

    quiche::ObliviousHttpRequest::Context client_context =
        CreateBiddingAndAuctionEncryptionContext();
    std::string encrypted_response;
    if (test_case.encrypt) {
      encrypted_response =
          quiche::ObliviousHttpResponse::CreateServerObliviousResponse(
              test_case.response, client_context)
              ->EncapsulateAndSerialize();
    } else {
      encrypted_response = test_case.response;
    }

    ad_auction_page_data_ = PageUserData<AdAuctionPageData>::GetOrCreateForPage(
        web_contents()->GetPrimaryPage());

    if (test_case.witnessed) {
      std::string witnessed_hash = crypto::SHA256HashString(encrypted_response);
      ad_auction_page_data_->AddAuctionResultWitnessForOrigin(
          url::Origin::Create(test_case.seller_decision_logic_url),
          witnessed_hash);
    }

    AdAuctionRequestContext context(kSeller, {}, std::move(client_context));
    ad_auction_page_data_->RegisterAdAuctionRequestContext(request_id,
                                                           std::move(context));

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
    StartAuction(test_case.seller_decision_logic_url, std::move(bidders));

    abortable_ad_auction_->ResolvedAuctionAdResponsePromise(
        blink::mojom::AuctionAdConfigAuctionId::NewMainAuction(0),
        mojo_base::BigBuffer(
            base::as_bytes(base::make_span(encrypted_response))));

    task_environment()->RunUntilIdle();
    EXPECT_THAT(result_.errors, testing::ElementsAreArray(test_case.errors));
  }

  // Clear this before the page expires to avoid the dangling ptr error.
  ad_auction_page_data_ = nullptr;
}

}  // namespace
}  // namespace content

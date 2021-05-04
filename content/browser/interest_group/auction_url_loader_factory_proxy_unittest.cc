// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// URL for the scoring worklet.
const char kScoringWorkletUrl[] = "https://decision_logic_url.test/foo";

// URLs for bidding worklets and their trusted bidding signals. Third worklet
// has no trusted bidding signals URL.

const char kBiddingWorkletUrl1[] = "https://bidding_url1.test/";
const char kTrustedBiddingSignalsUrl1[] =
    "https://trusted_bidding_signals_url1.test/";

const char kBiddingWorkletUrl2[] = "https://bidding_url2.test/foo";
const char kTrustedBiddingSignalsUrl2[] =
    "https://trusted_bidding_signals_url2.test/bar";

const char kBiddingWorkletUrl3[] = "https://bidding_url3.test/baz?foobar";

// Values for the Accept header.
const char kAcceptJavascript[] = "application/javascript";
const char kAcceptJson[] = "application/json";

class ActionUrlLoaderFactoryProxyTest : public testing::Test {
 public:
  // Ways the proxy can behave in response to a request.
  enum class ExpectedResponse {
    kReject,
    kUseFrameFactory,
    kUseTrustedFactory,
  };

  ActionUrlLoaderFactoryProxyTest() { CreateUrlLoaderFactoryProxy(); }

  ~ActionUrlLoaderFactoryProxyTest() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void CreateUrlLoaderFactoryProxy() {
    // The AuctionURLLoaderFactoryProxy should only be created if there is no
    // old one, or the old one's pipe was closed.
    DCHECK(!remote_url_loader_factory_ ||
           !remote_url_loader_factory_.is_connected());
    blink::mojom::AuctionAdConfigPtr auction_config =
        blink::mojom::AuctionAdConfig::New();
    auction_config->decision_logic_url = GURL(kScoringWorkletUrl);
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;

    bidders.emplace_back(auction_worklet::mojom::BiddingInterestGroup::New());
    bidders.back()->group = blink::mojom::InterestGroup::New();
    bidders.back()->group->bidding_url = GURL(kBiddingWorkletUrl1);
    bidders.back()->group->trusted_bidding_signals_url =
        GURL(kTrustedBiddingSignalsUrl1);

    bidders.emplace_back(auction_worklet::mojom::BiddingInterestGroup::New());
    bidders.back()->group = blink::mojom::InterestGroup::New();
    bidders.back()->group->bidding_url = GURL(kBiddingWorkletUrl2);
    bidders.back()->group->trusted_bidding_signals_url =
        GURL(kTrustedBiddingSignalsUrl2);

    bidders.emplace_back(auction_worklet::mojom::BiddingInterestGroup::New());
    bidders.back()->group = blink::mojom::InterestGroup::New();
    bidders.back()->group->bidding_url = GURL(kBiddingWorkletUrl3);

    remote_url_loader_factory_.reset();
    url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
        remote_url_loader_factory_.BindNewPipeAndPassReceiver(),
        base::BindRepeating(
            [](network::mojom::URLLoaderFactory* factory) { return factory; },
            &proxied_frame_url_loader_factory_),
        base::BindRepeating(
            [](network::mojom::URLLoaderFactory* factory) { return factory; },
            &proxied_trusted_url_loader_factory_),
        frame_origin_, *auction_config, bidders);
  }

  // Attempts to make a request for `request`.
  void TryMakeRequest(const network::ResourceRequest& request,
                      ExpectedResponse expected_response) {
    // Create a new factory if the last test case closed the pipe.
    if (!remote_url_loader_factory_.is_connected())
      CreateUrlLoaderFactoryProxy();

    int initial_num_frame_requests =
        proxied_frame_url_loader_factory_.NumPending();
    int initial_num_trusted_requests =
        proxied_trusted_url_loader_factory_.NumPending();

    // Try to send a request. Requests are never run to completion, instead,
    // requests that make it to the nested `url_loader_factory_` are tracked in
    // its vector of pending requests.
    mojo::PendingRemote<network::mojom::URLLoader> receiver;
    mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
    // Set a couple random options, which should be ignored.
    int options = network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
                  network::mojom::kURLLoadOptionSniffMimeType;
    remote_url_loader_factory_->CreateLoaderAndStart(
        receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */, options,
        request, client.InitWithNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    // Wait until requests have made it through the Mojo pipe. NumPending()
    // actually spinds the message loop already, but seems best to be safe.
    remote_url_loader_factory_.FlushForTesting();

    network::TestURLLoaderFactory::PendingRequest const* pending_request;
    switch (expected_response) {
      case ExpectedResponse::kReject:
        // A request being rejected closes the receiver.
        EXPECT_EQ(initial_num_frame_requests,
                  proxied_frame_url_loader_factory_.NumPending());
        EXPECT_EQ(initial_num_trusted_requests,
                  proxied_trusted_url_loader_factory_.NumPending());
        // Rejecting a request should result in closing the factory mojo pipe.
        EXPECT_FALSE(remote_url_loader_factory_.is_connected());
        return;
      case ExpectedResponse::kUseFrameFactory:
        ASSERT_EQ(initial_num_frame_requests + 1,
                  proxied_frame_url_loader_factory_.NumPending());
        ASSERT_EQ(initial_num_trusted_requests,
                  proxied_trusted_url_loader_factory_.NumPending());
        EXPECT_TRUE(remote_url_loader_factory_.is_connected());
        pending_request =
            &proxied_frame_url_loader_factory_.pending_requests()->back();
        break;
      case ExpectedResponse::kUseTrustedFactory:
        ASSERT_EQ(initial_num_frame_requests,
                  proxied_frame_url_loader_factory_.NumPending());
        ASSERT_EQ(initial_num_trusted_requests + 1,
                  proxied_trusted_url_loader_factory_.NumPending());
        EXPECT_TRUE(remote_url_loader_factory_.is_connected());
        pending_request =
            &proxied_trusted_url_loader_factory_.pending_requests()->back();
        break;
    }

    // These should always be the same for all requests.
    EXPECT_EQ(0u, pending_request->options);
    EXPECT_EQ(
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        pending_request->traffic_annotation);

    // Each request should be assigned a unique ID. These should actually be
    // unique within the browser process, not just among requests using the
    // AuctionURLLoaderFactoryProxy.
    for (const auto& other_pending_request :
         *proxied_frame_url_loader_factory_.pending_requests()) {
      if (&other_pending_request == pending_request)
        continue;
      EXPECT_NE(other_pending_request.request_id, pending_request->request_id);
    }
    for (const auto& other_pending_request :
         *proxied_trusted_url_loader_factory_.pending_requests()) {
      if (&other_pending_request == pending_request)
        continue;
      EXPECT_NE(other_pending_request.request_id, pending_request->request_id);
    }

    const auto& observed_request = pending_request->request;

    // The URL should be unaltered.
    EXPECT_EQ(request.url, observed_request.url);

    // There should be an accept header, and it should be the same as before.
    std::string original_accept_header;
    std::string observed_accept_header;
    EXPECT_TRUE(request.headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                          &original_accept_header));
    EXPECT_TRUE(observed_request.headers.GetHeader(
        net::HttpRequestHeaders::kAccept, &observed_accept_header));
    EXPECT_EQ(original_accept_header, observed_accept_header);

    // The accept header should be the only accept header.
    EXPECT_EQ(1u, observed_request.headers.GetHeaderVector().size());

    // The request should not include credentials and not follow redirects.
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              observed_request.credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError,
              observed_request.redirect_mode);

    // The initiator should be set.
    EXPECT_EQ(frame_origin_, observed_request.request_initiator);

    if (expected_response == ExpectedResponse::kUseFrameFactory) {
      EXPECT_EQ(network::mojom::RequestMode::kCors, observed_request.mode);
      EXPECT_FALSE(observed_request.trusted_params);
    } else {
      EXPECT_EQ(network::mojom::RequestMode::kNoCors, observed_request.mode);
      ASSERT_TRUE(observed_request.trusted_params);
      EXPECT_FALSE(observed_request.trusted_params->disable_secure_dns);
      const auto& observed_isolation_info =
          observed_request.trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                observed_isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(request.url);
      EXPECT_EQ(expected_origin, observed_isolation_info.top_frame_origin());
      EXPECT_EQ(expected_origin, observed_isolation_info.frame_origin());
      EXPECT_TRUE(observed_isolation_info.site_for_cookies().IsNull());
    }
  }

  void TryMakeRequest(const std::string& url,
                      base::Optional<std::string> accept_value,
                      ExpectedResponse expected_response) {
    network::ResourceRequest request;
    request.url = GURL(url);
    if (accept_value) {
      request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                *accept_value);
    }
    TryMakeRequest(request, expected_response);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  url::Origin frame_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  network::TestURLLoaderFactory proxied_frame_url_loader_factory_;
  network::TestURLLoaderFactory proxied_trusted_url_loader_factory_;
  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
};

// Test exact URL matches. Trusted bidding signals URLs should be rejected
// unless that have a valid query string appended.
TEST_F(ActionUrlLoaderFactoryProxyTest, ExactURLMatch) {
  TryMakeRequest(kScoringWorkletUrl, kAcceptJavascript,
                 ExpectedResponse::kUseFrameFactory);
  TryMakeRequest(kScoringWorkletUrl, kAcceptJson, ExpectedResponse::kReject);
  TryMakeRequest(kScoringWorkletUrl, "Unknown/Unknown",
                 ExpectedResponse::kReject);
  TryMakeRequest(kScoringWorkletUrl, base::nullopt, ExpectedResponse::kReject);

  TryMakeRequest(kBiddingWorkletUrl1, kAcceptJavascript,
                 ExpectedResponse::kUseTrustedFactory);
  TryMakeRequest(kBiddingWorkletUrl1, kAcceptJson, ExpectedResponse::kReject);
  TryMakeRequest(kBiddingWorkletUrl1, "Unknown/Unknown",
                 ExpectedResponse::kReject);
  TryMakeRequest(kBiddingWorkletUrl1, base::nullopt, ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl1, kAcceptJavascript,
                 ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl1, kAcceptJson,
                 ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl1, "Unknown/Unknown",
                 ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl1, base::nullopt,
                 ExpectedResponse::kReject);

  TryMakeRequest(kBiddingWorkletUrl2, kAcceptJavascript,
                 ExpectedResponse::kUseTrustedFactory);
  TryMakeRequest(kBiddingWorkletUrl2, kAcceptJson, ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl2, kAcceptJavascript,
                 ExpectedResponse::kReject);
  TryMakeRequest(kTrustedBiddingSignalsUrl2, kAcceptJson,
                 ExpectedResponse::kReject);

  TryMakeRequest(kBiddingWorkletUrl3, kAcceptJavascript,
                 ExpectedResponse::kUseTrustedFactory);
  TryMakeRequest(kBiddingWorkletUrl3, kAcceptJson, ExpectedResponse::kReject);
}

TEST_F(ActionUrlLoaderFactoryProxyTest, QueryStrings) {
  const char* kValidBiddingSignalsQueryStrings[] = {
      "?hostname=foo.test&keys=bar,baz",
      "?hostname=foo.test&keys=bar",
      // Escaped &.
      "?hostname=foo.test&keys=bar%26",
  };

  const char* kInvalidBiddingSignalsQueryStrings[] = {
      "?hostname=bar.test&keys=bar,baz",
      "?hostname=foo.test&keys=bar&hats=foo",
      "?hostname=foo.test&keys=bar&hats",
      "?hostname=foo.test",
      "?hostname=foo.test&keys",
      "?bar=foo.test&keys=bar,baz",
      "?keys=bar,baz",
      "?",
  };

  for (std::string query_string : kValidBiddingSignalsQueryStrings) {
    SCOPED_TRACE(query_string);
    TryMakeRequest(kScoringWorkletUrl + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);

    TryMakeRequest(kBiddingWorkletUrl1 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedBiddingSignalsUrl1 + query_string, kAcceptJson,
                   ExpectedResponse::kUseTrustedFactory);

    TryMakeRequest(kBiddingWorkletUrl2 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedBiddingSignalsUrl2 + query_string, kAcceptJson,
                   ExpectedResponse::kUseTrustedFactory);

    TryMakeRequest(kBiddingWorkletUrl3 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
  }

  for (std::string query_string : kInvalidBiddingSignalsQueryStrings) {
    SCOPED_TRACE(query_string);
    TryMakeRequest(kScoringWorkletUrl + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);

    TryMakeRequest(kBiddingWorkletUrl1 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedBiddingSignalsUrl1 + query_string, kAcceptJson,
                   ExpectedResponse::kReject);

    TryMakeRequest(kBiddingWorkletUrl2 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedBiddingSignalsUrl2 + query_string, kAcceptJson,
                   ExpectedResponse::kReject);

    TryMakeRequest(kBiddingWorkletUrl3 + query_string, kAcceptJavascript,
                   ExpectedResponse::kReject);
  }
}

// Set a number of extra parameters on the ResourceRequest, which the
// AuctionURLLoaderFactoryProxy should ignore. This test relies heavily on the
// validity checks in TryMakeRequest().
TEST_F(ActionUrlLoaderFactoryProxyTest, ExtraParametersIgnored) {
  network::ResourceRequest request;

  request.credentials_mode = network::mojom::CredentialsMode::kInclude;
  request.trusted_params = network::ResourceRequest::TrustedParams();

  url::Origin other_origin =
      url::Origin::Create(GURL("https://somewhere.else.text"));
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, other_origin, other_origin,
      net::SiteForCookies::FromOrigin(other_origin));
  request.trusted_params->disable_secure_dns = true;

  request.headers.SetHeader("Flux-Capacitor", "Y");
  request.headers.SetHeader("Host", "Fred");
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            kAcceptJavascript);

  request.url = GURL(kScoringWorkletUrl);
  TryMakeRequest(request, ExpectedResponse::kUseFrameFactory);

  request.url = GURL(kBiddingWorkletUrl1);
  TryMakeRequest(request, ExpectedResponse::kUseTrustedFactory);

  request.url = GURL(std::string(kTrustedBiddingSignalsUrl1) +
                     "?hostname=foo.test&keys=bar");
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept, kAcceptJson);
  TryMakeRequest(request, ExpectedResponse::kUseTrustedFactory);
}

// If a bidder URL matches the scoring URL, the publisher frame's
// URLLoaderFactory should be used instead of the trusted one.
TEST_F(ActionUrlLoaderFactoryProxyTest, BidderUrlMatchesScoringUrl) {
  blink::mojom::AuctionAdConfigPtr auction_config =
      blink::mojom::AuctionAdConfig::New();
  auction_config->decision_logic_url = GURL(kScoringWorkletUrl);
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;

  bidders.emplace_back(auction_worklet::mojom::BiddingInterestGroup::New());
  bidders.back()->group = blink::mojom::InterestGroup::New();
  bidders.back()->group->bidding_url = GURL(kScoringWorkletUrl);

  remote_url_loader_factory_.reset();
  url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      remote_url_loader_factory_.BindNewPipeAndPassReceiver(),
      base::BindRepeating(
          [](network::mojom::URLLoaderFactory* factory) { return factory; },
          &proxied_frame_url_loader_factory_),
      base::BindRepeating(
          [](network::mojom::URLLoaderFactory* factory) { return factory; },
          &proxied_trusted_url_loader_factory_),
      frame_origin_, *auction_config, bidders);

  // Make request twice, as will actually happen in this case.
  TryMakeRequest(kScoringWorkletUrl, kAcceptJavascript,
                 ExpectedResponse::kUseFrameFactory);
  TryMakeRequest(kScoringWorkletUrl, kAcceptJavascript,
                 ExpectedResponse::kUseFrameFactory);
}

}  // namespace content

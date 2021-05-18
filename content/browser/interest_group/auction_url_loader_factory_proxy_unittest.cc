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
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

const char kScriptUrl[] = "https://host.test/script";
const char kTrustedSignalsUrl[] = "https://host.test/trusted_signals";

// Values for the Accept header.
const char kAcceptJavascript[] = "application/javascript";
const char kAcceptJson[] = "application/json";
const char kAcceptOther[] = "binary/ocelot-stream";

class ActionUrlLoaderFactoryProxyTest : public testing::Test {
 public:
  // Ways the proxy can behave in response to a request.
  enum class ExpectedResponse {
    kReject,
    kAllow,
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

    remote_url_loader_factory_.reset();
    url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
        remote_url_loader_factory_.BindNewPipeAndPassReceiver(),
        base::BindRepeating(
            [](network::mojom::URLLoaderFactory* factory) { return factory; },
            &proxied_url_loader_factory_),
        frame_origin_, use_cors_, GURL(kScriptUrl), trusted_signals_url_);
  }

  // Attempts to make a request for `request`.
  void TryMakeRequest(const network::ResourceRequest& request,
                      ExpectedResponse expected_response) {
    // Create a new factory if the last test case closed the pipe.
    if (!remote_url_loader_factory_.is_connected())
      CreateUrlLoaderFactoryProxy();

    int initial_num_requests = proxied_url_loader_factory_.NumPending();

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
        EXPECT_EQ(initial_num_requests,
                  proxied_url_loader_factory_.NumPending());
        // Rejecting a request should result in closing the factory mojo pipe.
        EXPECT_FALSE(remote_url_loader_factory_.is_connected());
        return;
      case ExpectedResponse::kAllow:
        EXPECT_EQ(initial_num_requests + 1,
                  proxied_url_loader_factory_.NumPending());
        EXPECT_TRUE(remote_url_loader_factory_.is_connected());
        pending_request =
            &proxied_url_loader_factory_.pending_requests()->back();
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
         *proxied_url_loader_factory_.pending_requests()) {
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

    if (use_cors_) {
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
                      absl::optional<std::string> accept_value,
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

  bool use_cors_ = false;
  absl::optional<GURL> trusted_signals_url_ = GURL(kTrustedSignalsUrl);

  url::Origin frame_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  network::TestURLLoaderFactory proxied_url_loader_factory_;
  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
};

TEST_F(ActionUrlLoaderFactoryProxyTest, Basic) {
  for (bool use_cors : {false, true}) {
    use_cors_ = use_cors;
    // Force creation of a new proxy, with correct CORS value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, absl::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, absl::nullopt,
                   ExpectedResponse::kReject);

    TryMakeRequest("https://host.test/", kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", absl::nullopt,
                   ExpectedResponse::kReject);
  }
}

TEST_F(ActionUrlLoaderFactoryProxyTest, NoTrustedSignalsUrl) {
  trusted_signals_url_ = absl::nullopt;

  for (bool use_cors : {false, true}) {
    use_cors_ = use_cors;
    // Force creation of a new proxy, with correct CORS value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, absl::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, absl::nullopt,
                   ExpectedResponse::kReject);

    TryMakeRequest("https://host.test/", kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", absl::nullopt,
                   ExpectedResponse::kReject);
  }
}

TEST_F(ActionUrlLoaderFactoryProxyTest, SameUrl) {
  trusted_signals_url_ = GURL(kScriptUrl);

  for (bool use_cors : {false, true}) {
    use_cors_ = use_cors;
    // Force creation of a new proxy, with correct CORS value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, absl::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, absl::nullopt,
                   ExpectedResponse::kReject);

    TryMakeRequest("https://host.test/", kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", absl::nullopt,
                   ExpectedResponse::kReject);
  }
}

}  // namespace content

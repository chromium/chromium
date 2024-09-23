// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"

#include <stdint.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using WorkletHandle = AuctionWorkletManager::WorkletHandle;
using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

const char kScriptUrl[] = "https://host.test/script";
const char kWasmUrl[] = "https://host.test/wasm";
const char kTrustedSignalsBaseUrl[] = "https://host.test/trusted_signals";
// Basic example of a trusted signals URL. Seller signals typically have URLs as
// keys, but AuctionUrlLoaderProxy doesn't currently verify that.
const char kTrustedSignalsUrl[] =
    "https://host.test/trusted_signals?hostname=top.test&keys=jabberwocky";

const char kAdAuctionTrustedSignalsContentType[] =
    "message/ad-auction-trusted-signals-request";

// Values for the Accept header.
const char kAcceptJavascript[] = "application/javascript";
const char kAcceptJson[] = "application/json";
const char kAcceptAdAuctionTrustedSignals[] =
    "message/ad-auction-trusted-signals-response";
const char kAcceptOther[] = "binary/ocelot-stream";
const char kAcceptWasm[] = "application/wasm";

const char kBundleUrl[] = "https://host.test/bundle";

const int kRenderProcessId = 123;

// The AuctionUrlLoaerFactoryProxy doesn't care which URL is used; its users are
// responsible for setting the correct URLs for the correct proxy.
const char kSubresourceUrl1[] = "https://host.test/signals?fakeSuffix1";
const char kSubresourceUrl2[] = "https://host.test/signals?fakeSuffix2";
const char kSubresourceUrl3[] = "https://host.test/signals?fakeSuffix3";

// We don't need to actually make real WorkletHandles --
// SubresourceUrlAuthorizations only cares about the addresses of
// WorkletHandles, so we can use hard-coded pointers.
const WorkletHandle* const kWorkletHandle1 =
    reinterpret_cast<const WorkletHandle*>(0xABC123);

const WorkletHandle* const kWorkletHandle2 =
    reinterpret_cast<const WorkletHandle*>(0xDEF456);

BundleSubresourceInfo MakeBundleSubresourceInfo(
    const std::string& bundle_url,
    const std::string& subresource_url) {
  blink::DirectFromSellerSignalsSubresource info_from_renderer;
  info_from_renderer.bundle_url = GURL(bundle_url);
  return BundleSubresourceInfo(GURL(subresource_url), info_from_renderer);
}

}  // namespace

class AuctionUrlLoaderFactoryProxyTest : public testing::TestWithParam<bool> {
 public:
  // Ways the proxy can behave in response to a request.
  enum class ExpectedResponse {
    kReject,
    kAllow,
  };

  AuctionUrlLoaderFactoryProxyTest() {
    if (PermitCrossOriginTrustedSignals()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kFledgePermitCrossOriginTrustedSignals);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kFledgePermitCrossOriginTrustedSignals);
    }

    // Other defaults are all reasonable, but this should always be true for
    // FLEDGE.
    client_security_state_->is_web_secure_context = true;
  }

  ~AuctionUrlLoaderFactoryProxyTest() override {
    // This should be both set and cleared during CreateUrlLoaderFactoryProxy().
    // Check that the callback passed to AuctionURLLoaderFactoryProxy's wasn't
    // invoked asynchronously unexpectedly.
    EXPECT_FALSE(preconnect_url_);
  }

  bool PermitCrossOriginTrustedSignals() const { return GetParam(); }

  void CreateUrlLoaderFactoryProxy() {
    // The AuctionURLLoaderFactoryProxy should only be created if there is no
    // old one, or the old one's pipe was closed.
    DCHECK(!remote_url_loader_factory_ ||
           !remote_url_loader_factory_.is_connected());

    // `preconnect_url_` should only be set by callback (sometimes) invoked by
    // AuctionURLLoaderFactoryProxy's constructor, and cleared at the end of
    // CreateUrlLoaderFactoryProxy(). Check that it's nullopt here to catch it
    // being set unexpectedly.
    EXPECT_TRUE(!preconnect_url_);

    remote_url_loader_factory_.reset();
    url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
        remote_url_loader_factory_.BindNewPipeAndPassReceiver(),
        base::BindRepeating(
            [](network::mojom::URLLoaderFactory* factory) { return factory; },
            &frame_url_loader_factory_),
        base::BindRepeating(
            [](network::mojom::URLLoaderFactory* factory) { return factory; },
            &trusted_url_loader_factory_),
        base::BindOnce(&AuctionUrlLoaderFactoryProxyTest::PreconnectSocket,
                       base::Unretained(this)),
        base::BindRepeating(
            []() -> std::optional<std::string> { return std::nullopt; }),
        base::BindRepeating([]() -> std::vector<std::string> { return {}; }),
        /*force_reload=*/force_reload_, top_frame_origin_, frame_origin_,
        /*renderer_process_id=*/kRenderProcessId, is_for_seller_,
        client_security_state_.Clone(), GURL(kScriptUrl), wasm_url_,
        trusted_signals_base_url_, needs_cors_for_additional_bid_,
        /*frame_tree_node_id=*/FrameTreeNodeId());

    EXPECT_EQ(preconnect_url_, trusted_signals_base_url_);
    if (trusted_signals_base_url_) {
      if (is_for_seller_) {
        EXPECT_TRUE(preconnect_network_anonymization_key_->IsTransient());
      } else {
        net::SchemefulSite buyer_site{GURL(kScriptUrl)};
        EXPECT_EQ(preconnect_network_anonymization_key_,
                  net::NetworkAnonymizationKey::CreateSameSite(buyer_site));
      }
    } else {
      // This check is not strictly needed, since `preconnect_url_` being equal
      // to `trusted_signals_base_url_`, as checked above, implies this must
      // also be nullopt if `trusted_signals_base_url_` is nullopt.
      EXPECT_FALSE(preconnect_network_anonymization_key_);
    }
    preconnect_url_ = std::nullopt;
  }

  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) {
    EXPECT_TRUE(!preconnect_url_);

    preconnect_url_ = url;
    preconnect_network_anonymization_key_ = network_anonymization_key;
  }

  // Attempts to make a request for `request`.
  //
  // `expected_isolation_info_origin` is the expected IsolationInfo's top-frame
  // and frame origins if the ResourceRequest is expected to include
  // TrustedParams.
  void TryMakeRequest(const network::ResourceRequest& request,
                      ExpectedResponse expected_response,
                      bool expect_bundle_request = false,
                      std::optional<url::Origin>
                          expected_isolation_info_origin = std::nullopt) {
    SCOPED_TRACE(is_for_seller_);
    SCOPED_TRACE(request.url);

    if (!expected_isolation_info_origin) {
      expected_isolation_info_origin = url::Origin::Create(request.url);
    }

    // Create a new factory if one has not been created yet, or the last test
    // case closed the pipe.
    if (!remote_url_loader_factory_ ||
        !remote_url_loader_factory_.is_connected()) {
      CreateUrlLoaderFactoryProxy();
    }

    size_t initial_num_frame_factory_requests =
        frame_url_loader_factory_.pending_requests()->size();
    size_t initial_num_trusted_factory_requests =
        trusted_url_loader_factory_.pending_requests()->size();

    // Try to send a request. Requests are never run to completion, instead,
    // requests that make it to the nested URLLoaderFactories are tracked in
    // their vectors of pending requests.
    mojo::PendingRemote<network::mojom::URLLoader> receiver;
    mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
    // Set a couple random options, which should be ignored.
    int options = network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
                  network::mojom::kURLLoadOptionSniffMimeType;
    remote_url_loader_factory_->CreateLoaderAndStart(
        receiver.InitWithNewPipeAndPassReceiver(), /*request_id=*/0, options,
        request, client.InitWithNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    // Wait until requests have made it through the Mojo pipe. NumPending()
    // actually spinds the message loop already, but seems best to be safe.
    remote_url_loader_factory_.FlushForTesting();

    bool trusted_factory_used = false;

    network::TestURLLoaderFactory::PendingRequest const* pending_request;
    switch (expected_response) {
      case ExpectedResponse::kReject:
        EXPECT_EQ(initial_num_frame_factory_requests,
                  frame_url_loader_factory_.pending_requests()->size());
        EXPECT_EQ(initial_num_trusted_factory_requests,
                  trusted_url_loader_factory_.pending_requests()->size());
        // Rejecting a request should result in closing the factory mojo pipe.
        EXPECT_FALSE(remote_url_loader_factory_.is_connected());
        return;
      case ExpectedResponse::kAllow:
        // There should either be a single new request in either the frame
        // factory or the trusted factory, but not both.
        if (frame_url_loader_factory_.pending_requests()->size() ==
            initial_num_frame_factory_requests + 1) {
          EXPECT_EQ(initial_num_trusted_factory_requests,
                    trusted_url_loader_factory_.pending_requests()->size());
          pending_request =
              &frame_url_loader_factory_.pending_requests()->back();
          trusted_factory_used = false;
        } else if (trusted_url_loader_factory_.pending_requests()->size() ==
                   initial_num_trusted_factory_requests + 1) {
          EXPECT_EQ(initial_num_frame_factory_requests,
                    frame_url_loader_factory_.pending_requests()->size());
          pending_request =
              &trusted_url_loader_factory_.pending_requests()->back();
          trusted_factory_used = true;
        } else {
          ADD_FAILURE() << "No new request found.";
          return;
        }

        // Pipe should still be open.
        EXPECT_TRUE(remote_url_loader_factory_.is_connected());
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
         *frame_url_loader_factory_.pending_requests()) {
      if (&other_pending_request == pending_request) {
        continue;
      }
      EXPECT_NE(other_pending_request.request_id, pending_request->request_id);
    }
    for (const auto& other_pending_request :
         *trusted_url_loader_factory_.pending_requests()) {
      if (&other_pending_request == pending_request) {
        continue;
      }
      EXPECT_NE(other_pending_request.request_id, pending_request->request_id);
    }

    const auto& observed_request = pending_request->request;

    if (expect_bundle_request) {
      ASSERT_TRUE(observed_request.web_bundle_token_params);
      EXPECT_EQ(observed_request.web_bundle_token_params->bundle_url,
                GURL(kBundleUrl));
      EXPECT_EQ(observed_request.web_bundle_token_params->render_process_id,
                kRenderProcessId);
    } else {
      EXPECT_FALSE(observed_request.web_bundle_token_params);
    }

    // The URL should be unaltered.
    EXPECT_EQ(request.url, observed_request.url);

    // There should be an accept header, and it should be the same as before.
    std::optional<std::string> original_accept_header =
        request.headers.GetHeader(net::HttpRequestHeaders::kAccept);
    ASSERT_TRUE(original_accept_header.has_value());
    EXPECT_EQ(original_accept_header, observed_request.headers.GetHeader(
                                          net::HttpRequestHeaders::kAccept));

    // The accept header should be the only accept header for GET requests.
    if (observed_request.method == net::HttpRequestHeaders::kGetMethod) {
      EXPECT_EQ(1u, observed_request.headers.GetHeaderVector().size());
    }

    // The request should not include credentials and not follow redirects.
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              observed_request.credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError,
              observed_request.redirect_mode);

    // Should bypass cache when in force-reload mode.
    EXPECT_EQ(force_reload_ ? net::LOAD_BYPASS_CACHE : 0,
              observed_request.load_flags);

    // Check method, body and content-type for POST requests.
    if (request.method == net::HttpRequestHeaders::kPostMethod) {
      EXPECT_EQ(observed_request.method, net::HttpRequestHeaders::kPostMethod);
      EXPECT_EQ(request.request_body, observed_request.request_body);
      if (request.headers.GetHeader(net::HttpRequestHeaders::kContentType)
              .has_value()) {
        EXPECT_EQ(
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType),
            observed_request.headers.GetHeader(
                net::HttpRequestHeaders::kContentType));
      }
    }

    bool cross_site_enabled_trusted_signals_request =
        PermitCrossOriginTrustedSignals() && !expect_bundle_request &&
        (*original_accept_header == kAcceptJson ||
         *original_accept_header == kAcceptAdAuctionTrustedSignals);

    // The initiator should be set.
    if (cross_site_enabled_trusted_signals_request) {
      EXPECT_EQ(url::Origin::Create(GURL(kScriptUrl)),
                observed_request.request_initiator);
    } else {
      EXPECT_EQ(frame_origin_, observed_request.request_initiator);
    }

    if (expect_bundle_request || needs_cors_for_additional_bid_ ||
        cross_site_enabled_trusted_signals_request) {
      EXPECT_EQ(network::mojom::RequestMode::kCors, observed_request.mode);
    } else {
      EXPECT_EQ(network::mojom::RequestMode::kNoCors, observed_request.mode);
    }

    if (is_for_seller_ || needs_cors_for_additional_bid_) {
      if (original_accept_header == kAcceptJavascript ||
          original_accept_header == kAcceptWasm ||
          needs_cors_for_additional_bid_) {
        // Seller worklet Javascript & WASM requests use the renderer's
        // untrusted URLLoaderFactory, so inherit security parameters from
        // there. The same happens for CORS-requiring additional bid reporting.
        EXPECT_FALSE(trusted_factory_used);
        EXPECT_FALSE(observed_request.trusted_params);
      } else {
        // Seller worklet trusted bidding signals currently use transient
        // IsolationInfos. DirectFromSellerSignals uses the frame's isolation
        // info, but with a trusted URLLoaderFactory (to override the
        // X-FLEDGE-Auction-Only request block).
        EXPECT_TRUE(trusted_factory_used);
        ASSERT_TRUE(observed_request.trusted_params);
        if (expect_bundle_request) {
          const auto& observed_isolation_info =
              observed_request.trusted_params->isolation_info;
          EXPECT_EQ(expected_isolation_info_origin,
                    observed_isolation_info.top_frame_origin());
          EXPECT_EQ(expected_isolation_info_origin,
                    observed_isolation_info.frame_origin());
          EXPECT_TRUE(observed_isolation_info.site_for_cookies().IsNull());
        } else {
          EXPECT_TRUE(observed_request.trusted_params->isolation_info
                          .network_isolation_key()
                          .IsTransient());
          // There should have been a preconnect in this case, with a
          // NetworkAnonymizationKey that's consistent with the request's
          // IsolationInfo.
          EXPECT_EQ(preconnect_network_anonymization_key_,
                    observed_request.trusted_params->isolation_info
                        .network_anonymization_key());
        }
        EXPECT_EQ(*client_security_state_,
                  *observed_request.trusted_params->client_security_state);
      }
    } else {
      // Bidder worklet requests use a trusted URLLoaderFactory, so need to set
      // their own security-related parameters.

      EXPECT_TRUE(trusted_factory_used);

      ASSERT_TRUE(observed_request.trusted_params);
      EXPECT_FALSE(observed_request.trusted_params->disable_secure_dns);

      const auto& observed_isolation_info =
          observed_request.trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                observed_isolation_info.request_type());
      EXPECT_EQ(expected_isolation_info_origin,
                observed_isolation_info.top_frame_origin());
      EXPECT_EQ(expected_isolation_info_origin,
                observed_isolation_info.frame_origin());
      EXPECT_TRUE(observed_isolation_info.site_for_cookies().IsNull());

      ASSERT_TRUE(observed_request.trusted_params->client_security_state);
      EXPECT_EQ(*client_security_state_,
                *observed_request.trusted_params->client_security_state);
    }
  }

  void TryMakeRequest(const GURL& url,
                      std::optional<std::string> accept_value,
                      ExpectedResponse expected_response,
                      bool expect_bundle_request = false,
                      std::optional<url::Origin>
                          expected_isolation_info_origin = std::nullopt) {
    SCOPED_TRACE(accept_value ? *accept_value : "No accept value");

    network::ResourceRequest request;
    request.url = url;
    if (accept_value) {
      request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                *accept_value);
    }
    TryMakeRequest(request, expected_response, expect_bundle_request,
                   expected_isolation_info_origin);
  }

  void TryMakeRequest(const std::string& url,
                      std::optional<std::string> accept_value,
                      ExpectedResponse expected_response,
                      bool expect_bundle_request = false,
                      std::optional<url::Origin>
                          expected_isolation_info_origin = std::nullopt) {
    TryMakeRequest(GURL(url), accept_value, expected_response,
                   expect_bundle_request, expected_isolation_info_origin);
  }

  void AuthorizeSubresourceUrls(
      const AuctionWorkletManager::WorkletHandle* worklet_handle,
      const std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>&
          authorized_subresource_urls) {
    url_loader_factory_proxy_->subresource_url_authorizations()
        .AuthorizeSubresourceUrls(worklet_handle, authorized_subresource_urls);
  }

  void OnWorkletHandleDestruction(
      const AuctionWorkletManager::WorkletHandle* worklet_handle) {
    url_loader_factory_proxy_->subresource_url_authorizations()
        .OnWorkletHandleDestruction(worklet_handle);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  bool is_for_seller_ = false;
  bool force_reload_ = false;
  const network::mojom::ClientSecurityStatePtr client_security_state_ =
      network::mojom::ClientSecurityState::New();
  std::optional<GURL> trusted_signals_base_url_ = GURL(kTrustedSignalsBaseUrl);
  std::optional<GURL> wasm_url_ = GURL(kWasmUrl);
  bool needs_cors_for_additional_bid_ = false;

  url::Origin top_frame_origin_ =
      url::Origin::Create(GURL("https://top.test/"));
  url::Origin frame_origin_ = url::Origin::Create(GURL("https://foo.test/"));

  std::optional<GURL> preconnect_url_;
  std::optional<net::NetworkAnonymizationKey>
      preconnect_network_anonymization_key_;

  network::TestURLLoaderFactory frame_url_loader_factory_;
  network::TestURLLoaderFactory trusted_url_loader_factory_;
  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
};

TEST_P(AuctionUrlLoaderFactoryProxyTest, Basic) {
  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptWasm, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, std::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptWasm, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, std::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kWasmUrl, kAcceptJavascript, ExpectedResponse::kReject);
    TryMakeRequest(kWasmUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kWasmUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kWasmUrl, kAcceptWasm, ExpectedResponse::kAllow);
    TryMakeRequest(kWasmUrl, std::nullopt, ExpectedResponse::kReject);

    // Invalid GURLs are considered matches for the trusted signals URL, when
    // there is one, since appending to the GURL can result in it exceeding the
    // max URL size. Passing a GURL that long across Mojo results in an invalid
    // GURL on the receiving side.
    //
    // The proxy then passes the invalid GURL the network service, which will
    // fail the request. This is how things work for fetch requests for long
    // GURLs as well.
    TryMakeRequest(GURL(), kAcceptJavascript, ExpectedResponse::kReject);
    TryMakeRequest(GURL(), kAcceptJson, ExpectedResponse::kAllow,
                   /*expect_bundle_request=*/false,
                   /*expected_isolation_info_origin=*/
                   url::Origin::Create(GURL(kScriptUrl)));
    TryMakeRequest(GURL(), kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(GURL(), std::nullopt, ExpectedResponse::kReject);

    TryMakeRequest("https://host.test/", kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", kAcceptWasm,
                   ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/", std::nullopt,
                   ExpectedResponse::kReject);
  }
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, ForceReload) {
  force_reload_ = true;
  // Force creation of a new proxy, with correct `force_reload` value.
  remote_url_loader_factory_.reset();
  CreateUrlLoaderFactoryProxy();

  TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, NoWasmUrl) {
  wasm_url_ = std::nullopt;
  CreateUrlLoaderFactoryProxy();
  TryMakeRequest(kWasmUrl, kAcceptJavascript, ExpectedResponse::kReject);
  TryMakeRequest(kWasmUrl, kAcceptJson, ExpectedResponse::kReject);
  TryMakeRequest(kWasmUrl, kAcceptOther, ExpectedResponse::kReject);
  TryMakeRequest(kWasmUrl, kAcceptWasm, ExpectedResponse::kReject);
  TryMakeRequest(kWasmUrl, std::nullopt, ExpectedResponse::kReject);
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, NoTrustedSignalsUrl) {
  trusted_signals_base_url_ = std::nullopt;

  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, std::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, std::nullopt, ExpectedResponse::kReject);

    // Invalid GURLs should be rejected without a `trusted_signals_base_url`,
    // and no matching invalid GURL.
    TryMakeRequest(GURL(), kAcceptJavascript, ExpectedResponse::kReject);
    TryMakeRequest(GURL(), kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(GURL(), kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(GURL(), std::nullopt, ExpectedResponse::kReject);

    TryMakeRequest(top_frame_origin_.GetURL().spec(), kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(top_frame_origin_.GetURL().spec(), kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest(top_frame_origin_.GetURL().spec(), kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest(top_frame_origin_.GetURL().spec(), std::nullopt,
                   ExpectedResponse::kReject);

    TryMakeRequest(frame_origin_.GetURL().spec(), kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(frame_origin_.GetURL().spec(), kAcceptJson,
                   ExpectedResponse::kReject);
    TryMakeRequest(frame_origin_.GetURL().spec(), kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest(frame_origin_.GetURL().spec(), std::nullopt,
                   ExpectedResponse::kReject);
  }
}

// This test focuses on validation of the requested trusted signals URLs.
TEST_P(AuctionUrlLoaderFactoryProxyTest, TrustedSignalsUrl) {
  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(
        "https://host.test/trusted_signals?hostname=top.test&keys=jabberwocky",
        kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?hostname=top.test&keys=jabberwocky,wakkawakka",
        kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?hostname=top.test"
        "&renderUrls=https%3A%2F%2Furl.test%2F,https%3A%2F%2Furl2.test%2F",
        kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?hostname=top.test"
        "&componentAdRenderUrls=https%3A%2F%2Furl3.test%2F,https%3A%2F%2Furl4."
        "test%2F",
        kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?hostname=top.test"
        "&renderUrls=https%3A%2F%2Furl.test%2F,https%3A%2F%2Furl2.test%2F"
        "&componentAdRenderUrls=https%3A%2F%2Furl3.test%2F,https%3A%2F%2Furl4."
        "test%2F",
        kAcceptJson, ExpectedResponse::kAllow);

    // No query parameters.
    TryMakeRequest("https://host.test/trusted_signals", kAcceptJson,
                   ExpectedResponse::kReject);

    // Extra characters before query parameters.
    TryMakeRequest(
        "https://host.test/trusted_signals/foo"
        "?hostname=top.test&keys=jabberwocky,wakkawakka",
        kAcceptJson, ExpectedResponse::kReject);

    // Wrong hostname / No hostname.
    TryMakeRequest(
        "https://host.test/trusted_signals?hostname=foo.test&keys=jabberwocky",
        kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(
        "https://host.test/trusted_signals?hostname=host.test&keys=jabberwocky",
        kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest("https://host.test/trusted_signals?keys=jabberwocky",
                   kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?renderUrls=https%3A%2F%2Furl.test%2F",
        kAcceptJson, ExpectedResponse::kReject);

    // Wrong order (should technically be ok, but it's not what the worklet
    // processes actually request, so no need to accept it).
    TryMakeRequest(
        "https://host.test/trusted_signals?keys=jabberwocky&hostname=top.test",
        kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?renderUrls=https%3A%2F%2Furl.test%2F&hostname=top."
        "test",
        kAcceptJson, ExpectedResponse::kReject);

    // No other parameters.
    TryMakeRequest("https://host.test/trusted_signals?hostname=top.test",
                   kAcceptJson, ExpectedResponse::kReject);

    // Fragments.
    TryMakeRequest(
        "https://host.test/trusted_signals?hostname=top.test&keys=jabberwocky#",
        kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(
        "https://host.test/"
        "trusted_signals?hostname=top.test&keys=jabberwocky#foo",
        kAcceptJson, ExpectedResponse::kReject);

    // Escaped #, &, and = are all allowed.
    TryMakeRequest(
        "https://host.test/trusted_signals?hostname=top.test&keys=%23%26%3D",
        kAcceptJson, ExpectedResponse::kAllow);

    // Valid Trusted Signals KVv2 POST request
    network::ResourceRequest request;
    request.method = net::HttpRequestHeaders::kPostMethod;
    request.url = GURL(kTrustedSignalsBaseUrl);
    request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                              kAcceptAdAuctionTrustedSignals);
    request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                              kAdAuctionTrustedSignalsContentType);
    TryMakeRequest(request, ExpectedResponse::kAllow);

    // Invalid Trusted Signals KVv2 POST request with mismatched base url.
    request.url = GURL("https://host.test/trusted_signals?");
    TryMakeRequest(request, ExpectedResponse::kReject);
  }
}

// Make sure all seller signals requests use the same transient
// NetworkAnonymizationKey.
TEST_P(AuctionUrlLoaderFactoryProxyTest, SellerSignalsNetworkIsolationKey) {
  is_for_seller_ = true;
  // Make 20 JSON requests, 10 with the same URL, 10 with different ones. All
  // should be plumbed through successfully.
  for (int i = 0; i < 10; ++i) {
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(kTrustedSignalsUrl + base::NumberToString(i), kAcceptJson,
                   ExpectedResponse::kAllow);
  }
  EXPECT_EQ(20u, trusted_url_loader_factory_.pending_requests()->size());

  // Make sure all 20 requests use the same transient NetworkAnonymizationKey.
  for (const auto& request : *trusted_url_loader_factory_.pending_requests()) {
    ASSERT_TRUE(request.request.trusted_params);
    EXPECT_TRUE(
        request.request.trusted_params->isolation_info.network_isolation_key()
            .IsTransient());
    EXPECT_EQ(
        request.request.trusted_params->isolation_info.network_isolation_key(),
        (*trusted_url_loader_factory_.pending_requests())[0]
            .request.trusted_params->isolation_info.network_isolation_key());
  }
}

// Test the case the same URL is used for trusted signals and the script (which
// seems weird, but should still work).
TEST_P(AuctionUrlLoaderFactoryProxyTest, SameUrl) {
  trusted_signals_base_url_ = GURL(kScriptUrl);

  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kScriptUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kScriptUrl, std::nullopt, ExpectedResponse::kReject);

    std::string url_with_parameters =
        std::string(kScriptUrl) + "?hostname=top.test&keys=jabberwocky";
    TryMakeRequest(url_with_parameters, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(url_with_parameters, kAcceptJson, ExpectedResponse::kAllow);
    TryMakeRequest(url_with_parameters, kAcceptOther,
                   ExpectedResponse::kReject);
    TryMakeRequest(url_with_parameters, std::nullopt,
                   ExpectedResponse::kReject);

    TryMakeRequest(kTrustedSignalsUrl, kAcceptJavascript,
                   ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptOther, ExpectedResponse::kReject);
    TryMakeRequest(kTrustedSignalsUrl, std::nullopt, ExpectedResponse::kReject);
  }
}

// Make sure that proxies for bidder worklets pass through ClientSecurityState.
// This test relies on the ClientSecurityState equality check in
// TryMakeRequest().
TEST_P(AuctionUrlLoaderFactoryProxyTest, ClientSecurityState) {
  is_for_seller_ = false;

  for (auto ip_address_space : {network::mojom::IPAddressSpace::kLocal,
                                network::mojom::IPAddressSpace::kPrivate,
                                network::mojom::IPAddressSpace::kPublic,
                                network::mojom::IPAddressSpace::kUnknown}) {
    client_security_state_->ip_address_space = ip_address_space;
    // Force creation of a new proxy, with correct `ip_address_space` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kAllow);
  }

  for (auto private_network_request_policy :
       {network::mojom::PrivateNetworkRequestPolicy::kAllow,
        network::mojom::PrivateNetworkRequestPolicy::kWarn,
        network::mojom::PrivateNetworkRequestPolicy::kBlock,
        network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock,
        network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock}) {
    client_security_state_->private_network_request_policy =
        private_network_request_policy;
    // Force creation of a new proxy, with correct `ip_address_space` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();

    TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
    TryMakeRequest(kTrustedSignalsUrl, kAcceptJson, ExpectedResponse::kAllow);
  }
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, BasicSubresourceBundles1) {
  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();
    AuthorizeSubresourceUrls(
        kWorkletHandle1,
        {MakeBundleSubresourceInfo(kBundleUrl, kSubresourceUrl1),
         MakeBundleSubresourceInfo(kBundleUrl, kSubresourceUrl2)});

    TryMakeRequest(kSubresourceUrl1, kAcceptJson, ExpectedResponse::kAllow,
                   /*expect_bundle_request=*/true);
    TryMakeRequest(kSubresourceUrl2, kAcceptJson, ExpectedResponse::kAllow,
                   /*expect_bundle_request=*/true);
    TryMakeRequest(kSubresourceUrl3, kAcceptJson, ExpectedResponse::kReject,
                   /*expect_bundle_request=*/true);

    OnWorkletHandleDestruction(kWorkletHandle1);
  }
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, BasicSubresourceBundles2) {
  for (bool is_for_seller : {false, true}) {
    is_for_seller_ = is_for_seller;
    // Force creation of a new proxy, with correct `is_for_seller` value.
    remote_url_loader_factory_.reset();
    CreateUrlLoaderFactoryProxy();
    AuthorizeSubresourceUrls(
        kWorkletHandle1,
        {MakeBundleSubresourceInfo(kBundleUrl, kSubresourceUrl1)});
    AuthorizeSubresourceUrls(
        kWorkletHandle2,
        {MakeBundleSubresourceInfo(kBundleUrl, kSubresourceUrl2)});

    TryMakeRequest(kSubresourceUrl1, kAcceptJson, ExpectedResponse::kAllow,
                   /*expect_bundle_request=*/true);
    TryMakeRequest(kSubresourceUrl2, kAcceptJson, ExpectedResponse::kAllow,
                   /*expect_bundle_request=*/true);

    OnWorkletHandleDestruction(kWorkletHandle1);

    TryMakeRequest(kSubresourceUrl1, kAcceptJson, ExpectedResponse::kReject,
                   /*expect_bundle_request=*/true);

    OnWorkletHandleDestruction(kWorkletHandle2);
  }
}

TEST_P(AuctionUrlLoaderFactoryProxyTest, AdditionalBidCors) {
  is_for_seller_ = false;
  needs_cors_for_additional_bid_ = true;

  remote_url_loader_factory_.reset();
  CreateUrlLoaderFactoryProxy();

  TryMakeRequest(kScriptUrl, kAcceptJavascript, ExpectedResponse::kAllow);
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    AuctionUrlLoaderFactoryProxyTest,
    testing::Bool());
}  // namespace content

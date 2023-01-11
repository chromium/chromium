// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"

#include <stdint.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

namespace {

const char kScriptUrl[] = "https://host.test/script";

// Values for the Accept header.
const char kAcceptJavascript[] = "application/javascript";

}  // namespace

class SharedStorageURLLoaderFactoryProxyTest : public testing::Test {
 public:
  // Ways the proxy can behave in response to a request.
  enum class ExpectedResponse {
    kReject,
    kAllow,
  };

  SharedStorageURLLoaderFactoryProxyTest() { CreateUrlLoaderFactoryProxy(); }

  ~SharedStorageURLLoaderFactoryProxyTest() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void CreateUrlLoaderFactoryProxy() {
    // The SharedStorageURLLoaderFactoryProxy should only be created if there is
    // no old one, or the old one's pipe was closed.
    DCHECK(!remote_url_loader_factory_ ||
           !remote_url_loader_factory_.is_connected());

    remote_url_loader_factory_.reset();

    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    proxied_url_loader_factory_.Clone(factory.BindNewPipeAndPassReceiver());

    url_loader_factory_proxy_ =
        std::make_unique<SharedStorageURLLoaderFactoryProxy>(
            factory.Unbind(),
            remote_url_loader_factory_.BindNewPipeAndPassReceiver(),
            frame_origin_, GURL(kScriptUrl));
  }

  // Attempts to make a request for `request`.
  void TryMakeRequest(const network::ResourceRequest& request,
                      ExpectedResponse expected_response) {
    // Create a new factory if the last test case closed the pipe.
    if (!remote_url_loader_factory_.is_connected())
      CreateUrlLoaderFactoryProxy();

    EXPECT_EQ(0, proxied_url_loader_factory_.NumPending());

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
    // actually spins the message loop already, but seems best to be safe.
    remote_url_loader_factory_.FlushForTesting();

    network::TestURLLoaderFactory::PendingRequest* pending_request;
    switch (expected_response) {
      case ExpectedResponse::kReject:
        EXPECT_EQ(0, proxied_url_loader_factory_.NumPending());
        return;
      case ExpectedResponse::kAllow:
        EXPECT_EQ(1, proxied_url_loader_factory_.NumPending());
        pending_request =
            &proxied_url_loader_factory_.pending_requests()->back();
        break;
    }

    // The pipe will be closed after the first request regardless of its
    // response type.
    EXPECT_FALSE(remote_url_loader_factory_.is_connected());

    // These should always be the same for all requests.
    EXPECT_EQ(0u, pending_request->options);
    EXPECT_EQ(
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        pending_request->traffic_annotation);

    // Each request should be assigned a unique ID. These should actually be
    // unique within the browser process, not just among requests using the
    // SharedStorageURLLoaderFactoryProxy.
    for (const auto& other_pending_request :
         *proxied_url_loader_factory_.pending_requests()) {
      if (&other_pending_request == pending_request)
        continue;
      EXPECT_NE(other_pending_request.request_id, pending_request->request_id);
    }

    const auto& observed_request = pending_request->request;

    // The URL should be unaltered.
    EXPECT_EQ(request.url, observed_request.url);

    // The accept kAcceptJavascript header should be the only header.
    std::string observed_accept_header;
    EXPECT_TRUE(observed_request.headers.GetHeader(
        net::HttpRequestHeaders::kAccept, &observed_accept_header));
    EXPECT_EQ(kAcceptJavascript, observed_accept_header);
    EXPECT_EQ(1u, observed_request.headers.GetHeaderVector().size());

    // The request should include credentials, and should not follow redirects.
    EXPECT_EQ(network::mojom::CredentialsMode::kSameOrigin,
              observed_request.credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError,
              observed_request.redirect_mode);

    // The initiator should be set.
    EXPECT_EQ(frame_origin_, observed_request.request_initiator);

    EXPECT_EQ(network::mojom::RequestMode::kSameOrigin, observed_request.mode);
    ASSERT_FALSE(observed_request.trusted_params);
  }

  void TryMakeRequest(const std::string& url,
                      ExpectedResponse expected_response) {
    network::ResourceRequest request;
    request.url = GURL(url);
    TryMakeRequest(request, expected_response);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  const url::Origin frame_origin_ = url::Origin::Create(GURL(kScriptUrl));
  network::TestURLLoaderFactory proxied_url_loader_factory_;
  std::unique_ptr<SharedStorageURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
};

TEST_F(SharedStorageURLLoaderFactoryProxyTest, Basic) {
  TryMakeRequest(kScriptUrl, ExpectedResponse::kAllow);
  TryMakeRequest("https://host.test/", ExpectedResponse::kReject);
  TryMakeRequest(kScriptUrl, ExpectedResponse::kAllow);
}

}  // namespace content

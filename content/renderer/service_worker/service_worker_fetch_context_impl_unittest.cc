// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"

#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"

namespace content {

class ServiceWorkerFetchContextImplTest : public testing::Test {
 public:
  ServiceWorkerFetchContextImplTest() = default;

  class FakeURLLoaderThrottle : public blink::URLLoaderThrottle {
   public:
    FakeURLLoaderThrottle() = default;
  };

  class FakeURLLoaderThrottleProvider : public URLLoaderThrottleProvider {
    std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
      NOTREACHED();
      return nullptr;
    }

    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
        int render_frame_id,
        const blink::WebURLRequest& request) override {
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
      throttles.emplace_back(std::make_unique<FakeURLLoaderThrottle>());
      return throttles;
    }

    void SetOnline(bool is_online) override { NOTREACHED(); }
  };
};

TEST_F(ServiceWorkerFetchContextImplTest, SkipThrottling) {
  const GURL kScriptUrl("https://example.com/main.js");
  const GURL kScriptUrlToSkipThrottling("https://example.com/skip.js");
  auto context = base::MakeRefCounted<ServiceWorkerFetchContextImpl>(
      blink::RendererPreferences(), kScriptUrl,
      /*pending_url_loader_factory=*/nullptr,
      /*pending_script_loader_factory=*/nullptr, kScriptUrlToSkipThrottling,
      std::make_unique<FakeURLLoaderThrottleProvider>(),
      /*websocket_handshake_throttle_provider=*/nullptr, mojo::NullReceiver(),
      mojo::NullReceiver(),
      /*service_worker_route_id=*/-1,
      /*cors_exempt_header_list=*/std::vector<std::string>());

  {
    // Call WillSendRequest() for kScriptURL.
    blink::WebURLRequest request;
    request.SetUrl(kScriptUrl);
    request.SetRequestContext(blink::mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should be created by the provider.
    auto* url_request_extra_data = static_cast<blink::WebURLRequestExtraData*>(
        request.GetURLRequestExtraData().get());
    ASSERT_TRUE(url_request_extra_data);
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        url_request_extra_data->TakeURLLoaderThrottles();
    EXPECT_EQ(1u, throttles.size());
  }
  {
    // Call WillSendRequest() for kScriptURLToSkipThrottling.
    blink::WebURLRequest request;
    request.SetUrl(kScriptUrlToSkipThrottling);
    request.SetRequestContext(blink::mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should not be created by the provider.
    auto* url_request_extra_data = static_cast<blink::WebURLRequestExtraData*>(
        request.GetURLRequestExtraData().get());
    ASSERT_TRUE(url_request_extra_data);
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        url_request_extra_data->TakeURLLoaderThrottles();
    EXPECT_TRUE(throttles.empty());
  }
}

}  // namespace content

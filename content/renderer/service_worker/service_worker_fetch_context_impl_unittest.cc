// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"

#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/loader/request_extra_data.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        const blink::WebURLRequest& request,
        ResourceType resource_type) override {
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
      blink::mojom::RendererPreferences(), kScriptUrl,
      /*url_loader_factory_info=*/nullptr,
      /*script_loader_factory_info=*/nullptr, kScriptUrlToSkipThrottling,
      std::make_unique<FakeURLLoaderThrottleProvider>(),
      /*websocket_handshake_throttle_provider=*/nullptr, mojo::NullReceiver(),
      mojo::NullReceiver(),
      /*service_worker_route_id=*/-1);

  {
    // Call WillSendRequest() for kScriptURL.
    blink::WebURLRequest request;
    request.SetUrl(kScriptUrl);
    request.SetRequestContext(blink::mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should be created by the provider.
    auto* extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
    ASSERT_TRUE(extra_data);
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        extra_data->TakeURLLoaderThrottles();
    EXPECT_EQ(1u, throttles.size());
  }
  {
    // Call WillSendRequest() for kScriptURLToSkipThrottling.
    blink::WebURLRequest request;
    request.SetUrl(kScriptUrlToSkipThrottling);
    request.SetRequestContext(blink::mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should not be created by the provider.
    auto* extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
    ASSERT_TRUE(extra_data);
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        extra_data->TakeURLLoaderThrottles();
    EXPECT_TRUE(throttles.empty());
  }
}

}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ScopedMockContentBrowserClient : public TestContentBrowserClient {
 public:
  ScopedMockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedMockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  MOCK_METHOD(
      bool,
      WillCreateURLLoaderFactory,
      (BrowserContext * browser_context,
       RenderFrameHost* frame,
       int render_process_id,
       URLLoaderFactoryType type,
       const url::Origin& request_initiator,
       absl::optional<int64_t> navigation_id,
       ukm::SourceIdObj ukm_source_id,
       mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
           factory_receiver,
       mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
           header_client,
       bool* bypass_redirect_checks,
       bool* disable_secure_dns,
       network::mojom::URLLoaderFactoryOverridePtr* factory_override),
      (override));

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

class PrefetchNetworkContextTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_content_browser_client_ =
        std::make_unique<ScopedMockContentBrowserClient>();

    prefetch_service_ = std::make_unique<PrefetchService>(browser_context());
  }

  void TearDown() override {
    prefetch_service_.reset();
    test_content_browser_client_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  ScopedMockContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
  }

  PrefetchService* prefetch_service() const { return prefetch_service_.get(); }

 private:
  std::unique_ptr<ScopedMockContentBrowserClient> test_content_browser_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PrefetchService> prefetch_service_;
};

TEST_F(PrefetchNetworkContextTest, CreateIsolatedURLLoaderFactory) {
  const GURL kReferringUrl = GURL("https://test.referring.origin.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
          testing::ResultOf(
              [&kReferringUrl](const url::Origin& request_initiator) {
                return request_initiator.IsSameOriginWith(kReferringUrl);
              },
              true),
          testing::Eq(absl::nullopt),
          ukm::SourceIdObj::FromInt64(main_rfh()->GetPageUkmSourceId()),
          testing::NotNull(), testing::NotNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  blink::mojom::Referrer referring_origin;
  referring_origin.url = kReferringUrl;

  std::unique_ptr<PrefetchNetworkContext> prefetch_network_context =
      std::make_unique<PrefetchNetworkContext>(
          prefetch_service(),
          PrefetchType(/*use_isolated_network_context=*/true,
                       /*use_prefetch_proxy=*/false,
                       blink::mojom::SpeculationEagerness::kEager),
          referring_origin, main_rfh()->GetGlobalId());

  prefetch_network_context->GetURLLoaderFactory();
}

TEST_F(PrefetchNetworkContextTest,
       CreateURLLoaderFactoryInDefaultNetworkContext) {
  const GURL kReferringUrl = GURL("https://test.referring.origin.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
          testing::ResultOf(
              [&kReferringUrl](const url::Origin request_initiator) {
                return request_initiator.IsSameOriginWith(kReferringUrl);
              },
              true),
          testing::Eq(absl::nullopt),
          ukm::SourceIdObj::FromInt64(main_rfh()->GetPageUkmSourceId()),
          testing::NotNull(), testing::NotNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  blink::mojom::Referrer referring_origin;
  referring_origin.url = kReferringUrl;

  std::unique_ptr<PrefetchNetworkContext> prefetch_network_context =
      std::make_unique<PrefetchNetworkContext>(
          prefetch_service(),
          PrefetchType(/*use_isolated_network_context=*/false,
                       /*use_prefetch_proxy=*/false,
                       blink::mojom::SpeculationEagerness::kEager),
          referring_origin, main_rfh()->GetGlobalId());

  prefetch_network_context->GetURLLoaderFactory();
}

}  // namespace
}  // namespace content
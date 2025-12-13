// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"

namespace content {
namespace {

// "arg" type is `url::Origin`.
// `url` type is `GURL`.
MATCHER_P(IsSameOriginWith, url, "") {
  return arg.IsSameOriginWith(url);
}

// "arg" type is `net::IsolationInfo`.
MATCHER(IsEmptyIsolationInfo, "") {
  return arg.IsEmpty();
}

class PrefetchNetworkContextTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_content_browser_client_ = std::make_unique<
        ::testing::StrictMock<ScopedMockContentBrowserClient>>();

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
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    kReferringUrl);

  EXPECT_CALL(*test_content_browser_client(),
              WillCreateURLLoaderFactory(
                  testing::NotNull(), main_rfh(),
                  main_rfh()->GetProcess()->GetDeprecatedID(),
                  ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
                  IsSameOriginWith(kReferringUrl), IsEmptyIsolationInfo(),
                  testing::Eq(std::nullopt),
                  ukm::SourceIdObj::FromInt64(main_rfh()->GetPageUkmSourceId()),
                  testing::_, testing::NotNull(), testing::NotNull(),
                  testing::IsNull(), testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchNetworkContext> prefetch_network_context =
      std::make_unique<PrefetchNetworkContext>(
          /*use_isolated_network_context=*/true,
          PrefetchType(PreloadingTriggerType::kSpeculationRule,
                       /*use_prefetch_proxy=*/false,
                       blink::mojom::SpeculationEagerness::kImmediate),
          main_rfh()->GetGlobalId(), main_rfh()->GetLastCommittedOrigin());

  prefetch_network_context->GetURLLoaderFactory(prefetch_service());
}

TEST_F(PrefetchNetworkContextTest,
       CreateURLLoaderFactoryInDefaultNetworkContext) {
  const GURL kReferringUrl = GURL("https://test.referring.origin.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    kReferringUrl);

  EXPECT_CALL(*test_content_browser_client(),
              WillCreateURLLoaderFactory(
                  testing::NotNull(), main_rfh(),
                  main_rfh()->GetProcess()->GetDeprecatedID(),
                  ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
                  IsSameOriginWith(kReferringUrl), IsEmptyIsolationInfo(),
                  testing::Eq(std::nullopt),
                  ukm::SourceIdObj::FromInt64(main_rfh()->GetPageUkmSourceId()),
                  testing::_, testing::NotNull(), testing::NotNull(),
                  testing::IsNull(), testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchNetworkContext> prefetch_network_context =
      std::make_unique<PrefetchNetworkContext>(
          /*use_isolated_network_context=*/false,
          PrefetchType(PreloadingTriggerType::kSpeculationRule,
                       /*use_prefetch_proxy=*/false,
                       blink::mojom::SpeculationEagerness::kImmediate),
          main_rfh()->GetGlobalId(), main_rfh()->GetLastCommittedOrigin());

  prefetch_network_context->GetURLLoaderFactory(prefetch_service());
}

TEST_F(PrefetchNetworkContextTest,
       CreateURLLoaderFactoryForBrowserInitiatedTriggersNetworkContext) {
  const GURL kReferringUrl = GURL("https://test.referring.origin.com");
  const url::Origin kReferringOrigin = url::Origin::Create(kReferringUrl);

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), testing::IsNull(),
          testing::Eq(content::ChildProcessHost::kInvalidUniqueID),
          ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
          IsSameOriginWith(kReferringUrl), IsEmptyIsolationInfo(),
          testing::Eq(std::nullopt), testing::Eq(ukm::kInvalidSourceIdObj),
          testing::_, testing::NotNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchNetworkContext> prefetch_network_context =
      std::make_unique<PrefetchNetworkContext>(
          /*use_isolated_network_context=*/false,
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/false),
          /*referring_render_frame_host_id=*/GlobalRenderFrameHostId(),
          kReferringOrigin);

  prefetch_network_context->GetURLLoaderFactory(prefetch_service());
}

}  // namespace
}  // namespace content

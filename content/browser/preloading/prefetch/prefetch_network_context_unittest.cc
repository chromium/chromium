// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_isolated_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "content/public/browser/browser_context.h"
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

  // Unused fields are marked as `{}`.
  auto prefetch_request = PrefetchRequest::CreateRendererInitiated(
      *static_cast<RenderFrameHostImpl*>(main_rfh()),
      /*referring_document_token=*/{}, /*url=*/{},
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kImmediate),
      /*referrer=*/{},
      /*speculation_rules_tags=*/{},
      /*no_vary_search_hint=*/{},
      /*priority=*/{},
      /*prefetch_document_manager=*/{},
      PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/PreloadingType::kPrefetch));
  // For now we just need to create the context but don't use it.
  std::ignore = std::make_unique<PrefetchIsolatedNetworkContext>(
      prefetch_service()->CreateIsolatedNetworkContextForTesting(
          /*is_proxy_required_when_cross_origin=*/false),
      *prefetch_request);
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

  // Unused fields are marked as `{}`.
  auto prefetch_request = PrefetchRequest::CreateRendererInitiated(
      *static_cast<RenderFrameHostImpl*>(main_rfh()),
      /*referring_document_token=*/{}, /*url=*/{},
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kImmediate),
      /*referrer=*/{},
      /*speculation_rules_tags=*/{},
      /*no_vary_search_hint=*/{},
      /*priority=*/{},
      /*prefetch_document_manager=*/{},
      PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/PreloadingType::kPrefetch));
  CreatePrefetchURLLoaderFactory(prefetch_request->browser_context()
                                     ->GetDefaultStoragePartition()
                                     ->GetNetworkContext(),
                                 *prefetch_request);
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

  // Unused fields are marked as `{}`.
  auto prefetch_request =
      PrefetchRequest::CreateBrowserInitiatedWithoutWebContents(
          browser_context(),
          /*url=*/{},
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/false),
          test::kPreloadingEmbedderHistgramSuffixForTesting,
          /*referrer=*/{},
          /*javascript_enabled=*/{}, kReferringOrigin,
          /*no_vary_search_hint=*/{},
          /*priority=*/{});
  CreatePrefetchURLLoaderFactory(prefetch_request->browser_context()
                                     ->GetDefaultStoragePartition()
                                     ->GetNetworkContext(),
                                 *prefetch_request);
}

}  // namespace
}  // namespace content

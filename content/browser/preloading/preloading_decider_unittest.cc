// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetcher.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MockAnchorElementPreconnector : public AnchorElementPreconnectDelegate {
 public:
  explicit MockAnchorElementPreconnector(RenderFrameHost& render_frame_host) {}
  ~MockAnchorElementPreconnector() override = default;

  void MaybePreconnect(const GURL& target) override { target_ = target; }
  absl::optional<GURL>& Target() { return target_; }

 private:
  absl::optional<GURL> target_;
};

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void PrefetchUrl(
      base::WeakPtr<PrefetchContainer> prefetch_container) override {
    prefetches_.push_back(prefetch_container);
  }

  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_;
};

class MockPrerenderer : public Prerenderer {
 public:
  ~MockPrerenderer() override = default;

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override {
    for (const auto& candidate : candidates) {
      MaybePrerender(candidate);
    }
  }

  bool MaybePrerender(
      const blink::mojom::SpeculationCandidatePtr& candidate) override {
    return prerenders_.insert(candidate->url).second;
  }

  bool ShouldWaitForPrerenderResult(const GURL& url) override {
    return prerenders_.find(url) != prerenders_.end();
  }

  std::set<GURL> prerenders_;
};

class ScopedMockPrerenderer {
 public:
  explicit ScopedMockPrerenderer(PreloadingDecider* preloading_decider)
      : preloading_decider_(preloading_decider) {
    auto new_prerenderer = std::make_unique<MockPrerenderer>();
    prerenderer_ = new_prerenderer.get();
    old_prerenderer_ = preloading_decider_->SetPrerendererForTesting(
        std::move(new_prerenderer));
  }

  ~ScopedMockPrerenderer() {
    preloading_decider_->SetPrerendererForTesting(std::move(old_prerenderer_));
  }

  MockPrerenderer* Get() { return prerenderer_.get(); }

 private:
  raw_ptr<PreloadingDecider> preloading_decider_;
  raw_ptr<MockPrerenderer> prerenderer_;
  std::unique_ptr<Prerenderer> old_prerenderer_;
};

class MockContentBrowserClient : public TestContentBrowserClient {
 public:
  MockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }
  ~MockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  std::unique_ptr<AnchorElementPreconnectDelegate>
  CreateAnchorElementPreconnectDelegate(
      RenderFrameHost& render_frame_host) override {
    auto delegate =
        std::make_unique<MockAnchorElementPreconnector>(render_frame_host);
    delegate_ = delegate.get();
    return delegate;
  }

  MockAnchorElementPreconnector* GetDelegate() { return delegate_; }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
  raw_ptr<MockAnchorElementPreconnector> delegate_;
};

enum class EventType {
  kPointerDown,
  kPointerHover,
};

class PreloadingDeciderTest
    : public RenderViewHostTestHarness,
      public ::testing::WithParamInterface<
          std::tuple<EventType, blink::mojom::SpeculationEagerness>> {
 public:
  PreloadingDeciderTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrefetchUseContentRefactor,
        {{"proxy_host", "https://testproxyhost.com"}});
  }
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(
            *web_contents_);
    web_contents_->NavigateAndCommit(GetSameOriginUrl("/"));
    prefetch_service_ =
        std::make_unique<TestPrefetchService>(GetBrowserContext());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }
  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& GetPrimaryMainFrame() {
    return web_contents_->GetPrimaryPage().GetMainDocument();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  TestPrefetchService* GetPrefetchService() { return prefetch_service_.get(); }

 private:
  test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<TestPrefetchService> prefetch_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

TEST_F(PreloadingDeciderTest, DefaultEagernessCandidatesStartOnStandby) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  // Create list of SpeculationCandidatePtrs.
  std::vector<std::tuple<bool, GURL, blink::mojom::SpeculationAction,
                         blink::mojom::SpeculationEagerness>>
      test_cases{{true, GetCrossOriginUrl("/candidate1.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kConservative},
                 {true, GetCrossOriginUrl("/candidate2.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kModerate},
                 {false, GetCrossOriginUrl("/candidate3.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kEager},
                 {true, GetCrossOriginUrl("/candidate1.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kConservative},
                 {true, GetCrossOriginUrl("/candidate2.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kModerate},
                 {false, GetCrossOriginUrl("/candidate3.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kEager}};
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  for (const auto& [should_be_on_standby, url, action, eagerness] :
       test_cases) {
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->action = action;
    candidate->url = url;
    candidate->referrer = blink::mojom::Referrer::New();
    candidate->eagerness = eagerness;
    candidates.push_back(std::move(candidate));
  }

  preloading_decider->UpdateSpeculationCandidates(candidates);

  for (const auto& [should_be_on_standby, url, action, eagerness] :
       test_cases) {
    EXPECT_EQ(should_be_on_standby,
              preloading_decider->IsOnStandByForTesting(url, action));
  }
}

TEST_P(PreloadingDeciderTest, PrefetchOnPointerEventHeuristics) {
  base::test::ScopedFeatureList scoped_features;
  switch (std::get<0>(GetParam())) {
    case EventType::kPointerDown:
      scoped_features.InitWithFeatures(
          {blink::features::kSpeculationRulesPointerDownHeuristics}, {});
      break;

    case EventType::kPointerHover:
      scoped_features.InitWithFeatures(
          {blink::features::kSpeculationRulesPointerHoverHeuristics}, {});
      break;
  }

  MockContentBrowserClient browser_client;

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  auto* preconnect_delegate = browser_client.GetDelegate();
  EXPECT_TRUE(preconnect_delegate != nullptr);

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto call_pointer_event_handler = [&preloading_decider](const GURL& url) {
    switch (std::get<0>(GetParam())) {
      case EventType::kPointerDown:
        preloading_decider->OnPointerDown(url);
        break;
      case EventType::kPointerHover:
        preloading_decider->OnPointerHover(url);
        break;
    }
  };

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = blink::mojom::Referrer::New();
  candidate1->eagerness = std::get<1>(GetParam());
  candidates.push_back(std::move(candidate1));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  // It should not pass kModerate or kConservative candidates directly
  EXPECT_TRUE(GetPrefetchService()->prefetches_.empty());

  call_pointer_event_handler(GetCrossOriginUrl("/candidate1.html"));
  EXPECT_FALSE(
      preconnect_delegate->Target().has_value());  // Shouldn't preconnect
  EXPECT_EQ(
      1u,
      GetPrefetchService()->prefetches_.size());  // It should only prefetch

  // Another pointer event should not change anything
  call_pointer_event_handler(GetCrossOriginUrl("/candidate1.html"));
  EXPECT_FALSE(preconnect_delegate->Target().has_value());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  // It should preconnect if the target is not safe to prefetch
  call_pointer_event_handler(GetCrossOriginUrl("/candidate2.html"));
  EXPECT_TRUE(preconnect_delegate->Target().has_value());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
}

TEST_P(PreloadingDeciderTest, PrerenderOnPointerEventHeuristics) {
  base::test::ScopedFeatureList scoped_features;
  switch (std::get<0>(GetParam())) {
    case EventType::kPointerDown:
      scoped_features.InitWithFeatures(
          {blink::features::kSpeculationRulesPointerDownHeuristics}, {});
      break;

    case EventType::kPointerHover:
      scoped_features.InitWithFeatures(
          {blink::features::kSpeculationRulesPointerHoverHeuristics}, {});
      break;
  }

  MockContentBrowserClient browser_client;

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  ScopedMockPrerenderer prerenderer(preloading_decider);

  auto* preconnect_delegate = browser_client.GetDelegate();
  EXPECT_TRUE(preconnect_delegate != nullptr);

  // Create list of SpeculationCandidatePtrs.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto create_candidate = [this](const auto& action, const auto& url,
                                 const auto& eagerness) {
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->action = action;
    candidate->url = GetSameOriginUrl(url);
    candidate->referrer = blink::mojom::Referrer::New();
    candidate->eagerness = eagerness;
    return candidate;
  };

  auto call_pointer_event_handler = [&preloading_decider](const GURL& url) {
    switch (std::get<0>(GetParam())) {
      case EventType::kPointerDown:
        preloading_decider->OnPointerDown(url);
        break;
      case EventType::kPointerHover:
        preloading_decider->OnPointerHover(url);
        break;
    }
  };

  candidates.push_back(
      create_candidate(blink::mojom::SpeculationAction::kPrerender,
                       "/candidate1.html", std::get<1>(GetParam())));
  candidates.push_back(
      create_candidate(blink::mojom::SpeculationAction::kPrefetch,
                       "/candidate2.html", std::get<1>(GetParam())));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  // It should not pass kModerate or kConservative candidates directly
  EXPECT_TRUE(prerenderer.Get()->prerenders_.empty());
  EXPECT_TRUE(GetPrefetchService()->prefetches_.empty());

  call_pointer_event_handler(GetSameOriginUrl("/candidate1.html"));
  EXPECT_FALSE(
      preconnect_delegate->Target().has_value());  // Shouldn't preconnect.
  EXPECT_EQ(0u,
            GetPrefetchService()->prefetches_.size());  // Shouldn't prefetch.
  EXPECT_EQ(1u,
            prerenderer.Get()->prerenders_.size());  // Should prerender.

  // Another pointer event should not change anything
  call_pointer_event_handler(GetSameOriginUrl("/candidate1.html"));

  EXPECT_FALSE(preconnect_delegate->Target().has_value());
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());
  EXPECT_EQ(1u, prerenderer.Get()->prerenders_.size());

  // It should prefetch if the target is safe to prefetch.
  call_pointer_event_handler(GetSameOriginUrl("/candidate2.html"));
  EXPECT_FALSE(preconnect_delegate->Target().has_value());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
  EXPECT_EQ(1u, prerenderer.Get()->prerenders_.size());

  // It should preconnect if the target is not safe to prerender nor safe to
  // prefetch.
  call_pointer_event_handler(GetSameOriginUrl("/candidate3.html"));
  EXPECT_TRUE(preconnect_delegate->Target().has_value());
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
  EXPECT_EQ(1u, prerenderer.Get()->prerenders_.size());
}

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PreloadingDeciderTest,
    testing::Combine(
        testing::Values(EventType::kPointerDown, EventType::kPointerHover),
        testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                        blink::mojom::SpeculationEagerness::kConservative)));

}  // namespace
}  // namespace content

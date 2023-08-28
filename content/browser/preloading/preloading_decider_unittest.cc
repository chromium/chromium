// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetcher.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

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

  void EvictPrefetch(size_t index) {
    DCHECK_LT(index, prefetches_.size());
    DCHECK(prefetches_[index]);
    base::WeakPtr<PrefetchContainer> prefetch_container = prefetches_[index];
    std::unique_ptr<PrefetchContainer> owned_prefetch_container =
        prefetch_container->GetPrefetchDocumentManager()
            ->ReleasePrefetchContainer(prefetch_container->GetURL());
    prefetches_.erase(prefetches_.begin() + index);
    PreloadingDecider::GetForCurrentDocument(
        RenderFrameHost::FromID(
            prefetch_container->GetReferringRenderFrameHostId()))
        ->OnPrefetchEvicted(prefetch_container->GetURL());
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
  raw_ptr<MockPrerenderer, DanglingUntriaged> prerenderer_;
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
  const auto [event_type, eagerness] = GetParam();

  base::test::ScopedFeatureList scoped_features;
  switch (event_type) {
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

  auto call_pointer_event_handler = [&](const GURL& url) {
    switch (event_type) {
      case EventType::kPointerDown:
        preloading_decider->OnPointerDown(url);
        break;
      case EventType::kPointerHover:
        preloading_decider->OnPointerHover(
            url, blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0));
        break;
    }
  };

  auto candidate1 = blink::mojom::SpeculationCandidate::New();
  candidate1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
  candidate1->url = GetCrossOriginUrl("/candidate1.html");
  candidate1->referrer = blink::mojom::Referrer::New();
  candidate1->eagerness = eagerness;
  candidates.push_back(std::move(candidate1));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  // It should not pass kModerate or kConservative candidates directly
  EXPECT_TRUE(GetPrefetchService()->prefetches_.empty());

  // By default, pointer hover is not enough to trigger conservative candidates.
  if (std::pair(event_type, eagerness) !=
      std::pair(EventType::kPointerHover,
                blink::mojom::SpeculationEagerness::kConservative)) {
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

    call_pointer_event_handler(GetCrossOriginUrl("/candidate2.html"));
    // It should preconnect if the target is not safe to prefetch and it is a
    // `kPointerDown` event.
    switch (event_type) {
      case EventType::kPointerDown:
        EXPECT_TRUE(preconnect_delegate->Target().has_value());
        break;
      case EventType::kPointerHover:
        EXPECT_FALSE(preconnect_delegate->Target().has_value());
        break;
    }
    EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
  } else {
    call_pointer_event_handler(GetCrossOriginUrl("/candidate1.html"));
    // It should preconnect if the target is not safe to prefetch and it is a
    // `kPointerDown` event.
    switch (event_type) {
      case EventType::kPointerDown:
        EXPECT_TRUE(preconnect_delegate->Target().has_value());
        break;
      case EventType::kPointerHover:
        EXPECT_FALSE(preconnect_delegate->Target().has_value());
        break;
    }
    EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());

    call_pointer_event_handler(GetCrossOriginUrl("/candidate2.html"));
    // It should preconnect if the target is not safe to prefetch and it is a
    // `kPointerDown` event.
    switch (event_type) {
      case EventType::kPointerDown:
        EXPECT_TRUE(preconnect_delegate->Target().has_value());
        break;
      case EventType::kPointerHover:
        EXPECT_FALSE(preconnect_delegate->Target().has_value());
        break;
    }
    EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());
  }
}

TEST_P(PreloadingDeciderTest, PrerenderOnPointerEventHeuristics) {
  const auto [event_type, eagerness] = GetParam();

  base::test::ScopedFeatureList scoped_features;
  switch (event_type) {
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

  auto create_candidate =
      [&](blink::mojom::SpeculationAction action, const std::string& url,
          network::mojom::NoVarySearchPtr&& no_vary_search_hint = nullptr) {
        auto candidate = blink::mojom::SpeculationCandidate::New();
        candidate->action = action;
        candidate->url = GetSameOriginUrl(url);
        candidate->referrer = blink::mojom::Referrer::New();
        candidate->eagerness = eagerness;
        if (no_vary_search_hint) {
          candidate->no_vary_search_hint = std::move(no_vary_search_hint);
        }
        return candidate;
      };

  auto call_pointer_event_handler = [&](const GURL& url) {
    switch (event_type) {
      case EventType::kPointerDown:
        preloading_decider->OnPointerDown(url);
        break;
      case EventType::kPointerHover:
        preloading_decider->OnPointerHover(
            url, blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0));
        break;
    }
  };

  candidates.push_back(create_candidate(
      blink::mojom::SpeculationAction::kPrerender, "/candidate1.html"));
  candidates.push_back(create_candidate(
      blink::mojom::SpeculationAction::kPrefetch, "/candidate2.html"));
  candidates.push_back(create_candidate(
      blink::mojom::SpeculationAction::kPrefetch, "/candidate4.html?a=1",
      network::mojom::NoVarySearch::New(
          network::mojom::SearchParamsVariance::NewNoVaryParams({"a"}), true)));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  // It should not pass kModerate or kConservative candidates directly
  EXPECT_TRUE(prerenderer.Get()->prerenders_.empty());
  EXPECT_TRUE(GetPrefetchService()->prefetches_.empty());

  // By default, pointer hover is not enough to trigger conservative candidates.
  if (std::pair(event_type, eagerness) !=
      std::pair(EventType::kPointerHover,
                blink::mojom::SpeculationEagerness::kConservative)) {
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

    // It should prefetch if there is a prefetch candidate matching by
    // No-Vary-Search hint.
    call_pointer_event_handler(GetSameOriginUrl("/candidate4.html"));
    EXPECT_FALSE(preconnect_delegate->Target().has_value());
    EXPECT_EQ(2u, GetPrefetchService()->prefetches_.size());
    EXPECT_EQ(1u, prerenderer.Get()->prerenders_.size());

    call_pointer_event_handler(GetSameOriginUrl("/candidate3.html"));
    // It should preconnect if the target is not safe to prerender nor safe to
    // prefetch and it is a `kPointerDown` event.
    switch (event_type) {
      case EventType::kPointerDown:
        EXPECT_TRUE(preconnect_delegate->Target().has_value());
        break;
      case EventType::kPointerHover:
        EXPECT_FALSE(preconnect_delegate->Target().has_value());
        break;
    }
    EXPECT_EQ(2u, GetPrefetchService()->prefetches_.size());
    EXPECT_EQ(1u, prerenderer.Get()->prerenders_.size());
  } else {
    call_pointer_event_handler(GetSameOriginUrl("/candidate1.html"));
    // It should preconnect if the target is not safe to prerender nor safe to
    // prefetch and it is a `kPointerDown` event.
    switch (event_type) {
      case EventType::kPointerDown:
        EXPECT_TRUE(preconnect_delegate->Target().has_value());
        break;
      case EventType::kPointerHover:
        EXPECT_FALSE(preconnect_delegate->Target().has_value());
        break;
    }
    EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());
    EXPECT_EQ(0u, prerenderer.Get()->prerenders_.size());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PreloadingDeciderTest,
    testing::Combine(
        testing::Values(EventType::kPointerDown, EventType::kPointerHover),
        testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                        blink::mojom::SpeculationEagerness::kConservative)));

TEST_F(PreloadingDeciderTest, CanOverridePointerDownEagerness) {
  // PreloadingDecider defaults to allowing it for conservative candidates,
  // but for this test we'll allow it only for moderate.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      blink::features::kSpeculationRulesPointerDownHeuristics,
      {{"pointer_down_eagerness", "moderate"}});

  MockContentBrowserClient browser_client;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->url = GetSameOriginUrl("/candidate1.html");
  candidate->eagerness = blink::mojom::SpeculationEagerness::kConservative;
  candidate->referrer = blink::mojom::Referrer::New();
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(std::move(candidate));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());

  preloading_decider->OnPointerDown(GetSameOriginUrl("/candidate1.html"));
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());
}

TEST_F(PreloadingDeciderTest, CanOverridePointerHoverEagerness) {
  // PreloadingDecider defaults to allowing it for moderate candidates,
  // but for this test we'll allow it only for conservative candidates too.
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      blink::features::kSpeculationRulesPointerHoverHeuristics,
      {{"pointer_hover_eagerness", "moderate,conservative"}});

  MockContentBrowserClient browser_client;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->url = GetSameOriginUrl("/candidate1.html");
  candidate->eagerness = blink::mojom::SpeculationEagerness::kConservative;
  candidate->referrer = blink::mojom::Referrer::New();
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(std::move(candidate));

  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());

  preloading_decider->OnPointerHover(
      GetSameOriginUrl("/candidate1.html"),
      blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0));
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
}

TEST_F(PreloadingDeciderTest, UmaRecallStats) {
  base::HistogramTester histogram_tester;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->url = GetCrossOriginUrl("/candidate1.html");
  candidate->referrer = blink::mojom::Referrer::New();
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.push_back(std::move(candidate));

  preloading_decider->UpdateSpeculationCandidates(candidates);

  PreloadingPredictor pointer_down_predictor{
      preloading_predictor::kUrlPointerDownOnAnchor};
  // PreloadingPredictor on_hover_predictor{
  //     preloading_predictor::kUrlPointerHoverOnAnchor};
  // Check recall UKM records.
  auto uma_predictor_recall = [](const PreloadingPredictor& predictor) {
    return base::StrCat({"Preloading.Predictor.", predictor.name(), ".Recall"});
  };

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&GetPrimaryMainFrame());
  web_contents->GetController().LoadURL(
      GURL("https://www.google.com"), {},
      ui::PageTransition::PAGE_TRANSITION_LINK, {});

  histogram_tester.ExpectBucketCount(
      uma_predictor_recall(pointer_down_predictor),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      uma_predictor_recall(pointer_down_predictor),
      PredictorConfusionMatrix::kFalseNegative, 0);
}

// Tests that an eager candidate is always processed before a non-eager
// candidate with the same URL, and that the non-eager candidate isn't marked as
// "on-standby".
TEST_F(PreloadingDeciderTest, CandidateWithMultipleEagernessValues) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->url = url;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kConservative;
  candidate_1->referrer = blink::mojom::Referrer::New();

  auto candidate_2 = candidate_1.Clone();
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kEager;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());

  // Add conservative prefetch candidate and prefetch on pointer-down.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  EXPECT_EQ(1u, prefetches.size());
  EXPECT_EQ(prefetches[0]->GetPrefetchType().GetEagerness(),
            blink::mojom::SpeculationEagerness::kEager);
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      url, blink::mojom::SpeculationAction::kPrefetch));
}

// Tests that a previously prefetched conservative candidate can be reprefetched
// after eviction (when retriggered).
TEST_F(PreloadingDeciderTest, CandidateCanBeReprefetchedAfterEviction) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->url = url;
  candidate->eagerness = blink::mojom::SpeculationEagerness::kConservative;
  candidate->referrer = blink::mojom::Referrer::New();
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate.Clone());

  // Add conservative prefetch candidate and prefetch on pointer-down.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());
  preloading_decider->OnPointerDown(url);
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  // Simulate eviction of non-eager prefetch.
  GetPrefetchService()->EvictPrefetch(0);
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());

  // Trigger prefetch for same URL again, it should succeed.
  preloading_decider->OnPointerDown(url);
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());

  // Simulate eviction of non-eager prefetch.
  GetPrefetchService()->EvictPrefetch(0);
  EXPECT_EQ(0u, GetPrefetchService()->prefetches_.size());

  auto eager_candidate = candidate.Clone();
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidates.clear();
  candidates.push_back(candidate.Clone());
  candidates.push_back(eager_candidate.Clone());

  // Add a new eager candidate (but also send the old non-eager candidate). A
  // prefetch should automatically trigger.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(1u, GetPrefetchService()->prefetches_.size());
}

// Tests that candidate removal causes a prefetch to be destroyed, and that
// a reinserted candidate with the same url is re-processed.
TEST_F(PreloadingDeciderTest, ProcessCandidates_EagerCandidateRemoval) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({::features::kPrefetchNewLimits}, {});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url_1 = GetSameOriginUrl("/candidate1.html");
  const GURL url_2 = GetSameOriginUrl("/candidate2.html");

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->referrer = blink::mojom::Referrer::New();
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->requires_anonymous_client_ip_when_cross_origin = false;

  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;
  ASSERT_EQ(2u, prefetches.size());
  EXPECT_EQ(prefetches[0]->GetURL(), url_1);
  EXPECT_EQ(prefetches[1]->GetURL(), url_2);

  // Remove |candidate_2|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  EXPECT_TRUE(prefetches[0]);
  EXPECT_FALSE(prefetches[1]);

  // Re-add |candidate_2|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  ASSERT_EQ(3u, prefetches.size());
  EXPECT_TRUE(prefetches[0]);
  EXPECT_FALSE(prefetches[1]);
  EXPECT_EQ(prefetches[2]->GetURL(), url_2);
}

// Tests that candidate removal works correctly for non-eager candidates, and
// that a non-eager candidate is reprocessed correctly after re-insertion.
TEST_F(PreloadingDeciderTest, ProcessCandidates_NonEagerCandidateRemoval) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({::features::kPrefetchNewLimits}, {});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url_1 = GetSameOriginUrl("/candidate1.html");
  const GURL url_2 = GetSameOriginUrl("/candidate2.html");

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->referrer = blink::mojom::Referrer::New();
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;

  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kConservative;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;
  ASSERT_EQ(1u, prefetches.size());
  EXPECT_EQ(prefetches[0]->GetURL(), url_1);

  preloading_decider->OnPointerDown(url_2);

  ASSERT_EQ(2u, prefetches.size());
  EXPECT_TRUE(prefetches[0]);
  EXPECT_EQ(prefetches[1]->GetURL(), url_2);

  // Remove |candidate_2|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  ASSERT_EQ(2u, prefetches.size());
  EXPECT_TRUE(prefetches[0]);
  EXPECT_FALSE(prefetches[1]);

  // Re-add |candidate_2|, remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  ASSERT_EQ(2u, prefetches.size());
  EXPECT_FALSE(prefetches[0]);

  preloading_decider->OnPointerDown(url_2);

  ASSERT_EQ(3u, prefetches.size());
  EXPECT_TRUE(prefetches[2]);
  EXPECT_EQ(prefetches[2]->GetURL(), url_2);
}

// Test to demonstrate current behaviour where a prefetch is still considered
// to have a speculation candidate even if its original triggering speculation
// candidate was removed; so long as there exists a candidate with the same
// URL.
TEST_F(PreloadingDeciderTest,
       ProcessCandidates_SecondCandidateWithSameUrlKeepsPrefetchAlive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({::features::kPrefetchNewLimits}, {});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url = GetSameOriginUrl("/candidate.html");

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->referrer = blink::mojom::Referrer::New();
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;

  auto candidate_2 = candidate_1.Clone();
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kConservative;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;
  ASSERT_EQ(prefetches.size(), 1u);
  EXPECT_EQ(prefetches[0]->GetURL(), url);

  // Remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  preloading_decider->UpdateSpeculationCandidates(candidates);

  EXPECT_EQ(prefetches.size(), 1u);
  EXPECT_TRUE(prefetches[0]);
}

TEST_F(PreloadingDeciderTest,
       OnPointerHoverWithMotionEstimatorIsRecordedToUMA) {
  base::HistogramTester histogram_tester;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  GURL url1{"https://www.example.com"};
  preloading_decider->OnPointerHover(
      url1, blink::mojom::AnchorElementPointerData::New(
                /*is_mouse_pointer=*/true,
                /*mouse_velocity=*/50.0,
                /*mouse_acceleration=*/0.0));

  GURL url2{"https://www.google.com"};
  preloading_decider->OnPointerHover(
      url2, blink::mojom::AnchorElementPointerData::New(
                /*is_mouse_pointer=*/true,
                /*mouse_velocity=*/75.0,
                /*mouse_acceleration=*/0.0));

  // Navigate to `url2`.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&GetPrimaryMainFrame());
  PreloadingDataImpl* preloading_data = static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents));
  ASSERT_TRUE(preloading_data);
  MockNavigationHandle navigation_handle{url2,
                                         web_contents->GetPrimaryMainFrame()};
  preloading_data->DidStartNavigation(&navigation_handle);

  // Check UMA records.
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPointerHoverWithMotionEstimator.Negative",
      /*100*(50-0/500)=*/10, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPointerHoverWithMotionEstimator.Negative",
      /*100*(75-0/500)=*/15, 0);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPointerHoverWithMotionEstimator.Positive",
      /*100*(50-0/500)=*/10, 0);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPointerHoverWithMotionEstimator.Positive",
      /*100*(75-0/500)=*/15, 1);
}

TEST_F(PreloadingDeciderTest, OnPreloadingHeuristicsModelDone) {
  base::HistogramTester histogram_tester;

  GURL url1{"https://www.example.com"};
  GetPrimaryMainFrame().OnPreloadingHeuristicsModelDone(
      /*url=*/url1, /*score=*/0.2);

  GURL url2{"https://www.google.com"};
  GetPrimaryMainFrame().OnPreloadingHeuristicsModelDone(
      /*url=*/url2, /*score=*/0.9);

  // Navigate to `url2`.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&GetPrimaryMainFrame());
  PreloadingDataImpl* preloading_data = static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents));
  ASSERT_TRUE(preloading_data);
  MockNavigationHandle navigation_handle{url2,
                                         web_contents->GetPrimaryMainFrame()};
  preloading_data->DidStartNavigation(&navigation_handle);

  // Check UMA records.
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPreloadingHeuristicsMLModel.Negative",
      /*100*0.2=*/20, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPreloadingHeuristicsMLModel.Negative",
      /*100*0.9=*/90, 0);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPreloadingHeuristicsMLModel.Positive",
      /*100*0.2=*/20, 0);
  histogram_tester.ExpectBucketCount(
      "Preloading.Experimental.OnPreloadingHeuristicsMLModel.Positive",
      /*100*0.9=*/90, 1);
}

}  // namespace
}  // namespace content

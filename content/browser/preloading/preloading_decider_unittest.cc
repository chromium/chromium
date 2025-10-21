// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetcher.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/common/features.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-data-view.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {
namespace {

class MockAnchorElementPreconnector : public AnchorElementPreconnectDelegate {
 public:
  explicit MockAnchorElementPreconnector(RenderFrameHost& render_frame_host) {}
  ~MockAnchorElementPreconnector() override = default;

  void MaybePreconnect(const GURL& target) override { target_ = target; }
  std::optional<GURL>& Target() { return target_; }

 private:
  std::optional<GURL> target_;
};

class MockPrerenderer : public Prerenderer {
 public:
  ~MockPrerenderer() override = default;
  struct PrerenderInfo {
    blink::mojom::SpeculationEagerness eagerness;
    blink::mojom::SpeculationAction action;
    GURL url;
  };

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
      bool enable_cross_origin_prerender_iframes = false) override {
    for (const auto& candidate : candidates) {
      // Immediate candidates are enacted by the same predictor that creates
      // them.
      PreloadingTriggerType trigger_type =
          PreloadingTriggerTypeFromSpeculationInjectionType(
              candidate->injection_type);
      PreloadingPredictor enacting_predictor =
          GetPredictorForPreloadingTriggerType(trigger_type);
      MaybePrerender(candidate, enacting_predictor, PreloadingConfidence{100});
    }
  }

  void OnLCPPredicted() override {}

  bool MaybePrerender(const blink::mojom::SpeculationCandidatePtr& candidate,
                      const PreloadingPredictor& enacting_predictor,
                      PreloadingConfidence confidence) override {
    if (PrerenderExists(candidate->url)) {
      return false;
    }
    prerenders_.emplace_back(candidate->eagerness, candidate->action,
                             candidate->url);
    return true;
  }

  bool ShouldWaitForPrerenderResult(const GURL& url) override {
    return PrerenderExists(url);
  }

  void SetPrerenderCancellationCallback(
      PrerenderCancellationCallback callback) override {
    prerender_cancellation_callback_ = std::move(callback);
  }

  void OnCancel(size_t index) {
    ASSERT_LT(index, prerenders_.size());
    const auto& [_, action, url] = prerenders_[index];
    prerender_cancellation_callback_.Run(url, action);
    prerenders_.erase(prerenders_.begin() + index);
  }

  bool PrerenderExists(const GURL& url) {
    return std::find_if(prerenders_.begin(), prerenders_.end(),
                        [&](const auto& prerender) {
                          return url == prerender.url;
                        }) != prerenders_.end();
  }

  std::vector<PrerenderInfo> prerenders_;
  PrerenderCancellationCallback prerender_cancellation_callback_ =
      base::DoNothing();
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
    prerenderer_ = nullptr;
    preloading_decider_->SetPrerendererForTesting(std::move(old_prerenderer_));
  }

  MockPrerenderer* Get() { return prerenderer_.get(); }

 private:
  raw_ptr<PreloadingDecider> preloading_decider_ = nullptr;
  raw_ptr<MockPrerenderer> prerenderer_ = nullptr;
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
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
  raw_ptr<MockAnchorElementPreconnector> delegate_ = nullptr;
};

enum class EventType {
  kPointerDown,
  kPointerHover,
};

class PreloadingDeciderTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(
            *web_contents());
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      GetSameOriginUrl("/"));
    prefetch_service_ =
        std::make_unique<TestPrefetchService>(GetBrowserContext());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }
  void TearDown() override {
    // The PrefetchService we created for the test contains a
    // PrefetchOriginProber, which holds a raw pointer to the BrowserContext.
    // When tearing down, it's important to free our PrefetchService
    // before freeing the BrowserContext, to avoid any chance of a use after
    // free.
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    prefetch_service_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& GetPrimaryMainFrame() {
    return *static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  TestPrefetchService* GetPrefetchService() { return prefetch_service_.get(); }

  blink::mojom::SpeculationCandidatePtr MakeCandidate(
      const GURL& url,
      blink::mojom::SpeculationAction action,
      blink::mojom::SpeculationEagerness eagerness) {
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->url = url;
    candidate->action = action;
    candidate->eagerness = eagerness;
    candidate->referrer = blink::mojom::Referrer::New();
    candidate->tags = {std::nullopt};

    return candidate;
  }

 private:
  test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::unique_ptr<TestPrefetchService> prefetch_service_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

TEST_F(PreloadingDeciderTest, DefaultEagernessCandidatesStartOnStandby) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  const bool use_eager_heurisctics = base::FeatureList::IsEnabled(
      blink::features::kPreloadingEagerHoverHeuristics);

  // Create list of SpeculationCandidatePtrs.
  std::vector<std::tuple<bool, GURL, blink::mojom::SpeculationAction,
                         blink::mojom::SpeculationEagerness>>
      test_cases{{true, GetCrossOriginUrl("/candidate1.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kConservative},
                 {true, GetCrossOriginUrl("/candidate2.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kModerate},
                 {use_eager_heurisctics, GetCrossOriginUrl("/candidate3.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kEager},
                 {false, GetCrossOriginUrl("/candidate4.html"),
                  blink::mojom::SpeculationAction::kPrefetch,
                  blink::mojom::SpeculationEagerness::kImmediate},
                 {true, GetCrossOriginUrl("/candidate1.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kConservative},
                 {true, GetCrossOriginUrl("/candidate2.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kModerate},
                 {use_eager_heurisctics, GetCrossOriginUrl("/candidate3.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kEager},
                 {false, GetCrossOriginUrl("/candidate4.html"),
                  blink::mojom::SpeculationAction::kPrerender,
                  blink::mojom::SpeculationEagerness::kImmediate}};
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  for (const auto& [should_be_on_standby, url, action, eagerness] :
       test_cases) {
    candidates.push_back(MakeCandidate(url, action, eagerness));
  }

  preloading_decider->UpdateSpeculationCandidates(candidates);

  for (const auto& [should_be_on_standby, url, action, eagerness] :
       test_cases) {
    EXPECT_EQ(should_be_on_standby,
              preloading_decider->IsOnStandByForTesting(url, action));
  }
}

class PreloadingDeciderPointerEventHeuristicsTest
    : public PreloadingDeciderTest,
      public ::testing::WithParamInterface<
          std::tuple<EventType, blink::mojom::SpeculationEagerness>> {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        blink::features::kPreloadingEagerHoverHeuristics);
    PreloadingDeciderTest::SetUp();
  }

  void TearDown() override {
    PreloadingDeciderTest::TearDown();
    feature_list_.Reset();
  }

 protected:
  void TestPrerenderLikeAction(
      blink::mojom::SpeculationAction prerender_like_action) {
    const auto [event_type, eagerness] = GetParam();

    MockContentBrowserClient browser_client;

    auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
        &GetPrimaryMainFrame());
    ASSERT_TRUE(preloading_decider != nullptr);

    ScopedMockPrerenderer prerenderer(preloading_decider);

    auto* preconnect_delegate = browser_client.GetDelegate();
    EXPECT_TRUE(preconnect_delegate != nullptr);

    // Create list of SpeculationCandidatePtrs.
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

    auto create_candidate =
        [&](blink::mojom::SpeculationAction action, const std::string& url,
            network::mojom::NoVarySearchPtr&& no_vary_search_hint = nullptr) {
          auto candidate =
              MakeCandidate(GetSameOriginUrl(url), action, eagerness);
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
              url, blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0),
              eagerness);
          break;
      }
    };

    candidates.push_back(
        create_candidate(prerender_like_action, "/candidate1.html"));
    candidates.push_back(create_candidate(
        blink::mojom::SpeculationAction::kPrefetch, "/candidate2.html"));
    candidates.push_back(create_candidate(
        blink::mojom::SpeculationAction::kPrefetch, "/candidate4.html?a=1",
        network::mojom::NoVarySearch::New(
            network::mojom::SearchParamsVariance::NewNoVaryParams({"a"}),
            true)));
    candidates.push_back(create_candidate(
        prerender_like_action, "/candidate5.html?a=1",
        network::mojom::NoVarySearch::New(
            network::mojom::SearchParamsVariance::NewNoVaryParams({"a"}),
            true)));

    preloading_decider->UpdateSpeculationCandidates(candidates);
    // It should not pass kModerate or kConservative candidates directly
    EXPECT_TRUE(prerenderer.Get()->prerenders_.empty());
    EXPECT_TRUE(GetPrefetchService()->prefetches_.empty());

    // By default, pointer hover is not enough to trigger conservative
    // candidates.
    if (std::pair(event_type, eagerness) !=
        std::pair(EventType::kPointerHover,
                  blink::mojom::SpeculationEagerness::kConservative)) {
      call_pointer_event_handler(GetSameOriginUrl("/candidate1.html"));
      EXPECT_FALSE(
          preconnect_delegate->Target().has_value());  // Shouldn't preconnect.
      EXPECT_EQ(
          0u,
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

      // It should prerender if there is a prerender candidate matching by
      // No-Vary-Search hint.
      call_pointer_event_handler(GetSameOriginUrl("/candidate5.html"));
      EXPECT_FALSE(
          preconnect_delegate->Target().has_value());  // Shouldn't preconnect
      EXPECT_EQ(
          2u,
          GetPrefetchService()->prefetches_.size());  // Shouldn't prefetch
      EXPECT_EQ(2u, prerenderer.Get()->prerenders_.size());  // Should prerender

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
      EXPECT_EQ(2u, prerenderer.Get()->prerenders_.size());
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PreloadingDeciderPointerEventHeuristicsTest,
       PrefetchOnPointerEventHeuristics) {
  const auto [event_type, eagerness] = GetParam();

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
            url, blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0),
            eagerness);
        break;
    }
  };

  auto candidate1 =
      MakeCandidate(GetCrossOriginUrl("/candidate1.html"),
                    blink::mojom::SpeculationAction::kPrefetch, eagerness);
  candidate1->requires_anonymous_client_ip_when_cross_origin = true;
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

TEST_P(PreloadingDeciderPointerEventHeuristicsTest,
       PrerenderOnPointerEventHeuristics) {
  TestPrerenderLikeAction(blink::mojom::SpeculationAction::kPrerender);
}

TEST_P(PreloadingDeciderPointerEventHeuristicsTest,
       PrerenderUntilScriptOnPointerEventHeuristics) {
  TestPrerenderLikeAction(
      blink::mojom::SpeculationAction::kPrerenderUntilScript);
}

INSTANTIATE_TEST_SUITE_P(
    ParameterizedTests,
    PreloadingDeciderPointerEventHeuristicsTest,
    testing::Combine(
        testing::Values(EventType::kPointerDown, EventType::kPointerHover),
        testing::Values(blink::mojom::SpeculationEagerness::kEager,
                        blink::mojom::SpeculationEagerness::kModerate,
                        blink::mojom::SpeculationEagerness::kConservative)));

TEST_F(PreloadingDeciderTest, UmaRecallStats) {
  base::HistogramTester histogram_tester;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(GetCrossOriginUrl("/candidate1.html"),
                    blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kImmediate);
  candidates.push_back(std::move(candidate));

  preloading_decider->UpdateSpeculationCandidates(candidates);

  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://www.google.com"), &GetPrimaryMainFrame());

  // Check recall.
  const std::string kUmaName = base::StrCat(
      {"Preloading.Predictor.",
       preloading_predictor::kUrlPointerDownOnAnchor.name(), ".Recall"});
  histogram_tester.ExpectBucketCount(
      kUmaName, PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      kUmaName, PredictorConfusionMatrix::kFalseNegative, 1);
}

// Test that speculation rules tags merging works as expected if multiple
// matched rules applies.
TEST_F(PreloadingDeciderTest,
       SpeculationRulesTagsMergingForNonImmediatePrefetch) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate_1 =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kConservative);

  auto candidate_2 = candidate_1.Clone();
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kModerate;

  candidate_1->tags = {"tag1"};
  candidate_2->tags = {"tag2"};

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());

  // Add conservative and moderate preload candidate and preload on
  // pointer-down.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  preloading_decider->OnPointerDown(url);

  EXPECT_TRUE(prefetches[0]->request().speculation_rules_tags());
  EXPECT_EQ(prefetches[0]
                ->request()
                .speculation_rules_tags()
                ->ConvertStringToHeaderString(),
            "\"tag1\", \"tag2\"");
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      url, blink::mojom::SpeculationAction::kPrefetch));
}

// Test that no speculation rules tags merging happens if multiple candidates
// are in the queue but only one is enacted.
TEST_F(PreloadingDeciderTest,
       SpeculationRulesTagsNoMergingForNonImmediatePrefetch) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate_1 =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kConservative);

  auto candidate_2 = candidate_1.Clone();
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kModerate;

  candidate_1->tags = {"tag1"};
  candidate_2->tags = {"tag2"};

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());

  // Add conservative and moderate preload candidate and preload on
  // pointer-hover.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  preloading_decider->OnPointerHover(
      url,
      blink::mojom::AnchorElementPointerData::New(
          /*is_mouse_pointer=*/true,
          /*mouse_velocity=*/75.0,
          /*mouse_acceleration=*/0.0),
      blink::mojom::SpeculationEagerness::kModerate);

  EXPECT_TRUE(prefetches[0]->request().speculation_rules_tags());
  EXPECT_EQ(prefetches[0]
                ->request()
                .speculation_rules_tags()
                ->ConvertStringToHeaderString(),
            "\"tag2\"");
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      url, blink::mojom::SpeculationAction::kPrefetch));
}

// Test that speculation rules tags merging works as expected if multiple
// immediate rules added.
TEST_F(PreloadingDeciderTest, SpeculationRulesTagsMergingForImmediatePrefetch) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate_1 =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kImmediate);

  auto candidate_2 = candidate_1.Clone();

  candidate_1->tags = {"tag1"};
  candidate_2->tags = {"tag2"};

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());

  preloading_decider->UpdateSpeculationCandidates(candidates);
  const auto& prefetches = GetPrefetchService()->prefetches_;

  EXPECT_TRUE(prefetches[0]->request().speculation_rules_tags());
  EXPECT_EQ(prefetches[0]
                ->request()
                .speculation_rules_tags()
                ->ConvertStringToHeaderString(),
            "\"tag1\", \"tag2\"");
  EXPECT_FALSE(preloading_decider->IsOnStandByForTesting(
      url, blink::mojom::SpeculationAction::kPrefetch));
}

class PreloadingDeciderWithParameterizedSpeculationActionTest
    : public PreloadingDeciderTest,
      public ::testing::WithParamInterface<blink::mojom::SpeculationAction> {
 public:
  PreloadingDeciderWithParameterizedSpeculationActionTest() = default;

  void SetUp() override {
    PreloadingDeciderTest::SetUp();

    if (GetSpeculationAction() == blink::mojom::SpeculationAction::kPrerender ||
        GetSpeculationAction() ==
            blink::mojom::SpeculationAction::kPrerenderUntilScript) {
      old_prerenderer_ =
          PreloadingDecider::GetOrCreateForCurrentDocument(
              &GetPrimaryMainFrame())
              ->SetPrerendererForTesting(std::make_unique<MockPrerenderer>());
    }
  }

  void TearDown() override {
    if (old_prerenderer_) {
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame())
          ->SetPrerendererForTesting(std::move(old_prerenderer_));
    }

    PreloadingDeciderTest::TearDown();
  }

  blink::mojom::SpeculationAction GetSpeculationAction() { return GetParam(); }

  MockPrerenderer* GetPrerenderer() {
    return static_cast<MockPrerenderer*>(
        &PreloadingDecider::GetOrCreateForCurrentDocument(
             &GetPrimaryMainFrame())
             ->GetPrerendererForTesting());
  }

  size_t GetNumOfExistingPreloads() {
    switch (GetSpeculationAction()) {
      case blink::mojom::SpeculationAction::kPrefetch:
        return GetPrefetchService()->prefetches_.size();
      case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
        NOTREACHED();
      case blink::mojom::SpeculationAction::kPrerender:
      case blink::mojom::SpeculationAction::kPrerenderUntilScript:
        return GetPrerenderer()->prerenders_.size();
    }
  }

  void DiscardPreloads(size_t index) {
    switch (GetSpeculationAction()) {
      case blink::mojom::SpeculationAction::kPrefetch:
        GetPrefetchService()->EvictPrefetch(index);
        break;
      case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
        NOTREACHED();
      case blink::mojom::SpeculationAction::kPrerender:
      case blink::mojom::SpeculationAction::kPrerenderUntilScript:
        GetPrerenderer()->OnCancel(index);
        break;
    }
  }

 private:
  std::unique_ptr<Prerenderer> old_prerenderer_;
};

INSTANTIATE_TEST_SUITE_P(
    ParameterizedTests,
    PreloadingDeciderWithParameterizedSpeculationActionTest,
    testing::Values(blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationAction::kPrerenderUntilScript),
    [](const testing::TestParamInfo<blink::mojom::SpeculationAction>& info) {
      switch (info.param) {
        case blink::mojom::SpeculationAction::kPrefetch:
          return "kPrefetch";
        case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
          NOTREACHED();
        case blink::mojom::SpeculationAction::kPrerender:
          return "kPrerender";
        case blink::mojom::SpeculationAction::kPrerenderUntilScript:
          return "kPrerenderUntilScript";
      }
    });

// Tests that an immediate candidate is always processed before a non-immediate
// candidate with the same URL, and that the non-immediate candidate isn't
// marked as "on-standby".
TEST_P(PreloadingDeciderWithParameterizedSpeculationActionTest,
       CandidateWithMultipleEagernessValues) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate_1 =
      MakeCandidate(url, GetSpeculationAction(),
                    blink::mojom::SpeculationEagerness::kConservative);

  auto candidate_2 = candidate_1.Clone();
  candidate_2->eagerness = blink::mojom::SpeculationEagerness::kImmediate;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());

  // Add conservative preload candidate and preload on pointer-down.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  ASSERT_EQ(1u, GetNumOfExistingPreloads());
  auto get_preload_eagerness = [&]() {
    switch (GetSpeculationAction()) {
      case blink::mojom::SpeculationAction::kPrefetch:
        return GetPrefetchService()
            ->prefetches_[0]
            ->request()
            .prefetch_type()
            .GetEagerness();
      case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
        NOTREACHED();
      case blink::mojom::SpeculationAction::kPrerenderUntilScript:
      case blink::mojom::SpeculationAction::kPrerender:
        return GetPrerenderer()->prerenders_[0].eagerness;
    }
  };
  EXPECT_EQ(get_preload_eagerness(),
            blink::mojom::SpeculationEagerness::kImmediate);
  EXPECT_FALSE(
      preloading_decider->IsOnStandByForTesting(url, GetSpeculationAction()));
}

// Tests that a previously preloaded conservative candidate can be
// reprocessed after discard (when retriggered).
TEST_P(PreloadingDeciderWithParameterizedSpeculationActionTest,
       CandidateCanBeReprefetchedAfterDiscard) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  auto candidate =
      MakeCandidate(url, GetSpeculationAction(),
                    blink::mojom::SpeculationEagerness::kConservative);
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate.Clone());

  // Add conservative preloading candidate and preload on pointer-down.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(0u, GetNumOfExistingPreloads());
  preloading_decider->OnPointerDown(url);
  EXPECT_EQ(1u, GetNumOfExistingPreloads());

  // Simulate discard of non-immediate preload.
  DiscardPreloads(0);
  EXPECT_EQ(0u, GetNumOfExistingPreloads());

  // Trigger preload for same URL again, it should succeed.
  preloading_decider->OnPointerDown(url);
  EXPECT_EQ(1u, GetNumOfExistingPreloads());

  // Simulate discard of non-immediate preload.
  DiscardPreloads(0);
  EXPECT_EQ(0u, GetNumOfExistingPreloads());

  auto immediate_candidate = candidate.Clone();
  candidate->eagerness = blink::mojom::SpeculationEagerness::kImmediate;
  candidates.clear();
  candidates.push_back(candidate.Clone());
  candidates.push_back(immediate_candidate.Clone());

  // Add a new immediate candidate (but also send the old non-immediate
  // candidate). A preload should automatically trigger.
  preloading_decider->UpdateSpeculationCandidates(candidates);
  EXPECT_EQ(1u, GetNumOfExistingPreloads());
}

// Tests that candidate removal causes a prefetch to be destroyed, and that
// a reinserted candidate with the same url is re-processed.
TEST_F(PreloadingDeciderTest, ProcessCandidates_ImmediateCandidateRemoval) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url_1 = GetSameOriginUrl("/candidate1.html");
  const GURL url_2 = GetSameOriginUrl("/candidate2.html");

  auto candidate_1 =
      MakeCandidate(url_1, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kImmediate);
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

// Tests that candidate removal works correctly for non-immediate candidates,
// and that a non-immediate candidate is reprocessed correctly after
// re-insertion.
TEST_F(PreloadingDeciderTest, ProcessCandidates_NonImmediateCandidateRemoval) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url_1 = GetSameOriginUrl("/candidate1.html");
  const GURL url_2 = GetSameOriginUrl("/candidate2.html");

  auto candidate_1 =
      MakeCandidate(url_1, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kImmediate);

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
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  const GURL url = GetSameOriginUrl("/candidate.html");

  auto candidate_1 =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kImmediate);

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
      url1,
      blink::mojom::AnchorElementPointerData::New(
          /*is_mouse_pointer=*/true,
          /*mouse_velocity=*/50.0,
          /*mouse_acceleration=*/0.0),
      /*is_eager=*/blink::mojom::SpeculationEagerness::kModerate);

  GURL url2{"https://www.google.com"};
  preloading_decider->OnPointerHover(
      url2,
      blink::mojom::AnchorElementPointerData::New(
          /*is_mouse_pointer=*/true,
          /*mouse_velocity=*/75.0,
          /*mouse_acceleration=*/0.0),
      /*is_eager=*/blink::mojom::SpeculationEagerness::kModerate);

  // Navigate to `url2`.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);

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

TEST_F(PreloadingDeciderTest,
       ModerateViewportHeuristicPredictionIsNotEnactedIfDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kPreloadingModerateViewportHeuristics,
      {{"enact_candidates", "false"}});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnModerateViewportHeuristicTriggered(url);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  EXPECT_TRUE(prefetches.empty());
}

TEST_F(
    PreloadingDeciderTest,
    ModerateViewportHeuristicPredictionIsEnactedForModeratePrefetchCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kPreloadingModerateViewportHeuristics,
      {{"enact_candidates", "true"}});

  base::HistogramTester histogram_tester;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnModerateViewportHeuristicTriggered(url);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  ASSERT_EQ(prefetches.size(), 1u);
  EXPECT_EQ(prefetches[0]->GetURL(), url);

  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation->Start();

  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.ViewportHeuristic.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.ViewportHeuristic.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
}

TEST_F(PreloadingDeciderTest,
       EagerViewportHeuristicPredictionIsEnactedForEagerPrefetchCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kPreloadingEagerViewportHeuristics);

  base::HistogramTester histogram_tester;
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kEager);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnEagerViewportHeuristicTriggered(url);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  ASSERT_EQ(prefetches.size(), 1u);
  EXPECT_EQ(prefetches[0]->GetURL(), url);

  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation->Start();

  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.EagerViewportHeuristic.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.EagerViewportHeuristic.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
}

TEST_F(PreloadingDeciderTest,
       ModerateViewportHeuristicIsEnactedForModeratePrerenderCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kPreloadingModerateViewportHeuristics,
      {{"enact_candidates", "true"}});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);
  ScopedMockPrerenderer mock_prerender(preloading_decider);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationEagerness::kModerate);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnModerateViewportHeuristicTriggered(url);
  ASSERT_EQ(mock_prerender.Get()->prerenders_.size(), 1u);
  EXPECT_EQ(mock_prerender.Get()->prerenders_[0].url, url);
}

TEST_F(PreloadingDeciderTest,
       EagerViewportHeuristicIsEnactedForEagerPrerenderCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kPreloadingEagerViewportHeuristics);

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);
  ScopedMockPrerenderer mock_prerender(preloading_decider);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationEagerness::kEager);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnEagerViewportHeuristicTriggered(url);
  ASSERT_EQ(mock_prerender.Get()->prerenders_.size(), 1u);
  EXPECT_EQ(mock_prerender.Get()->prerenders_[0].url, url);
}

TEST_F(PreloadingDeciderTest,
       ViewportHeuristicsAreNotEnactedForConservativePrefetchCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kPreloadingModerateViewportHeuristics,
        {{"enact_candidates", "true"}}},
       {blink::features::kPreloadingEagerViewportHeuristics, {}}},
      /*disabled_features=*/{});

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider != nullptr);

  const GURL url("https://example.com");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  auto candidate =
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kConservative);
  candidates.push_back(std::move(candidate));
  preloading_decider->UpdateSpeculationCandidates(candidates);

  preloading_decider->OnModerateViewportHeuristicTriggered(url);
  preloading_decider->OnEagerViewportHeuristicTriggered(url);
  const auto& prefetches = GetPrefetchService()->prefetches_;
  EXPECT_TRUE(prefetches.empty());
}

class PreloadingDeciderMLModelTest
    : public PreloadingDeciderTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PreloadingDeciderMLModelTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPreloadingHeuristicsMLModel,
          {{"enact_candidates", base::ToString(GetParam())}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ParameterizedTests,
                         PreloadingDeciderMLModelTest,
                         testing::Bool());

TEST_P(PreloadingDeciderMLModelTest, OnPreloadingHeuristicsModelDone) {
  base::HistogramTester histogram_tester;

  GURL url1{"https://www.example.com"};
  GetPrimaryMainFrame().OnPreloadingHeuristicsModelDone(
      /*url=*/url1, /*score=*/0.2);

  GURL url2{"https://www.google.com"};
  GetPrimaryMainFrame().OnPreloadingHeuristicsModelDone(
      /*url=*/url2, /*score=*/0.9);

  // Navigate to `url2`.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);

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

TEST_P(PreloadingDeciderMLModelTest, UseHoverHeuristicWhenNoMLModelPresent) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;

  EXPECT_TRUE(prefetches.empty());
  // The page has never received a prediction from the ML model, so we fallback
  // to the decisions of the hover heuristic.
  preloading_decider->OnPointerHover(
      url, blink::mojom::AnchorElementPointerData::New(true, 0.0, 0.0),
      blink::mojom::SpeculationEagerness::kModerate);
  EXPECT_EQ(1u, prefetches.size());
}

class PreloadingDeciderMLModelActiveTest : public PreloadingDeciderTest {
 public:
  PreloadingDeciderMLModelActiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPreloadingHeuristicsMLModel,
          {{"enact_candidates", "true"},
           {"prefetch_moderate_threshold", "40"},
           {"prerender_moderate_threshold", "60"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PreloadingDeciderMLModelActiveTest,
       ModelEnactsModeratePrefetchCandidate) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;

  EXPECT_TRUE(prefetches.empty());
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.99);
  EXPECT_EQ(1u, prefetches.size());
}

TEST_F(PreloadingDeciderMLModelActiveTest,
       ModelEnactsModeratePrerenderCandidate) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  ScopedMockPrerenderer prerenderer(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prerenders = prerenderer.Get()->prerenders_;

  EXPECT_TRUE(prerenders.empty());
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.99);
  EXPECT_EQ(1u, prerenders.size());
}

TEST_F(PreloadingDeciderMLModelActiveTest,
       ModelPrerendersCandidateOverPrefetch) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationEagerness::kModerate));
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  ScopedMockPrerenderer prerenderer(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;
  const auto& prerenders = prerenderer.Get()->prerenders_;

  EXPECT_TRUE(prefetches.empty());
  EXPECT_TRUE(prerenders.empty());
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.99);
  EXPECT_TRUE(prefetches.empty());
  EXPECT_EQ(1u, prerenders.size());
}

TEST_F(PreloadingDeciderMLModelActiveTest, ModelConfidenceThreshold) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrerender,
                    blink::mojom::SpeculationEagerness::kModerate));
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  ScopedMockPrerenderer prerenderer(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;
  const auto& prerenders = prerenderer.Get()->prerenders_;

  EXPECT_TRUE(prefetches.empty());
  EXPECT_TRUE(prerenders.empty());
  // The test is configured such that this is a high enough confidence for
  // prefetch, but not for prerender.
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.50);
  EXPECT_EQ(1u, prefetches.size());
  EXPECT_TRUE(prerenders.empty());
}

TEST_F(PreloadingDeciderMLModelActiveTest, ModelNoPreconnectFallback) {
  const GURL url = GetSameOriginUrl("/candidate1.html");

  MockContentBrowserClient browser_client;

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  ScopedMockPrerenderer prerenderer(preloading_decider);
  auto* preconnect_delegate = browser_client.GetDelegate();

  const auto& prefetches = GetPrefetchService()->prefetches_;
  const auto& prerenders = prerenderer.Get()->prerenders_;

  EXPECT_FALSE(preconnect_delegate->Target().has_value());
  EXPECT_TRUE(prefetches.empty());
  EXPECT_TRUE(prerenders.empty());
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.99);
  EXPECT_FALSE(preconnect_delegate->Target().has_value());
  EXPECT_TRUE(prefetches.empty());
  EXPECT_TRUE(prerenders.empty());
}

TEST_F(PreloadingDeciderMLModelActiveTest, ModelSupersedesHoverHeuristic) {
  const GURL url = GetSameOriginUrl("/candidate1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                    blink::mojom::SpeculationEagerness::kModerate));

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  preloading_decider->UpdateSpeculationCandidates(candidates);

  const auto& prefetches = GetPrefetchService()->prefetches_;

  EXPECT_TRUE(prefetches.empty());
  preloading_decider->OnPreloadingHeuristicsModelDone(url, /*score=*/0.05);
  EXPECT_TRUE(prefetches.empty());
  // The model has indicated that the candidate is not worth prefetching, so we
  // should not prefetch based on the hover heuristic either.
  preloading_decider->OnPointerHover(
      url, blink::mojom::AnchorElementPointerData::New(true, 0.0, 0.0),
      blink::mojom::SpeculationEagerness::kModerate);
  EXPECT_TRUE(prefetches.empty());
  // But once we have a stronger signal like pointer down, we should prefetch.
  preloading_decider->OnPointerDown(url);
  EXPECT_EQ(1u, prefetches.size());
}

}  // namespace

TEST_F(PreloadingDeciderTest, SpeculationRulesTagsMergingForNVSMatch) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  const GURL kPreloadingUrl1("https://example.com/page.html?id=1");
  const GURL kPreloadingUrl2("https://example.com/page.html?id=2");
  const GURL kActivationUrl("https://example.com/page.html?id=3");

  auto make_nvs_candidate = [&](const GURL& url, const std::string& tag) {
    auto candidate =
        MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                      blink::mojom::SpeculationEagerness::kConservative);
    candidate->no_vary_search_hint = network::mojom::NoVarySearch::New(
        network::mojom::SearchParamsVariance::NewNoVaryParams({"id"}),
        /*vary_on_key_order=*/true);
    candidate->tags = {tag};
    return candidate;
  };

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(make_nvs_candidate(kPreloadingUrl1, "tag1"));
  candidates.push_back(make_nvs_candidate(kPreloadingUrl2, "tag2"));

  preloading_decider->UpdateSpeculationCandidates(candidates);

  // Both candidates should be on standby.
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      kPreloadingUrl1, blink::mojom::SpeculationAction::kPrefetch));
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      kPreloadingUrl2, blink::mojom::SpeculationAction::kPrefetch));

  // Now, check if tags are merged correctly for a matching URL.
  const PreloadingDecider::SpeculationCandidateKey lookup_key{
      kActivationUrl, blink::mojom::SpeculationAction::kPrefetch};
  const PreloadingPredictor predictor =
      preloading_predictor::kUrlPointerDownOnAnchor;

  auto merged_tags =
      preloading_decider->GetMergedSpeculationTagsFromSuitableCandidates(
          lookup_key, predictor, PreloadingConfidence{100},
          /*eagerness_to_exclude=*/{});

  // The merged tags should contain tags from both NVS-matched candidates.
  EXPECT_EQ(merged_tags.size(), 2u);
  EXPECT_TRUE(base::Contains(merged_tags, "tag1"));
  EXPECT_TRUE(base::Contains(merged_tags, "tag2"));
}

TEST_F(PreloadingDeciderTest,
       SpeculationRulesTagsMergingForNVSMatchWithNullTags) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  const GURL kPreloadingUrl1("https://example.com/page.html?id=1");
  const GURL kPreloadingUrl2("https://example.com/page.html?id=2");
  const GURL kActivationUrl("https://example.com/page.html?id=3");

  auto make_nvs_candidate_no_tags = [&](const GURL& url) {
    auto candidate =
        MakeCandidate(url, blink::mojom::SpeculationAction::kPrefetch,
                      blink::mojom::SpeculationEagerness::kConservative);
    candidate->no_vary_search_hint = network::mojom::NoVarySearch::New(
        network::mojom::SearchParamsVariance::NewNoVaryParams({"id"}),
        /*vary_on_key_order=*/true);
    // No tags specified - candidates->tags remains default ({std::nullopt})
    return candidate;
  };

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(make_nvs_candidate_no_tags(kPreloadingUrl1));
  candidates.push_back(make_nvs_candidate_no_tags(kPreloadingUrl2));

  preloading_decider->UpdateSpeculationCandidates(candidates);

  // Both candidates should be on standby.
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      kPreloadingUrl1, blink::mojom::SpeculationAction::kPrefetch));
  EXPECT_TRUE(preloading_decider->IsOnStandByForTesting(
      kPreloadingUrl2, blink::mojom::SpeculationAction::kPrefetch));

  // Check if tags are merged correctly for a matching URL when no tags exist.
  const PreloadingDecider::SpeculationCandidateKey lookup_key{
      kActivationUrl, blink::mojom::SpeculationAction::kPrefetch};
  const PreloadingPredictor predictor =
      preloading_predictor::kUrlPointerDownOnAnchor;

  auto merged_tags =
      preloading_decider->GetMergedSpeculationTagsFromSuitableCandidates(
          lookup_key, predictor, PreloadingConfidence{100},
          /*eagerness_to_exclude=*/{});

  // The merged tags should contain a single std::nullopt since no candidates
  // have tags.
  EXPECT_EQ(merged_tags.size(), 1u);
  EXPECT_TRUE(base::Contains(merged_tags, std::optional<std::string>{}));
}
}  // namespace content

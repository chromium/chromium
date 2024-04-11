// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerenderer_impl.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrerendererTest : public RenderViewHostTestHarness {
 public:
  PrerendererTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(
            *web_contents_);
    web_contents_->NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl* GetRenderFrameHost() {
    return web_contents_->GetPrimaryMainFrame();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return GURL("https://example2.com" + path);
  }

  PrerenderHostRegistry* GetPrerenderHostRegistry() const {
    return web_contents_->GetPrerenderHostRegistry();
  }

  blink::mojom::SpeculationCandidatePtr CreatePrerenderCandidate(
      const GURL& url) {
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->action = blink::mojom::SpeculationAction::kPrerender;
    candidate->url = url;
    candidate->referrer = blink::mojom::Referrer::New();
    candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
    return candidate;
  }

  blink::mojom::SpeculationCandidatePtr CreatePrerenderCandidateWithEagerness(
      const GURL& url,
      blink::mojom::SpeculationEagerness eagerness) {
    blink::mojom::SpeculationCandidatePtr candidate =
        CreatePrerenderCandidate(url);
    candidate->eagerness = eagerness;
    return candidate;
  }

 private:
  test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

// Tests that Prerenderer starts prerendering when it receives prerender
// speculation candidates.
TEST_F(PrerendererTest, StartPrerender) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/empty.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(kPrerenderingUrl));

  prerenderer.ProcessCandidatesForPrerender(std::move(candidates));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that Prerenderer should not start prerendering when
// kLCPTimingPredictorPrerender2 is enabled and until OnLCPPredicted is called.
TEST_F(PrerendererTest, LCPTimingPredictorPrerender2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kLCPTimingPredictorPrerender2);

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/empty.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(kPrerenderingUrl));

  prerenderer.ProcessCandidatesForPrerender(std::move(candidates));
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  prerenderer.OnLCPPredicted();
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that Prerenderer will skip a cross-site candidate even if it is the
// first prerender candidate in the candidate list.
TEST_F(PrerendererTest, ProcessFirstSameOriginPrerenderCandidate) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  const GURL kFirstPrerenderingUrlCrossSite = GetCrossSiteUrl("/title.html");
  const GURL kSecondPrerenderingUrlSameOrigin =
      GetSameOriginUrl("/title1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      CreatePrerenderCandidate(kFirstPrerenderingUrlCrossSite));
  candidates.push_back(
      CreatePrerenderCandidate(kSecondPrerenderingUrlSameOrigin));

  prerenderer.ProcessCandidatesForPrerender(std::move(candidates));

  // The first prerender candidate is a cross-site one, so Prerenderer should
  // not prerender it.
  EXPECT_FALSE(
      registry->FindHostByUrlForTesting(kFirstPrerenderingUrlCrossSite));
  // The second element in this list is the first same-origin prerender
  // candidate, so Prerenderer should prerender this candidate.
  EXPECT_TRUE(
      registry->FindHostByUrlForTesting(kSecondPrerenderingUrlSameOrigin));
}

class PrerenderHostRegistryObserver : public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderHostRegistryObserver(
      PrerenderHostRegistry* prerender_host_registry) {
    observation_.Observe(prerender_host_registry);
  }

  void OnTrigger(const GURL& url) override { trigger_sequence_.push_back(url); }

  void OnRegistryDestroyed() override { observation_.Reset(); }

  const std::vector<GURL>& GetTriggerSequence() { return trigger_sequence_; }

 private:
  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};

  std::vector<GURL> trigger_sequence_;
};

// Test that ProcessCandidatesForPrerender will trigger candidates in the same
// order as the input of the candidate list (crbug.com/1505301).
TEST_F(PrerendererTest, TriggerPrerenderWithInsertionOrder) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerenderHostRegistryObserver observer{registry};
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  std::vector<GURL> urls = {
      GetSameOriginUrl("/empty.html?a"),
      GetSameOriginUrl("/empty.html?c"),
      GetSameOriginUrl("/empty.html?b"),
  };

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  for (const auto& url : urls) {
    candidates.push_back(CreatePrerenderCandidate(url));
  }

  prerenderer.ProcessCandidatesForPrerender(std::move(candidates));

  for (const auto& url : urls) {
    ASSERT_TRUE(registry->FindHostByUrlForTesting(url));
  }
  EXPECT_EQ(observer.GetTriggerSequence(), urls);
}

// Tests that Prerenderer will remove the rendered host, if the url is removed
// from candidates list.
TEST_F(PrerendererTest, RemoveRendererHostAfterCandidateRemoved) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  const GURL urls[]{GetSameOriginUrl("/title1.html"),
                    GetSameOriginUrl("/title2.html")};
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  for (const auto& url : urls) {
    candidates.push_back(CreatePrerenderCandidate(url));
  }
  prerenderer.ProcessCandidatesForPrerender(std::move(candidates));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(urls[0]));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(urls[1]));

  std::vector<blink::mojom::SpeculationCandidatePtr> new_candidates;
  new_candidates.push_back(CreatePrerenderCandidate(urls[1]));
  prerenderer.ProcessCandidatesForPrerender(std::move(new_candidates));
  EXPECT_FALSE(registry->FindHostByUrlForTesting(urls[0]));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(urls[1]));

  prerenderer.ProcessCandidatesForPrerender(
      std::vector<blink::mojom::SpeculationCandidatePtr>{});
  EXPECT_FALSE(registry->FindHostByUrlForTesting(urls[0]));
  EXPECT_FALSE(registry->FindHostByUrlForTesting(urls[1]));
}

class PrerendererNewLimitAndSchedulerTest : public PrerendererTest {
 public:
  PrerendererNewLimitAndSchedulerTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_non_eager_prerenders",
            base::NumberToString(
                MaxNumOfRunningSpeculationRulesNonEagerPrerenders())}}}},
        {});
  }

  int MaxNumOfRunningSpeculationRulesNonEagerPrerenders() { return 2; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that Prerenderer will remove the host if the host is canceled with
// non-eager limit, and the canceled host can be reprocessed.
TEST_F(PrerendererNewLimitAndSchedulerTest,
       RemoveRendererHostAfterNonEagerLimitCancel) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  std::vector<GURL> urls;

  // Prerender as many times as limit + 1. All prerenders should be started
  // once.
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesNonEagerPrerenders() + 1;
       i++) {
    const GURL url = GetSameOriginUrl("/empty.html?" + base::ToString(i));
    urls.push_back(url);
    blink::mojom::SpeculationCandidatePtr candidate =
        CreatePrerenderCandidateWithEagerness(
            url, blink::mojom::SpeculationEagerness::kConservative);
    prerenderer.MaybePrerender(std::move(candidate),
                               preloading_predictor::kUnspecified,
                               PreloadingConfidence{100});

    EXPECT_TRUE(registry->FindHostByUrlForTesting(url));
  }

  for (int i = 0; i < MaxNumOfRunningSpeculationRulesNonEagerPrerenders() + 1;
       i++) {
    if (i == 0) {
      // The first (= oldest) prerender should be removed since the (limit +
      // 1)-th prerender was started.
      EXPECT_FALSE(registry->FindHostByUrlForTesting(urls[i]));
    } else {
      EXPECT_TRUE(registry->FindHostByUrlForTesting(urls[i]));
    }
  }

  // Retrigger canceled host. It should be started and instead the second oldest
  // prerender should be canceled.
  blink::mojom::SpeculationCandidatePtr candidate =
      CreatePrerenderCandidateWithEagerness(
          urls[0], blink::mojom::SpeculationEagerness::kConservative);
  prerenderer.MaybePrerender(std::move(candidate),
                             preloading_predictor::kUnspecified,
                             PreloadingConfidence{100});
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesNonEagerPrerenders() + 1;
       i++) {
    if (i == 1) {
      EXPECT_FALSE(registry->FindHostByUrlForTesting(urls[i]));
    } else {
      EXPECT_TRUE(registry->FindHostByUrlForTesting(urls[i]));
    }
  }
}

// Tests that it is possible to start a prerender using MaybePrerender and
// ShouldWaitForPrerenderResult methods.
TEST_F(PrerendererTest, MaybePrerenderAndShouldWaitForPrerenderResult) {
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  PrerendererImpl prerenderer(*GetRenderFrameHost());

  const GURL kUrlToCancel = GetSameOriginUrl("/to_cancel.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidateToCancel;
  candidateToCancel.push_back(CreatePrerenderCandidate(kUrlToCancel));

  // Candidate is not processed yet. So, it should return false.
  EXPECT_FALSE(prerenderer.ShouldWaitForPrerenderResult(kUrlToCancel));
  // Process the candidate.
  prerenderer.ProcessCandidatesForPrerender(std::move(candidateToCancel));
  EXPECT_TRUE(prerenderer.ShouldWaitForPrerenderResult(kUrlToCancel));
  // Cancel the prerender
  prerenderer.ProcessCandidatesForPrerender(
      std::vector<blink::mojom::SpeculationCandidatePtr>{});
  EXPECT_FALSE(prerenderer.ShouldWaitForPrerenderResult(kUrlToCancel));

  const GURL kPrerenderingUrl = GetSameOriginUrl("/empty.html");
  const auto candidate = CreatePrerenderCandidate(kPrerenderingUrl);

  // Candidate is not processed yet. So, it should return false.
  EXPECT_FALSE(prerenderer.ShouldWaitForPrerenderResult(kPrerenderingUrl));
  // MaybePrerender the candidate and check if ShouldWaitForPrerenderResult
  // returns true.
  EXPECT_TRUE(prerenderer.MaybePrerender(candidate,
                                         preloading_predictor::kUnspecified,
                                         PreloadingConfidence{100}));
  EXPECT_TRUE(prerenderer.ShouldWaitForPrerenderResult(kPrerenderingUrl));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

}  // namespace
}  // namespace content

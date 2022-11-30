// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/system/functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class PrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  PrerenderWebContentsDelegate() = default;

  bool IsPrerender2Supported(WebContents& web_contents) override {
    return true;
  }
};

class SpeculationHostImplTest : public RenderViewHostImplTestHarness {
 public:
  SpeculationHostImplTest() = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->SetDelegate(&web_contents_delegate_);
    web_contents_->NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
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
    return candidate;
  }

 private:
  test::ScopedPrerenderFeatureList prerender_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  PrerenderWebContentsDelegate web_contents_delegate_;
};

class ScopedPreloadingDeciderObserver
    : public PreloadingDeciderObserverForTesting {
 public:
  explicit ScopedPreloadingDeciderObserver(RenderFrameHostImpl* rfh)
      : rfh_(rfh) {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    old_observer_ = preloading_decider->SetObserverForTesting(this);
  }
  ~ScopedPreloadingDeciderObserver() override {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    EXPECT_EQ(this, preloading_decider->SetObserverForTesting(old_observer_));
  }

  void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override {
    for (const auto& candidate : candidates) {
      candidates_.push_back(candidate.Clone());
    }
  }
  void OnPointerDown(const GURL& url) override {}
  void OnPointerHover(const GURL& url) override {}

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates_;

 private:
  raw_ptr<RenderFrameHostImpl> rfh_;
  raw_ptr<PreloadingDeciderObserverForTesting> old_observer_;
};

// Tests that SpeculationHostImpl dispatches the candidates to
// PreloadingDecider.
TEST_F(SpeculationHostImplTest, StartPrerender) {
  ScopedPreloadingDeciderObserver observer(GetRenderFrameHost());
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  mojo::Remote<blink::mojom::SpeculationHost> remote;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/empty.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(kPrerenderingUrl));

  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
  EXPECT_EQ(1u, observer.candidates_.size());
  EXPECT_EQ(kPrerenderingUrl, observer.candidates_[0]->url);
}

// Tests that SpeculationHostImpl crashes the renderer process if it receives
// non-http prerender candidates.
TEST_F(SpeculationHostImplTest, ReportNonHttpMessage) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  mojo::Remote<blink::mojom::SpeculationHost> remote;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  const GURL kPrerenderingUrl = GURL("blob:https://bar");
  candidates.push_back(CreatePrerenderCandidate(kPrerenderingUrl));

  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "SH_NON_HTTP");
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that SpeculationHostImpl crashes the renderer process if it receives
// prefetch candidates that have a valid `target_browsing_context_name_hint`.
TEST_F(SpeculationHostImplTest,
       ReportTargetBrowsingContextNameHintOnPrefetchCandidate) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  mojo::Remote<blink::mojom::SpeculationHost> remote;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  // Create a prefetch candidate that has a valid target hint.
  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->url = GetSameOriginUrl("/empty.html");
  candidate->referrer = blink::mojom::Referrer::New();
  candidate->target_browsing_context_name_hint =
      blink::mojom::SpeculationTargetHint::kBlank;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(std::move(candidate));

  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "SH_TARGET_HINT_ON_PREFETCH");
}

class TestSpeculationHostDelegate : public SpeculationHostDelegate {
 public:
  TestSpeculationHostDelegate() = default;
  ~TestSpeculationHostDelegate() override = default;

  // Disallows copy and move operations.
  TestSpeculationHostDelegate(const TestSpeculationHostDelegate&) = delete;
  TestSpeculationHostDelegate& operator=(const TestSpeculationHostDelegate&) =
      delete;

  // SpeculationRulesDelegate implementation.
  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
      base::WeakPtr<
          content::SpeculationHostDevToolsObserver> /*devtools_observer*/)
      override {
    candidates.clear();
  }
};

class ScopedSpeculationHostImplContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ScopedSpeculationHostImplContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedSpeculationHostImplContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  // ContentBrowserClient implementation.
  std::unique_ptr<SpeculationHostDelegate> CreateSpeculationHostDelegate(
      RenderFrameHost& render_frame_host) override {
    return std::make_unique<TestSpeculationHostDelegate>();
  }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

// Tests that SpeculationHostDelegate can take the process candidates away and
// SpeculationHostImpl cannot handle the processed ones.
TEST_F(SpeculationHostImplTest, AllCandidatesProcessedByDelegate) {
  ScopedSpeculationHostImplContentBrowserClient test_browser_client;
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  mojo::Remote<blink::mojom::SpeculationHost> remote;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/empty.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(kPrerenderingUrl));

  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();

  // Since SpeculationHostDelegate has removed all candidates,
  // SpeculationHostImpl cannot start prerendering for the prerender candidate.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

}  // namespace
}  // namespace content

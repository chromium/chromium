// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/speculation_host_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/public/common/content_client.h"
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
  SpeculationHostImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }

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
    return web_contents_->GetMainFrame();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
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
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  PrerenderWebContentsDelegate web_contents_delegate_;
};

// Tests that SpeculationHostImpl starts prerendering when it receives prerender
// speculation candidates.
TEST_F(SpeculationHostImplTest, StartPrerender) {
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
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that SpeculationHostImpl will skip a cross-origin candidate even if it
// is the first prerender candidate in the candidate list.
TEST_F(SpeculationHostImplTest, ProcessFirstSameOriginPrerenderCandidate) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  mojo::Remote<blink::mojom::SpeculationHost> remote;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote.BindNewPipeAndPassReceiver());

  const GURL kFirstPrerenderingUrlCrossOrigin =
      GetCrossOriginUrl("/title.html");
  const GURL kSecondPrerenderingUrlSameOrigin =
      GetSameOriginUrl("/title1.html");
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(
      CreatePrerenderCandidate(kFirstPrerenderingUrlCrossOrigin));
  candidates.push_back(
      CreatePrerenderCandidate(kSecondPrerenderingUrlSameOrigin));

  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();

  // The first prerender candidate is a cross-origin one, so SpeculationHostImpl
  // should not prerender it.
  EXPECT_FALSE(
      registry->FindHostByUrlForTesting(kFirstPrerenderingUrlCrossOrigin));
  // The second element in this list is the first same-origin prerender
  // candidate, so SpeculationHostImpl should prerender this candidate.
  EXPECT_TRUE(
      registry->FindHostByUrlForTesting(kSecondPrerenderingUrlSameOrigin));
}

// Tests that SpeculationHostImpl crash the renderer process if it receives
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
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) override {
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

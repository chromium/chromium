// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;
};

class PrerenderHostTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostTest() = default;
  ~PrerenderHostTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  void ExpectFinalStatus(PrerenderHost::FinalStatus status) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus", status, 1);
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents_delegate_ = std::make_unique<TestWebContentsDelegate>();
    web_contents->SetDelegate(web_contents_delegate_.get());
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContentsDelegate> web_contents_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrerenderHostTest, Activate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  ASSERT_TRUE(initiator_rfh);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_rfh->GetLastCommittedOrigin(),
      *web_contents);

  // Start the prerendering navigation.
  prerender_host->StartPrerendering();

  // Finish the prerendering navigation. Normally we could use
  // EmbeddedTestServer to provide a response, but this test uses
  // RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order
  // to complete. Use NavigationSimulator to finish the navigation on the
  // WebContents.
  WebContents* prerender_contents = WebContents::FromRenderFrameHost(
      prerender_host->GetPrerenderedMainFrameHostForTesting());
  ASSERT_TRUE(prerender_contents);
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulator::CreateFromPending(prerender_contents);
  sim->ReadyToCommit();
  sim->Commit();
  EXPECT_TRUE(prerender_host->is_ready_for_activation());

  // Activate.
  EXPECT_TRUE(prerender_host->ActivatePrerenderedContents(*initiator_rfh));
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, DontActivate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  ASSERT_TRUE(initiator_rfh);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes), initiator_rfh->GetLastCommittedOrigin(),
      *web_contents);

  // Start the prerendering navigation, but don't activate it.
  prerender_host->StartPrerendering();
  prerender_host.reset();
  ExpectFinalStatus(PrerenderHost::FinalStatus::kDestroyed);
}

}  // namespace
}  // namespace content

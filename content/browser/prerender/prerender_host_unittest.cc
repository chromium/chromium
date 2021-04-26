// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

// Finish a prerendering navigation that was already started with
// CreateAndStartHost().
void CommitPrerenderNavigation(PrerenderHost& host) {
  // Normally we could use EmbeddedTestServer to provide a response, but these
  // tests use RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order to
  // complete. Use NavigationSimulator to finish the navigation.
  FrameTreeNode* ftn = FrameTreeNode::From(host.GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  sim->ReadyToCommit();
  sim->Commit();
  EXPECT_TRUE(host.is_ready_for_activation());
}

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;
};

class PrerenderHostTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
  }

  ~PrerenderHostTest() override = default;

  void SetUp() override {
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
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  CommitPrerenderNavigation(*prerender_host);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page.
  auto sim_2 = NavigationSimulatorImpl::CreateBrowserInitiated(
      kPrerenderingUrl, web_contents.get());
  sim_2->Start();
  sim_2->Commit();
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, DontActivate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  // Start the prerendering navigation, but don't activate it.
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  registry->AbandonHost(prerender_frame_tree_node_id);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kDestroyed);
}

// Tests that main frame navigations in a prerendered page can occur if they
// start after the prerendered page has been reserved for activation.
//
// Regression test for https://crbug.com/1190262.
TEST_F(PrerenderHostTest, MainFrameNavigationForReservedHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  const int prerender_ftn_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_ftn_id);
  CommitPrerenderNavigation(*prerender_host);

  // Reserve the host.
  FrameTreeNode* initiator_ftn = FrameTreeNode::From(initiator_rfh);
  int ftn_id =
      registry->ReserveHostToActivate(kPrerenderingUrl, *initiator_ftn);
  EXPECT_EQ(ftn_id, prerender_ftn_id);

  // Start a cross-origin navigation in the prerendered page. It should not
  // be deferred.
  // TODO(https://crbug.com/1198395): Defer or cancel in this case, which will
  // change this expectation.
  RenderFrameHostImpl* prerender_rfh =
      prerender_host->GetPrerenderedMainFrameHost();
  auto sim_2 = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://example2.test/"), prerender_rfh);
  sim_2->Start();
  EXPECT_FALSE(sim_2->IsDeferred());
  sim_2->Commit();

  // Activate the prerendered page.
  auto sim_3 = NavigationSimulatorImpl::CreateBrowserInitiated(
      kPrerenderingUrl, web_contents.get());
  sim_3->Start();
  prerender_host->Activate(*sim_3->GetNavigationHandle());
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

// Tests that cross-origin subframe navigations in a prerendered page are
// deferred even if they start after the prerendered page has been reserved for
// activation.
//
// Regression test for https://crbug.com/1190262.
TEST_F(PrerenderHostTest, SubframeNavigationForReservedHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  const int prerender_ftn_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_ftn_id);
  CommitPrerenderNavigation(*prerender_host);

  // Reserve the host.
  FrameTreeNode* initiator_ftn = FrameTreeNode::From(initiator_rfh);
  int ftn_id =
      registry->ReserveHostToActivate(kPrerenderingUrl, *initiator_ftn);
  EXPECT_EQ(ftn_id, prerender_ftn_id);

  // Start a cross-origin subframe navigation in the prerendered page. It
  // should be deferred.
  RenderFrameHostImpl* prerender_rfh =
      prerender_host->GetPrerenderedMainFrameHost();
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(prerender_rfh)->AppendChild("subframe");
  std::unique_ptr<NavigationSimulatorImpl> subframe_nav_sim =
      NavigationSimulatorImpl::CreateRendererInitiated(
          GURL("https://example2.test/"), subframe);
  subframe_nav_sim->SetAutoAdvance(false);
  subframe_nav_sim->Start();
  EXPECT_TRUE(subframe_nav_sim->IsDeferred());

  // Activate the prerendered page.
  std::unique_ptr<NavigationSimulatorImpl> sim_2 =
      NavigationSimulatorImpl::CreateBrowserInitiated(kPrerenderingUrl,
                                                      web_contents.get());
  sim_2->Start();
  prerender_host->Activate(*sim_2->GetNavigationHandle());
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);

  // The subframe navigation should no longer be deferred.
  subframe_nav_sim->Wait();
  EXPECT_FALSE(subframe_nav_sim->IsDeferred());
}

}  // namespace
}  // namespace content

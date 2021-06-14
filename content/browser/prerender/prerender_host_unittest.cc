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
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  int prerender_frame_tree_node_id =
      web_contents->AddPrerender(kPrerenderingUrl);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  CommitPrerenderNavigation(*prerender_host);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                    kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, DontActivate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const GURL kPrerenderingUrl("https://example.com/next");

  // Start the prerendering navigation, but don't activate it.
  const int prerender_frame_tree_node_id =
      web_contents->AddPrerender(kPrerenderingUrl);
  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
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
  RenderFrameHostImpl* prerender_rfh =
      web_contents->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  const int prerender_ftn_id = prerender_rfh->GetFrameTreeNodeId();
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_ftn_id);

  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                       initiator_rfh);
  navigation->Start();
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(navigation->GetNavigationHandle());

  EXPECT_EQ(navigation_request->prerender_frame_tree_node_id(),
            prerender_ftn_id);

  // Start a cross-origin navigation in the prerendered page. It should not
  // be deferred.
  // TODO(https://crbug.com/1198395): Defer or cancel in this case, which will
  // change this expectation.
  auto navigation_2 = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://example2.test/"), prerender_rfh);
  navigation_2->Start();
  EXPECT_FALSE(navigation_2->IsDeferred());
  navigation_2->Commit();

  // Activate the prerendered page.
  prerender_host->Activate(*navigation->GetNavigationHandle());
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
  RenderFrameHostImpl* prerender_rfh =
      web_contents->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  const int prerender_ftn_id = prerender_rfh->GetFrameTreeNodeId();
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_ftn_id);

  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                       initiator_rfh);
  navigation->Start();
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(navigation->GetNavigationHandle());

  EXPECT_EQ(navigation_request->prerender_frame_tree_node_id(),
            prerender_ftn_id);

  // Start a cross-origin subframe navigation in the prerendered page. It
  // should be deferred.
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(prerender_rfh)->AppendChild("subframe");
  std::unique_ptr<NavigationSimulatorImpl> subframe_nav_sim =
      NavigationSimulatorImpl::CreateRendererInitiated(
          GURL("https://example2.test/"), subframe);
  subframe_nav_sim->SetAutoAdvance(false);
  subframe_nav_sim->Start();
  EXPECT_TRUE(subframe_nav_sim->IsDeferred());

  // Activate the prerendered page.
  prerender_host->Activate(*navigation->GetNavigationHandle());
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);

  // The subframe navigation should no longer be deferred.
  subframe_nav_sim->Wait();
  EXPECT_FALSE(subframe_nav_sim->IsDeferred());
}

// Tests that an activation can successfully commit after the prerendering page
// has updated its PageState.
TEST_F(PrerenderHostTest, ActivationAfterPageStateUpdate) {
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

  FrameTreeNode* prerender_root_ftn =
      FrameTreeNode::GloballyFindByID(prerender_frame_tree_node_id);
  RenderFrameHostImpl* prerender_rfh = prerender_root_ftn->current_frame_host();
  NavigationEntryImpl* prerender_nav_entry =
      prerender_root_ftn->frame_tree()->controller().GetLastCommittedEntry();
  FrameNavigationEntry* prerender_root_fne =
      prerender_nav_entry->GetFrameEntry(prerender_root_ftn);

  blink::PageState page_state =
      blink::PageState::CreateForTestingWithSequenceNumbers(
          GURL("about:blank"), prerender_root_fne->item_sequence_number(),
          prerender_root_fne->document_sequence_number());

  // Update PageState for prerender RFH, causing it to become different from
  // the one stored in RFH's last commit params.
  static_cast<mojom::FrameHost*>(prerender_rfh)->UpdateState(page_state);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page. The main expectation is that this navigation commits
  // successfully and doesn't hit any DCHECKs.
  NavigationSimulatorImpl::NavigateAndCommitFromBrowser(web_contents.get(),
                                                        kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);

  // Ensure that the the page_state was preserved.
  EXPECT_EQ(web_contents->GetMainFrame(), prerender_rfh);
  NavigationEntryImpl* activated_nav_entry =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_EQ(
      page_state,
      activated_nav_entry->GetFrameEntry(web_contents->GetFrameTree()->root())
          ->page_state());
}

}  // namespace
}  // namespace content

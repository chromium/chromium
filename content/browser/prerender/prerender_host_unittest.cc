// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

// TODO(nhiroki): Merge this into TestNavigationObserver for code
// simplification.
class ActivationObserver : public PrerenderHost::Observer {
 public:
  // PrerenderHost::Observer implementations.
  void OnActivated() override { was_activated_ = true; }
  void OnHostDestroyed() override {
    was_host_destroyed_ = true;
    if (quit_closure_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(quit_closure_));
    }
  }

  void WaitUntilHostDestroyed() {
    if (was_host_destroyed_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_activated() const { return was_activated_; }

 private:
  base::OnceClosure quit_closure_;
  bool was_activated_ = false;
  bool was_host_destroyed_ = false;
};

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

void ActivatePrerenderedPage(const GURL& prerendering_url,
                             WebContentsImpl& web_contents) {
  // Make sure the page for `prerendering_url` has been prerendered.
  PrerenderHostRegistry* registry = web_contents.GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(prerendering_url);
  EXPECT_TRUE(prerender_host);
  int prerender_host_id = prerender_host->frame_tree_node_id();

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Activate the prerendered page.
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(
          prerendering_url, web_contents.GetMainFrame());
  navigation->Commit();
  activation_observer.WaitUntilHostDestroyed();

  EXPECT_EQ(web_contents.GetMainFrame()->GetLastCommittedURL(),
            prerendering_url);

  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindReservedHostById(prerender_host_id), nullptr);
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
  const GURL kOriginUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents(kOriginUrl);
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
  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
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

// Tests that main frame navigations in a prerendered page cannot occur even if
// they start after the prerendered page has been reserved for activation.
TEST_F(PrerenderHostTest, MainFrameNavigationForReservedHost) {
  const GURL kOriginUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents(kOriginUrl);
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  RenderFrameHostImpl* prerender_rfh =
      web_contents->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  const int prerender_ftn_id = prerender_rfh->GetFrameTreeNodeId();
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_ftn_id);
  FrameTreeNode* ftn = prerender_rfh->frame_tree_node();
  EXPECT_FALSE(ftn->HasNavigation());

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;
  MockCommitDeferringConditionWrapper condition(/*is_ready_to_commit=*/false);
  {
    MockCommitDeferringConditionInstaller installer(condition.PassToDelegate());

    // Start trying to activate the prerendered page.
    navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        kPrerenderingUrl, initiator_rfh);
    navigation->Start();

    // Wait for the condition to pause the activation.
    condition.WaitUntilInvoked();
  }

  // The request should be deferred by the condition.
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());

  // The primary page should still be the original page.
  EXPECT_EQ(web_contents->GetURL(), kOriginUrl);

  const GURL kBadUrl("https://example2.test/");
  TestNavigationManager tno(web_contents.get(), kBadUrl);

  // Start a cross-origin navigation in the prerendered page. It should be
  // cancelled.
  auto navigation_2 =
      NavigationSimulatorImpl::CreateRendererInitiated(kBadUrl, prerender_rfh);
  navigation_2->Start();
  EXPECT_EQ(NavigationThrottle::CANCEL,
            navigation_2->GetLastThrottleCheckResult());
  tno.WaitForNavigationFinished();
  EXPECT_FALSE(tno.was_committed());

  // The cross-origin navigation cancels the activation.
  condition.CallResumeClosure();
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kMainFrameNavigation);

  // The activation falls back to regular navigation.
  navigation->ReadyToCommit();
  navigation->Commit();
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
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
  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
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

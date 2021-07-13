// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/speculation_rules/speculation_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

blink::mojom::SpeculationCandidatePtr CreatePrerenderCandidate(
    const GURL& url) {
  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrerender;
  candidate->url = url;
  candidate->referrer = blink::mojom::Referrer::New();
  return candidate;
}

void SendCandidate(const GURL& url,
                   mojo::Remote<blink::mojom::SpeculationHost>& remote) {
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(url));
  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
}

// This definition is needed because this constant is odr-used in gtest macros.
// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
const int kNoFrameTreeNodeId = RenderFrameHost::kNoFrameTreeNodeId;

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

class PrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  PrerenderWebContentsDelegate() = default;

  bool IsPrerender2Supported() override { return true; }
};

class PrerenderHostRegistryTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostRegistryTest() = default;
  ~PrerenderHostRegistryTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents->SetDelegate(&web_contents_delegate_);
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

  RenderFrameHostImpl* NavigatePrimaryPage(TestWebContents* web_contents,
                                           const GURL& dest_url) {
    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(
            dest_url, web_contents->GetMainFrame());
    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Start();
    navigation->Commit();
    RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
    EXPECT_EQ(render_frame_host->GetLastCommittedURL(), dest_url);
    return render_frame_host;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  PrerenderWebContentsDelegate web_contents_delegate_;
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

// Helper method to test that prerendering activation fails when an individual
// NavigationParams parameter value does not match between the initial and
// activation navigations. Use setup_callback to set the individual parameter
// value that is to be tested.
void CheckNotActivatedForParams(
    base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback,
    BrowserContext* browser_context) {
  const GURL kOriginalUrl("https://example.com/");

  std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
      browser_context, SiteInstanceImpl::Create(browser_context)));
  web_contents->NavigateAndCommit(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                       render_frame_host);
  // Change a parameter to differentiate the activation request from the
  // prerendering request.
  std::move(setup_callback).Run(navigation.get());
  navigation->Start();
  NavigationRequest* navigation_request = navigation->GetNavigationHandle();
  EXPECT_FALSE(navigation_request->IsPrerenderedPageActivation());
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHostForSameURL) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl;

  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int frame_tree_node_id1 =
      registry->CreateAndStartHost(std::move(attributes1), *render_frame_host);
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  const int frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *render_frame_host);
  EXPECT_EQ(frame_tree_node_id1, frame_tree_node_id2);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);
  CommitPrerenderNavigation(*prerender_host1);

  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1.
TEST_F(PrerenderHostRegistryTest, NumberLimit_Activation) {
  base::HistogramTester histogram_tester;
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  const GURL kPrerenderingUrl1("https://example.com/next1");
  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl1;

  const GURL kPrerenderingUrl2("https://example.com/next2");
  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl2;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  int frame_tree_node_id1 =
      registry->CreateAndStartHost(std::move(attributes1), *render_frame_host);
  int frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *render_frame_host);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  EXPECT_NE(frame_tree_node_id1, kNoFrameTreeNodeId);
  EXPECT_EQ(frame_tree_node_id2, kNoFrameTreeNodeId);

  // Activate the first prerender.
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl1);
  CommitPrerenderNavigation(*prerender_host1);
  ActivatePrerenderedPage(kPrerenderingUrl1, *web_contents);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl2;
  frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *render_frame_host);
  EXPECT_NE(frame_tree_node_id2, kNoFrameTreeNodeId);
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new same-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_SameOriginNavigateAway) {
  base::HistogramTester histogram_tester;
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidate(kPrerenderingUrl1, remote1);
  SendCandidate(kPrerenderingUrl2, remote1);
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // The initiator document navigates away.
  render_frame_host = NavigatePrimaryPage(
      web_contents.get(), GURL("https://example.com/elsewhere"));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  SendCandidate(kPrerenderingUrl2, remote2);

  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new cross-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_CrossOriginNavigateAway) {
  base::HistogramTester histogram_tester;
  const GURL kOriginalUrl("https://example.com/");

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidate(kPrerenderingUrl1, remote1);
  SendCandidate(kPrerenderingUrl2, remote1);
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // The initiator document navigates away to a cross-origin page.
  render_frame_host =
      NavigatePrimaryPage(web_contents.get(), GURL("https://example.org/"));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  const GURL kPrerenderingUrl3("https://example.org/next1");
  SendCandidate(kPrerenderingUrl3, remote2);
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl3), nullptr);
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
}

TEST_F(PrerenderHostRegistryTest,
       ReserveHostToActivateBeforeReadyForActivation) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  FrameTreeNode* ftn =
      FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulatorImpl> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  // Ensure that navigation in prerendering frame tree does not commit and
  // PrerenderHost doesn't become ready for activation.
  sim->SetAutoAdvance(false);

  EXPECT_FALSE(prerender_host->is_ready_for_activation());

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Start activation.
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                       render_frame_host);
  navigation->Start();

  // Wait until PrerenderCommitDeferringCondition runs.
  // TODO(nhiroki): Avoid using base::RunUntilIdle() and instead use some
  // explicit signal of the running condition.
  base::RunLoop().RunUntilIdle();

  // The activation should be deferred by PrerenderCommitDeferringCondition
  // until the main frame navigation in the prerendering frame tree finishes.
  NavigationRequest* navigation_request = navigation->GetNavigationHandle();
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(), kOriginalUrl);

  // Finish the main frame navigation.
  sim->ReadyToCommit();
  sim->Commit();

  // Finish the activation.
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

TEST_F(PrerenderHostRegistryTest, CancelHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// Test cancelling a prerender while a CommitDeferringCondition is running.
// This activation should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelHostWhileCommitDeferringConditionIsRunning) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  // Start prerendering.
  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

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
        kPrerenderingUrl, render_frame_host);
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
  EXPECT_EQ(web_contents->GetURL(), kOriginalUrl);

  // Cancel the prerender while the CommitDeferringCondition is running.
  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Resume the activation. This should fall back to a regular navigation.
  condition.CallResumeClosure();
  navigation->Commit();
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

// Test cancelling a prerender and then starting a new prerender for the same
// URL while a CommitDeferringCondition is running. This activation should not
// reserve the second prerender and should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelAndStartHostWhileCommitDeferringConditionIsRunning) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

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
        kPrerenderingUrl, render_frame_host);
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
  EXPECT_EQ(web_contents->GetURL(), kOriginalUrl);

  // Cancel the prerender while the CommitDeferringCondition is running.
  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Start the second prerender for the same URL.
  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl;

  const int prerender_frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *render_frame_host);
  ASSERT_NE(prerender_frame_tree_node_id2, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host2 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host2);

  EXPECT_NE(prerender_frame_tree_node_id, prerender_frame_tree_node_id2);

  // Resume the activation. This should not reserve the second prerender and
  // should fall back to a regular navigation.
  condition.CallResumeClosure();
  navigation->Commit();
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);

  // The second prerender should still exist.
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// -------------------------------------------------
// Activation navigation parameter matching unit tests.
// These tests change a parameter to differentiate the activation request from
// the prerendering request.

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_InitiatorFrameToken) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->SetInitiatorFrame(nullptr);
        navigation->set_initiator_origin(url::Origin::Create(kOriginalUrl));
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_Headers) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_headers("User-Agent: Test");
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_LoadFlags) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_ONLY_FROM_CACHE);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SkipServiceWorker) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_skip_service_worker(true);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_MixedContentContextType) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_mixed_content_context_type(
            blink::mojom::MixedContentContextType::kNotMixedContent);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_IsFormSubmission) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetIsFormSubmission(true);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormUrl) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->set_searchable_form_url(kOriginalUrl);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormEncoding) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_searchable_form_encoding("Test encoding");
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_InitiatorOrigin) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_initiator_origin(url::Origin());
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_ShouldCheckMainWorldCSP) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_should_check_main_world_csp(
            network::mojom::CSPDisposition::DO_NOT_CHECK);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_HistoryURLForDataURL) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->set_history_url_for_data_url(kOriginalUrl);
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_Method) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetMethod("POST");
      }),
      std::move(GetBrowserContext()));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_HrefTranslate) {
  CheckNotActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_href_translate("test");
      }),
      std::move(GetBrowserContext()));
}

// End navigation parameter matching tests ---------

}  // namespace
}  // namespace content

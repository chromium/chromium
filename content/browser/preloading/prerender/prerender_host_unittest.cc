// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"

namespace content {
namespace {

using ::testing::_;
using ExpectedReadyForActivationState =
    base::StrongAlias<class ExpectedReadyForActivationStateType, bool>;

// Finish a prerendering navigation that was already started with
// CreateAndStartHost().
void CommitPrerenderNavigation(
    PrerenderHost& host,
    ExpectedReadyForActivationState ready_for_activation =
        ExpectedReadyForActivationState(true)) {
  // Normally we could use EmbeddedTestServer to provide a response, but these
  // tests use RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order to
  // complete. Use NavigationSimulator to finish the navigation.
  FrameTreeNode* ftn = FrameTreeNode::From(host.GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  sim->Commit();
  EXPECT_EQ(host.is_ready_for_activation(), ready_for_activation.value());
}

std::unique_ptr<NavigationSimulatorImpl> CreateActivation(
    const GURL& prerendering_url,
    WebContentsImpl& web_contents) {
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(
          prerendering_url, web_contents.GetPrimaryMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents.GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  return navigation;
}

PrerenderAttributes GeneratePrerenderAttributes(const GURL& url,
                                                RenderFrameHostImpl* rfh) {
  return PrerenderAttributes(
      url, PrerenderTriggerType::kSpeculationRule,
      /*embedder_histogram_suffix=*/"", Referrer(),
      rfh->GetLastCommittedOrigin(), rfh->GetLastCommittedURL(),
      rfh->GetProcess()->GetID(), rfh->GetFrameToken(),
      rfh->GetFrameTreeNodeId(), rfh->GetPageUkmSourceId(),
      ui::PAGE_TRANSITION_LINK,
      /*url_match_predicate=*/absl::nullopt);
}

PrerenderAttributes GeneratePrerenderAttributesWithPredicate(
    const GURL& url,
    RenderFrameHostImpl* rfh,
    base::RepeatingCallback<bool(const GURL&)> url_match_predicate) {
  return PrerenderAttributes(
      url, PrerenderTriggerType::kSpeculationRule,
      /*embedder_histogram_suffix=*/"", Referrer(),
      rfh->GetLastCommittedOrigin(), rfh->GetLastCommittedURL(),
      rfh->GetProcess()->GetID(), rfh->GetFrameToken(),
      rfh->GetFrameTreeNodeId(), rfh->GetPageUkmSourceId(),
      ui::PAGE_TRANSITION_LINK, std::move(url_match_predicate));
}

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;
  bool IsPrerender2Supported(WebContents& web_contents) override {
    return true;
  }
};

class PrerenderHostTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
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
    // Check FinalStatus in UMA.
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        status, 1);

    // Check all entries in UKM to make sure that the recorded FinalStatus is
    // equal to `status`. At least one entry should exist.
    bool final_status_entry_found = false;
    const auto entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::PrerenderPageLoad::kEntryName);
    for (const auto* entry : entries) {
      if (ukm_recorder_.EntryHasMetric(
              entry, ukm::builders::PrerenderPageLoad::kFinalStatusName)) {
        final_status_entry_found = true;
        ukm_recorder_.ExpectEntryMetric(
            entry, ukm::builders::PrerenderPageLoad::kFinalStatusName,
            static_cast<int>(status));
      }
    }

    EXPECT_TRUE(final_status_entry_found);
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
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
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
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
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
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  RenderFrameHostImpl* prerender_rfh =
      web_contents->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  FrameTreeNode* ftn = prerender_rfh->frame_tree_node();
  EXPECT_FALSE(ftn->HasNavigation());

  test::PrerenderHostObserver prerender_host_observer(*web_contents,
                                                      kPrerenderingUrl);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;

  {
    MockCommitDeferringConditionInstaller installer(
        kPrerenderingUrl,
        /*is_ready_to_commit=*/false);
    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *web_contents);
    navigation->Start();

    // Wait for the condition to pause the activation.
    installer.WaitUntilInstalled();
    installer.condition().WaitUntilInvoked();

    // The request should be deferred by the condition.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
    EXPECT_TRUE(
        navigation_request->IsCommitDeferringConditionDeferredForTesting());

    // The primary page should still be the original page.
    EXPECT_EQ(web_contents->GetLastCommittedURL(), kOriginUrl);

    const GURL kBadUrl("https://example2.test/");
    TestNavigationManager tno(web_contents.get(), kBadUrl);

    // Start a cross-origin navigation in the prerendered page. It should be
    // cancelled.
    auto navigation_2 = NavigationSimulatorImpl::CreateRendererInitiated(
        kBadUrl, prerender_rfh);
    navigation_2->Start();
    EXPECT_EQ(NavigationThrottle::CANCEL,
              navigation_2->GetLastThrottleCheckResult());
    tno.WaitForNavigationFinished();
    EXPECT_FALSE(tno.was_committed());

    // The cross-origin navigation cancels the activation.
    installer.condition().CallResumeClosure();
    prerender_host_observer.WaitForDestroyed();
    EXPECT_FALSE(prerender_host_observer.was_activated());
    EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
    ExpectFinalStatus(PrerenderHost::FinalStatus::kMainFrameNavigation);
  }

  // The activation falls back to regular navigation.
  navigation->Commit();
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

// Tests that an activation can successfully commit after the prerendering page
// has updated its PageState.
TEST_F(PrerenderHostTest, ActivationAfterPageStateUpdate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
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
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);

  // Ensure that the the page_state was preserved.
  EXPECT_EQ(web_contents->GetPrimaryMainFrame(), prerender_rfh);
  NavigationEntryImpl* activated_nav_entry =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_EQ(page_state,
            activated_nav_entry
                ->GetFrameEntry(web_contents->GetPrimaryFrameTree().root())
                ->page_state());
}

// Test that WebContentsObserver::LoadProgressChanged is not invoked when the
// page gets loaded while prerendering but is invoked on prerender activation.
// Check that in case the load is incomplete with load progress
// `kPartialLoadProgress`, we would see
// LoadProgressChanged(kPartialLoadProgress) called on activation.
TEST_F(PrerenderHostTest, LoadProgressChangedInvokedOnActivation) {
  const GURL kOriginUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents(kOriginUrl);
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents.get());

  web_contents_impl->set_minimum_delay_between_loading_updates_for_testing(
      base::Milliseconds(0));

  // Initialize a MockWebContentsObserver and ensure that LoadProgressChanged is
  // not invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(web_contents_impl);
  testing::InSequence s;
  EXPECT_CALL(observer, LoadProgressChanged(testing::_)).Times(0);

  // Start prerendering a page and commit prerender navigation.
  const GURL kPrerenderingUrl("https://example.com/next");
  constexpr double kPartialLoadProgress = 0.7;
  RenderFrameHostImpl* prerender_rfh =
      web_contents->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  FrameTreeNode* ftn = prerender_rfh->frame_tree_node();
  EXPECT_FALSE(ftn->HasNavigation());

  // Verify and clear all expectations on the mock observer before setting new
  // ones.
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Activate the prerendered page. This should result in invoking
  // LoadProgressChanged for the following cases:
  {
    // 1) During DidStartLoading LoadProgressChanged is invoked with
    // kInitialLoadProgress value.
    EXPECT_CALL(observer, LoadProgressChanged(blink::kInitialLoadProgress));

    // Verify that DidFinishNavigation is invoked before final load progress
    // notification.
    EXPECT_CALL(observer, DidFinishNavigation(testing::_));

    // 2) After DidCommitNavigationInternal on activation with
    // LoadProgressChanged is invoked with kPartialLoadProgress value.
    EXPECT_CALL(observer, LoadProgressChanged(kPartialLoadProgress));

    // 3) During DidStopLoading LoadProgressChanged is invoked with
    // kFinalLoadProgress.
    EXPECT_CALL(observer, LoadProgressChanged(blink::kFinalLoadProgress));
  }

  // Set load_progress value to kPartialLoadProgress in prerendering state,
  // this should result in invoking LoadProgressChanged(kPartialLoadProgress) on
  // activation.
  prerender_rfh->GetPage().set_load_progress(kPartialLoadProgress);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page.
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, CancelPrerenderWhenTriggerGetsHidden) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to HIDDEN will cause prerendering cancelled.
  web_contents->WasHidden();
  ExpectFinalStatus(PrerenderHost::FinalStatus::kTriggerBackgrounded);
}

TEST_F(PrerenderHostTest, DontCancelPrerenderWhenTriggerGetsVisible) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to VISIBLE will not stop prerendering.
  web_contents->WasShown();
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

// Skip this test on Android as it doesn't support the OCCLUDED state.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PrerenderHostTest, DontCancelPrerenderWhenTriggerGetsOcculded) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to OCCLUDED will not stop prerendering.
  web_contents->WasOccluded();
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}
#endif

TEST_F(PrerenderHostTest, UrlMatchPredicate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  base::RepeatingCallback callback =
      base::BindRepeating([](const GURL&) { return true; });
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributesWithPredicate(kPrerenderingUrl, initiator_rfh,
                                               callback),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  const GURL kActivatedUrl = GURL("https://example.com/empty.html?activate");
  ASSERT_NE(kActivatedUrl, kPrerenderingUrl);
  EXPECT_TRUE(prerender_host->IsUrlMatch(kActivatedUrl));
  // Even if the predicate always returns true, a cross-origin url shouldn't be
  // able to activate a prerendered page.
  EXPECT_FALSE(
      prerender_host->IsUrlMatch(GURL("https://example2.com/empty.html")));
}

// Regression test for https://crbug.com/1366046: This test will crash if
// PrerenderHost is set to "ready_for_activation" after getting canceled.
TEST_F(PrerenderHostTest, CanceledPrerenderCannotBeReadyForActivation) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  auto* preloading_data =
      PreloadingData::GetOrCreateForWebContents(web_contents.get());

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt.
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      ToPreloadingPredictor(ContentPreloadingPredictor::kSpeculationRules),
      PreloadingType::kPrerender, std::move(same_url_matcher));

  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents, preloading_attempt);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);

  // Registry keeps alive through this test, so it is safe to use
  // base::Unretained.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&PrerenderHostRegistry::CancelHost),
                     base::Unretained(registry), prerender_frame_tree_node_id,
                     PrerenderHost::FinalStatus::kTriggerDestroyed));

  // For some reasons triggers want to set the failure reason by themselves,
  // this would happen together with cancelling prerender.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PreloadingAttempt::SetFailureReason,
          base::Unretained(preloading_attempt),
          static_cast<PreloadingFailureReason>(
              static_cast<int>(PrerenderHost::FinalStatus::kTriggerDestroyed) +
              static_cast<int>(PreloadingFailureReason::
                                   kPreloadingFailureReasonCommonEnd))));

  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        CommitPrerenderNavigation(*prerender_host,
                                  ExpectedReadyForActivationState(false));
        run_loop.Quit();
      }));

  // Wait for the completion of CommitPrerenderNavigation() above.
  run_loop.Run();

  auto* preloading_attempt_impl =
      static_cast<PreloadingAttemptImpl*>(preloading_attempt);
  EXPECT_EQ(preloading_attempt_impl->get_triggering_outcome_for_testing(),
            PreloadingTriggeringOutcome::kFailure);
}

// TODO(crbug.com/1356907): Remove this and merge it to PrerenderHostTest once
// kPrerender2InBackground is enabled by default.
class PrerenderHostInBackgroundTest : public PrerenderHostTest {
 public:
  PrerenderHostInBackgroundTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2,
         // Enable to run prerenderings in the background.
         blink::features::kPrerender2InBackground},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }

  ~PrerenderHostInBackgroundTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrerenderHostInBackgroundTest,
       DontCancelPrerenderWhenTriggerGetsHidden) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to HIDDEN will not stop prerendering.
  web_contents->WasHidden();
  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_F(PrerenderHostInBackgroundTest, CancelPrerenderWhenTimeout) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // The timer should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(prerender_host->GetTimerForTesting()->IsRunning());

  // Inject mock time task runner.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  prerender_host->SetTaskRunnerForTesting(task_runner);

  // Changing the visibility state to HIDDEN will not stop prerendering.
  web_contents->WasHidden();
  ASSERT_TRUE(prerender_host->GetTimerForTesting()->IsRunning());

  task_runner->FastForwardBy(PrerenderHost::kTimeToLiveInBackground);

  ExpectFinalStatus(PrerenderHost::FinalStatus::kTimeoutBackgrounded);
}

TEST_F(PrerenderHostInBackgroundTest,
       TimerResetWhenHiddenPageGoBackToForeground) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetPrimaryMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      *web_contents);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // The timer should not start yet when the prerendered page is in the
  // foreground.
  ASSERT_FALSE(prerender_host->GetTimerForTesting()->IsRunning());

  // Changing the visibility state to HIDDEN will not stop prerendering.
  web_contents->WasHidden();
  ASSERT_TRUE(prerender_host->GetTimerForTesting()->IsRunning());

  // The timer should be reset when the hidden page goes back to the foreground.
  web_contents->WasShown();
  ASSERT_FALSE(prerender_host->GetTimerForTesting()->IsRunning());

  web_contents->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

}  // namespace
}  // namespace content

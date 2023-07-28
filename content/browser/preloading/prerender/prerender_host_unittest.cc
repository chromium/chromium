// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {
namespace {

using ::testing::_;

TEST(IsActivationHeaderMatchTest, OrderInsensitive) {
  net::HttpRequestHeaders prerender_headers;
  prerender_headers.AddHeadersFromString(
      "name1: value1 \r\n name2: value2 \r\n name3:value3");
  net::HttpRequestHeaders potential_activation_headers;
  potential_activation_headers.AddHeadersFromString(
      "name2: value2 \r\n name3:value3  \r\n name1: value1 ");
  EXPECT_TRUE(PrerenderHost::IsActivationHeaderMatch(
      potential_activation_headers, prerender_headers));
}

TEST(IsActivationHeaderMatchTest, KeyCaseInsensitive) {
  net::HttpRequestHeaders prerender_headers;
  prerender_headers.AddHeadersFromString(
      "NAME1: value1 \r\n name2: value2 \r\n name3:value3");
  net::HttpRequestHeaders potential_activation_headers;
  potential_activation_headers.AddHeadersFromString(
      "name1: value1 \r\n name2: value2  \r\n name3: value3 ");
  EXPECT_TRUE(PrerenderHost::IsActivationHeaderMatch(
      potential_activation_headers, prerender_headers));
}

TEST(IsActivationHeaderMatchTest, ValueCaseInsensitive) {
  net::HttpRequestHeaders prerender_headers;
  prerender_headers.AddHeadersFromString(
      "name1: value1 \r\n name2: value2 \r\n name3:value3");
  net::HttpRequestHeaders potential_activation_headers;
  potential_activation_headers.AddHeadersFromString(
      "name1: value1 \r\n name2: VALUE2  \r\n name3: value3 ");
  EXPECT_TRUE(PrerenderHost::IsActivationHeaderMatch(
      potential_activation_headers, prerender_headers));
}

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

class PrerenderHostTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrerender2MainFrameNavigation);
  }

  ~PrerenderHostTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(*contents());
    contents()->NavigateAndCommit(GURL("https://example.com"));
  }

  PrerenderAttributes GeneratePrerenderAttributes(const GURL& url) {
    return GeneratePrerenderAttributesWithPredicate(
        url, /*url_match_predicate=*/absl::nullopt);
  }

  PrerenderAttributes GeneratePrerenderAttributesWithPredicate(
      const GURL& url,
      absl::optional<base::RepeatingCallback<bool(const GURL&)>>
          url_match_predicate) {
    RenderFrameHostImpl* rfh = contents()->GetPrimaryMainFrame();
    return PrerenderAttributes(
        url, PrerenderTriggerType::kSpeculationRule,
        /*embedder_histogram_suffix=*/"", Referrer(),
        blink::mojom::SpeculationEagerness::kEager,
        rfh->GetLastCommittedOrigin(), rfh->GetProcess()->GetID(),
        contents()->GetWeakPtr(), rfh->GetFrameToken(),
        rfh->GetFrameTreeNodeId(), rfh->GetPageUkmSourceId(),
        ui::PAGE_TRANSITION_LINK, std::move(url_match_predicate));
  }

  void ExpectFinalStatus(PrerenderFinalStatus status) {
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

  PrerenderHostRegistry& registry() {
    return *contents()->GetPrerenderHostRegistry();
  }

 private:
  test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrerenderHostTest, Activate) {
  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  int prerender_frame_tree_node_id = contents()->AddPrerender(kPrerenderingUrl);
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  CommitPrerenderNavigation(*prerender_host);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page.
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, DontActivate) {
  // Start the prerendering navigation, but don't activate it.
  const GURL kPrerenderingUrl("https://example.com/next");
  const int prerender_frame_tree_node_id =
      contents()->AddPrerender(kPrerenderingUrl);
  registry().CancelHost(prerender_frame_tree_node_id,
                        PrerenderFinalStatus::kDestroyed);
  ExpectFinalStatus(PrerenderFinalStatus::kDestroyed);
}

// Tests that cross-site main frame navigations in a prerendered page cannot
// occur even if they start after the prerendered page has been reserved for
// activation.
TEST_F(PrerenderHostTest, MainFrameNavigationForReservedHost) {
  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  RenderFrameHostImpl* prerender_rfh =
      contents()->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  FrameTreeNode* ftn = prerender_rfh->frame_tree_node();
  EXPECT_FALSE(ftn->HasNavigation());

  test::PrerenderHostObserver prerender_host_observer(*contents(),
                                                      kPrerenderingUrl);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;

  {
    MockCommitDeferringConditionInstaller installer(
        kPrerenderingUrl, CommitDeferringCondition::Result::kDefer);
    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *contents());
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
    EXPECT_EQ(contents()->GetLastCommittedURL(), "https://example.com/");

    const GURL kBadUrl("https://example2.test/");
    TestNavigationManager tno(contents(), kBadUrl);

    // Start a cross-site navigation in the prerendered page. It should be
    // cancelled.
    auto navigation_2 = NavigationSimulatorImpl::CreateRendererInitiated(
        kBadUrl, prerender_rfh);
    navigation_2->Start();
    EXPECT_EQ(NavigationThrottle::CANCEL,
              navigation_2->GetLastThrottleCheckResult());
    ASSERT_TRUE(tno.WaitForNavigationFinished());
    EXPECT_FALSE(tno.was_committed());

    // The cross-site navigation cancels the activation.
    installer.condition().CallResumeClosure();
    prerender_host_observer.WaitForDestroyed();
    EXPECT_FALSE(prerender_host_observer.was_activated());
    EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
    ExpectFinalStatus(
        PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation);
  }

  // The activation falls back to regular navigation.
  navigation->Commit();
  EXPECT_EQ(contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

// Tests that an activation can successfully commit after the prerendering page
// has updated its PageState.
TEST_F(PrerenderHostTest, ActivationAfterPageStateUpdate) {
  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  CommitPrerenderNavigation(*prerender_host);

  auto* prerender_root_ftn =
      FrameTreeNode::GloballyFindByID(prerender_frame_tree_node_id);
  RenderFrameHostImpl* prerender_rfh = prerender_root_ftn->current_frame_host();
  NavigationEntryImpl* prerender_nav_entry =
      prerender_root_ftn->frame_tree().controller().GetLastCommittedEntry();
  FrameNavigationEntry* prerender_root_fne =
      prerender_nav_entry->GetFrameEntry(prerender_root_ftn);

  auto page_state = blink::PageState::CreateForTestingWithSequenceNumbers(
      GURL("about:blank"), prerender_root_fne->item_sequence_number(),
      prerender_root_fne->document_sequence_number());

  // Update PageState for prerender RFH, causing it to become different from
  // the one stored in RFH's last commit params.
  static_cast<mojom::FrameHost*>(prerender_rfh)->UpdateState(page_state);

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page. The main expectation is that this navigation commits
  // successfully and doesn't hit any CHECKs.
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);

  // Ensure that the the page_state was preserved.
  EXPECT_EQ(contents()->GetPrimaryMainFrame(), prerender_rfh);
  NavigationEntryImpl* activated_nav_entry =
      contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(page_state,
            activated_nav_entry
                ->GetFrameEntry(contents()->GetPrimaryFrameTree().root())
                ->page_state());
}

// Test that WebContentsObserver::LoadProgressChanged is not invoked when the
// page gets loaded while prerendering but is invoked on prerender activation.
// Check that in case the load is incomplete with load progress
// `kPartialLoadProgress`, we would see
// LoadProgressChanged(kPartialLoadProgress) called on activation.
TEST_F(PrerenderHostTest, LoadProgressChangedInvokedOnActivation) {
  contents()->set_minimum_delay_between_loading_updates_for_testing(
      base::Milliseconds(0));

  // Initialize a MockWebContentsObserver and ensure that LoadProgressChanged is
  // not invoked while prerendering.
  testing::NiceMock<MockWebContentsObserver> observer(contents());
  testing::InSequence s;
  EXPECT_CALL(observer, LoadProgressChanged(testing::_)).Times(0);

  // Start prerendering a page and commit prerender navigation.
  const GURL kPrerenderingUrl("https://example.com/next");
  constexpr double kPartialLoadProgress = 0.7;
  RenderFrameHostImpl* prerender_rfh =
      contents()->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
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
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, DontCancelPrerenderWhenTriggerGetsHidden) {
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to HIDDEN will not stop prerendering.
  contents()->WasHidden();

  // Activation from the foreground page should succeed.
  contents()->WasShown();
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);
}

TEST_F(PrerenderHostTest, CancelActivationFromHiddenPage) {
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to HIDDEN will not stop prerendering.
  contents()->WasHidden();

  // Activation from the background page should fail.
  test::PrerenderHostObserver prerender_host_observer(
      *contents(), prerender_frame_tree_node_id);
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(
          kPrerenderingUrl, contents()->GetPrimaryMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
  prerender_host_observer.WaitForDestroyed();

  EXPECT_FALSE(prerender_host_observer.was_activated());
  ExpectFinalStatus(PrerenderFinalStatus::kActivatedInBackground);
}

TEST_F(PrerenderHostTest, DontCancelPrerenderWhenTriggerGetsVisible) {
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to VISIBLE will not stop prerendering.
  contents()->WasShown();
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);
}

// Skip this test on Android as it doesn't support the OCCLUDED state.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PrerenderHostTest, DontCancelPrerenderWhenTriggerGetsOcculded) {
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);
  CommitPrerenderNavigation(*prerender_host);

  // Changing the visibility state to OCCLUDED will not stop prerendering.
  contents()->WasOccluded();
  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  ExpectFinalStatus(PrerenderFinalStatus::kActivated);
}
#endif

TEST_F(PrerenderHostTest, UrlMatchPredicate) {
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  base::RepeatingCallback callback =
      base::BindRepeating([](const GURL&) { return true; });
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributesWithPredicate(kPrerenderingUrl, callback));
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
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
  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");

  auto* preloading_data = PreloadingData::GetOrCreateForWebContents(contents());

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt.
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      content_preloading_predictor::kSpeculationRules,
      PreloadingType::kPrerender, std::move(same_url_matcher));

  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl), preloading_attempt);
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  ASSERT_NE(prerender_host, nullptr);

  // Registry keeps alive through this test, so it is safe to capture the
  // reference to `registry`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(base::BindLambdaForTesting([&]() {
        registry().CancelHost(prerender_frame_tree_node_id,
                              PrerenderFinalStatus::kTriggerDestroyed);
      })));

  // For some reasons triggers want to set the failure reason by themselves,
  // this would happen together with cancelling prerender.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PreloadingAttempt::SetFailureReason,
          base::Unretained(preloading_attempt),
          static_cast<PreloadingFailureReason>(
              static_cast<int>(PrerenderFinalStatus::kTriggerDestroyed) +
              static_cast<int>(PreloadingFailureReason::
                                   kPreloadingFailureReasonCommonEnd))));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        CommitPrerenderNavigation(*prerender_host,
                                  ExpectedReadyForActivationState(false));
        run_loop.Quit();
      }));

  // Wait for the completion of CommitPrerenderNavigation() above.
  run_loop.Run();

  EXPECT_EQ(test::PreloadingAttemptAccessor(preloading_attempt)
                .GetTriggeringOutcome(),
            PreloadingTriggeringOutcome::kFailure);
}

}  // namespace
}  // namespace content

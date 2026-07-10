// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_fast_fetch_manager.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/features.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class NavigationFastFetchManagerTest : public RenderViewHostImplTestHarness {
 public:
  NavigationFastFetchManagerTest() {
    feature_list_.InitAndEnableFeature(features::kNavigationFastFetchDryRun);
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    auto* wrapper = static_cast<ServiceWorkerContextWrapper*>(
        contents()
            ->GetPrimaryMainFrame()
            ->GetStoragePartition()
            ->GetServiceWorkerContext());
    if (wrapper && wrapper->context()) {
      wrapper->context()->registry().WaitForRegistrationsInitializedForTest();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NavigationFastFetchManagerTest, EligibleNavigation) {
  base::HistogramTester histogram_tester;

  // A standard HTTPS GET navigation in the outermost main frame should be
  // eligible.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), contents());
  static_cast<NavigationSimulatorImpl*>(simulator.get())
      ->set_response_postprocess_hook(
          base::BindRepeating([](network::mojom::URLResponseHead& response) {
            response.request_start = base::TimeTicks::Now();
          }));
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kEligible);

  simulator->Commit();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kEligible, 1);
  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.NavigationOutcome",
      NavigationFastFetchManager::NavigationOutcome::kCommitted, 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.EligibilityCheckDuration", 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.OpportunityTime.LoaderStart", 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.OpportunityTime.FetchStart", 1);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleMethod) {
  base::HistogramTester histogram_tester;

  // POST navigation should be ineligible.
  auto simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), main_rfh());
  simulator->SetMethod("POST");
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kNotGetMethod);

  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kNotGetMethod, 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.NavigationOutcome", 0);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleScheme) {
  base::HistogramTester histogram_tester;

  // HTTP navigation should be ineligible.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://example.com"), contents());
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kNotHttpsScheme);

  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kNotHttpsScheme, 1);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleSubframe) {
  // Navigate the main frame first. This will record a sample we want to ignore.
  NavigateAndCommit(GURL("https://example.com"));

  // Create a child frame.
  RenderFrameHostTester* rfh_tester = RenderFrameHostTester::For(main_rfh());
  RenderFrameHost* child_rfh = rfh_tester->AppendChild("child");

  base::HistogramTester histogram_tester;

  auto simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/child"), child_rfh);
  simulator->Start();

  // For subframe, the navigation request is not on the main frame.
  // We need to find it from the FrameTreeNode.
  FrameTreeNode* child_node = FrameTreeNode::From(child_rfh);
  NavigationRequest* request = child_node->navigation_request();

  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(
      request->fast_fetch_manager_for_testing()
          ->eligibility_reason_for_testing(),
      NavigationFastFetchManager::EligibilityReason::kNotInOutermostMainFrame);

  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kNotInOutermostMainFrame,
      1);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleDevTools) {
  base::HistogramTester histogram_tester;

  TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(contents());

  EXPECT_TRUE(DevToolsAgentHost::IsDebuggerAttached(contents()));

  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), contents());
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kDevToolsAttached);

  devtools_client.DetachProtocolClient();
  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kDevToolsAttached, 1);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleSignedExchange) {
  base::HistogramTester histogram_tester;

  GURL url("https://example.com");

  // Create a mock cache and add an entry for the URL.
  auto cache = base::MakeRefCounted<PrefetchedSignedExchangeCache>();
  cache->AddEntryForTesting(url);

  // Set the cache on the RFH before starting navigation.
  static_cast<TestRenderFrameHost*>(main_rfh())
      ->SetPrefetchedSignedExchangeCacheForTesting(cache);

  // Renderer-initiated navigation is required to use the cache.
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kHasSignedExchange);

  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kHasSignedExchange, 1);
}

TEST_F(NavigationFastFetchManagerTest, FailedNavigation) {
  base::HistogramTester histogram_tester;

  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), contents());
  static_cast<NavigationSimulatorImpl*>(simulator.get())
      ->set_response_postprocess_hook(
          base::BindRepeating([](network::mojom::URLResponseHead& response) {
            response.request_start = base::TimeTicks::Now();
          }));
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kEligible);

  // Fail the navigation with net::ERR_CONNECTION_RESET.
  simulator->Fail(net::ERR_CONNECTION_RESET);

  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kEligible, 1);
  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.NavigationOutcome",
      NavigationFastFetchManager::NavigationOutcome::kFailed, 1);
  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.FailedNetError",
      net::ERR_CONNECTION_RESET * -1, 1);
  histogram_tester.ExpectUniqueSample(
      "Navigation.Experimental.FastFetch.SkipThrottles", false, 1);
}

TEST_F(NavigationFastFetchManagerTest, IneligibleSameDocument) {
  // Set up two same-document history entries.
  NavigateAndCommit(GURL("https://example.com/#1"));
  NavigateAndCommit(GURL("https://example.com/#2"));

  // Start recording histograms here to ignore the setup navigations.
  base::HistogramTester histogram_tester;

  // Go back (offset -1). Same-document history navigation.
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), /*is_renderer_initiated=*/false);
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kSameDocument);

  DeleteContents();

  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kSameDocument, 1);
}

TEST_F(NavigationFastFetchManagerTest, RestartedNavigation) {
  NavigateAndCommit(GURL("https://example.com/#1"));
  NavigateAndCommit(GURL("https://example.com/#2"));

  // Start back navigation to index 0 (same-document history navigation).
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), /*is_renderer_initiated=*/false);
  base::HistogramTester histogram_tester;
  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);
  ASSERT_NE(request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kSameDocument);

  base::UnguessableToken token = request->commit_params().navigation_token;

  // Trigger restart via SimulateOnSameDocumentCommitProcessed on
  // TestRenderFrameHost. This will call ResetForCrossDocumentRestart and
  // BeginNavigation.
  static_cast<TestRenderFrameHost*>(main_rfh())
      ->SimulateOnSameDocumentCommitProcessed(
          token, /*should_replace_current_entry=*/false,
          blink::mojom::CommitResult::RestartCrossDocument);

  // The request should now be back in FrameTreeNode, and restarted.
  NavigationRequest* restarted_request =
      static_cast<TestRenderFrameHost*>(main_rfh())
          ->frame_tree_node()
          ->navigation_request();
  ASSERT_NE(restarted_request, nullptr);

  // It should have been re-evaluated and marked as history (ineligible).
  ASSERT_NE(restarted_request->fast_fetch_manager_for_testing(), nullptr);
  EXPECT_EQ(restarted_request->fast_fetch_manager_for_testing()
                ->eligibility_reason_for_testing(),
            NavigationFastFetchManager::EligibilityReason::kIsHistory);

  DeleteContents();

  // We only expect kIsHistory.
  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kIsHistory, 1);
  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kSameDocument, 0);
}

TEST_F(NavigationFastFetchManagerTest, NegativeOpportunityTime) {
  base::HistogramTester histogram_tester;

  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), contents());

  // Set response request_start to 10 seconds in the past.
  // This will be used to set first_fetch_start_time during commit.
  static_cast<NavigationSimulatorImpl*>(simulator.get())
      ->set_response_postprocess_hook(
          base::BindRepeating([](network::mojom::URLResponseHead& response) {
            response.request_start = base::TimeTicks::Now() - base::Seconds(10);
          }));

  simulator->Start();

  NavigationRequest* request =
      static_cast<NavigationRequest*>(simulator->GetNavigationHandle());
  ASSERT_NE(request, nullptr);

  // Set loader_start_time to the past.
  NavigationHandleTiming timing = request->GetNavigationHandleTiming();
  timing.loader_start_time = base::TimeTicks::Now() - base::Seconds(10);
  request->SetNavigationHandleTimingForTesting(timing);

  // Commit.
  simulator->Commit();

  // The metrics should NOT be recorded because the deltas are negative.
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.OpportunityTime.LoaderStart", 0);
  histogram_tester.ExpectTotalCount(
      "Navigation.Experimental.FastFetch.OpportunityTime.FetchStart", 0);
}

}  // namespace content

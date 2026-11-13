// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"

#import <limits>
#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Creates `WebPerformanceMetricsTabHelper` for `web_state` with a main frame
// and a child frame.
web::FakeWebFramesManager* SetUpWebStateWithTabHelper(
    web::FakeWebState& web_state) {
  // FakeWebFramesManager is needed for the observer.
  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto* manager_ptr = web_frames_manager.get();
  manager_ptr->AddWebFrame(web::FakeWebFrame::CreateMainWebFrame());
  manager_ptr->AddWebFrame(web::FakeWebFrame::CreateChildWebFrame());
  web_state.SetWebFramesManager(
      WebPerformanceMetricsJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::move(web_frames_manager));
  WebPerformanceMetricsTabHelper::CreateForWebState(&web_state);
  return manager_ptr;
}

// Simulates a committed cross-document navigation in `web_state`.
void CommitCrossDocumentNavigation(web::FakeWebState& web_state) {
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  web_state.OnNavigationStarted(&navigation_context);
  web_state.OnNavigationFinished(&navigation_context);
}

}  // namespace

using WebPerformanceMetricsTabHelperTest = PlatformTest;

// Test fixture that simulates web frame lifecycles across navigations to verify
// that INP interaction data is attributed to the correct page.
class WebPerformanceMetricsTabHelperFrameAttributionTest : public PlatformTest {
 protected:
  // Frame IDs must be hexadecimal strings.
  static constexpr char kPageAMainFrameId[] = "aaaa";
  static constexpr char kPageBMainFrameId[] = "bbbb";
  static constexpr char kPageCMainFrameId[] = "cccc";
  static constexpr char kErrorPageFrameId[] = "eeee";

  WebPerformanceMetricsTabHelperFrameAttributionTest() {
    web_state_ = std::make_unique<web::FakeWebState>();
    web_frames_manager_ = SetUpWebStateWithTabHelper(*web_state_);
    tab_helper_ =
        WebPerformanceMetricsTabHelper::FromWebState(web_state_.get());

    NavigateTo(kPageAMainFrameId);
  }

  // Simulates a committed cross-document navigation, removing existing frames
  // before commit and registering the new page's main frame afterwards.
  void NavigateTo(const std::string& new_main_frame_id) {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetHasCommitted(true);
    web_state_->OnNavigationStarted(&navigation_context);
    for (web::WebFrame* frame : web_frames_manager_->GetAllWebFrames()) {
      web_frames_manager_->RemoveWebFrame(frame->GetFrameId());
    }
    web_state_->OnNavigationFinished(&navigation_context);
    web_frames_manager_->AddWebFrame(
        web::FakeWebFrame::Create(new_main_frame_id, /*is_main_frame=*/true));
  }

  // Reports one interaction of `duration` from the frame `frame_id`.
  void ReportInteraction(const std::string& frame_id,
                         base::TimeDelta duration) {
    tab_helper_->SetFrameInteractionData(frame_id, {duration},
                                         /*interaction_count=*/1,
                                         /*is_main_frame=*/true);
  }

  // Destroys the WebState, as when its tab is closed.
  void DestroyWebState() {
    tab_helper_ = nullptr;
    web_frames_manager_ = nullptr;
    web_state_.reset();
  }

  void ExpectNoInteractionToNextPaintSample() {
    histogram_tester_.ExpectTotalCount(
        web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
    histogram_tester_.ExpectTotalCount(
        web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
    histogram_tester_.ExpectTotalCount(
        web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 0);
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
  raw_ptr<WebPerformanceMetricsTabHelper> tab_helper_ = nullptr;
};

// Tests that Interaction to Next Paint is buffered across a navigation commit
// to allow late messages to arrive, and logged at the following commit.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_DeferredFlushOnNavigation) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  SetUpWebStateWithTabHelper(fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(120)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);

  // Simulate the next navigation starting.
  web::FakeNavigationContext navigation_context;
  fake_web_state.OnNavigationStarted(&navigation_context);

  // No histogram sample should be recorded prior to navigation commit.
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);

  // Simulate the next navigation committing. The outgoing page is not logged
  // yet so late messages can still be attributed to it.
  navigation_context.SetHasCommitted(true);
  fake_web_state.OnNavigationFinished(&navigation_context);

  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);

  // The following committed navigation logs the metric.
  CommitCrossDocumentNavigation(fake_web_state);

  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(120), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(120), 1);
}

// Tests that Interaction to Next Paint is flushed when WebState is destroyed.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_FlushOnWebStateDestroyed) {
  base::HistogramTester histogram_tester;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  SetUpWebStateWithTabHelper(*fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(fake_web_state.get());

  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(95)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);
  tab_helper->SetFrameInteractionData(web::kChildFakeFrameId,
                                      {base::Milliseconds(60)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/false);

  // Destroy WebState (tab closure).
  fake_web_state.reset();

  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(95), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(95), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram,
      base::Milliseconds(60), 1);
}

// Tests that the aggregate INP correctly merges multiple frames and discards
// high-percentile outliers across the entire page.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_AggregateHighPercentileAcrossFrames) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  SetUpWebStateWithTabHelper(fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  // Main frame has 100 interactions with worst latency 45ms.
  // We only need to set the 45ms interaction. The other 99 will be synthesized
  // by the calculation directly from the interaction count.
  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(45)},
                                      /*interaction_count=*/100,
                                      /*is_main_frame=*/true);

  // Subframe has 2 interactions: 300ms and 200ms.
  tab_helper->SetFrameInteractionData(
      web::kChildFakeFrameId,
      {base::Milliseconds(300), base::Milliseconds(200)},
      /*interaction_count=*/2, /*is_main_frame=*/false);

  // Flush the tab helper to verify that the message was recorded.
  tab_helper->FlushInteractionToNextPaintMetrics();

  // Total interactions = 102 -> outlier count = floor(102 / 50) = 2.
  // Merged worst durations = {300ms, 200ms, 45ms}.
  // Index 2 is selected -> 45ms for aggregate page INP!
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(45), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      web_performance_metrics::kSubThresholdInteractionDuration, 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram,
      base::Milliseconds(300), 1);
}

// Tests that when no user interactions occurred on the page, no aggregate
// histogram sample is recorded on navigation commit.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_NoInteraction_NoFlush) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  SetUpWebStateWithTabHelper(fake_web_state);

  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  fake_web_state.OnNavigationStarted(&navigation_context);
  fake_web_state.OnNavigationFinished(&navigation_context);

  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 0);
}

// Tests that same-document navigations do not flush or reset page performance
// metrics (INP, FID, FCP), and that a subsequent cross-document navigation
// resets state and makes the accumulated INP loggable at the next committed
// navigation.
TEST_F(WebPerformanceMetricsTabHelperTest,
       SameDocumentNavigationPreservesDocumentState) {
  constexpr double kFirstContentfulPaintTime = 1234.0;

  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  SetUpWebStateWithTabHelper(fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(90)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);
  tab_helper->SetFirstInputDelayLoggingStatus(true);
  tab_helper->SetAggregateAbsoluteFirstContentfulPaint(
      kFirstContentfulPaintTime);

  // Simulate the user hiding and re-showing the tab before the same-document
  // navigation. `HasBeenHiddenSinceNavigationStarted()` must remain true even
  // though the tab is visible when the same-document navigation starts.
  fake_web_state.WasHidden();
  fake_web_state.WasShown();

  web::FakeNavigationContext same_document_context;
  same_document_context.SetIsSameDocument(true);
  same_document_context.SetHasCommitted(true);
  fake_web_state.OnNavigationStarted(&same_document_context);
  fake_web_state.OnNavigationFinished(&same_document_context);

  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
  EXPECT_TRUE(tab_helper->GetFirstInputDelayLoggingStatus());
  EXPECT_EQ(kFirstContentfulPaintTime,
            tab_helper->GetAggregateAbsoluteFirstContentfulPaint());
  EXPECT_TRUE(tab_helper->HasBeenHiddenSinceNavigationStarted());

  // More interactions on the same document replace the frame's data.
  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(130)},
                                      /*interaction_count=*/2,
                                      /*is_main_frame=*/true);

  // Perform a cross-document navigation. Starting it resets FID, FCP, and
  // hidden state; committing it defers logging the outgoing page's INP.
  web::FakeNavigationContext cross_document_context;
  cross_document_context.SetHasCommitted(true);
  fake_web_state.OnNavigationStarted(&cross_document_context);
  EXPECT_FALSE(tab_helper->GetFirstInputDelayLoggingStatus());
  EXPECT_EQ(std::numeric_limits<double>::max(),
            tab_helper->GetAggregateAbsoluteFirstContentfulPaint());
  EXPECT_FALSE(tab_helper->HasBeenHiddenSinceNavigationStarted());
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  fake_web_state.OnNavigationFinished(&cross_document_context);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);

  CommitCrossDocumentNavigation(fake_web_state);

  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(130), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(130), 1);
  EXPECT_FALSE(tab_helper->GetFirstInputDelayLoggingStatus());
  EXPECT_EQ(std::numeric_limits<double>::max(),
            tab_helper->GetAggregateAbsoluteFirstContentfulPaint());
  EXPECT_FALSE(tab_helper->HasBeenHiddenSinceNavigationStarted());
}

// Tests that uncommitted navigations do not flush INP and that the page keeps
// accumulating interactions until a later navigation commits.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_UncommittedNavigation_NoFlush) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  SetUpWebStateWithTabHelper(fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(70)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);
  // The subframe data is not updated after the uncommitted navigation, so it is
  // only logged if the buffered pre-navigation data survives.
  tab_helper->SetFrameInteractionData(web::kChildFakeFrameId,
                                      {base::Milliseconds(50)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/false);

  web::FakeNavigationContext uncommitted_context;
  uncommitted_context.SetHasCommitted(false);
  fake_web_state.OnNavigationStarted(&uncommitted_context);
  fake_web_state.OnNavigationFinished(&uncommitted_context);

  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 0);

  // The page keeps accumulating interactions, and the buffered data (including
  // interactions recorded before the uncommitted navigation) is logged once
  // after a later navigation commits.
  tab_helper->SetFrameInteractionData(web::kMainFakeFrameId,
                                      {base::Milliseconds(140)},
                                      /*interaction_count=*/2,
                                      /*is_main_frame=*/true);

  web::FakeNavigationContext committed_context;
  committed_context.SetHasCommitted(true);
  fake_web_state.OnNavigationStarted(&committed_context);
  fake_web_state.OnNavigationFinished(&committed_context);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);

  CommitCrossDocumentNavigation(fake_web_state);

  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(140), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(140), 1);
  histogram_tester.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram,
      base::Milliseconds(50), 1);
}

// Tests that late messages from the previous page arriving after the next page
// has committed are correctly attributed to the previous page.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       LateMessageFromPreviousPage_LoggedAtNextCommit) {
  NavigateTo(kPageBMainFrameId);
  ReportInteraction(kPageBMainFrameId, base::Milliseconds(200));

  // Page A's last message arrives after page B has committed, from a
  // frame that is no longer in the WebFramesManager.
  ASSERT_FALSE(web_frames_manager_->GetFrameWithId(kPageAMainFrameId));
  ReportInteraction(kPageAMainFrameId, base::Milliseconds(80));
  ExpectNoInteractionToNextPaintSample();

  // Leaving page B logs page A on its own.
  NavigateTo(kPageCMainFrameId);
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(80), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(80), 1);
}

// Tests that destroying the WebState logs both the previous page (with late
// messages) and the current page separately.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       WebStateDestroyedWithPendingPages) {
  NavigateTo(kPageBMainFrameId);
  ReportInteraction(kPageAMainFrameId, base::Milliseconds(80));
  ReportInteraction(kPageBMainFrameId, base::Milliseconds(200));
  ExpectNoInteractionToNextPaintSample();

  DestroyWebState();

  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(80), 1);
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 2);
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(80), 1);
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 2);
}

// Tests that messages from a page that was already logged are dropped instead
// of being attributed to the current page.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       MessageAfterPageWasLogged_Dropped) {
  NavigateTo(kPageBMainFrameId);
  // A second commit happens before page A's message arrives, which logs page A
  // (without data).
  NavigateTo(kPageCMainFrameId);

  ReportInteraction(kPageAMainFrameId, base::Milliseconds(80));
  ReportInteraction(kPageCMainFrameId, base::Milliseconds(120));
  DestroyWebState();

  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(120), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(120), 1);
}

// Tests that a frame restored from the back/forward cache (reusing its frame
// ID) is attributed to the restored page once it becomes available again.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       FrameRestoredFromBackForwardCache_AttributedToRestoredPage) {
  ReportInteraction(kPageAMainFrameId, base::Milliseconds(80));
  NavigateTo(kPageBMainFrameId);

  // Navigating back restores page A with the same frame ID, logging the first
  // visit of page A.
  NavigateTo(kPageAMainFrameId);
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(80), 1);

  ReportInteraction(kPageAMainFrameId, base::Milliseconds(120));
  DestroyWebState();

  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(80), 1);
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(120), 1);
  histogram_tester_.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 2);
}

// Tests that a subsequent cumulative snapshot from the same frame replaces the
// earlier one rather than accumulating interactions across snapshots.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       HiddenSnapshotThenLaterSnapshot_LoggedOnceWithLatestValues) {
  // Snapshot sent when the tab was hidden.
  tab_helper_->SetFrameInteractionData(kPageAMainFrameId,
                                       {base::Milliseconds(300)},
                                       /*interaction_count=*/60,
                                       /*is_main_frame=*/true);
  // The user returned to the tab and interacted more before leaving the page.
  tab_helper_->SetFrameInteractionData(
      kPageAMainFrameId,
      {base::Milliseconds(300), base::Milliseconds(200),
       base::Milliseconds(100)},
      /*interaction_count=*/99, /*is_main_frame=*/true);

  NavigateTo(kPageBMainFrameId);
  NavigateTo(kPageCMainFrameId);

  // 99 interactions ignore 1 outlier, so INP is the second longest duration.
  // Adding both snapshots (159 interactions, 3 outliers) would give 100ms.
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
      base::Milliseconds(200), 1);
}

// Tests that an error page frame registered before the navigation finishes is
// attributed to the error page rather than the previous page.
TEST_F(WebPerformanceMetricsTabHelperFrameAttributionTest,
       ErrorPageFrameRegisteredBeforeCommit_AttributedToErrorPage) {
  ReportInteraction(kPageAMainFrameId, base::Milliseconds(80));

  // Simulates an error page navigation where the frame is registered before
  // navigation finishes.
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  web_state_->OnNavigationStarted(&navigation_context);
  for (web::WebFrame* frame : web_frames_manager_->GetAllWebFrames()) {
    web_frames_manager_->RemoveWebFrame(frame->GetFrameId());
  }
  web_frames_manager_->AddWebFrame(
      web::FakeWebFrame::Create(kErrorPageFrameId, /*is_main_frame=*/true));
  web_state_->OnNavigationFinished(&navigation_context);

  ReportInteraction(kErrorPageFrameId, base::Milliseconds(120));
  DestroyWebState();

  // Separate samples show the pages were logged separately. Merged, they would
  // give a single 120ms sample.
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(80), 1);
  histogram_tester_.ExpectTimeBucketCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
      base::Milliseconds(120), 1);
  histogram_tester_.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 2);
}

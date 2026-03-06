// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
#import "ios/web/common/features.h"
#import "testing/platform_test.h"

// Test fixture for Fullscreen metrics.
class FullscreenMetricsTest : public PlatformTest {
 public:
  FullscreenMetricsTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    mediator_ = std::make_unique<TestFullscreenMediator>(controller(), model());
    observer_ = std::make_unique<TestFullscreenControllerObserver>();
    // Set toolbar height to 100 for easy progress calculations.
    SetUpFullscreenModelForTesting(model(), 100);
    mediator_->AddObserver(observer_.get());
  }
  ~FullscreenMetricsTest() override {
    mediator_->Disconnect();
    mediator_->RemoveObserver(observer_.get());
  }

  TestFullscreenController* controller() {
    return TestFullscreenController::FromBrowser(browser_.get());
  }

  FullscreenModel* model() { return controller()->getModel(); }
  TestFullscreenControllerObserver& observer() { return *observer_; }

  // Manually finishes the animator to trigger the completion block.
  // In unit tests, UIViewPropertyAnimator does not automatically finish.
  // We must manually stop and finish it to trigger the completion block
  // in FullscreenMediator, which is responsible for recording the metric.
  // We set fractionComplete to 1.0 so that currentProgress becomes
  // finalProgress, which ensures the mediator records the correct transition
  // (Enter vs Exit).
  void FinishAnimator(FullscreenAnimator* animator) {
    if (!animator) {
      return;
    }
    animator.fractionComplete = 1.0;
    [animator stopAnimation:NO];
    [animator finishAnimationAtPosition:UIViewAnimatingPositionEnd];
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestFullscreenMediator> mediator_;
  std::unique_ptr<TestFullscreenControllerObserver> observer_;
};

// Tests that FullscreenModel records kForcedByCode when reset for navigation.
TEST_F(FullscreenMetricsTest, RecordsExitForcedByCodeOnReset) {
  model()->ResetForNavigation();  // Clear initial state if any.
  base::HistogramTester histogram_tester;
  model()->ResetForNavigation();
  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kForcedByCode, 1);
}

// Tests that FullscreenModel records kUserControlled when progress reaches 0.0
// via scrolling.
TEST_F(FullscreenMetricsTest, RecordsEnterUserControlled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {web::features::kSmoothScrollingDefault,
                                 web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  base::HistogramTester histogram_tester;
  // Large scroll down to reach progress 0.0.
  SimulateFullscreenUserScrollWithDelta(model(), 500.0);

  ASSERT_EQ(model()->progress(), 0.0);
  histogram_tester.ExpectUniqueSample(
      kEnterFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserControlled, 1);
}

// Tests that FullscreenModel records kUserControlled when progress reaches 1.0
// via scrolling.
TEST_F(FullscreenMetricsTest, RecordsExitUserControlled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {web::features::kSmoothScrollingDefault,
                                 web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  // Ensure heights are set to avoid disabling the model.
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);

  // 1. Enter fullscreen.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  ASSERT_EQ(model()->progress(), 0.0);

  base::HistogramTester histogram_tester;
  // 2. Exit fullscreen by scrolling up.
  SimulateFullscreenUserScrollWithDelta(model(), -100.0);

  ASSERT_EQ(model()->progress(), 1.0);
  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserControlled, 1);
}

// Tests that FullscreenMediator records kUserInitiatedFinishedByCode when
// entering fullscreen via small scroll that triggers animation.
TEST_F(FullscreenMetricsTest, RecordsEnterUserInitiatedFinishedByCode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({web::features::kSmoothScrollingDefault},
                                {web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);
  ASSERT_EQ(model()->progress(), 1.0);

  base::HistogramTester histogram_tester;
  // Scroll down a bit (progress 0.4).
  SimulateFullscreenUserScrollWithDeltaWithoutEnding(model(), 60.0);
  ASSERT_EQ(model()->progress(), 0.4);
  model()->SetScrollViewIsDragging(false);
  model()->SetScrollViewIsScrolling(false);

  // Smooth scrolling should have started an animator because progress is 0.4.
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 0.0);

  histogram_tester.ExpectUniqueSample(
      kEnterFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode, 1);
}

// Tests that FullscreenMediator records kUserInitiatedFinishedByCode when
// entering fullscreen via small scroll with smooth scrolling disabled.
TEST_F(FullscreenMetricsTest,
       RecordsEnterUserInitiatedFinishedByCodeWithSmoothScrollingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(web::features::kSmoothScrollingDefault);

  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);
  ASSERT_EQ(model()->progress(), 1.0);

  base::HistogramTester histogram_tester;
  // Scroll down a bit (direction kDown).
  SimulateFullscreenUserScrollWithDelta(model(), 10.0);
  ASSERT_EQ(model()->GetLastScrollDirection(),
            FullscreenModelScrollDirection::kDown);

  // When smooth scrolling is disabled, the mediator uses the scroll direction
  // to start an animation. Direction kDown triggers an ENTER_FULLSCREEN
  // animation.
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 0.0);

  histogram_tester.ExpectUniqueSample(
      kEnterFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode, 1);
}

// Tests that FullscreenMediator records kUserInitiatedFinishedByCode when
// exiting fullscreen via small scroll up that triggers animation.
TEST_F(FullscreenMetricsTest, RecordsExitUserInitiatedFinishedByCode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({web::features::kSmoothScrollingDefault},
                                {web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);

  // Enter fullscreen first.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  // Finish the animation.
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 0.0);

  base::HistogramTester histogram_tester;
  // Scroll up a little bit (progress 0.6).
  // Initial y_offset was 100. base_offset = 0.
  // Scroll up by 60 => y_offset = 40.
  SimulateFullscreenUserScrollWithDeltaWithoutEnding(model(), -60.0);
  ASSERT_EQ(model()->progress(), 0.6);
  model()->SetScrollViewIsDragging(false);
  model()->SetScrollViewIsScrolling(false);

  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 1.0);

  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode, 1);
}

// Tests that FullscreenMediator records kUserInitiatedFinishedByCode when
// exiting fullscreen via small scroll with smooth scrolling disabled.
TEST_F(FullscreenMetricsTest,
       RecordsExitUserInitiatedFinishedByCodeWithSmoothScrollingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(web::features::kSmoothScrollingDefault);

  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);

  // Enter fullscreen first.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  // Finish the animation.
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 0.0);

  base::HistogramTester histogram_tester;
  // Scroll up a bit (direction kUp).
  SimulateFullscreenUserScrollWithDelta(model(), -10.0);
  ASSERT_EQ(model()->GetLastScrollDirection(),
            FullscreenModelScrollDirection::kUp);

  // When smooth scrolling is disabled, the mediator uses the scroll direction
  // to start an animation. Direction kUp triggers an EXIT_FULLSCREEN
  // animation.
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 1.0);

  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode, 1);
}

// Tests that FullscreenMediator records kForcedByCode when exiting fullscreen
// is forced by code.
TEST_F(FullscreenMetricsTest, RecordsExitForcedByCode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {web::features::kSmoothScrollingDefault,
                                 web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);

  // Enter fullscreen.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  ASSERT_EQ(model()->progress(), 0.0);

  base::HistogramTester histogram_tester;
  mediator_->ExitFullscreen(FullscreenModeTransitionTrigger::kForcedByCode);
  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 1.0);

  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kForcedByCode, 1);
}

// Tests that FullscreenMediator records kBottomReached when exiting fullscreen
// because the bottom of the page was reached twice.
TEST_F(FullscreenMetricsTest, RecordsExitBottomReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {web::features::kSmoothScrollingDefault,
                                 web::features::kFullscreenScrollThreshold});

  model()->ResetForNavigation();
  model()->SetContentHeight(1000.0);
  model()->SetScrollViewHeight(200.0);

  // 1. Enter fullscreen.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  ASSERT_EQ(model()->progress(), 0.0);

  // 2. Reach bottom for the first time.
  // max_offset = 1000 - 200 = 800.
  SimulateScrollToBottom(model());
  // Should not have started an exit animation yet.
  EXPECT_FALSE(observer().animator());

  base::HistogramTester histogram_tester;
  // 3. Reach bottom for the second time.
  SimulateScrollToBottom(model());

  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 1.0);

  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kBottomReached, 1);
}

// Tests that FullscreenMediator records kUserTapped when exiting fullscreen.
TEST_F(FullscreenMetricsTest, RecordsExitUserTapped) {
  model()->ResetForNavigation();
  model()->SetScrollViewHeight(200.0);
  model()->SetContentHeight(1000.0);

  // Enter fullscreen.
  SimulateFullscreenUserScrollWithDelta(model(), 100.0);
  ASSERT_EQ(model()->progress(), 0.0);

  base::HistogramTester histogram_tester;
  // Simulate user tap on status bar.
  mediator_->ExitFullscreen(FullscreenModeTransitionTrigger::kUserTapped);

  FinishAnimator(observer().animator());
  ASSERT_EQ(model()->progress(), 1.0);

  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kUserTapped, 1);
}

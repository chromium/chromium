// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"

#import "base/ios/ios_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_metrics_util.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils_test_support.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class DefaultBrowserPromoNonModalSchedulerTest : public PlatformTest {
 protected:
  DefaultBrowserPromoNonModalSchedulerTest() {}

  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    std::unique_ptr<TestChromeBrowserState> chrome_browser_state =
        test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state.get());

    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kInfobarBanner)
        ->SetPresentationContext(&overlay_presentation_context_);

    // Add initial web state
    auto web_state = std::make_unique<web::FakeWebState>();
    test_web_state_ = web_state.get();
    test_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(test_web_state_);
    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_ACTIVATE,
                                                WebStateOpener());

    ClearDefaultBrowserPromoData();

    promo_commands_handler_ =
        OCMStrictProtocolMock(@protocol(DefaultBrowserPromoNonModalCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:promo_commands_handler_
                     forProtocol:@protocol(
                                     DefaultBrowserPromoNonModalCommands)];

    scheduler_ = [[DefaultBrowserPromoNonModalScheduler alloc] init];
    scheduler_.browser = browser_.get();
    scheduler_.dispatcher = browser_->GetCommandDispatcher();

    // Stub application so the settings panel doesn't actually open.
    application_ = OCMClassMock([UIApplication class]);
    OCMStub([application_ sharedApplication]).andReturn(application_);
  }

  ~DefaultBrowserPromoNonModalSchedulerTest() override {
    [application_ stopMocking];
  }

  void TearDown() override {
    ClearDefaultBrowserPromoData();
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kInfobarBanner)
        ->SetPresentationContext(nullptr);
  }

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState* test_web_state_;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext overlay_presentation_context_;
  id promo_commands_handler_;
  DefaultBrowserPromoNonModalScheduler* scheduler_;
  id application_ = nil;
};

// Tests that the omnibox paste event triggers the promo to show.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestOmniboxPasteShowsPromo) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // First advance the timer by a small delay. This should not trigger the
  // promo.
  task_env_.FastForwardBy(base::Seconds(1));

  // Then advance the timer by the remaining post-load delay. This should
  // trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(2));

  [promo_commands_handler_ verify];
}

// Tests that the entering the app via first party scheme event triggers the
// promo.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestFirstPartySchemeShowsPromo) {

  [scheduler_ logUserEnteredAppViaFirstPartyScheme];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // First advance the timer by a small delay. This should not trigger the
  // promo.
  task_env_.FastForwardBy(base::Seconds(1));

  // Then advance the timer by the remaining post-load delay. This should
  // trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(2));

  [promo_commands_handler_ verify];
}

// Tests that the completed share event triggers the promo.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestShareCompletedShowsPromo) {

  [scheduler_ logUserFinishedActivityFlow];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-share delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(1));

  [promo_commands_handler_ verify];
}

// Tests that the promo dismisses automatically after the dismissal time and
// the event is stored.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestTimeoutDismissesPromo) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));

  [promo_commands_handler_ verify];

  // Advance the timer by the default dismissal time. This should dismiss the
  // promo.
  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:YES];
  task_env_.FastForwardBy(base::Seconds(60));
  [promo_commands_handler_ verify];

  // Check that NSUserDefaults has been updated.
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 1);
}

// Tests that if the user takes the promo action, that is handled correctly.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestActionDismissesPromo) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));

  [promo_commands_handler_ verify];

  [[application_ expect]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:@{}
      completionHandler:nil];
  [scheduler_ logUserPerformedPromoAction];

  [application_ verify];

  // Check that NSUserDefaults has been updated.
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 1);
}

// Tests that if the user switches to a different tab before the post-load timer
// finishes, the promo does not show.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestTabSwitchPreventsPromoShown) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Switch to a new tab.
  auto web_state = std::make_unique<web::FakeWebState>();
  test_web_state_ = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      1, std::move(web_state), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Advance the timer and the mock handler should not have any interactions.
  task_env_.FastForwardBy(base::Seconds(60));
}

// Tests that if a message is triggered on page load, the promo is not shown.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestMessagePreventsPromoShown) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  InfobarType type = InfobarType::kInfobarTypePasswordSave;

  std::unique_ptr<InfoBarIOS> added_infobar =
      std::make_unique<FakeInfobarIOS>(type, u"");
  InfoBarIOS* infobar = added_infobar.get();
  InfoBarManagerImpl::FromWebState(test_web_state_)
      ->AddInfoBar(std::move(added_infobar));

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      test_web_state_, OverlayModality::kInfobarBanner);

  // Showing a message will also dismiss any existing promos.
  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:YES];
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          infobar, InfobarOverlayType::kBanner, infobar->high_priority()));

  [promo_commands_handler_ verify];

  // Advance the timer and the mock handler not have any interaction.
  task_env_.FastForwardBy(base::Seconds(60));
}

// Tests that backgrounding the app with the promo showing hides the promo but
// does not update the shown promo count.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestBackgroundingDismissesPromo) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));

  [promo_commands_handler_ verify];

  // Background the app.
  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:NO];
  [scheduler_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelBackground];
  [promo_commands_handler_ verify];

  // Check that NSUserDefaults has not been updated.
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 0);
}

// Tests that entering the tab grid with the promo showing hides the promo but
// does not update the shown promo count.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestTabGridDismissesPromo) {

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));

  [promo_commands_handler_ verify];

  // Enter the tab grid.
  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:YES];
  [scheduler_ logTabGridEntered];
  [promo_commands_handler_ verify];

  // Check that NSUserDefaults has not been updated.
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 0);
}

// Tests background cancel metric logs correctly.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestBackgroundCancelMetric) {

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);

  [scheduler_ logUserPastedInOmnibox];

  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:NO];

  [scheduler_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelBackground];

  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 1);
}

// Tests background cancel metric is not logged after a promo is shown.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestBackgroundCancelMetricNotLogAfterPromoShown) {

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));
  [promo_commands_handler_ verify];

  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:NO];

  [scheduler_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelBackground];

  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);
}

// Tests background cancel metric is not logged after a promo is dismissed.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestBackgroundCancelMetricNotLogAfterPromoDismiss) {

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler_ expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::Seconds(3));
  [promo_commands_handler_ verify];

  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:YES];

  task_env_.FastForwardBy(base::Seconds(100));

  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:NO];

  [scheduler_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelBackground];

  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);
}

// Tests background cancel metric is not logged when a promo can't be shown.
// Prevents crbug.com/1221379 regression.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest,
       TestBackgroundCancelMetricDoesNotLogWhenPromoNotShown) {

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);

  // Disable the promo by creating a fake cool down.
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setObject:[NSDate date]
                       forKey:@"lastTimeUserInteractedWithFullscreenPromo"];

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should not trigger the
  // promo.
  task_env_.FastForwardBy(base::Seconds(3));
  // Advance the timer by the post-load delay. This should not dismiss the
  // promo.
  task_env_.FastForwardBy(base::Seconds(100));

  [[promo_commands_handler_ expect]
      dismissDefaultBrowserNonModalPromoAnimated:NO];

  [scheduler_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelBackground];

  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink",
      NonModalPromoAction::kBackgroundCancel, 0);
}

// Tests that if the user currently has Chrome as default, the promo does not
// show. Prevents regression of crbug.com/1224875
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, NoPromoIfDefault) {

  // Mark Chrome as currently default
  LogOpenHTTPURLFromExternalURL();

  [scheduler_ logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer and the mock handler should not have any interactions.
  task_env_.FastForwardBy(base::Seconds(60));
}

// Tests that if the promo can't be shown, the state is cleaned up, so a
// DCHECK is not fired on the next page load. Prevents regression of
// crbug.com/1224427
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, NoDCHECKIfPromoNotShown) {

  [scheduler_ logUserPastedInOmnibox];

  // Switch to a new tab before loading a page. This will prevent the promo from
  // showing.
  auto web_state = std::make_unique<web::FakeWebState>();
  browser_->GetWebStateList()->InsertWebState(
      1, std::move(web_state), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Activate the first page again.
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer and the mock handler should not have any interactions and
  // there should be no DCHECK.
  task_env_.FastForwardBy(base::Seconds(60));
}

}  // namespace

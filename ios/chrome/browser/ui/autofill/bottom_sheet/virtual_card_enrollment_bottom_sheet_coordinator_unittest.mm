// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Tests the SetUpListView and subviews.
class VirtualCardEnrollmentBottomSheetCoordinatorTest : public PlatformTest {
 public:
  VirtualCardEnrollmentBottomSheetCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Create a FakeWebState.
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(browser_state_.get());

    // Create a FakeWebFramesmanager with AutofillBottomSheetJavaScriptFeature.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    fake_web_state->SetWebFramesManager(content_world,
                                        std::move(frames_manager));

    // Insert and activete FakeWebState.
    int web_state_index =
        browser_->GetWebStateList()->InsertWebState(std::move(fake_web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(web_state_index);
    web::WebState* web_state =
        browser_->GetWebStateList()->GetWebStateAt(web_state_index);

    // Create the tab helper that will hold the callbacks.
    AutofillBottomSheetTabHelper::CreateForWebState(web_state);
    AutofillBottomSheetTabHelper::FromWebState(web_state)
        ->ShowVirtualCardEnrollmentBottomSheet(
            model_, autofill::VirtualCardEnrollmentCallbacks(
                        /*accept_callback=*/base::BindOnce(
                            &VirtualCardEnrollmentBottomSheetCoordinatorTest::
                                OnAcceptVirtualCard,
                            weak_factory_.GetWeakPtr()),
                        /*cancel_callback=*/base::BindOnce(
                            &VirtualCardEnrollmentBottomSheetCoordinatorTest::
                                OnDeclineVirtualCard,
                            weak_factory_.GetWeakPtr())));

    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    model_.enrollment_fields.virtual_card_enrollment_source =
        autofill::VirtualCardEnrollmentSource::kDownstream;

    application_commands_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_commands_
                     forProtocol:@protocol(ApplicationCommands)];

    coordinator_ = [[VirtualCardEnrollmentBottomSheetCoordinator alloc]
             initWithUIModel:model_
          baseViewController:window_.rootViewController
                     browser:browser_.get()];
  }

 protected:
  void OnAcceptVirtualCard() { times_accept_virtual_card_called_++; }
  void OnDeclineVirtualCard() { times_decline_virtual_card_called_++; }

  int times_accept_virtual_card_called_ = 0;
  int times_decline_virtual_card_called_ = 0;
  base::HistogramTester histogram_tester_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  id<ApplicationCommands> application_commands_;
  UIWindow* window_;
  VirtualCardEnrollmentBottomSheetCoordinator* coordinator_;
  autofill::VirtualCardEnrollUiModel model_;
  base::WeakPtrFactory<VirtualCardEnrollmentBottomSheetCoordinatorTest>
      weak_factory_{this};
};

#pragma mark - Tests

// Test that pushing accept calls the provided callback.
TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, AcceptButtonPushed) {
  [coordinator_ start];

  [coordinator_ didAccept];
  EXPECT_EQ(times_accept_virtual_card_called_, 1);
  EXPECT_EQ(times_decline_virtual_card_called_, 0);

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}

// Test that the result metric is logged when the prompt is accepted.
TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, LogsAcceptedMetric) {
  [coordinator_ start];

  [coordinator_ didAccept];
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Downstream.FirstShow",
      autofill::VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      /*expected_count=*/1);

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}

// Test that using the primary button logs the correct exit reason.
TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, CancelButtonPushed) {
  [coordinator_ start];

  [coordinator_ didCancel];
  EXPECT_EQ(times_accept_virtual_card_called_, 0);
  EXPECT_EQ(times_decline_virtual_card_called_, 1);

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}

// Test that the result metric is logged when the prompt is cancelled.
TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, LogsCancelledMetric) {
  [coordinator_ start];

  [coordinator_ didCancel];
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Downstream.FirstShow",
      autofill::VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
      /*expected_count=*/1);

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}

TEST_F(VirtualCardEnrollmentBottomSheetCoordinatorTest, OpensNewTabForLinks) {
  [coordinator_ start];
  CrURL* url = [[CrURL alloc]
      initWithNSURL:[NSURL URLWithString:@"https://example.test"]];

  OCMExpect([application_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return command.URL == url.gurl;
      }]]);

  [coordinator_ didTapLinkURL:url text:nil];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
}

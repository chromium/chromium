// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/payments/coordinator/payments_suggestion_bottom_sheet_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_util.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/payments/coordinator/payments_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests the SetUpListView and subviews.
class PaymentsSuggestionBottomSheetCoordinatorTest : public PlatformTest {
 public:
  PaymentsSuggestionBottomSheetCoordinatorTest() {
    TestProfileIOS::Builder builder;
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(TipsManagerIOSFactory::GetInstance(),
                              TipsManagerIOSFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
    // Set circular SyncService dependency to null.
    personal_data_manager->SetSyncServiceForTest(nullptr);

    mock_autofill_commands_handler_ =
        OCMProtocolMock(@protocol(AutofillCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_autofill_commands_handler_
                     forProtocol:@protocol(AutofillCommands)];

    mock_settings_commands_handler_ =
        OCMProtocolMock(@protocol(SettingsCommands));

    window_ = scoped_key_window_.Get();
    UIView.animationsEnabled = NO;

    InsertWebState();
    credit_card_ = autofill::test::GetCreditCard();
    virtual_card_ = autofill::test::GetVirtualCard();
    personal_data_manager->payments_data_manager().AddServerCreditCardForTest(
        std::make_unique<autofill::CreditCard>(credit_card_));
    personal_data_manager->payments_data_manager().SetSyncingForTest(true);

    params_.has_user_gesture = true;
    coordinator_ = [[PaymentsSuggestionBottomSheetCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                            params:params_];
    coordinator_.settingsHandler = mock_settings_commands_handler_;
  }

  ~PaymentsSuggestionBottomSheetCoordinatorTest() override {
    UIView.animationsEnabled = YES;
  }

 protected:
  // Creates and inserts a new WebState.
  int InsertWebState() {
    web::WebState::CreateParams params(profile_.get());
    std::unique_ptr<web::WebState> web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get());

    int insertion_index = browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->ActivateWebStateAt(insertion_index);

    return insertion_index;
  }

  NSString* BackendIdentifier() {
    return base::SysUTF8ToNSString(credit_card_.guid());
  }

  // Returns true if the bottom sheet view controller has been presented and its
  // presentation transition has completed. UIKit drops dismissals requested
  // while a presentation transition is still in flight.
  [[nodiscard]] bool WaitForSheetPresentation() {
    UIViewController* root_view_controller = window_.rootViewController;
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool {
          UIViewController* presented_view_controller =
              root_view_controller.presentedViewController;
          return presented_view_controller &&
                 !presented_view_controller.beingPresented;
        });
  }

  // Returns true if `dismiss_condition` is satisfied after stopping the
  // coordinator and spinning the run loop for the asynchronous UIKit dismissal
  // completion block. Stopping the coordinator first disconnects the mediator
  // before the completion block runs, as `FormSuggestionTabHelper` is not
  // attached in this test fixture.
  [[nodiscard]] bool StopCoordinatorAndWaitForDismissal(
      ConditionBlock dismiss_condition) {
    [coordinator_ stop];
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, dismiss_condition);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_autofill_commands_handler_;
  id mock_settings_commands_handler_;
  ScopedKeyWindow scoped_key_window_;
  UIWindow* window_;
  PaymentsSuggestionBottomSheetCoordinator* coordinator_;
  autofill::FormActivityParams params_;
  autofill::CreditCard credit_card_;
  autofill::CreditCard virtual_card_;
};

#pragma mark - Tests

// Test that using the primary button logs the correct exit reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PrimaryButton) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];
  ASSERT_TRUE(WaitForSheetPresentation());

  __block bool dismiss_invoked = false;
  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet])
      .andDo(^(NSInvocation* invocation) {
        dismiss_invoked = true;
      });

  [coordinator_ primaryButtonTappedForCard:[[CreditCardData alloc]
                                               initWithCreditCard:credit_card_
                                                             icon:nil]
                                   atIndex:0];
  EXPECT_TRUE(StopCoordinatorAndWaitForDismissal(^bool {
    return dismiss_invoked;
  }));

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kUsePaymentsSuggestion, 1);
}

// Test that using the primary button logs the correct exit reason when a
// virtual card is used
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PrimaryButtonVirtualCard) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];
  ASSERT_TRUE(WaitForSheetPresentation());

  __block bool dismiss_invoked = false;
  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet])
      .andDo(^(NSInvocation* invocation) {
        dismiss_invoked = true;
      });

  [coordinator_ primaryButtonTappedForCard:[[CreditCardData alloc]
                                               initWithCreditCard:virtual_card_
                                                             icon:nil]
                                   atIndex:0];
  EXPECT_TRUE(StopCoordinatorAndWaitForDismissal(^bool {
    return dismiss_invoked;
  }));

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kUsePaymentsSuggestion, 1);
}

// Test that using the secondary button logs the correct exit reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, SecondaryButton) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet]);

  [coordinator_ secondaryButtonTapped];
  [coordinator_ viewDidDisappear];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kDismissal, 1);
}

// Test that using the payments method long press menu logs the correct exit
// reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PaymentsMethods) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];
  ASSERT_TRUE(WaitForSheetPresentation());

  OCMExpect([mock_settings_commands_handler_ showCreditCardSettings]);
  __block bool dismiss_invoked = false;
  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet])
      .andDo(^(NSInvocation* invocation) {
        dismiss_invoked = true;
      });

  [coordinator_ displayPaymentMethods];
  EXPECT_TRUE(StopCoordinatorAndWaitForDismissal(^bool {
    return dismiss_invoked;
  }));

  EXPECT_OCMOCK_VERIFY(mock_settings_commands_handler_);
  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kShowPaymentMethods, 1);
}

// Test that using the payments details long press menu logs the correct exit
// reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PaymentsDetails) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];
  ASSERT_TRUE(WaitForSheetPresentation());

  __block bool dismiss_invoked = false;
  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet])
      .andDo(^(NSInvocation* invocation) {
        dismiss_invoked = true;
      });

  [coordinator_
      displayPaymentDetailsForCreditCardIdentifier:BackendIdentifier()];
  EXPECT_TRUE(StopCoordinatorAndWaitForDismissal(^bool {
    return dismiss_invoked;
  }));

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kShowPaymentDetails, 1);
}

// Test that a change of the active WebState tears the sheet down and logs
// kNavigationOrTabChange exactly once, i.e. the subsequent viewDidDisappear
// does not log a second sample.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, NavigationDismissal) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  OCMExpect([mock_autofill_commands_handler_ dismissPaymentsBottomSheet]);

  // Inserting and activating a new WebState changes the active WebState,
  // triggering mediator dismissal due to navigation/WebState change.
  InsertWebState();

  [coordinator_ viewDidDisappear];

  [coordinator_ stop];

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_handler_);
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kNavigationOrTabChange, 1);
}

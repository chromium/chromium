// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Tests the SetUpListView and subviews.
class PaymentsSuggestionBottomSheetCoordinatorTest : public PlatformTest {
 public:
  PaymentsSuggestionBottomSheetCoordinatorTest() {
    TestChromeBrowserState::Builder builder;
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browser_state_.get());
    // Set circular SyncService dependency to null.
    personal_data_manager->SetSyncServiceForTest(nullptr);

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    InsertWebState();
    credit_card_ = autofill::test::GetCreditCard();
    personal_data_manager->AddServerCreditCardForTest(
        std::make_unique<autofill::CreditCard>(credit_card_));
    personal_data_manager->SetSyncingForTest(true);

    coordinator_ = [[PaymentsSuggestionBottomSheetCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                            params:params_];
  }

 protected:
  // Creates and inserts a new WebState.
  int InsertWebState() {
    web::WebState::CreateParams params(browser_state_.get());
    std::unique_ptr<web::WebState> web_state = web::WebState::Create(params);
    AttachTabHelpers(web_state.get(), NO);

    int insertion_index = browser_->GetWebStateList()->InsertWebState(
        /*index=*/0, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->ActivateWebStateAt(insertion_index);

    return insertion_index;
  }

  NSString* BackendIdentifier() {
    return base::SysUTF8ToNSString(credit_card_.guid());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIWindow* window_;
  PaymentsSuggestionBottomSheetCoordinator* coordinator_;
  autofill::FormActivityParams params_;
  autofill::CreditCard credit_card_;
};

#pragma mark - Tests

// Test that using the primary button logs the correct exit reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PrimaryButton) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  [coordinator_ primaryButtonTapped:BackendIdentifier()];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kUsePaymentsSuggestion, 1);
}

// Test that using the secondary button logs the correct exit reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, SecondaryButton) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  [coordinator_ secondaryButtonTapped];
  [coordinator_ viewDidDisappear:NO];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kDismissal, 1);
}

// Test that using the payments method long press menu logs the correct exit
// reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PaymentsMethods) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  [coordinator_ displayPaymentMethods];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kShowPaymentMethods, 1);
}

// Test that using the payments details long press menu logs the correct exit
// reason.
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PaymentsDetails) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  [coordinator_
      displayPaymentDetailsForCreditCardIdentifier:BackendIdentifier()];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kShowPaymentDetails, 1);
}

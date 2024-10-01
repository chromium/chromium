// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
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
    TestProfileIOS::Builder builder;
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
    // Set circular SyncService dependency to null.
    personal_data_manager->SetSyncServiceForTest(nullptr);

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    InsertWebState();
    credit_card_ = autofill::test::GetCreditCard();
    virtual_card_ = autofill::test::GetVirtualCard();
    personal_data_manager->payments_data_manager().AddServerCreditCardForTest(
        std::make_unique<autofill::CreditCard>(credit_card_));
    personal_data_manager->payments_data_manager().SetSyncingForTest(true);

    coordinator_ = [[PaymentsSuggestionBottomSheetCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                            params:params_];
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

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
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

  [coordinator_ primaryButtonTappedForCard:[[CreditCardData alloc]
                                               initWithCreditCard:credit_card_
                                                             icon:nil]
                                   atIndex:0];
  [coordinator_ stop];
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      PaymentsSuggestionBottomSheetExitReason::kUsePaymentsSuggestion, 1);
}

// Test that using the primary button logs the correct exit reason when a
// virtual card is used
TEST_F(PaymentsSuggestionBottomSheetCoordinatorTest, PrimaryButtonVirtualCard) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  [coordinator_ primaryButtonTappedForCard:[[CreditCardData alloc]
                                               initWithCreditCard:virtual_card_
                                                             icon:nil]
                                   atIndex:0];
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
  [coordinator_ viewDidDisappear];

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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface PaymentsScanSaveAndFillOfferBottomSheetCoordinator (Testing) <
    PaymentsScanSaveAndFillOfferBottomSheetDelegate>
@end

class PaymentsScanSaveAndFillOfferBottomSheetCoordinatorTest
    : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    fake_web_state->SetWebFramesManager(
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::make_unique<web::FakeWebFramesManager>());
    int web_state_index =
        browser_->GetWebStateList()->InsertWebState(std::move(fake_web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(web_state_index);

    mock_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_commands_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    autofill::FormActivityParams formActivityParams;
    coordinator_ = [[PaymentsScanSaveAndFillOfferBottomSheetCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                            params:formActivityParams];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_commands_handler_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
  PaymentsScanSaveAndFillOfferBottomSheetCoordinator* coordinator_;
};

// Tests that `paymentsBottomSheetDidDisappear` triggers the
// `dismissPaymentSuggestions` command.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetCoordinatorTest,
       PaymentsBottomSheetDidDisappear) {
  [coordinator_ start];

  [[mock_commands_handler_ expect] dismissPaymentSuggestions];

  [coordinator_ paymentsBottomSheetDidDisappear];

  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Tests that `didTapScanCardButton` triggers the
// `dismissPaymentSuggestions` command.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetCoordinatorTest,
       DidTapScanCardButton) {
  [coordinator_ start];

  __block BOOL method_invoked = NO;
  [[[mock_commands_handler_ expect] andDo:^(NSInvocation* invocation) {
    method_invoked = YES;
  }] dismissPaymentSuggestions];

  [coordinator_ didTapScanCardButton];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return method_invoked;
      }));
  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Tests that `didTapOnCancelButton` triggers the
// `dismissPaymentSuggestions` command.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetCoordinatorTest,
       DidTapOnCancelButton) {
  [coordinator_ start];

  __block BOOL method_invoked = NO;
  [[[mock_commands_handler_ expect] andDo:^(NSInvocation* invocation) {
    method_invoked = YES;
  }] dismissPaymentSuggestions];

  [coordinator_ didTapOnCancelButton];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return method_invoked;
      }));
  EXPECT_OCMOCK_VERIFY(mock_commands_handler_);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/functional/callback_helpers.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SaveCardBottomSheetCoordinatorTest : public PlatformTest {
 public:
  SaveCardBottomSheetCoordinatorTest() {
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
    web::WebState* web_state =
        browser_->GetWebStateList()->GetWebStateAt(web_state_index);

    // Set details of the ui info required for the coordinator to start.
    autofill::AutofillSaveCardUiInfo ui_info =
        autofill::AutofillSaveCardUiInfo();
    ui_info.logo_icon_id = IDR_AUTOFILL_GOOGLE_PAY;
    ui_info.card_label = std::u16string(u"CardName ****2345");
    ui_info.card_sub_label = std::u16string(u"01/29");

    // Create the tab helper that will hold the model.
    AutofillBottomSheetTabHelper::CreateForWebState(web_state);
    AutofillBottomSheetTabHelper::FromWebState(web_state)
        ->ShowSaveCardBottomSheet(
            std::make_unique<autofill::SaveCardBottomSheetModel>(
                std::move(ui_info),
                std::make_unique<autofill::AutofillSaveCardDelegate>(
                    /*save_card_callback=*/static_cast<
                        autofill::payments::PaymentsAutofillClient::
                            UploadSaveCardPromptCallback>(base::DoNothing()),
                    autofill::payments::PaymentsAutofillClient::
                        SaveCreditCardOptions()
                            .with_num_strikes(0))));

    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    application_commands_handler_ =
        OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    autofill_commands_handler_ = OCMProtocolMock(@protocol(AutofillCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:autofill_commands_handler_
                     forProtocol:@protocol(AutofillCommands)];

    coordinator_ = [[SaveCardBottomSheetCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()];
  }

  ~SaveCardBottomSheetCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)application_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)autofill_commands_handler_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIWindow* window_;
  id<ApplicationCommands> application_commands_handler_;
  id<AutofillCommands> autofill_commands_handler_;
  SaveCardBottomSheetCoordinator* coordinator_;
};

// Test when link is clicked, new tab is opened and bottomsheet
// result `kLinkClicked` is logged.
TEST_F(SaveCardBottomSheetCoordinatorTest, OpensNewTabForLinkClicked) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];
  CrURL* url = [[CrURL alloc]
      initWithNSURL:[NSURL URLWithString:@"https://example.test"]];

  OCMExpect([application_commands_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return command.URL == url.gurl;
      }]]);

  [coordinator_ didTapLinkURL:url];

  [coordinator_ stop];

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.0."
      "NoFixFlow",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kLinkClicked,
      /*expected_count=*/1);
}

// Test `OnViewDisappeared` dismisses bottomsheet and bottomsheet result
// `kSwiped` is logged.
TEST_F(SaveCardBottomSheetCoordinatorTest, OnViewDisappeared) {
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  OCMExpect([autofill_commands_handler_ dismissSaveCardBottomSheet]);
  [coordinator_ onViewDisappeared];

  [coordinator_ stop];

  histogram_tester.ExpectBucketCount(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.0."
      "NoFixFlow",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kSwiped,
      /*expected_count=*/1);
}

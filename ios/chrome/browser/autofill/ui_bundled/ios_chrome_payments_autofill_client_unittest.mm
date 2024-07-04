// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

#import "base/test/scoped_feature_list.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface FakeAutofillCommands : NSObject <AutofillCommands>

// Returns the model provided to showVirtualCardEnrollmentBottomSheet
- (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)
    getVirtualCardEnrollUiModel;
@end

@implementation FakeAutofillCommands {
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> _virtualCardEnrollUiModel;
}

- (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)
    getVirtualCardEnrollUiModel {
  return std::move(_virtualCardEnrollUiModel);
}

#pragma mark - AutofillCommands

- (void)showCardUnmaskAuthentication {
}
- (void)continueCardUnmaskWithOtpAuth {
}
- (void)continueCardUnmaskWithCvcAuth {
}
- (void)showPasswordBottomSheet:(const autofill::FormActivityParams&)params {
}
- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params {
}
- (void)showPlusAddressesBottomSheet {
}

- (void)showVirtualCardEnrollmentBottomSheet:
    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model {
  _virtualCardEnrollUiModel = std::move(model);
}

- (void)showEditAddressBottomSheet {
}
- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext {
}
- (void)dismissAutofillErrorDialog {
}
- (void)showAutofillProgressDialog {
}
- (void)dismissAutofillProgressDialog {
}

@end

namespace autofill {
namespace {

using ::testing::_;

class TestChromeAutofillClient : public ChromeAutofillClientIOS {
 public:
  explicit TestChromeAutofillClient(ChromeBrowserState* browser_state,
                                    web::WebState* web_state,
                                    infobars::InfoBarManager* infobar_manager,
                                    AutofillAgent* autofill_agent)
      : ChromeAutofillClientIOS(browser_state,
                                web_state,
                                infobar_manager,
                                autofill_agent) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.test/");
    save_card_delegate_ = MockAutofillSaveCardInfoBarDelegateMobileFactory::
        CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(/*upload=*/true,
                                                               credit_card);
  }
  MockAutofillSaveCardInfoBarDelegateMobile*
  GetAutofillSaveCardInfoBarDelegateIOS() override {
    return save_card_delegate_.get();
  }

 private:
  std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
      save_card_delegate_;
};

class IOSChromePaymentsAutofillClientTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state_.get());
    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                          webState:web_state_.get()];
    autofill_client_ = std::make_unique<TestChromeAutofillClient>(
        browser_state_.get(), web_state_.get(), infobar_manager,
        autofill_agent_);

    // Inject the autofill commands fake into the AutofillTabHelper
    autofill_commands_ = [[FakeAutofillCommands alloc] init];
    AutofillBottomSheetTabHelper::CreateForWebState(web_state_.get());
    AutofillBottomSheetTabHelper::FromWebState(web_state_.get())
        ->SetAutofillBottomSheetHandler(autofill_commands_);
  }

  TestChromeAutofillClient* client() { return autofill_client_.get(); }

  payments::IOSChromePaymentsAutofillClient* payments_client() {
    return client()->GetPaymentsAutofillClient();
  }

  std::unique_ptr<VirtualCardEnrollUiModel> ShowVirtualCardEnrollDialog() {
    payments_client()->ShowVirtualCardEnrollDialog(
        autofill::VirtualCardEnrollmentFields(),
        /*accept_virtual_card_callback=*/base::DoNothing(),
        /*decline_virtual_card_callback=*/base::DoNothing());
    std::unique_ptr<VirtualCardEnrollUiModel> ui_model =
        [autofill_commands_ getVirtualCardEnrollUiModel];
    return ui_model;
  }

 protected:
  FakeAutofillCommands* autofill_commands_;

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  AutofillAgent* autofill_agent_;
  std::unique_ptr<TestChromeAutofillClient> autofill_client_;
};

TEST_F(IOSChromePaymentsAutofillClientTest, CreditCardUploadCompleted) {
  EXPECT_CALL(*(client()->GetAutofillSaveCardInfoBarDelegateIOS()),
              CreditCardUploadCompleted(_, _));
  payments_client()->CreditCardUploadCompleted(
      /*card_saved=*/true, /*on_confirmation_closed_callback=*/std::nullopt);
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithSucess) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  std::unique_ptr<VirtualCardEnrollUiModel> ui_model =
      ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(/*is_vcn_enrolled=*/true);

  EXPECT_EQ(ui_model->enrollment_progress(),
            autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithFailure) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  std::unique_ptr<VirtualCardEnrollUiModel> ui_model =
      ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(/*is_vcn_enrolled=*/false);

  EXPECT_EQ(ui_model->enrollment_progress(),
            autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kFailed);
}

}  // namespace

}  // namespace autofill

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeAutofillCommands : NSObject <AutofillCommands>

// Returns the model provided to showVirtualCardEnrollmentBottomSheet
- (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)
    getVirtualCardEnrollUiModel;

// Returns the error context provided to showAutofillErrorDialog.
- (const std::optional<autofill::AutofillErrorDialogContext>&)
    autofillErrorDialogContext;

// Returns whether showSaveCardBottomSheet was called.
- (BOOL)showSaveCardBottomSheetCalled;
@end

@implementation FakeAutofillCommands {
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> _virtualCardEnrollUiModel;
  std::optional<autofill::AutofillErrorDialogContext> _errorContext;
  BOOL _showSaveCardBottomSheet;
}

- (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)
    getVirtualCardEnrollUiModel {
  return std::move(_virtualCardEnrollUiModel);
}

- (const std::optional<autofill::AutofillErrorDialogContext>&)
    autofillErrorDialogContext {
  return _errorContext;
}

- (BOOL)showSaveCardBottomSheetCalled {
  return _showSaveCardBottomSheet;
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

- (void)showSaveCardBottomSheetOnOriginWebState:(web::WebState*)originWebState {
  _showSaveCardBottomSheet = YES;
}

- (void)dismissSaveCardBottomSheet {
}

- (void)showVirtualCardEnrollmentBottomSheet:
            (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                              originWebState:(web::WebState*)originWebState {
  _virtualCardEnrollUiModel = std::move(model);
}

- (void)showEditAddressBottomSheet {
}

- (void)dismissEditAddressBottomSheet {
}

- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext {
  _errorContext = std::move(errorContext);
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

class TestChromeAutofillClient
    : public WithFakedFromWebState<ChromeAutofillClientIOS> {
 public:
  explicit TestChromeAutofillClient(ProfileIOS* profile,
                                    web::WebState* web_state,
                                    infobars::InfoBarManager* infobar_manager,
                                    AutofillAgent* autofill_agent)
      : WithFakedFromWebState<ChromeAutofillClientIOS>(profile,
                                                       web_state,
                                                       infobar_manager,
                                                       autofill_agent) {
  }

  void RemoveAutofillSaveCardInfoBar() override {
    removed_save_card_infobar_ = true;
  }

  bool DidRemoveSaveCardInfobar() { return removed_save_card_infobar_; }

 private:
  bool removed_save_card_infobar_ = false;
};

class IOSChromePaymentsAutofillClientTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state_.get());
    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:profile_->GetPrefs()
                                          webState:web_state_.get()];
    autofill_client_ = std::make_unique<TestChromeAutofillClient>(
        profile_.get(), web_state_.get(), infobar_manager, autofill_agent_);

    // Inject the autofill commands fake into the AutofillTabHelper and
    // ChromeAutofillClient.
    autofill_commands_ = [[FakeAutofillCommands alloc] init];
    AutofillBottomSheetTabHelper::CreateForWebState(web_state_.get());
    bottomsheet_tab_helper_ =
        AutofillBottomSheetTabHelper::FromWebState(web_state_.get());
    bottomsheet_tab_helper_->SetAutofillBottomSheetHandler(autofill_commands_);

    autofill_client_->set_commands_handler(autofill_commands_);
  }

  TestChromeAutofillClient* client() { return autofill_client_.get(); }

  FakeAutofillCommands* autofill_commands() { return autofill_commands_; }

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
  raw_ptr<AutofillBottomSheetTabHelper> bottomsheet_tab_helper_;

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  AutofillAgent* autofill_agent_;
  std::unique_ptr<TestChromeAutofillClient> autofill_client_;
};

// Test that save card bottomsheet is not shown for local save when flag is
// disabled.
TEST_F(IOSChromePaymentsAutofillClientTest,
       DoNotShowLocalSaveCardBottomSheet_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillLocalSaveCardBottomSheet);
  payments_client()->ShowSaveCreditCardLocally(
      test::GetCreditCard(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);
}

// Test that save card bottomsheet is not shown for upload save when flag is
// disabled.
TEST_F(IOSChromePaymentsAutofillClientTest,
       DoNotShowSaveCardBottomSheet_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillSaveCardBottomSheet);
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);
}

// Test that on credit card upload completed successfully with infobar showing,
// `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completed_` is set and
// runs
// `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completion_callback_`
// with card saved.
TEST_F(IOSChromePaymentsAutofillClientTest,
       CreditCardUploadCompleted_CardSaved_WithInfobar) {
  // Shows card upload in an infobar for a card with 1 strike.
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(1)
          .with_show_prompt(true),
      base::DoNothing());

  // Sets credit card upload completion callback that gets executed with the
  // save card result as saved.
  base::MockCallback<base::OnceCallback<void(bool card_saved)>>
      mock_credit_card_upload_completion_callback;
  client()
      ->GetAutofillSaveCardInfoBarDelegateIOS()
      ->SetCreditCardUploadCompletionCallback(
          mock_credit_card_upload_completion_callback.Get());

  EXPECT_CALL(mock_credit_card_upload_completion_callback,
              Run(/*card_saved=*/true));
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_TRUE(client()
                  ->GetAutofillSaveCardInfoBarDelegateIOS()
                  ->IsCreditCardUploadComplete());
  EXPECT_FALSE(client()->DidRemoveSaveCardInfobar());
}

// Test that on credit card upload completed unsuccessfully with infobar
// showing, `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completed_`
// is set and runs
// `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completion_callback_`
// with card not saved and error context is set.
TEST_F(IOSChromePaymentsAutofillClientTest,
       CreditCardUploadCompleted_CardNotSaved_WithInfobar) {
  // Shows card upload in an infobar for a card with 1 strike.
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(1)
          .with_show_prompt(true),
      base::DoNothing());

  // Sets credit card upload completion callback that gets executed with the
  // save card result as not saved.
  base::MockCallback<base::OnceCallback<void(bool card_saved)>>
      mock_credit_card_upload_completion_callback;
  client()
      ->GetAutofillSaveCardInfoBarDelegateIOS()
      ->SetCreditCardUploadCompletionCallback(
          mock_credit_card_upload_completion_callback.Get());

  EXPECT_CALL(mock_credit_card_upload_completion_callback,
              Run(/*card_saved=*/false));
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_TRUE(client()
                  ->GetAutofillSaveCardInfoBarDelegateIOS()
                  ->IsCreditCardUploadComplete());
  EXPECT_TRUE(client()->DidRemoveSaveCardInfobar());
  const std::optional<AutofillErrorDialogContext>& error_context =
      [autofill_commands() autofillErrorDialogContext];
  EXPECT_TRUE(error_context.has_value());
  EXPECT_EQ(error_context.value().type,
            AutofillErrorDialogType::kCreditCardUploadError);
}

// Test that on credit card upload's client-side timeout with infobar showing,
// `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completed_` is set and
// runs
// `AutofillSaveCardInfoBarDelegateIOS.credit_card_upload_completion_callback_`
// with save card result as not saved and error context is not set.
TEST_F(
    IOSChromePaymentsAutofillClientTest,
    CreditCardUploadCompleted_ClientSideTimeout_WithInfobar_NoErrorConfirmation) {
  // Shows card upload in an infobar for a card with 1 strike.
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(1)
          .with_show_prompt(true),
      base::DoNothing());

  // Sets credit card upload completion callback that gets executed with the
  // save card result as not saved.
  base::MockCallback<base::OnceCallback<void(bool card_saved)>>
      mock_credit_card_upload_completion_callback;
  client()
      ->GetAutofillSaveCardInfoBarDelegateIOS()
      ->SetCreditCardUploadCompletionCallback(
          mock_credit_card_upload_completion_callback.Get());

  EXPECT_CALL(mock_credit_card_upload_completion_callback,
              Run(/*card_saved=*/false));
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::
          kClientSideTimeout,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_TRUE(client()
                  ->GetAutofillSaveCardInfoBarDelegateIOS()
                  ->IsCreditCardUploadComplete());
  EXPECT_TRUE(client()->DidRemoveSaveCardInfobar());
  const std::optional<AutofillErrorDialogContext>& error_context =
      [autofill_commands() autofillErrorDialogContext];
  EXPECT_FALSE(error_context.has_value());
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithSucess) {
  std::unique_ptr<VirtualCardEnrollUiModel> ui_model =
      ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_EQ(ui_model->enrollment_progress(),
            autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithFailureSetsEnrollmentProgress) {
  std::unique_ptr<VirtualCardEnrollUiModel> ui_model =
      ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  EXPECT_EQ(ui_model->enrollment_progress(),
            autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kFailed);
}

// Tests metrics for save card confirmation view shown for card not uploaded.
TEST_F(IOSChromePaymentsAutofillClientTest,
       ConfirmationViewShownForCardNotUploaded_Metrics) {
  base::HistogramTester histogram_tester;

  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure,
      /*on_confirmation_closed_callback=*/std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardNotUploaded",
      /*is_shown=*/true, 1);
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithFailureShowsErrorDialog) {
  ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  autofill::AutofillErrorDialogContext expected_context;
  expected_context.type =
      autofill::AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError;
  EXPECT_EQ([autofill_commands_ autofillErrorDialogContext],
            std::make_optional(expected_context));
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithSuccessDoesNotShowAlert) {
  ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  // Expect showAutofillErrorDialog has not been called.
  EXPECT_EQ([autofill_commands_ autofillErrorDialogContext], std::nullopt);
}

TEST_F(IOSChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompletedWithClientSideTimeoutDoesNotShowAlert) {
  ShowVirtualCardEnrollDialog();

  payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout);

  // Expect showAutofillErrorDialog has not been called.
  EXPECT_EQ([autofill_commands_ autofillErrorDialogContext], std::nullopt);
}

class IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest
    : public IOSChromePaymentsAutofillClientTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillSaveCardBottomSheet};
};

TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       ShowSaveCardBottomSheet_FlagEnabled) {
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_TRUE([autofill_commands() showSaveCardBottomSheetCalled]);
}

TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       DoNoShowSaveCardBottomSheet_CardWithMoreThan0Strike) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(1)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.1."
      "NoFixFlow",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/1);
}

TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       DoNoShowSaveCardBottomSheet_ForRequestingCardHolderName) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardToCloud(
      test::GetIncompleteCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.0."
      "RequestingCardHolderName",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/1);
}

TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       DoNoShowSaveCardBottomSheet_ForRequestingExpiryDate) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardToCloud(
      test::GetExpiredCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_expiration_date_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.0."
      "RequestingExpiryDate",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/1);
}

TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       DoNoShowSaveCardBottomSheet_ForRequestingCardHolderNameAndExpiryDate) {
  base::HistogramTester histogram_tester;

  // Passing an empty CreditCard() to `ShowSaveCreditCardToCloud`
  // since this test is regarding missing cardholder name and expiry date.
  payments_client()->ShowSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true)
          .with_should_request_expiration_date_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Server.BottomSheet.NumStrikes.0."
      "RequestingCardHolderNameAndExpiryDate",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/1);
}

// Test that on save card success, the save card bottomsheet model's state is
// set to kSaved.
TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       CreditCardUploadCompleted_CardSaved) {
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_EQ((bottomsheet_tab_helper_->GetSaveCardBottomSheetModel())
                ->save_card_state(),
            autofill::SaveCardBottomSheetModel::SaveCardState::kSaved);
}

// Test that on save card failure, the save card bottomsheet model's state is
// set to kFailed and the error dialog is shown.
TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       CreditCardUploadCompleted_CardNotSaved) {
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_EQ((bottomsheet_tab_helper_->GetSaveCardBottomSheetModel())
                ->save_card_state(),
            autofill::SaveCardBottomSheetModel::SaveCardState::kFailed);
  const std::optional<AutofillErrorDialogContext>& error_context =
      [autofill_commands() autofillErrorDialogContext];
  EXPECT_TRUE(error_context.has_value());
  EXPECT_EQ(error_context.value().type,
            AutofillErrorDialogType::kCreditCardUploadError);
}

// Test that on getting client-side timeout, the save card bottomsheet model's
// state is set to kFailed and the error dialog is not shown.
TEST_F(IOSChromePaymentsAutofillClientWithSaveCardBottomSheetTest,
       CreditCardUploadCompleted_ClientSideTimeout_NoErrorConfirmation) {
  payments_client()->ShowSaveCreditCardToCloud(
      test::GetCreditCard(), LegalMessageLines(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  payments_client()->CreditCardUploadCompleted(
      /*result=*/payments::PaymentsAutofillClient::PaymentsRpcResult::
          kClientSideTimeout,
      /*on_confirmation_closed_callback=*/std::nullopt);
  EXPECT_EQ((bottomsheet_tab_helper_->GetSaveCardBottomSheetModel())
                ->save_card_state(),
            autofill::SaveCardBottomSheetModel::SaveCardState::kFailed);
  const std::optional<AutofillErrorDialogContext>& error_context =
      [autofill_commands() autofillErrorDialogContext];
  EXPECT_FALSE(error_context.has_value());
}

class IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest
    : public IOSChromePaymentsAutofillClientTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillLocalSaveCardBottomSheet};
};

TEST_F(IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest,
       ShowSaveCardBottomSheet_FlagEnabled) {
  payments_client()->ShowSaveCreditCardLocally(
      test::GetCreditCard(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_TRUE([autofill_commands() showSaveCardBottomSheetCalled]);
}

TEST_F(IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest,
       DoNoShowSaveCardBottomSheet_CardWithMoreThan0Strike) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardLocally(
      test::GetCreditCard(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_num_strikes(1)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_FALSE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Local.BottomSheet.NumStrikes.1."
      "NoFixFlow",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/1);
}

TEST_F(IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest,
       ShowSaveCardBottomSheet_WithMissingCardHolderName) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardLocally(
      test::GetIncompleteCreditCard(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_TRUE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Local.BottomSheet.NumStrikes.0."
      "RequestingCardHolderName",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/0);
}

TEST_F(IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest,
       ShowSaveCardBottomSheet_WithInvalidExpiryDate) {
  base::HistogramTester histogram_tester;

  payments_client()->ShowSaveCreditCardLocally(
      test::GetExpiredCreditCard(),
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_expiration_date_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_TRUE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Local.BottomSheet.NumStrikes.0."
      "RequestingExpiryDate",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/0);
}

TEST_F(IOSChromePaymentsAutofillClientWithLocalSaveCardBottomSheetTest,
       ShowSaveCardBottomSheet_WithMissingCardHolderNameAndInvalidExpiryDate) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetIncompleteCreditCard();
  card.SetExpirationMonth(1);
  card.SetExpirationYear(2020);
  payments_client()->ShowSaveCreditCardLocally(
      card,
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true)
          .with_should_request_expiration_date_from_user(true)
          .with_num_strikes(0)
          .with_show_prompt(true),
      base::DoNothing());
  EXPECT_TRUE([autofill_commands() showSaveCardBottomSheetCalled]);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.IOS.Local.BottomSheet.NumStrikes.0."
      "RequestingCardHolderNameAndExpiryDate",
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
      /*expected_count=*/0);
}

}  // namespace

}  // namespace autofill

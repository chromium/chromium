// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/strings/grit/components_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_otp_verifier_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"

namespace ios_web_view {

// Fake unmask delegate used to handle OTP unmask events.
class FakeOtpUnmaskDelegate : public autofill::OtpUnmaskDelegate {
 public:
  FakeOtpUnmaskDelegate() : weak_factory_(this) {}

  FakeOtpUnmaskDelegate(const FakeOtpUnmaskDelegate&) = delete;
  FakeOtpUnmaskDelegate& operator=(const FakeOtpUnmaskDelegate&) = delete;

  virtual ~FakeOtpUnmaskDelegate() = default;

  // autofill::OtpUnmaskDelegate implementation.
  void OnUnmaskPromptAccepted(const std::u16string& otp) override {
    last_otp_ = otp;
  }
  void OnUnmaskPromptClosed(bool user_closed_dialog) override {
    dialog_closed_ = true;
    user_closed_dialog_ = user_closed_dialog;
  }
  void OnNewOtpRequested() override { new_otp_requested_ = true; }
  base::WeakPtr<autofill::OtpUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Sets the CWVCreditCardOTPVerifier instance to which this delegate is
  // related.
  void SetCreditCardOTPVerifier(
      CWVCreditCardOTPVerifier* credit_card_otp_verifier) {
    credit_card_otp_verifier_ = credit_card_otp_verifier;
  }

  const std::u16string& last_otp() const { return last_otp_; }
  bool new_otp_requested() const { return new_otp_requested_; }
  bool dialog_closed() const { return dialog_closed_; }
  bool user_closed_dialog() const { return user_closed_dialog_; }

 private:
  __weak CWVCreditCardOTPVerifier* credit_card_otp_verifier_;

  std::u16string last_otp_;
  bool new_otp_requested_ = false;
  bool dialog_closed_ = false;
  bool user_closed_dialog_ = false;

  base::WeakPtrFactory<FakeOtpUnmaskDelegate> weak_factory_;
};

class CWVCreditCardOTPVerifierTest : public PlatformTest {
 protected:
  CWVCreditCardOTPVerifierTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();

    base::FilePath pak_file;
    base::PathService::Get(base::DIR_ASSETS, &pak_file);

    pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file, ui::k200Percent);
    base::FilePath components_pak_file;
    base::PathService::Get(base::DIR_ASSETS, &components_pak_file);
    components_pak_file = components_pak_file.Append(
        FILE_PATH_LITERAL("components_strings_en-US.pak"));
    resource_bundle.AddDataPackFromPath(components_pak_file,
                                        ui::kScaleFactorNone);
  }

  ~CWVCreditCardOTPVerifierTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    otp_unmask_delegate_ = std::make_unique<FakeOtpUnmaskDelegate>();

    autofill::CardUnmaskChallengeOption challenge_option;
    challenge_option.type = autofill::CardUnmaskChallengeOptionType::kSmsOtp;
    challenge_option.challenge_input_length = 6;

    credit_card_otp_verifier_ = [[CWVCreditCardOTPVerifier alloc]
        initWithCardType:autofill::CreditCard::RecordType::kMaskedServerCard
         challengeOption:challenge_option
          unmaskDelegate:otp_unmask_delegate_->GetWeakPtr()];
    otp_unmask_delegate_->SetCreditCardOTPVerifier(credit_card_otp_verifier_);

    // Set up mock delegate for UI updates
    mock_ui_delegate_ =
        OCMProtocolMock(@protocol(CWVCreditCardOTPVerifierDelegate));
    credit_card_otp_verifier_.delegate = mock_ui_delegate_;
  }

  void TearDown() override {
    credit_card_otp_verifier_ = nil;
    otp_unmask_delegate_.reset();
    mock_ui_delegate_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<FakeOtpUnmaskDelegate> otp_unmask_delegate_;
  CWVCreditCardOTPVerifier* credit_card_otp_verifier_;
  id mock_ui_delegate_;
};

// Tests CWVCreditCardOTPVerifier properties.
TEST_F(CWVCreditCardOTPVerifierTest, Properties) {
  EXPECT_FALSE([credit_card_otp_verifier_.dialogTitle isEqualToString:@""]);
  EXPECT_FALSE(
      [credit_card_otp_verifier_.textfieldPlaceholderText isEqualToString:@""]);
  EXPECT_FALSE([credit_card_otp_verifier_.okButtonLabel isEqualToString:@""]);
  EXPECT_FALSE([credit_card_otp_verifier_.newCodeLinkText isEqualToString:@""]);
}

// Tests CWVCreditCardOTPVerifier's |isOTPValid| method.
TEST_F(CWVCreditCardOTPVerifierTest, IsOTPValid) {
  EXPECT_FALSE([credit_card_otp_verifier_ isOTPValid:@"12345"]);
  EXPECT_FALSE([credit_card_otp_verifier_ isOTPValid:@"123456789"]);
  EXPECT_FALSE([credit_card_otp_verifier_ isOTPValid:@"12345a"]);
  EXPECT_FALSE([credit_card_otp_verifier_ isOTPValid:@""]);

  EXPECT_TRUE([credit_card_otp_verifier_ isOTPValid:@"123456"]);
  EXPECT_TRUE([credit_card_otp_verifier_ isOTPValid:@"123567"]);
}

// Tests CWVCreditCardOTPVerifier's verification method handles success case.
TEST_F(CWVCreditCardOTPVerifierTest, VerifyCardSucceeded) {
  NSString* otp = @"123456";
  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ verifyWithOTP:otp
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  EXPECT_NSEQ(otp, base::SysUTF16ToNSString(otp_unmask_delegate_->last_otp()));

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  // Additional HidePendingState is called inside dialogDidDismissWithSuccess
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);

  [credit_card_otp_verifier_ didReceiveUnmaskOtpVerificationResult:
                                 autofill::OtpUnmaskResult::kSuccess];

  EXPECT_TRUE(completionCalled);
  EXPECT_EQ(completionError, nil);
  [mock_ui_delegate_ verify];
}

// Tests CWVCreditCardOTPVerifier's verification method handles mismatch
// failure.
TEST_F(CWVCreditCardOTPVerifierTest, VerifyCardFailedMismatch) {
  NSString* otp = @"111111";
  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ verifyWithOTP:otp
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  NSString* expectedMessage =
      base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_ENTER_CORRECT_CODE_LABEL));
  OCMExpect([mock_ui_delegate_ creditCardOTPVerifier:credit_card_otp_verifier_
                                 displayErrorMessage:expectedMessage]);

  [credit_card_otp_verifier_ didReceiveUnmaskOtpVerificationResult:
                                 autofill::OtpUnmaskResult::kOtpMismatch];

  EXPECT_TRUE(completionCalled);
  ASSERT_NE(completionError, nil);
  EXPECT_EQ(CWVCreditCardOTPVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardOTPVerificationErrorMismatch, completionError.code);
  EXPECT_TRUE(completionError.userInfo[NSLocalizedDescriptionKey]);
  [mock_ui_delegate_ verify];
}

// Tests CWVCreditCardOTPVerifier's verification method handles expired failure.
TEST_F(CWVCreditCardOTPVerifierTest, VerifyCardFailedExpired) {
  NSString* otp = @"123456";
  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ verifyWithOTP:otp
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  NSString* expectedMessage = base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_VERIFICATION_CODE_EXPIRED_LABEL));
  OCMExpect([mock_ui_delegate_ creditCardOTPVerifier:credit_card_otp_verifier_
                                 displayErrorMessage:expectedMessage]);

  [credit_card_otp_verifier_ didReceiveUnmaskOtpVerificationResult:
                                 autofill::OtpUnmaskResult::kOtpExpired];

  EXPECT_TRUE(completionCalled);
  ASSERT_NE(completionError, nil);
  EXPECT_EQ(CWVCreditCardOTPVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardOTPVerificationErrorExpired, completionError.code);
  EXPECT_TRUE(completionError.userInfo[NSLocalizedDescriptionKey]);
  [mock_ui_delegate_ verify];
}

// Tests CWVCreditCardOTPVerifier's verification method handles permanent
// failure.
TEST_F(CWVCreditCardOTPVerifierTest, VerifyCardFailedPermanent) {
  NSString* otp = @"123456";
  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ verifyWithOTP:otp
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  // Additional HidePendingState is called inside dialogDidDismissWithSuccess
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);

  [credit_card_otp_verifier_ didReceiveUnmaskOtpVerificationResult:
                                 autofill::OtpUnmaskResult::kPermanentFailure];

  EXPECT_TRUE(completionCalled);
  ASSERT_NE(completionError, nil);
  EXPECT_EQ(CWVCreditCardOTPVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardOTPVerificationErrorPermanentFailure,
            completionError.code);
  [mock_ui_delegate_ verify];
}

// Tests that the user cancelling the dialog (via UI) fires the completion
// handler with a cancellation error.
TEST_F(CWVCreditCardOTPVerifierTest, VerifyCardCanceledByUser) {
  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ verifyWithOTP:@"123456"
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ dialogDidDismissWithSuccess:NO
                                        userClosedDialog:YES];

  EXPECT_TRUE(completionCalled);
  ASSERT_NE(completionError, nil);
  EXPECT_EQ(CWVCreditCardOTPVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardOTPVerificationErrorUserCancelled,
            completionError.code);
  [mock_ui_delegate_ verify];
}

// Tests that requesting a new OTP calls the delegate.
TEST_F(CWVCreditCardOTPVerifierTest, RequestNewOTP) {
  EXPECT_FALSE(otp_unmask_delegate_->new_otp_requested());
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ requestNewOTP];
  EXPECT_TRUE(otp_unmask_delegate_->new_otp_requested());
  [mock_ui_delegate_ verify];
}

// Tests that dealloc calls OnDialogClosed if the dialog was not already
// finished.
TEST_F(CWVCreditCardOTPVerifierTest, DeallocCallsOnDialogClosed) {
  EXPECT_FALSE(otp_unmask_delegate_->dialog_closed());

  credit_card_otp_verifier_ = nil;

  EXPECT_TRUE(otp_unmask_delegate_->dialog_closed());
  EXPECT_TRUE(otp_unmask_delegate_->user_closed_dialog());
}

// Tests that if the dialog flow finishes (e.g., success), dealloc does not
// call OnDialogClosed a second time.
TEST_F(CWVCreditCardOTPVerifierTest, SuccessfulDismissPreventsDeallocCall) {
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_
          verifyWithOTP:@"123456"
      completionHandler:^(NSError* _Nullable error){/* do nothing */}];

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  // Additional HidePendingState is called inside dialogDidDismissWithSuccess
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);

  [credit_card_otp_verifier_ didReceiveUnmaskOtpVerificationResult:
                                 autofill::OtpUnmaskResult::kSuccess];

  EXPECT_TRUE(otp_unmask_delegate_->dialog_closed());
  EXPECT_FALSE(otp_unmask_delegate_->user_closed_dialog());

  credit_card_otp_verifier_ = nil;
  [mock_ui_delegate_ verify];
}

// Tests that if the user cancels *without* ever starting a verification,
// OnDialogClosed is still called on the delegate.
TEST_F(CWVCreditCardOTPVerifierTest, UserCancelWithoutVerification) {
  EXPECT_FALSE(otp_unmask_delegate_->dialog_closed());

  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierHidePendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_ dialogDidDismissWithSuccess:NO
                                        userClosedDialog:YES];

  EXPECT_TRUE(otp_unmask_delegate_->dialog_closed());
  EXPECT_TRUE(otp_unmask_delegate_->user_closed_dialog());
  [mock_ui_delegate_ verify];
}

// Tests that a second verification call fails if one is already in progress.
TEST_F(CWVCreditCardOTPVerifierTest, VerificationInProgress) {
  OCMExpect([mock_ui_delegate_
      creditCardOTPVerifierShowPendingState:credit_card_otp_verifier_]);
  [credit_card_otp_verifier_
          verifyWithOTP:@"123456"
      completionHandler:^(NSError* _Nullable error){/* do nothing */}];

  __block BOOL completionCalled = NO;
  __block NSError* completionError = nil;
  [credit_card_otp_verifier_ verifyWithOTP:@"654321"
                         completionHandler:^(NSError* error) {
                           completionCalled = YES;
                           completionError = error;
                         }];

  EXPECT_TRUE(completionCalled);
  ASSERT_NE(completionError, nil);
  EXPECT_EQ(CWVCreditCardOTPVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardOTPVerificationErrorUnknown, completionError.code);
  EXPECT_NSEQ(@"Verification already in progress.",
              completionError.localizedDescription);
  [mock_ui_delegate_ verify];
}

}  // namespace ios_web_view

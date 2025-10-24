// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>
#import <string_view>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"
#import "components/strings/grit/components_strings.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_otp_verifier_internal.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

NSErrorDomain const CWVCreditCardOTPVerifierErrorDomain =
    @"org.chromium.chromewebview.CreditCardOTPVerifierErrorDomain";

namespace ios_web_view {

// WebView implementation of CardUnmaskOtpInputDialogView. This class acts
// as a bridge between the C++ controller
// (CardUnmaskOtpInputDialogControllerImpl) and the Objective-C
// CWVCreditCardOTPVerifier, handling view-related callbacks.
class WebViewCardUnmaskOtpInputDialogView
    : public autofill::CardUnmaskOtpInputDialogView {
 public:
  // |verifier| is the Objective-C verifier instance that this view is
  // associated with.
  explicit WebViewCardUnmaskOtpInputDialogView(
      CWVCreditCardOTPVerifier* verifier)
      : verifier_(verifier) {}

  // CardUnmaskOtpInputDialogView:
  void ShowInvalidState(const std::u16string& message) override {
    [verifier_.delegate
        creditCardOTPVerifier:verifier_
          displayErrorMessage:base::SysUTF16ToNSString(message)];
  }

  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override {
    // The dismissal is considered a success if the dialog was not closed by the
    // user and a confirmation should be shown, indicating a successful OTP
    // verification.
    BOOL success = !user_closed_dialog && show_confirmation_before_closing;
    [verifier_ dialogDidDismissWithSuccess:success
                          userClosedDialog:user_closed_dialog];
  }

  void ShowPendingState() override {
    [verifier_.delegate creditCardOTPVerifierShowPendingState:verifier_];
  }

  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // The Objective-C verifier.
  __weak CWVCreditCardOTPVerifier* verifier_;
  base::WeakPtrFactory<WebViewCardUnmaskOtpInputDialogView> weak_ptr_factory_{
      this};
};
}  // namespace ios_web_view

@implementation CWVCreditCardOTPVerifier {
  // The underlying C++ controller that manages the OTP unmasking logic.
  std::unique_ptr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      _otpController;
  // The C++ view implementation for the WebView environment.
  std::unique_ptr<ios_web_view::WebViewCardUnmaskOtpInputDialogView>
      _otpDialogView;
  // Completion handler to be called when the verification process finishes.
  void (^_Nullable _pendingCompletionHandler)(NSError* _Nullable);
  // Whether the OTP dialog has been closed.
  BOOL _dialogClosed;
}

@synthesize creditCard = _creditCard;
@synthesize delegate = _delegate;

- (instancetype)
    initWithCardType:(autofill::CreditCard::RecordType)cardType
     challengeOption:(const autofill::CardUnmaskChallengeOption&)challengeOption
      unmaskDelegate:
          (base::WeakPtr<autofill::OtpUnmaskDelegate>)unmaskDelegate {
  self = [super init];
  if (self) {
    _otpController =
        std::make_unique<autofill::CardUnmaskOtpInputDialogControllerImpl>(
            cardType, challengeOption, unmaskDelegate);
    __weak CWVCreditCardOTPVerifier* weakSelf = self;
    auto createViewCallback = base::BindOnce(
        ^base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>() {
          return [weakSelf createOtpDialogView];
        });
    _otpController->ShowDialog(std::move(createViewCallback));
  }
  return self;
}

- (void)dealloc {
  if (!_dialogClosed && _otpController) {
    _otpController->OnDialogClosed(/*user_closed_dialog=*/true,
                                   /*server_request_succeeded=*/false);
  }
}

// Creates and returns the C++ dialog view instance.
- (base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>)createOtpDialogView {
  if (_otpDialogView) {
    return _otpDialogView->GetWeakPtr();
  }
  _otpDialogView =
      std::make_unique<ios_web_view::WebViewCardUnmaskOtpInputDialogView>(self);
  return _otpDialogView->GetWeakPtr();
}

#pragma mark - Public Properties

- (NSString*)dialogTitle {
  return base::SysUTF16ToNSString(_otpController->GetWindowTitle());
}

- (NSString*)textfieldPlaceholderText {
  return base::SysUTF16ToNSString(
      _otpController->GetTextfieldPlaceholderText());
}

- (NSString*)okButtonLabel {
  return base::SysUTF16ToNSString(_otpController->GetOkButtonLabel());
}

- (NSString*)newCodeLinkText {
  return base::SysUTF16ToNSString(_otpController->GetNewCodeLinkText());
}

#pragma mark - Public Methods

- (BOOL)isOTPValid:(NSString*)OTP {
  return _otpController->IsValidOtp(base::SysNSStringToUTF16(OTP));
}

- (void)verifyWithOTP:(NSString*)OTP
    completionHandler:(void (^)(NSError* _Nullable error))completionHandler {
  if (_pendingCompletionHandler) {
    if (completionHandler) {
      completionHandler([NSError
          errorWithDomain:CWVCreditCardOTPVerifierErrorDomain
                     code:CWVCreditCardOTPVerificationErrorUnknown
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Verification already in progress."
                 }]);
    }
    return;
  }
  _pendingCompletionHandler = completionHandler;
  [self.delegate creditCardOTPVerifierShowPendingState:self];
  _otpController->OnOkButtonClicked(base::SysNSStringToUTF16(OTP));
}

- (void)requestNewOTP {
  [self.delegate creditCardOTPVerifierShowPendingState:self];
  _otpController->OnNewCodeLinkClicked();
}

#pragma mark - Internal Methods

- (void)didReceiveUnmaskOtpVerificationResult:
    (autofill::OtpUnmaskResult)result {
  if (!_otpDialogView || _dialogClosed) {
    return;
  }

  [self.delegate creditCardOTPVerifierHidePendingState:self];

  switch (result) {
    case autofill::OtpUnmaskResult::kSuccess:
      _otpDialogView->Dismiss(/*show_confirmation_before_closing=*/true,
                              /*user_closed_dialog=*/false);
      break;
    case autofill::OtpUnmaskResult::kPermanentFailure:
      _otpDialogView->Dismiss(/*show_confirmation_before_closing=*/false,
                              /*user_closed_dialog=*/false);
      break;
    case autofill::OtpUnmaskResult::kOtpExpired:
    case autofill::OtpUnmaskResult::kOtpMismatch: {
      NSString* message;
      CWVCreditCardOTPVerificationError errorCode;
      if (result == autofill::OtpUnmaskResult::kOtpMismatch) {
        message = base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
            IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_ENTER_CORRECT_CODE_LABEL));
        errorCode = CWVCreditCardOTPVerificationErrorMismatch;
      } else {  // kOtpExpired
        message = base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
            IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_VERIFICATION_CODE_EXPIRED_LABEL));
        errorCode = CWVCreditCardOTPVerificationErrorExpired;
      }
      // Call completion for the failed attempt.
      if (_pendingCompletionHandler) {
        NSError* error =
            [NSError errorWithDomain:CWVCreditCardOTPVerifierErrorDomain
                                code:errorCode
                            userInfo:@{NSLocalizedDescriptionKey : message}];
        _pendingCompletionHandler(error);
        _pendingCompletionHandler = nil;
      }
      // Tell the C++ view to show the invalid state, which triggers the
      // delegate.
      _otpDialogView->ShowInvalidState(base::SysNSStringToUTF16(message));
      break;
    }
    case autofill::OtpUnmaskResult::kUnknownType:
      NOTREACHED() << "OtpUnmaskResult should not be kUnknownType";
  }
}

- (void)dialogDidDismissWithSuccess:(BOOL)success
                   userClosedDialog:(BOOL)userClosedDialog {
  if (_dialogClosed) {
    return;
  }
  _dialogClosed = YES;

  [self.delegate creditCardOTPVerifierHidePendingState:self];

  NSError* completionError = nil;
  if (userClosedDialog) {
    completionError =
        [NSError errorWithDomain:CWVCreditCardOTPVerifierErrorDomain
                            code:CWVCreditCardOTPVerificationErrorUserCancelled
                        userInfo:nil];
  } else if (!success) {
    completionError = [NSError
        errorWithDomain:CWVCreditCardOTPVerifierErrorDomain
                   code:CWVCreditCardOTPVerificationErrorPermanentFailure
               userInfo:nil];
  }

  if (_pendingCompletionHandler) {
    _pendingCompletionHandler(completionError);
    _pendingCompletionHandler = nil;
  }

  if (_otpController) {
    bool serverRequestSucceeded = success && !userClosedDialog;
    _otpController->OnDialogClosed(userClosedDialog, serverRequestSucceeded);
  }
}

@end

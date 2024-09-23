// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#include "ui/base/resource/resource_bundle.h"

NSErrorDomain const CWVCreditCardVerifierErrorDomain =
    @"org.chromium.chromewebview.CreditCardVerifierErrorDomain";
NSErrorUserInfoKey const CWVCreditCardVerifierRetryAllowedKey =
    @"retry_allowed";

namespace {
// Converts an autofill::payments::PaymentsAutofillClient::PaymentsRpcResult to
// a CWVCreditCardVerificationError.
CWVCreditCardVerificationError CWVConvertPaymentsRPCResult(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  switch (result) {
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kNone:
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kSuccess:
    // The following two errors are not expected on iOS.
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kVcnRetrievalTryAgainFailure:
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kVcnRetrievalPermanentFailure:
      NOTREACHED_IN_MIGRATION();
      return CWVCreditCardVerificationErrorNone;
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kTryAgainFailure:
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kClientSideTimeout:
      return CWVCreditCardVerificationErrorTryAgainFailure;
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kPermanentFailure:
      return CWVCreditCardVerificationErrorPermanentFailure;
    case autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
        kNetworkError:
      return CWVCreditCardVerificationErrorNetworkFailure;
  }
}
}  // namespace

@interface CWVCreditCardVerifier ()

// Used to receive |GotVerificationResult| from WebViewCardUnmaskPromptView.
- (void)didReceiveVerificationResultWithErrorMessage:(NSString*)errorMessage
                                        retryAllowed:(BOOL)retryAllowed;

@end

namespace ios_web_view {
// Webview implementation of CardUnmaskPromptView.
class WebViewCardUnmaskPromptView : public autofill::CardUnmaskPromptView {
 public:
  explicit WebViewCardUnmaskPromptView(CWVCreditCardVerifier* verifier)
      : verifier_(verifier) {}

  // CardUnmaskPromptView:
  void Show() override {
    // No op.
  }
  void ControllerGone() override {
    // No op.
  }
  void DisableAndWaitForVerification() override {
    // No op.
  }
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override {
    NSString* ns_error_message = base::SysUTF16ToNSString(error_message);
    [verifier_ didReceiveVerificationResultWithErrorMessage:ns_error_message
                                               retryAllowed:allow_retry];
  }

 private:
  __weak CWVCreditCardVerifier* verifier_;
};
}  // namespace ios_web_view

@implementation CWVCreditCardVerifier {
  // Used to interface with |_unmaskingController|.
  std::unique_ptr<ios_web_view::WebViewCardUnmaskPromptView> _unmaskingView;
  // The main class that is wrapped by this class.
  std::unique_ptr<autofill::CardUnmaskPromptControllerImpl>
      _unmaskingController;
  // Completion handler to be called when verification completes.
  void (^_Nullable _completionHandler)(NSError* _Nullable);
  // The callback to invoke for returning risk data.
  base::OnceCallback<void(const std::string&)> _riskDataCallback;
  // Verification was attempted at least once.
  BOOL _verificationAttempted;
}

@synthesize creditCard = _creditCard;

- (instancetype)
     initWithPrefs:(PrefService*)prefs
    isOffTheRecord:(BOOL)isOffTheRecord
        creditCard:(const autofill::CreditCard&)creditCard
            reason:
                (autofill::payments::PaymentsAutofillClient::UnmaskCardReason)
                    reason
          delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)delegate {
  self = [super init];
  if (self) {
    _creditCard = [[CWVCreditCard alloc] initWithCreditCard:creditCard];
    _unmaskingController =
        std::make_unique<autofill::CardUnmaskPromptControllerImpl>(
            prefs, creditCard,
            autofill::CardUnmaskPromptOptions(std::nullopt, reason), delegate);
    __weak CWVCreditCardVerifier* weakSelf = self;
    _unmaskingController->ShowPrompt(
        base::BindOnce(^autofill::CardUnmaskPromptView*() {
          return [weakSelf createUnmaskingView];
        }));
  }
  return self;
}

// Factory function to CardUnmaskPromptController::ShowPrompt. This should
// return std:unique_ptr<autofill::CardUnmaskPromptView>> but there are tests
// which don't do the ownership correctly, so ownership is retained in the
// CWVCreditCardVerifier instance.
- (autofill::CardUnmaskPromptView*)createUnmaskingView {
  DCHECK(!_unmaskingView);
  _unmaskingView =
      std::make_unique<ios_web_view::WebViewCardUnmaskPromptView>(self);
  return _unmaskingView.get();
}

- (void)dealloc {
  // autofill::CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed, despite its
  // name, should only be called if the user does not attempt any verification
  // at all.
  if (!_verificationAttempted) {
    _unmaskingController->OnUnmaskDialogClosed();
  }
}

#pragma mark - Public Methods

- (NSString*)navigationTitle {
  return base::SysUTF16ToNSString(_unmaskingController->GetNavigationTitle());
}

- (NSString*)instructionMessage {
  return base::SysUTF16ToNSString(
      _unmaskingController->GetInstructionsMessage());
}

- (NSString*)confirmButtonLabel {
  return base::SysUTF16ToNSString(_unmaskingController->GetOkButtonLabel());
}

- (UIImage*)CVCHintImage {
  int resourceID = _unmaskingController->GetCvcImageRid();
  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(resourceID)
      .ToUIImage();
}

- (NSInteger)expectedCVCLength {
  return _unmaskingController->GetExpectedCvcLength();
}

- (BOOL)shouldRequestUpdateForExpirationDate {
  return _unmaskingController->ShouldRequestExpirationDate();
}

- (void)verifyWithCVC:(NSString*)CVC
      expirationMonth:(nullable NSString*)expirationMonth
       expirationYear:(nullable NSString*)expirationYear
             riskData:(NSString*)riskData
    completionHandler:(void (^)(NSError* _Nullable error))completionHandler {
  _verificationAttempted = YES;
  _completionHandler = completionHandler;

  // It is possible for |_riskDataCallback| to be null when a failed
  // verification attempt is retried.
  if (_riskDataCallback) {
    std::move(_riskDataCallback).Run(base::SysNSStringToUTF8(riskData));
  }

  _unmaskingController->OnUnmaskPromptAccepted(
      base::SysNSStringToUTF16(CVC), base::SysNSStringToUTF16(expirationMonth),
      base::SysNSStringToUTF16(expirationYear), /*enable_fido_auth=*/false,
      /*was_checkbox_visible=*/false);
}

- (BOOL)isCVCValid:(NSString*)CVC {
  return _unmaskingController->InputCvcIsValid(base::SysNSStringToUTF16(CVC));
}

- (BOOL)isExpirationDateValidForMonth:(NSString*)month year:(NSString*)year {
  return _unmaskingController->InputExpirationIsValid(
      base::SysNSStringToUTF16(month), base::SysNSStringToUTF16(year));
}

- (void)requestUpdateForExpirationDate {
  _unmaskingController->NewCardLinkClicked();
}

#pragma mark - Private Methods

- (void)didReceiveVerificationResultWithErrorMessage:(NSString*)errorMessage
                                        retryAllowed:(BOOL)retryAllowed {
  if (_completionHandler) {
    NSError* error;
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result =
        _unmaskingController->GetVerificationResult();
    if (errorMessage.length > 0 &&
        result != autofill::payments::PaymentsAutofillClient::
                      PaymentsRpcResult::kNone &&
        result != autofill::payments::PaymentsAutofillClient::
                      PaymentsRpcResult::kSuccess) {
      NSDictionary* userInfo = @{
        NSLocalizedDescriptionKey : errorMessage,
        CWVCreditCardVerifierRetryAllowedKey : @(retryAllowed),
      };
      error = [NSError errorWithDomain:CWVCreditCardVerifierErrorDomain
                                  code:CWVConvertPaymentsRPCResult(result)
                              userInfo:userInfo];
    }
    _completionHandler(error);
    _completionHandler = nil;
  }
}

#pragma mark - Internal Methods

- (void)didReceiveUnmaskVerificationResult:
    (autofill::payments::PaymentsAutofillClient::PaymentsRpcResult)result {
  _unmaskingController->OnVerificationResult(result);
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  _riskDataCallback = std::move(callback);
}

@end

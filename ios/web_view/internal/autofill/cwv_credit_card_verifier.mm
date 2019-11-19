// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSErrorDomain const CWVCreditCardVerifierErrorDomain =
    @"org.chromium.chromewebview.CreditCardVerifierErrorDomain";
NSErrorUserInfoKey const CWVCreditCardVerifierRetryAllowedKey =
    @"retry_allowed";

namespace {
// Converts an autofill::AutofillClient::PaymentsRpcResult to a
// CWVCreditCardVerificationError.
CWVCreditCardVerificationError CWVConvertPaymentsRPCResult(
    autofill::AutofillClient::PaymentsRpcResult result) {
  switch (result) {
    case autofill::AutofillClient::NONE:
    case autofill::AutofillClient::SUCCESS:
      NOTREACHED();
      return CWVCreditCardVerificationErrorNone;
    case autofill::AutofillClient::TRY_AGAIN_FAILURE:
      return CWVCreditCardVerificationErrorTryAgainFailure;
    case autofill::AutofillClient::PERMANENT_FAILURE:
      return CWVCreditCardVerificationErrorPermanentFailure;
    case autofill::AutofillClient::NETWORK_ERROR:
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
  void GotVerificationResult(const base::string16& error_message,
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

- (instancetype)initWithPrefs:(PrefService*)prefs
               isOffTheRecord:(BOOL)isOffTheRecord
                   creditCard:(const autofill::CreditCard&)creditCard
                       reason:(autofill::AutofillClient::UnmaskCardReason)reason
                     delegate:
                         (base::WeakPtr<autofill::CardUnmaskDelegate>)delegate {
  self = [super init];
  if (self) {
    _creditCard = [[CWVCreditCard alloc] initWithCreditCard:creditCard];
    _unmaskingView =
        std::make_unique<ios_web_view::WebViewCardUnmaskPromptView>(self);
    _unmaskingController =
        std::make_unique<autofill::CardUnmaskPromptControllerImpl>(
            prefs, isOffTheRecord);
    _unmaskingController->ShowPrompt(
        base::BindOnce(^autofill::CardUnmaskPromptView*() {
          return _unmaskingView.get();
        }),
        creditCard, reason, delegate);
  }
  return self;
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

- (BOOL)canStoreLocally {
  return _unmaskingController->CanStoreLocally();
}

- (BOOL)lastStoreLocallyValue {
  return _unmaskingController->GetStoreLocallyStartState();
}

- (NSString*)navigationTitle {
  return base::SysUTF16ToNSString(_unmaskingController->GetWindowTitle());
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
         storeLocally:(BOOL)storeLocally
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
      base::SysNSStringToUTF16(expirationYear), storeLocally,
      /*enable_fido_auth=*/false);
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
    autofill::AutofillClient::PaymentsRpcResult result =
        _unmaskingController->GetVerificationResult();
    if (errorMessage.length > 0 && result != autofill::AutofillClient::NONE &&
        result != autofill::AutofillClient::SUCCESS) {
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
    (autofill::AutofillClient::PaymentsRpcResult)result {
  _unmaskingController->OnVerificationResult(result);
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  _riskDataCallback = std::move(callback);
}

@end

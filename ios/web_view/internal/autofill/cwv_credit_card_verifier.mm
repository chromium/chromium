// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/public/cwv_credit_card_verifier_data_source.h"
#import "ios/web_view/public/cwv_credit_card_verifier_delegate.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSErrorDomain const CWVCreditCardVerifierErrorDomain =
    @"org.chromium.chromewebview.CreditCardVerifierErrorDomain";
NSString* const CWVCreditCardVerifierErrorMessageKey = @"error_message";
NSString* const CWVCreditCardVerifierRetryAllowedKey = @"retry_allowed";

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
  // The main class that is wrapped by this class.
  std::unique_ptr<autofill::CardUnmaskPromptControllerImpl>
      _unmaskingController;
  // Used to interface with |_unmaskingController|.
  std::unique_ptr<ios_web_view::WebViewCardUnmaskPromptView> _unmaskingView;
  // Data source to provide risk data.
  __weak id<CWVCreditCardVerifierDataSource> _dataSource;
  // Delegate to receive callbacks.
  __weak id<CWVCreditCardVerifierDelegate> _delegate;
  // The callback to invoke for returning risk data.
  base::OnceCallback<void(const std::string&)> _riskDataCallback;
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
    _unmaskingController->ShowPrompt(_unmaskingView.get(), creditCard, reason,
                                     delegate);
  }
  return self;
}

- (void)dealloc {
  _unmaskingController->OnUnmaskDialogClosed();
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

- (BOOL)needsUpdateForExpirationDate {
  return _unmaskingController->ShouldRequestExpirationDate();
}

- (void)verifyWithCVC:(NSString*)CVC
      expirationMonth:(nullable NSString*)expirationMonth
       expirationYear:(nullable NSString*)expirationYear
         storeLocally:(BOOL)storeLocally
           dataSource:(__weak id<CWVCreditCardVerifierDataSource>)dataSource
             delegate:
                 (nullable __weak id<CWVCreditCardVerifierDelegate>)delegate {
  _dataSource = dataSource;
  _delegate = delegate;

  // It is possible for |_riskDataCallback| to be null when a failed
  // verification attempt is retried.
  if (!_riskDataCallback.is_null()) {
    [_dataSource creditCardVerifier:self
        getRiskDataWithCompletionHandler:^(NSString* _Nonnull riskData) {
          std::move(_riskDataCallback).Run(base::SysNSStringToUTF8(riskData));
        }];
  }

  _unmaskingController->OnUnmaskResponse(
      base::SysNSStringToUTF16(CVC), base::SysNSStringToUTF16(expirationMonth),
      base::SysNSStringToUTF16(expirationYear), storeLocally);
}

- (BOOL)isCVCValid:(NSString*)CVC {
  return _unmaskingController->InputCvcIsValid(base::SysNSStringToUTF16(CVC));
}

- (BOOL)isExpirationDateValidForMonth:(NSString*)month year:(NSString*)year {
  return _unmaskingController->InputExpirationIsValid(
      base::SysNSStringToUTF16(month), base::SysNSStringToUTF16(year));
}

#pragma mark - Private Methods

- (void)didReceiveVerificationResultWithErrorMessage:(NSString*)errorMessage
                                        retryAllowed:(BOOL)retryAllowed {
  if ([_delegate respondsToSelector:@selector
                 (creditCardVerifier:didFinishVerificationWithError:)]) {
    NSError* error;
    autofill::AutofillClient::PaymentsRpcResult result =
        _unmaskingController->GetVerificationResult();
    if (errorMessage.length > 0 && result != autofill::AutofillClient::NONE &&
        result != autofill::AutofillClient::SUCCESS) {
      NSDictionary* userInfo = @{
        CWVCreditCardVerifierErrorMessageKey : errorMessage,
        CWVCreditCardVerifierRetryAllowedKey : @(retryAllowed),
      };
      error = [NSError errorWithDomain:CWVCreditCardVerifierErrorDomain
                                  code:CWVConvertPaymentsRPCResult(result)
                              userInfo:userInfo];
    }

    [_delegate creditCardVerifier:self didFinishVerificationWithError:error];
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

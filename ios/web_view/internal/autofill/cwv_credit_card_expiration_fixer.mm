// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_expiration_fixer_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Empty implementation of autofill::CardExpirationDateFixFlowView.
class WebViewCardExpirationFixFlowView
    : public autofill::CardExpirationDateFixFlowView {
 public:
  void Show() override {
    // No op.
  }
  void ControllerGone() override {
    // No op.
  }
};

}  // namespace ios_web_view

@implementation CWVCreditCardExpirationFixer {
  std::unique_ptr<ios_web_view::WebViewCardExpirationFixFlowView> _view;
  std::unique_ptr<autofill::CardExpirationDateFixFlowControllerImpl>
      _controller;
}

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                          callback:
                              (base::OnceCallback<void(const std::u16string&,
                                                       const std::u16string&)>)
                                  callback {
  self = [super init];
  if (self) {
    _card = [[CWVCreditCard alloc] initWithCreditCard:creditCard];
    _view = std::make_unique<ios_web_view::WebViewCardExpirationFixFlowView>();
    _controller =
        std::make_unique<autofill::CardExpirationDateFixFlowControllerImpl>();
    _controller->Show(_view.get(), creditCard, std::move(callback));
  }
  return self;
}

- (void)dealloc {
  _controller->OnDialogClosed();
}

#pragma mark - Public

- (NSString*)titleText {
  return base::SysUTF16ToNSString(_controller->GetTitleText());
}

- (NSString*)saveButtonLabel {
  return base::SysUTF16ToNSString(_controller->GetSaveButtonLabel());
}

- (NSString*)cardLabel {
  return base::SysUTF16ToNSString(_controller->GetCardLabel());
}

- (NSString*)cancelButtonLabel {
  return base::SysUTF16ToNSString(_controller->GetCancelButtonLabel());
}

- (NSString*)inputLabel {
  return base::SysUTF16ToNSString(_controller->GetInputLabel());
}

- (NSString*)dateSeparator {
  return base::SysUTF16ToNSString(_controller->GetDateSeparator());
}

- (NSString*)invalidDateErrorMessage {
  return base::SysUTF16ToNSString(_controller->GetInvalidDateError());
}

- (BOOL)acceptWithMonth:(NSString*)month year:(NSString*)year {
  BOOL isValidDate = autofill::IsValidCreditCardExpirationDate(
      year.intValue, month.intValue, autofill::AutofillClock::Now());
  if (isValidDate) {
    _controller->OnAccepted(base::SysNSStringToUTF16(month),
                            base::SysNSStringToUTF16(year));
  }
  return isValidDate;
}

- (void)cancel {
  _controller->OnDismissed();
}

@end

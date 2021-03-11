// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_name_fixer_internal.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Empty implementation of |autofill::CardNameFixFlowView|.
class WebViewCardNameFixFlowView : public autofill::CardNameFixFlowView {
 public:
  void Show() override {
    // No op.
  }
  void ControllerGone() override {
    // No op.
  }
};

}  // namespace ios_web_view

@implementation CWVCreditCardNameFixer {
  std::unique_ptr<autofill::CardNameFixFlowControllerImpl> _controller;
  std::unique_ptr<ios_web_view::WebViewCardNameFixFlowView> _view;
}

- (instancetype)initWithName:(NSString*)name
                    callback:(base::OnceCallback<void(const std::u16string&)>)
                                 callback {
  self = [super init];
  if (self) {
    _view = std::make_unique<ios_web_view::WebViewCardNameFixFlowView>();
    _controller = std::make_unique<autofill::CardNameFixFlowControllerImpl>();
    _controller->Show(_view.get(), base::SysNSStringToUTF16(name),
                      std::move(callback));
  }
  return self;
}

- (void)dealloc {
  _controller->OnConfirmNameDialogClosed();
}

#pragma mark - Public

- (NSString*)inferredCardHolderName {
  return base::SysUTF16ToNSString(_controller->GetInferredCardholderName());
}

- (NSString*)cancelButtonLabel {
  return base::SysUTF16ToNSString(_controller->GetCancelButtonLabel());
}

- (NSString*)inferredNameTooltipText {
  return base::SysUTF16ToNSString(_controller->GetInferredNameTooltipText());
}

- (NSString*)inputLabel {
  return base::SysUTF16ToNSString(_controller->GetInputLabel());
}

- (NSString*)inputPlaceholderText {
  return base::SysUTF16ToNSString(_controller->GetInputLabel());
}

- (NSString*)saveButtonLabel {
  return base::SysUTF16ToNSString(_controller->GetSaveButtonLabel());
}

- (NSString*)titleText {
  return base::SysUTF16ToNSString(_controller->GetTitleText());
}

- (void)acceptWithName:(NSString*)name {
  _controller->OnNameAccepted(base::SysNSStringToUTF16(name));
}

- (void)cancel {
  _controller->OnDismissed();
}

@end

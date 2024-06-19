// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_mediator.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_mediator_delegate.h"

AutofillErrorDialogMediator::AutofillErrorDialogMediator(
    base::WeakPtr<autofill::AutofillErrorDialogController> model_controller,
    id<AutofillErrorDialogMediatorDelegate> delegate)
    : model_controller_(model_controller), delegate_(delegate) {
  CHECK(model_controller_);
  CHECK(delegate_);
}

AutofillErrorDialogMediator::~AutofillErrorDialogMediator() {
  if (model_controller_) {
    model_controller_->OnDismissed();
  }
}

void AutofillErrorDialogMediator::Dismiss() {}

base::WeakPtr<autofill::AutofillErrorDialogView>
AutofillErrorDialogMediator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<autofill::AutofillErrorDialogView>
AutofillErrorDialogMediator::Show() {
  [delegate_
      showErrorDialog:base::SysUTF16ToNSString(model_controller_->GetTitle())
              message:base::SysUTF16ToNSString(
                          model_controller_->GetDescription())
          buttonLabel:base::SysUTF16ToNSString(
                          model_controller_->GetButtonLabel())];
  return GetWeakPtr();
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_mediator.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_consumer.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_content.h"

OtpInputDialogMediator::OtpInputDialogMediator(
    base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
        model_controller)
    : model_controller_(model_controller) {}

OtpInputDialogMediator::~OtpInputDialogMediator() {
  // If the closure is not initiated from the backend side (via Dismiss()), it
  // means the dialog/tab/browser is closed by the user. Notify
  // `model_controller` for logging.
  if (model_controller_) {
    model_controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                      /*server_request_succeeded=*/false);
  }
}

void OtpInputDialogMediator::ShowPendingState() {
  [consumer_ showPendingState];
}

void OtpInputDialogMediator::ShowInvalidState(
    const std::u16string& invalid_label_text) {
  [consumer_ showInvalidState:base::SysUTF16ToNSString(invalid_label_text)];
}

void OtpInputDialogMediator::Dismiss(bool show_confirmation_before_closing,
                                     bool user_closed_dialog) {
  if (model_controller_) {
    model_controller_->OnDialogClosed(user_closed_dialog,
                                      show_confirmation_before_closing);
    model_controller_ = nullptr;
  }
  // TODO(b/324611600): Invoked MediatorDelegate to close the view and terminate
  // everything.
}

base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>
OtpInputDialogMediator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OtpInputDialogMediator::SetConsumer(id<OtpInputDialogConsumer> consumer) {
  consumer_ = consumer;
  if (!model_controller_) {
    return;
  }
  OtpInputDialogContent* content = [[OtpInputDialogContent alloc] init];
  content.windowTitle =
      base::SysUTF16ToNSString(model_controller_->GetWindowTitle());
  content.textFieldPlaceholder = base::SysUTF16ToNSString(
      model_controller_->GetTextfieldPlaceholderText());
  content.confirmButtonLabel =
      base::SysUTF16ToNSString(model_controller_->GetOkButtonLabel());
  [consumer_ setContent:content];
}

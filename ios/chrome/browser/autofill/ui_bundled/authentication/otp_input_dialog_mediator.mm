// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_content.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator_bridge_target.h"

OtpInputDialogMediator::OtpInputDialogMediator(
    base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
        model_controller,
    id<OtpInputDialogMediatorDelegate> delegate)
    : model_controller_(model_controller), delegate_(delegate) {
  base::WeakPtr<OtpInputDialogMutatorBridgeTarget>
      mutator_bridge_target_weak_ptr(weak_ptr_factory_.GetWeakPtr());
  mutator_bridge_ = [[OtpInputDialogMutatorBridge alloc]
      initWithTarget:mutator_bridge_target_weak_ptr];
}

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
  [delegate_ dismissDialog];
}

base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>
OtpInputDialogMediator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OtpInputDialogMediator::DidTapConfirmButton(
    const std::u16string& input_value) {
  if (model_controller_) {
    model_controller_->OnOkButtonClicked(input_value);
    [consumer_ showPendingState];
  }
}

void OtpInputDialogMediator::DidTapCancelButton() {
  [delegate_ dismissDialog];
}

void OtpInputDialogMediator::OnOtpInputChanges(
    const std::u16string& input_value) {
  if (model_controller_) {
    [consumer_
        setConfirmButtonEnabled:model_controller_->IsValidOtp(input_value)];
  }
}

void OtpInputDialogMediator::DidTapNewCodeLink() {
  if (model_controller_) {
    model_controller_->OnNewCodeLinkClicked();
  }
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

id<OtpInputDialogMutator> OtpInputDialogMediator::AsMutator() {
  return mutator_bridge_;
}

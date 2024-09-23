// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator_delegate.h"

AutofillProgressDialogMediator::AutofillProgressDialogMediator(
    base::WeakPtr<autofill::AutofillProgressDialogControllerImpl>
        model_controller,
    id<AutofillProgressDialogMediatorDelegate> delegate)
    : model_controller_(model_controller), delegate_(delegate) {}

AutofillProgressDialogMediator::~AutofillProgressDialogMediator() {
  // If the closure is not initiated from the backend side (via Dismiss()), it
  // means the dialog/tab/browser is closed by the user. Notify
  // `model_controller` for logging.
  if (model_controller_) {
    model_controller_->OnDismissed(is_canceled_by_user_);
  }
}

void AutofillProgressDialogMediator::Dismiss(
    bool show_confirmation_before_closing,
    bool is_canceled_by_user) {
  // TODO(crbug.com/324603292): Check whether we need the confirmation on iOS.
  is_canceled_by_user_ = is_canceled_by_user;
  [delegate_ dismissDialog];
}

void AutofillProgressDialogMediator::InvalidateControllerForCallbacks() {
  model_controller_ = nullptr;
}

base::WeakPtr<autofill::AutofillProgressDialogView>
AutofillProgressDialogMediator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillProgressDialogMediator::SetConsumer(id<AlertConsumer> consumer) {
  consumer_ = consumer;
  [consumer_
      setTitle:base::SysUTF16ToNSString(model_controller_->GetLoadingTitle())];
  [consumer_ setMessage:base::SysUTF16ToNSString(
                            model_controller_->GetLoadingMessage())];
  base::WeakPtr<AutofillProgressDialogMediator> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  AlertAction* buttonAction = [AlertAction
      actionWithTitle:base::SysUTF16ToNSString(
                          model_controller_->GetCancelButtonLabel())
                style:UIAlertActionStyleCancel
              handler:^(AlertAction* action) {
                if (weak_ptr) {
                  // When the handler is invoked, `this` will for sure exist.
                  weak_ptr->OnCancelButtonTapped();
                }
              }];
  [consumer_ setActions:@[ @[ buttonAction ] ]];
  [consumer_ setShouldShowActivityIndicator:YES];
}

void AutofillProgressDialogMediator::OnCancelButtonTapped() {
  is_canceled_by_user_ = true;
  [delegate_ dismissDialog];
}

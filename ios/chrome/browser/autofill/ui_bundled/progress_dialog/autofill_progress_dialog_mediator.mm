// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ui/base/l10n/l10n_util.h"

AutofillProgressDialogMediator::AutofillProgressDialogMediator(
    autofill::AutofillProgressDialogController* model_controller,
    id<AutofillProgressDialogMediatorDelegate> delegate)
    : delegate_(delegate) {
  if (model_controller) {
    model_controller_ = model_controller->GetWeakPtr();
  }
}

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
  is_canceled_by_user_ = is_canceled_by_user;
  if (!show_confirmation_before_closing) {
    DismissDialog();
    return;
  }

  [consumer_ setProgressState:ProgressIndicatorStateSuccess];

  NSString* cancelTitle =
      base::SysUTF16ToNSString(model_controller_->GetCancelButtonLabel());
  AlertAction* disabledCancelAction =
      [AlertAction actionWithTitle:cancelTitle
                             style:UIAlertActionStyleCancel
                           handler:nil];
  disabledCancelAction.enabled = NO;
  [consumer_ setActions:@[ @[ disabledCancelAction ] ]];

  consumer_.confirmationAccessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_PROGRESS_DIALOG_CONFIRMATION_ACCESSIBILITY_ANNOUNCEMENT);

  base::OnceClosure closure =
      base::BindOnce(&AutofillProgressDialogMediator::DismissDialog,
                     weak_ptr_factory_.GetWeakPtr());

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(closure),
      autofill_ui_constants::kProgressDialogConfirmationDismissDelay);
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

  base::OnceCallback<void(AlertAction*)> handler =
      base::IgnoreArgs<AlertAction*>(
          base::BindOnce(&AutofillProgressDialogMediator::OnCancelButtonTapped,
                         weak_ptr_factory_.GetWeakPtr()));

  AlertAction* buttonAction = [AlertAction
      actionWithTitle:base::SysUTF16ToNSString(
                          model_controller_->GetCancelButtonLabel())
                style:UIAlertActionStyleCancel
              handler:base::CallbackToBlock(std::move(handler))];
  [consumer_ setActions:@[ @[ buttonAction ] ]];
  [consumer_ setShouldShowActivityIndicator:YES];
}

void AutofillProgressDialogMediator::OnCancelButtonTapped() {
  is_canceled_by_user_ = true;
  DismissDialog();
}

void AutofillProgressDialogMediator::DismissDialog() {
  [delegate_ dismissDialog];
}

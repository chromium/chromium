// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/web/public/test/fakes/fake_java_script_dialog_presenter.h"

namespace web {

FakeJavaScriptAlertDialog::FakeJavaScriptAlertDialog() = default;

FakeJavaScriptAlertDialog::~FakeJavaScriptAlertDialog() = default;

FakeJavaScriptConfirmDialog::FakeJavaScriptConfirmDialog() = default;

FakeJavaScriptConfirmDialog::~FakeJavaScriptConfirmDialog() = default;

FakeJavaScriptPromptDialog::FakeJavaScriptPromptDialog() = default;

FakeJavaScriptPromptDialog::~FakeJavaScriptPromptDialog() = default;

FakeJavaScriptDialogPresenter::FakeJavaScriptDialogPresenter() = default;

FakeJavaScriptDialogPresenter::~FakeJavaScriptDialogPresenter() {
  // Unpause callback execution so that all callbacks are executed before
  // deallocation.
  set_callback_execution_paused(false);
}

void FakeJavaScriptDialogPresenter::RunJavaScriptAlertDialog(
    WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceClosure callback) {
  auto dialog = std::make_unique<FakeJavaScriptAlertDialog>();
  dialog->web_state = web_state;
  dialog->origin_url = origin_url;
  dialog->message_text = [message_text copy];
  dialog->callback = std::move(callback);

  requested_alert_dialogs_.push_back(std::move(dialog));

  if (!callback_execution_paused()) {
    ExecuteAlertDialogCallback(requested_alert_dialogs_.back().get());
  }
}

void FakeJavaScriptDialogPresenter::RunJavaScriptConfirmDialog(
    WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceCallback<void(bool success)> callback) {
  auto dialog = std::make_unique<FakeJavaScriptConfirmDialog>();
  dialog->web_state = web_state;
  dialog->origin_url = origin_url;
  dialog->message_text = [message_text copy];
  dialog->callback = std::move(callback);

  requested_confirm_dialogs_.push_back(std::move(dialog));

  if (!callback_execution_paused()) {
    ExecuteConfirmDialogCallback(requested_confirm_dialogs_.back().get());
  }
}
void FakeJavaScriptDialogPresenter::RunJavaScriptPromptDialog(
    WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    base::OnceCallback<void(NSString* user_input)> callback) {
  auto dialog = std::make_unique<FakeJavaScriptPromptDialog>();
  dialog->web_state = web_state;
  dialog->origin_url = origin_url;
  dialog->message_text = [message_text copy];
  dialog->default_prompt_text = [default_prompt_text copy];
  dialog->callback = std::move(callback);

  requested_prompt_dialogs_.push_back(std::move(dialog));

  if (!callback_execution_paused())
    ExecutePromptDialogCallback(requested_prompt_dialogs_.back().get());
}

void FakeJavaScriptDialogPresenter::CancelDialogs(WebState* web_state) {
  cancel_dialogs_called_ = true;
}

void FakeJavaScriptDialogPresenter::ExecuteAllDialogCallbacks() {
  DCHECK(!callback_execution_paused());
  for (std::unique_ptr<FakeJavaScriptAlertDialog>& dialog :
       requested_alert_dialogs_) {
    ExecuteAlertDialogCallback(dialog.get());
  }
  for (std::unique_ptr<FakeJavaScriptConfirmDialog>& dialog :
       requested_confirm_dialogs_) {
    ExecuteConfirmDialogCallback(dialog.get());
  }
  for (std::unique_ptr<FakeJavaScriptPromptDialog>& dialog :
       requested_prompt_dialogs_) {
    ExecutePromptDialogCallback(dialog.get());
  }
}

void FakeJavaScriptDialogPresenter::ExecuteAlertDialogCallback(
    FakeJavaScriptAlertDialog* dialog) {
  base::OnceClosure& callback = dialog->callback;
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void FakeJavaScriptDialogPresenter::ExecuteConfirmDialogCallback(
    FakeJavaScriptConfirmDialog* dialog) {
  base::OnceCallback<void(BOOL success)>& callback = dialog->callback;
  if (!callback.is_null()) {
    std::move(callback).Run(callback_success_argument_);
  }
}

void FakeJavaScriptDialogPresenter::ExecutePromptDialogCallback(
    FakeJavaScriptPromptDialog* dialog) {
  base::OnceCallback<void(NSString * user_input)>& callback = dialog->callback;
  if (!callback.is_null()) {
    std::move(callback).Run(callback_user_input_argument_);
  }
}

}  // namespace web

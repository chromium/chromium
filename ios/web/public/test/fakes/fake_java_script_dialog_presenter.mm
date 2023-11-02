// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_java_script_dialog_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeJavaScriptDialog::FakeJavaScriptDialog() = default;

FakeJavaScriptDialog::~FakeJavaScriptDialog() = default;

FakeJavaScriptDialogPresenter::FakeJavaScriptDialogPresenter() = default;

FakeJavaScriptDialogPresenter::~FakeJavaScriptDialogPresenter() {
  // Unpause callback execution so that all callbacks are executed before
  // deallocation.
  set_callback_execution_paused(false);
}

void FakeJavaScriptDialogPresenter::RunJavaScriptDialog(
    WebState* web_state,
    const GURL& origin_url,
    JavaScriptDialogType java_script_dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    DialogClosedCallback callback) {
  auto dialog = std::make_unique<FakeJavaScriptDialog>();
  dialog->web_state = web_state;
  dialog->origin_url = origin_url;
  dialog->java_script_dialog_type = java_script_dialog_type;
  dialog->message_text = [message_text copy];
  dialog->default_prompt_text = [default_prompt_text copy];
  dialog->callback = std::move(callback);

  requested_dialogs_.push_back(std::move(dialog));

  if (!callback_execution_paused())
    ExecuteDialogCallback(requested_dialogs_.back().get());
}

void FakeJavaScriptDialogPresenter::CancelDialogs(WebState* web_state) {
  cancel_dialogs_called_ = true;
}

void FakeJavaScriptDialogPresenter::ExecuteAllDialogCallbacks() {
  DCHECK(!callback_execution_paused());
  for (std::unique_ptr<FakeJavaScriptDialog>& dialog : requested_dialogs_) {
    ExecuteDialogCallback(dialog.get());
  }
}

void FakeJavaScriptDialogPresenter::ExecuteDialogCallback(
    FakeJavaScriptDialog* dialog) {
  DialogClosedCallback& callback = dialog->callback;
  if (!callback.is_null()) {
    std::move(callback).Run(callback_success_argument_,
                            callback_user_input_argument_);
  }
}

}  // namespace web

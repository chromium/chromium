// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import "ios/web/public/ui/java_script_dialog_presenter.h"

#include <memory>
#include <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"

namespace web {

struct FakeJavaScriptAlertDialog {
  FakeJavaScriptAlertDialog();
  ~FakeJavaScriptAlertDialog();
  raw_ptr<WebState> web_state = nullptr;
  GURL origin_url;
  NSString* message_text;
  base::OnceClosure callback;
};

struct FakeJavaScriptConfirmDialog {
  FakeJavaScriptConfirmDialog();
  ~FakeJavaScriptConfirmDialog();
  raw_ptr<WebState> web_state = nullptr;
  GURL origin_url;
  NSString* message_text;
  base::OnceCallback<void(bool success)> callback;
};

struct FakeJavaScriptPromptDialog {
  FakeJavaScriptPromptDialog();
  ~FakeJavaScriptPromptDialog();
  raw_ptr<WebState> web_state = nullptr;
  GURL origin_url;
  NSString* message_text;
  NSString* default_prompt_text;
  base::OnceCallback<void(NSString* user_input)> callback;
};

// Fake presenter to check that the JavaScriptDialogPresenter methods are called
// as expected. `RunJavaScriptDialog` always calls `callback` with
// `callback_success_argument` and `callback_user_input_argument` values.
class FakeJavaScriptDialogPresenter : public JavaScriptDialogPresenter {
 public:
  FakeJavaScriptDialogPresenter();
  ~FakeJavaScriptDialogPresenter() override;

  // JavaScriptDialogPresenter overrides:
  void RunJavaScriptAlertDialog(WebState* web_state,
                                const GURL& origin_url,
                                NSString* message_text,
                                base::OnceClosure callback) override;
  void RunJavaScriptConfirmDialog(
      WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback) override;
  void RunJavaScriptPromptDialog(
      WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback) override;
  void CancelDialogs(WebState* web_state) override;

  // Whether callback execution is paused.  If a dialog presentation is
  // requested while true, the callback will not be executed until unpaused.
  bool callback_execution_paused() const { return callback_execution_paused_; }
  void set_callback_execution_paused(bool callback_execution_paused) {
    if (callback_execution_paused_ == callback_execution_paused)
      return;
    callback_execution_paused_ = callback_execution_paused;
    if (!callback_execution_paused_)
      ExecuteAllDialogCallbacks();
  }

  // True if the JavaScriptDialogPresenter CancelDialogs method has been called.
  bool cancel_dialogs_called() const { return cancel_dialogs_called_; }

  // Returns a vector of requested dialogs.
  const std::vector<std::unique_ptr<FakeJavaScriptAlertDialog>>&
  requested_alert_dialogs() const {
    return requested_alert_dialogs_;
  }
  const std::vector<std::unique_ptr<FakeJavaScriptConfirmDialog>>&
  requested_confirm_dialogs() const {
    return requested_confirm_dialogs_;
  }
  const std::vector<std::unique_ptr<FakeJavaScriptPromptDialog>>&
  requested_prompt_dialogs() const {
    return requested_prompt_dialogs_;
  }

  // Sets `success` argument to be used for RunJavaScriptDialog callback.
  void set_callback_success_argument(bool success) {
    callback_success_argument_ = success;
  }

  // Sets `user_input` argument to be used for RunJavaScriptDialog callback.
  void set_callback_user_input_argument(NSString* user_input) {
    callback_user_input_argument_ = user_input;
  }

 private:
  // Executes all non-executed callbacks in `requested_dialogs_`.
  void ExecuteAllDialogCallbacks();
  // Executes the callback for `dialog`.
  void ExecuteAlertDialogCallback(FakeJavaScriptAlertDialog* dialog);
  void ExecuteConfirmDialogCallback(FakeJavaScriptConfirmDialog* dialog);
  void ExecutePromptDialogCallback(FakeJavaScriptPromptDialog* dialog);

  bool callback_execution_paused_ = false;
  bool cancel_dialogs_called_ = false;
  std::vector<std::unique_ptr<FakeJavaScriptAlertDialog>>
      requested_alert_dialogs_;
  std::vector<std::unique_ptr<FakeJavaScriptConfirmDialog>>
      requested_confirm_dialogs_;
  std::vector<std::unique_ptr<FakeJavaScriptPromptDialog>>
      requested_prompt_dialogs_;
  bool callback_success_argument_ = false;
  NSString* callback_user_input_argument_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_JAVA_SCRIPT_DIALOG_PRESENTER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#include "ios/web/public/ui/java_script_dialog_presenter.h"

// Implementation of JavaScriptDialogPresenter that uses OverlayPresenter to run
// JavaScript dialogs.
class OverlayJavaScriptDialogPresenter final
    : public web::JavaScriptDialogPresenter {
 public:
  OverlayJavaScriptDialogPresenter();
  OverlayJavaScriptDialogPresenter(OverlayJavaScriptDialogPresenter&& other);
  OverlayJavaScriptDialogPresenter& operator=(
      OverlayJavaScriptDialogPresenter&& other);
  ~OverlayJavaScriptDialogPresenter() override;

  // web::JavaScriptDialogPresenter:
  void RunJavaScriptAlertDialog(web::WebState* web_state,
                                const GURL& origin_url,
                                NSString* message_text,
                                base::OnceClosure callback) override;
  void RunJavaScriptConfirmDialog(
      web::WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback) override;
  void RunJavaScriptPromptDialog(
      web::WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback) override;
  void CancelDialogs(web::WebState* web_state) override;
};

#endif  // IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

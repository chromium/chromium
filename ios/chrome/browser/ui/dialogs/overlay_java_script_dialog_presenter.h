// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

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
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           web::DialogClosedCallback callback) override;
  void CancelDialogs(web::WebState* web_state) override;
};

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_

#include "ios/web/public/ui/java_script_dialog_presenter.h"

@class AlertCoordinator;
@class DialogPresenter;

// The maximum characters to use for the JavaScript dialog message text.
extern const size_t kJavaScriptDialogMaxMessageLength;

class JavaScriptDialogPresenterImpl final
    : public web::JavaScriptDialogPresenter {
 public:
  explicit JavaScriptDialogPresenterImpl(DialogPresenter* dialogPresenter);
  ~JavaScriptDialogPresenterImpl() override;

  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           web::DialogClosedCallback callback) override;

  void CancelDialogs(web::WebState* web_state) override;

  // JavaScript dialogs presented by this class cap the message text length to
  // kJavaScriptDialogMaxMessageLength.  This utility function performs that
  // operation on an input NSString.
  // TODO(crbug.com/674649): Remove this after switching to custom dialog
  // implementation.
  static NSString* GetTruncatedMessageText(NSString* message_text);

 private:
  // The underlying DialogPresenter handling the dialog UI.
  DialogPresenter* dialog_presenter_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogPresenterImpl);
};

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_PRESENTER_IMPL_H_

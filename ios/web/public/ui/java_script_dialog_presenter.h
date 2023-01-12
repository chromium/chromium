// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import "base/functional/callback.h"
#import "base/functional/callback_forward.h"
#import "url/gurl.h"

@class NSString;

namespace web {

class WebState;

class JavaScriptDialogPresenter {
 public:
  virtual ~JavaScriptDialogPresenter() = default;

  // Notifies the delegate that a JavaScript alert needs to be presented.
  virtual void RunJavaScriptAlertDialog(WebState* web_state,
                                        const GURL& origin_url,
                                        NSString* message_text,
                                        base::OnceClosure callback) = 0;

  // Notifies the delegate that a JavaScript confirmation dialog needs to be
  // presented.
  virtual void RunJavaScriptConfirmDialog(
      WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback) = 0;

  // Notifies the delegate that a JavaScript prompt needs to be presented.
  virtual void RunJavaScriptPromptDialog(
      WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback) = 0;

  // Informs clients that all requested dialogs associated with `web_state`
  // should be dismissed.
  virtual void CancelDialogs(WebState* web_state) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_

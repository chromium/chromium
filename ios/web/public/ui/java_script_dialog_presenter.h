// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import "ios/web/public/ui/java_script_dialog_callback.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#include "url/gurl.h"

@class NSString;

namespace web {

class WebState;

class JavaScriptDialogPresenter {
 public:
  virtual ~JavaScriptDialogPresenter() = default;

  // Requests presentation of a JavaScript dialog. Clients must always call
  // |callback| even if they choose not to present the dialog.
  virtual void RunJavaScriptDialog(WebState* web_state,
                                   const GURL& origin_url,
                                   JavaScriptDialogType dialog_type,
                                   NSString* message_text,
                                   NSString* default_prompt_text,
                                   DialogClosedCallback callback) = 0;
  // Informs clients that all requested dialogs associated with |web_state|
  // should be dismissed.
  virtual void CancelDialogs(WebState* web_state) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_JAVA_SCRIPT_DIALOG_PRESENTER_H_

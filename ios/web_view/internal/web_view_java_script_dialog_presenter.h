// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import "ios/web/public/ui/java_script_dialog_presenter.h"

@class CWVWebView;
@protocol CWVUIDelegate;

namespace ios_web_view {

// WebView implementation of JavaScriptDialogPresenter. Passes JavaScript alert
// handling to |ui_delegate_|.
class WebViewJavaScriptDialogPresenter final
    : public web::JavaScriptDialogPresenter {
 public:
  WebViewJavaScriptDialogPresenter(CWVWebView* web_view,
                                   id<CWVUIDelegate> ui_delegate);
  ~WebViewJavaScriptDialogPresenter() override;

  void SetUIDelegate(id<CWVUIDelegate> ui_delegate);

  // web::JavaScriptDialogPresenter overrides:
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           web::DialogClosedCallback callback) override;
  void CancelDialogs(web::WebState* web_state) override;

 private:
  // Displays JavaScript alert.
  void HandleJavaScriptAlert(const GURL& origin_url,
                             NSString* message_text,
                             web::DialogClosedCallback callback);

  // Displays JavaScript confirm dialog.
  void HandleJavaScriptConfirmDialog(const GURL& origin_url,
                                     NSString* message_text,
                                     web::DialogClosedCallback callback);

  // Displays JavaScript text prompt.
  void HandleJavaScriptTextPrompt(const GURL& origin_url,
                                  NSString* message_text,
                                  NSString* default_prompt_text,
                                  web::DialogClosedCallback callback);

  // The underlying delegate handling the dialog UI.
  __weak id<CWVUIDelegate> ui_delegate_ = nil;
  // The web view which originated the dialogs.
  __weak CWVWebView* web_view_ = nil;

  DISALLOW_COPY_AND_ASSIGN(WebViewJavaScriptDialogPresenter);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_

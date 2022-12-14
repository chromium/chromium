// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#import <Foundation/Foundation.h>

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

  WebViewJavaScriptDialogPresenter(const WebViewJavaScriptDialogPresenter&) =
      delete;
  WebViewJavaScriptDialogPresenter& operator=(
      const WebViewJavaScriptDialogPresenter&) = delete;

  ~WebViewJavaScriptDialogPresenter() override;

  void SetUIDelegate(id<CWVUIDelegate> ui_delegate);

  // web::JavaScriptDialogPresenter overrides:
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

 private:
  // The underlying delegate handling the dialog UI.
  __weak id<CWVUIDelegate> ui_delegate_ = nil;
  // The web view which originated the dialogs.
  __weak CWVWebView* web_view_ = nil;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_JAVA_SCRIPT_DIALOG_PRESENTER_H_

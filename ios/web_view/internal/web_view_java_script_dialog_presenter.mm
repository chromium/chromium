// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_java_script_dialog_presenter.h"

#import "base/functional/callback_helpers.h"
#import "ios/web_view/public/cwv_ui_delegate.h"
#import "net/base/apple/url_conversions.h"

namespace ios_web_view {

WebViewJavaScriptDialogPresenter::WebViewJavaScriptDialogPresenter(
    CWVWebView* web_view,
    id<CWVUIDelegate> ui_delegate)
    : ui_delegate_(ui_delegate), web_view_(web_view) {}

WebViewJavaScriptDialogPresenter::~WebViewJavaScriptDialogPresenter() = default;

void WebViewJavaScriptDialogPresenter::RunJavaScriptAlertDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceClosure callback) {
  SEL delegate_method = @selector(webView:
       runJavaScriptAlertPanelWithMessage:pageURL:completionHandler:);
  if (![ui_delegate_ respondsToSelector:delegate_method]) {
    std::move(callback).Run();
    return;
  }
  [ui_delegate_ webView:web_view_
      runJavaScriptAlertPanelWithMessage:message_text
                                 pageURL:net::NSURLWithGURL(origin_url)
                       completionHandler:base::CallbackToBlock(
                                             std::move(callback))];
}

void WebViewJavaScriptDialogPresenter::RunJavaScriptConfirmDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceCallback<void(bool success)> callback) {
  SEL delegate_method = @selector(webView:
      runJavaScriptConfirmPanelWithMessage:pageURL:completionHandler:);
  if (![ui_delegate_ respondsToSelector:delegate_method]) {
    std::move(callback).Run(false);
    return;
  }
  [ui_delegate_ webView:web_view_
      runJavaScriptConfirmPanelWithMessage:message_text
                                   pageURL:net::NSURLWithGURL(origin_url)
                         completionHandler:base::CallbackToBlock(
                                               std::move(callback))];
}

void WebViewJavaScriptDialogPresenter::RunJavaScriptPromptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    base::OnceCallback<void(NSString* user_input)> callback) {
  SEL delegate_method = @selector(webView:
      runJavaScriptTextInputPanelWithPrompt:defaultText:pageURL
                                           :completionHandler:);
  if (![ui_delegate_ respondsToSelector:delegate_method]) {
    std::move(callback).Run(nil);
    return;
  }
  [ui_delegate_ webView:web_view_
      runJavaScriptTextInputPanelWithPrompt:message_text
                                defaultText:default_prompt_text
                                    pageURL:net::NSURLWithGURL(origin_url)
                          completionHandler:base::CallbackToBlock(
                                                std::move(callback))];
}

void WebViewJavaScriptDialogPresenter::CancelDialogs(web::WebState* web_state) {
}

void WebViewJavaScriptDialogPresenter::SetUIDelegate(
    id<CWVUIDelegate> ui_delegate) {
  ui_delegate_ = ui_delegate;
}

}  // namespace ios_web_view

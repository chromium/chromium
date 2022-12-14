// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_java_script_dialog_presenter.h"

#import "ios/web_view/public/cwv_ui_delegate.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  __block base::OnceClosure scoped_callback = std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptAlertPanelWithMessage:message_text
                                 pageURL:net::NSURLWithGURL(origin_url)
                       completionHandler:^{
                         if (!scoped_callback.is_null()) {
                           std::move(scoped_callback).Run();
                         }
                       }];
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
  __block base::OnceCallback<void(bool success)> scoped_callback =
      std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptConfirmPanelWithMessage:message_text
                                   pageURL:net::NSURLWithGURL(origin_url)
                         completionHandler:^(BOOL is_confirmed) {
                           if (!scoped_callback.is_null()) {
                             std::move(scoped_callback).Run(is_confirmed);
                           }
                         }];
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
  __block base::OnceCallback<void(NSString * user_input)> scoped_callback =
      std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptTextInputPanelWithPrompt:message_text
                                defaultText:default_prompt_text
                                    pageURL:net::NSURLWithGURL(origin_url)
                          completionHandler:^(NSString* text_input) {
                            if (!scoped_callback.is_null()) {
                              std::move(scoped_callback).Run(text_input);
                            }
                          }];
}

void WebViewJavaScriptDialogPresenter::CancelDialogs(web::WebState* web_state) {
}

void WebViewJavaScriptDialogPresenter::SetUIDelegate(
    id<CWVUIDelegate> ui_delegate) {
  ui_delegate_ = ui_delegate;
}

}  // namespace ios_web_view

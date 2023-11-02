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

void WebViewJavaScriptDialogPresenter::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    web::DialogClosedCallback callback) {
  switch (dialog_type) {
    case web::JAVASCRIPT_DIALOG_TYPE_ALERT:
      HandleJavaScriptAlert(origin_url, message_text, std::move(callback));
      break;
    case web::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      HandleJavaScriptConfirmDialog(origin_url, message_text,
                                    std::move(callback));
      break;
    case web::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      HandleJavaScriptTextPrompt(origin_url, message_text, default_prompt_text,
                                 std::move(callback));
      break;
  }
}

void WebViewJavaScriptDialogPresenter::HandleJavaScriptAlert(
    const GURL& origin_url,
    NSString* message_text,
    web::DialogClosedCallback callback) {
  if (![ui_delegate_ respondsToSelector:@selector
                     (webView:runJavaScriptAlertPanelWithMessage:pageURL
                                :completionHandler:)]) {
    std::move(callback).Run(NO, nil);
    return;
  }
  __block web::DialogClosedCallback scoped_callback = std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptAlertPanelWithMessage:message_text
                                 pageURL:net::NSURLWithGURL(origin_url)
                       completionHandler:^{
                         if (!scoped_callback.is_null()) {
                           std::move(scoped_callback).Run(YES, nil);
                         }
                       }];
}

void WebViewJavaScriptDialogPresenter::HandleJavaScriptConfirmDialog(
    const GURL& origin_url,
    NSString* message_text,
    web::DialogClosedCallback callback) {
  if (![ui_delegate_ respondsToSelector:@selector
                     (webView:runJavaScriptConfirmPanelWithMessage:pageURL
                                :completionHandler:)]) {
    std::move(callback).Run(NO, nil);
    return;
  }
  __block web::DialogClosedCallback scoped_callback = std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptConfirmPanelWithMessage:message_text
                                   pageURL:net::NSURLWithGURL(origin_url)
                         completionHandler:^(BOOL is_confirmed) {
                           if (!scoped_callback.is_null()) {
                             std::move(scoped_callback).Run(is_confirmed, nil);
                           }
                         }];
}

void WebViewJavaScriptDialogPresenter::HandleJavaScriptTextPrompt(
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    web::DialogClosedCallback callback) {
  if (![ui_delegate_ respondsToSelector:@selector
                     (webView:runJavaScriptTextInputPanelWithPrompt:defaultText
                                :pageURL:completionHandler:)]) {
    std::move(callback).Run(NO, nil);
    return;
  }
  __block web::DialogClosedCallback scoped_callback = std::move(callback);
  [ui_delegate_ webView:web_view_
      runJavaScriptTextInputPanelWithPrompt:message_text
                                defaultText:default_prompt_text
                                    pageURL:net::NSURLWithGURL(origin_url)
                          completionHandler:^(NSString* text_input) {
                            if (!scoped_callback.is_null()) {
                              if (text_input == nil) {
                                std::move(scoped_callback).Run(NO, nil);
                              } else {
                                std::move(scoped_callback).Run(YES, text_input);
                              }
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

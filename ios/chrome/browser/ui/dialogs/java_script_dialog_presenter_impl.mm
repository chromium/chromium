// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/dialogs/java_script_dialog_presenter_impl.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#include "ios/chrome/browser/ui/dialogs/java_script_dialog_metrics.h"
#include "ui/gfx/text_elider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const size_t kJavaScriptDialogMaxMessageLength = 150;

JavaScriptDialogPresenterImpl::JavaScriptDialogPresenterImpl(
    DialogPresenter* dialogPresenter)
    : dialog_presenter_(dialogPresenter) {}

JavaScriptDialogPresenterImpl::~JavaScriptDialogPresenterImpl() {}

void JavaScriptDialogPresenterImpl::RunJavaScriptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    web::JavaScriptDialogType dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    web::DialogClosedCallback callback) {
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  if (JavaScriptDialogBlockingState::FromWebState(web_state)->blocked()) {
    // Block the dialog if needed.
    RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause::kBlocked);
    std::move(callback).Run(NO, nil);
    return;
  }
  message_text =
      JavaScriptDialogPresenterImpl::GetTruncatedMessageText(message_text);
  switch (dialog_type) {
    case web::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      __block web::DialogClosedCallback scoped_callback = std::move(callback);
      [dialog_presenter_
          runJavaScriptAlertPanelWithMessage:message_text
                                  requestURL:origin_url
                                    webState:web_state
                           completionHandler:^{
                             RecordDialogDismissalCause(
                                 IOSJavaScriptDialogDismissalCause::kUser);
                             if (!scoped_callback.is_null()) {
                               std::move(scoped_callback).Run(YES, nil);
                             }
                           }];
      break;
    }
    case web::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      __block web::DialogClosedCallback scoped_callback = std::move(callback);
      [dialog_presenter_
          runJavaScriptConfirmPanelWithMessage:message_text
                                    requestURL:origin_url
                                      webState:web_state
                             completionHandler:^(BOOL is_confirmed) {
                               RecordDialogDismissalCause(
                                   IOSJavaScriptDialogDismissalCause::kUser);
                               if (!scoped_callback.is_null()) {
                                 std::move(scoped_callback)
                                     .Run(is_confirmed, nil);
                               }
                             }];
      break;
    }
    case web::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      __block web::DialogClosedCallback scoped_callback = std::move(callback);
      [dialog_presenter_
          runJavaScriptTextInputPanelWithPrompt:message_text
                                    defaultText:default_prompt_text
                                     requestURL:origin_url
                                       webState:web_state
                              completionHandler:^(NSString* text_input) {
                                RecordDialogDismissalCause(
                                    IOSJavaScriptDialogDismissalCause::kUser);
                                if (!scoped_callback.is_null()) {
                                  std::move(scoped_callback)
                                      .Run(YES, text_input);
                                }
                              }];
      break;
    }
    default:
      break;
  }
}

void JavaScriptDialogPresenterImpl::CancelDialogs(web::WebState* web_state) {
  [dialog_presenter_ cancelDialogForWebState:web_state];
}

// static
NSString* JavaScriptDialogPresenterImpl::GetTruncatedMessageText(
    NSString* message_text) {
  if (message_text.length <= kJavaScriptDialogMaxMessageLength)
    return message_text;
  return base::SysUTF16ToNSString(gfx::TruncateString(
      base::SysNSStringToUTF16(message_text), kJavaScriptDialogMaxMessageLength,
      gfx::CHARACTER_BREAK));
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/overlay_java_script_dialog_presenter.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#include "ios/chrome/browser/ui/dialogs/java_script_dialog_metrics.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverlayJavaScriptDialogPresenter::OverlayJavaScriptDialogPresenter()
    : weak_factory_(this) {}

OverlayJavaScriptDialogPresenter::~OverlayJavaScriptDialogPresenter() = default;

void OverlayJavaScriptDialogPresenter::Close() {
  closing_ = true;
}

void OverlayJavaScriptDialogPresenter::RunJavaScriptDialog(
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

  std::unique_ptr<OverlayRequest> request;
  bool from_main_frame_origin =
      origin_url.GetOrigin() == web_state->GetLastCommittedURL().GetOrigin();
  const JavaScriptDialogSource source(web_state, origin_url,
                                      from_main_frame_origin);
  switch (dialog_type) {
    case web::JAVASCRIPT_DIALOG_TYPE_ALERT:
      request =
          OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
              source, base::SysNSStringToUTF8(message_text));
      break;
    case web::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      request = OverlayRequest::CreateWithConfig<
          JavaScriptConfirmationOverlayRequestConfig>(
          source, base::SysNSStringToUTF8(message_text));
      break;
    case web::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      request = OverlayRequest::CreateWithConfig<
          JavaScriptPromptOverlayRequestConfig>(
          source, base::SysNSStringToUTF8(message_text),
          base::SysNSStringToUTF8(default_prompt_text));
      break;
  }
  request->set_callback(base::BindOnce(
      &OverlayJavaScriptDialogPresenter::HandleJavaScriptDialogResponse,
      weak_factory_.GetWeakPtr(), std::move(callback), source, dialog_type));
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void OverlayJavaScriptDialogPresenter::CancelDialogs(web::WebState* web_state) {
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->CancelAllRequests();
}

void OverlayJavaScriptDialogPresenter::HandleJavaScriptDialogResponse(
    web::DialogClosedCallback callback,
    JavaScriptDialogSource source,
    web::JavaScriptDialogType dialog_type,
    OverlayResponse* response) {
  // Notify the blocking state that the dialog was shown.
  web::WebState* web_state = source.web_state();
  if (web_state) {
    JavaScriptDialogBlockingState::FromWebState(web_state)
        ->JavaScriptDialogWasShown();
  }

  if (!response) {
    // Cancelled dialogs have no response.  Cancellations that don't occur
    // during tab closures are due to navigation.
    IOSJavaScriptDialogDismissalCause cause =
        closing_ ? IOSJavaScriptDialogDismissalCause::kClosure
                 : IOSJavaScriptDialogDismissalCause::kNavigation;
    RecordDialogDismissalCause(cause);
    std::move(callback).Run(false, nil);
    return;
  }

  RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause::kUser);
  bool success = false;
  NSString* user_input = nil;
  switch (dialog_type) {
    case web::JAVASCRIPT_DIALOG_TYPE_ALERT:
      success = true;
      break;
    case web::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      JavaScriptConfirmationOverlayResponseInfo* confirmation_info =
          response->GetInfo<JavaScriptConfirmationOverlayResponseInfo>();
      success = confirmation_info && confirmation_info->dialog_confirmed();
    } break;
    case web::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      JavaScriptPromptOverlayResponseInfo* prompt_info =
          response->GetInfo<JavaScriptPromptOverlayResponseInfo>();
      if (prompt_info) {
        success = true;
        user_input = base::SysUTF8ToNSString(prompt_info->text_input());
      }
    } break;
  }
  std::move(callback).Run(success, user_input);
}

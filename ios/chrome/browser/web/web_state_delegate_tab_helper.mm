// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(WebStateDelegateTabHelper)

WebStateDelegateTabHelper::WebStateDelegateTabHelper(web::WebState* web_state)
    : weak_factory_(this) {
  web_state->AddObserver(this);
}

WebStateDelegateTabHelper::~WebStateDelegateTabHelper() = default;

web::JavaScriptDialogPresenter*
WebStateDelegateTabHelper::GetJavaScriptDialogPresenter(web::WebState* source) {
  return &java_script_dialog_presenter_;
}

void WebStateDelegateTabHelper::OnAuthRequired(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    const web::WebStateDelegate::AuthCallback& callback) {
  AuthCallback local_callback(callback);
  std::string message = base::SysNSStringToUTF8(
      nsurlprotectionspace_util::MessageForHTTPAuth(protection_space));
  std::string default_username;
  if (proposed_credential.user)
    default_username = base::SysNSStringToUTF8(proposed_credential.user);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          message, default_username);
  request->set_callback(
      base::BindOnce(&WebStateDelegateTabHelper::OnHTTPAuthOverlayFinished,
                     weak_factory_.GetWeakPtr(), callback));
  OverlayRequestQueue::FromWebState(source, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

#pragma mark - WebStateObserver

void WebStateDelegateTabHelper::WebStateDestroyed(web::WebState* web_state) {
  java_script_dialog_presenter_.Close();
  web_state->RemoveObserver(this);
}

#pragma mark - Overlay Callbacks

void WebStateDelegateTabHelper::OnHTTPAuthOverlayFinished(
    web::WebStateDelegate::AuthCallback callback,
    OverlayResponse* response) {
  if (!response) {
    callback.Run(nil, nil);
    return;
  }
  HTTPAuthOverlayResponseInfo* auth_info =
      response->GetInfo<HTTPAuthOverlayResponseInfo>();
  if (!auth_info) {
    callback.Run(nil, nil);
    return;
  }
  callback.Run(base::SysUTF8ToNSString(auth_info->username()),
               base::SysUTF8ToNSString(auth_info->password()));
}

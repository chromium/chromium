// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Callback for HTTP authentication dialogs.  This callback is a standalone
// function rather than an instance method.  This is to ensure that the callback
// can be executed regardless of whether the tab helper has been destroyed.
void OnHTTPAuthOverlayFinished(web::WebStateDelegate::AuthCallback callback,
                               OverlayResponse* response) {
  if (response) {
    HTTPAuthOverlayResponseInfo* auth_info =
        response->GetInfo<HTTPAuthOverlayResponseInfo>();
    if (auth_info) {
      std::move(callback).Run(base::SysUTF8ToNSString(auth_info->username()),
                              base::SysUTF8ToNSString(auth_info->password()));
      return;
    }
  }
  std::move(callback).Run(nil, nil);
}
}  // namespace

WEB_STATE_USER_DATA_KEY_IMPL(WebStateDelegateTabHelper)

WebStateDelegateTabHelper::WebStateDelegateTabHelper(web::WebState* web_state) {
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
    web::WebStateDelegate::AuthCallback callback) {
  std::string message = base::SysNSStringToUTF8(
      nsurlprotectionspace_util::MessageForHTTPAuth(protection_space));
  std::string default_username;
  if (proposed_credential.user)
    default_username = base::SysNSStringToUTF8(proposed_credential.user);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          nsurlprotectionspace_util::RequesterOrigin(protection_space), message,
          default_username);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&OnHTTPAuthOverlayFinished, std::move(callback)));
  OverlayRequestQueue::FromWebState(source, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

#pragma mark - WebStateObserver

void WebStateDelegateTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}


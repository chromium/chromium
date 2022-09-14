// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_overlay_request_callback_installer.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardMainAction;
using save_card_infobar_overlays::SaveCardModalRequestConfig;
using save_card_infobar_overlays::SaveCardLoadURL;

SaveCardInfobarModalOverlayRequestCallbackInstaller::
    SaveCardInfobarModalOverlayRequestCallbackInstaller(
        SaveCardInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          SaveCardModalRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

SaveCardInfobarModalOverlayRequestCallbackInstaller::
    ~SaveCardInfobarModalOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void SaveCardInfobarModalOverlayRequestCallbackInstaller::
    SaveCardCredentialsCallback(OverlayRequest* request,
                                OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;

  SaveCardMainAction* info = response->GetInfo<SaveCardMainAction>();
  interaction_handler_->UpdateCredentials(
      GetOverlayRequestInfobar(request),
      base::SysNSStringToUTF16(info->cardholder_name()),
      base::SysNSStringToUTF16(info->expiration_month()),
      base::SysNSStringToUTF16(info->expiration_year()));
}

void SaveCardInfobarModalOverlayRequestCallbackInstaller::LoadURLCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  SaveCardLoadURL* info = response->GetInfo<SaveCardLoadURL>();
  interaction_handler_->LoadURL(GetOverlayRequestInfobar(request),
                                info->link_url());
}

#pragma mark - OverlayRequestCallbackInstaller

void SaveCardInfobarModalOverlayRequestCallbackInstaller::
    InstallCallbacksInternal(OverlayRequest* request) {
  InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
      request);
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&SaveCardInfobarModalOverlayRequestCallbackInstaller::
                              SaveCardCredentialsCallback,
                          weak_factory_.GetWeakPtr(), request),
      SaveCardMainAction::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &SaveCardInfobarModalOverlayRequestCallbackInstaller::LoadURLCallback,
          weak_factory_.GetWeakPtr(), request),
      SaveCardLoadURL::ResponseSupport()));
}

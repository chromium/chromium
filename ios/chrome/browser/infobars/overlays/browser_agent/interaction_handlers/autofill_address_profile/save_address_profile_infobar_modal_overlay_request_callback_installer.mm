// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::CancelViewAction;

SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller(
        SaveAddressProfileInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          SaveAddressProfileModalRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    ~SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    SaveEditedProfileDetailsCallback(OverlayRequest* request,
                                     OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar) {
    return;
  }

  EditedProfileSaveAction* info = response->GetInfo<EditedProfileSaveAction>();
  interaction_handler_->SaveEditedProfile(infobar, info->profile_data());
}

void SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    CancelModalCallback(OverlayRequest* request, OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar) {
    return;
  }

  CancelViewAction* info = response->GetInfo<CancelViewAction>();
  interaction_handler_->CancelModal(infobar, info->edit_view_is_dismissed());
}

#pragma mark - OverlayRequestCallbackInstaller

void SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    InstallCallbacksInternal(OverlayRequest* request) {
  InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
      request);
  OverlayCallbackManager* manager = request->GetCallbackManager();

  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
              SaveEditedProfileDetailsCallback,
          weak_factory_.GetWeakPtr(), request),
      EditedProfileSaveAction::ResponseSupport()));

  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
              CancelModalCallback,
          weak_factory_.GetWeakPtr(), request),
      CancelViewAction::ResponseSupport()));
}

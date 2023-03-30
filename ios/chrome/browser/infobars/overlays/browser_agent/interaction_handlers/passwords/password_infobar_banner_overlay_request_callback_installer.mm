// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_overlay_request_callback_installer.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PasswordInfobarBannerOverlayRequestCallbackInstaller::
    PasswordInfobarBannerOverlayRequestCallbackInstaller(
        PasswordInfobarBannerInteractionHandler* interaction_handler,
        password_modal::PasswordAction action_type)
    : InfobarBannerOverlayRequestCallbackInstaller(
          PasswordInfobarBannerOverlayRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler),
      password_action_(action_type) {
  DCHECK(interaction_handler_);
}

PasswordInfobarBannerOverlayRequestCallbackInstaller::
    ~PasswordInfobarBannerOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void PasswordInfobarBannerOverlayRequestCallbackInstaller::
    MainActionButtonTapped(OverlayRequest* request, OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar) {
    return;
  }
  // Since this installer is used for both Save and Update password overlays,
  // the callback will be executed for both types. Return early if the request
  // is not the same type.
  if (request->GetConfig<PasswordInfobarBannerOverlayRequestConfig>()
          ->action() != password_action_) {
    return;
  }
  interaction_handler_->MainButtonTapped(infobar);
}

void PasswordInfobarBannerOverlayRequestCallbackInstaller::
    ShowModalButtonTapped(OverlayRequest* request, OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar) {
    return;
  }
  // Since this installer is used for both Save and Update password overlays,
  // the callback will be executed for both types. Return early if the request
  // is not the same type.
  if (request->GetConfig<PasswordInfobarBannerOverlayRequestConfig>()
          ->action() != password_action_) {
    return;
  }
  interaction_handler_->ShowModalButtonTapped(infobar,
                                              request->GetQueueWebState());
}

void PasswordInfobarBannerOverlayRequestCallbackInstaller::
    BannerDismissedByUser(OverlayRequest* request, OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar) {
    return;
  }
  // Since this installer is used for both Save and Update password overlays,
  // the callback will be executed for both types. Return early if the request
  // is not the same type.
  if (request->GetConfig<PasswordInfobarBannerOverlayRequestConfig>()
          ->action() != password_action_) {
    return;
  }
  interaction_handler_->BannerDismissedByUser(infobar);
}

void PasswordInfobarBannerOverlayRequestCallbackInstaller::RemoveInfobar(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar || infobar->removed_from_owner()) {
    return;
  }
  // Since this installer is used for both Save and Update password overlays,
  // the callback will be executed for both types. Return early if the request
  // is not the same type.
  if (request->GetConfig<PasswordInfobarBannerOverlayRequestConfig>()
          ->action() != password_action_) {
    return;
  }

  InfoBarManagerImpl::FromWebState(request->GetQueueWebState())
      ->RemoveInfoBar(infobar);
}

#pragma mark - OverlayRequestCallbackInstaller

void PasswordInfobarBannerOverlayRequestCallbackInstaller::
    InstallCallbacksInternal(OverlayRequest* request) {
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &PasswordInfobarBannerOverlayRequestCallbackInstaller::
              MainActionButtonTapped,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerMainActionResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &PasswordInfobarBannerOverlayRequestCallbackInstaller::
              ShowModalButtonTapped,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerShowModalResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &PasswordInfobarBannerOverlayRequestCallbackInstaller::
              BannerDismissedByUser,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerUserInitiatedDismissalResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &PasswordInfobarBannerOverlayRequestCallbackInstaller::RemoveInfobar,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerRemoveInfobarResponse::ResponseSupport()));
}

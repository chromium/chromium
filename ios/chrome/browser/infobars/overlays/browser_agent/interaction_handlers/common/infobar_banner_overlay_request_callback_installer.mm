// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#import "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarBannerOverlayRequestCallbackInstaller::
    InfobarBannerOverlayRequestCallbackInstaller(
        const OverlayRequestSupport* request_support,
        InfobarBannerInteractionHandler* interaction_handler)
    : request_support_(request_support),
      interaction_handler_(interaction_handler) {
  DCHECK(request_support_);
  DCHECK(interaction_handler_);
}

InfobarBannerOverlayRequestCallbackInstaller::
    ~InfobarBannerOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void InfobarBannerOverlayRequestCallbackInstaller::MainActionButtonTapped(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->MainButtonTapped(infobar);
}

void InfobarBannerOverlayRequestCallbackInstaller::ShowModalButtonTapped(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->ShowModalButtonTapped(infobar,
                                              request->GetQueueWebState());
}

void InfobarBannerOverlayRequestCallbackInstaller::BannerDismissedByUser(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->BannerDismissedByUser(infobar);
}

void InfobarBannerOverlayRequestCallbackInstaller::RemoveInfobar(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  InfoBarControllerDelegate* infobar_controller_delegate =
      static_cast<InfoBarControllerDelegate*>(infobar);
  if (!infobar || !infobar_controller_delegate->IsOwned() ||
      !infobar->delegate() || infobar->removed_from_owner())
    return;

  infobar_controller_delegate->RemoveInfoBar();
}

#pragma mark - OverlayRequestCallbackInstaller

const OverlayRequestSupport*
InfobarBannerOverlayRequestCallbackInstaller::GetRequestSupport() const {
  return request_support_;
}

void InfobarBannerOverlayRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &InfobarBannerOverlayRequestCallbackInstaller::MainActionButtonTapped,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerMainActionResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &InfobarBannerOverlayRequestCallbackInstaller::ShowModalButtonTapped,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerShowModalResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &InfobarBannerOverlayRequestCallbackInstaller::BannerDismissedByUser,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerUserInitiatedDismissalResponse::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &InfobarBannerOverlayRequestCallbackInstaller::RemoveInfobar,
          weak_factory_.GetWeakPtr(), request),
      InfobarBannerRemoveInfobarResponse::ResponseSupport()));
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_modal_overlay_request_callback_installer.h"

#include "base/bind.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_modal_infobar_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using reading_list_infobar_modal_responses::NeverAsk;

namespace reading_list_infobar_overlay {

ModalRequestCallbackInstaller::ModalRequestCallbackInstaller(
    ReadingListInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          ReadingListInfobarModalOverlayRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

ModalRequestCallbackInstaller::~ModalRequestCallbackInstaller() = default;

#pragma mark - Private

void ModalRequestCallbackInstaller::NeverAskCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;

  interaction_handler_->NeverAsk(infobar);
}

#pragma mark - OverlayRequestCallbackInstaller

void ModalRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
      request);
  OverlayCallbackManager* manager = request->GetCallbackManager();

  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&ModalRequestCallbackInstaller::NeverAskCallback,
                          weak_factory_.GetWeakPtr(), request),
      NeverAsk::ResponseSupport()));
}

}  // namespace reading_list_infobar_overlay

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/default/default_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"

DefaultInfobarBannerInteractionHandler::DefaultInfobarBannerInteractionHandler(
    InfobarType infobar_type)
    : InfobarBannerInteractionHandler(
          DefaultInfobarOverlayRequestConfig::RequestSupport()),
      infobar_type_(infobar_type) {}

DefaultInfobarBannerInteractionHandler::
    ~DefaultInfobarBannerInteractionHandler() = default;

void DefaultInfobarBannerInteractionHandler::ShowModalButtonTapped(
    InfoBarIOS* infobar,
    web::WebState* web_state) {
  if (infobar->infobar_type() != infobar_type_) {
    return;
  }
  InfobarBannerInteractionHandler::ShowModalButtonTapped(infobar, web_state);
}

void DefaultInfobarBannerInteractionHandler::BannerDismissedByUser(
    InfoBarIOS* infobar) {
  if (infobar->infobar_type() != infobar_type_) {
    return;
  }
  InfobarBannerInteractionHandler::BannerDismissedByUser(infobar);
}

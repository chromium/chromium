// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/update_password_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/update_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UpdatePasswordInfobarInteractionHandler::
    UpdatePasswordInfobarInteractionHandler(Browser* browser)
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypePasswordUpdate,
          std::make_unique<PasswordInfobarBannerInteractionHandler>(
              UpdatePasswordInfobarBannerOverlayRequestConfig::
                  RequestSupport()),
          std::make_unique<PasswordInfobarModalInteractionHandler>(
              browser,
              password_modal::PasswordAction::kUpdate)) {}

UpdatePasswordInfobarInteractionHandler::
    ~UpdatePasswordInfobarInteractionHandler() = default;

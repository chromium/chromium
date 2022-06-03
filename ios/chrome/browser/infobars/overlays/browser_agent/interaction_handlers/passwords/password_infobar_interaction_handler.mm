// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PasswordInfobarInteractionHandler::PasswordInfobarInteractionHandler(
    Browser* browser)
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypePasswordSave,
          std::make_unique<PasswordInfobarBannerInteractionHandler>(
              SavePasswordInfobarBannerOverlayRequestConfig::RequestSupport()),
          std::make_unique<PasswordInfobarModalInteractionHandler>(
              browser,
              password_modal::PasswordAction::kSave)) {}

PasswordInfobarInteractionHandler::~PasswordInfobarInteractionHandler() =
    default;

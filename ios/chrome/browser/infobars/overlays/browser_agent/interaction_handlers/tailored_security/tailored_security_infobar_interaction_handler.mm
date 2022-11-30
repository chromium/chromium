// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TailoredSecurityInfobarInteractionHandler::
    TailoredSecurityInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypeTailoredSecurityService,
          std::make_unique<TailoredSecurityInfobarBannerInteractionHandler>(
              tailored_security_service_infobar_overlays::
                  TailoredSecurityServiceBannerRequestConfig::RequestSupport()),
          nil) {}

TailoredSecurityInfobarInteractionHandler::
    ~TailoredSecurityInfobarInteractionHandler() = default;

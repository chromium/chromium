// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/default/default_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DefaultInfobarBannerInteractionHandler::DefaultInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          DefaultInfobarOverlayRequestConfig::RequestSupport()) {}

DefaultInfobarBannerInteractionHandler::
    ~DefaultInfobarBannerInteractionHandler() = default;

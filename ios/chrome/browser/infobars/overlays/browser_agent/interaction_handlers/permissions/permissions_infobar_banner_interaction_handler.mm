// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/permissions/permissions_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/permissions_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarBannerInteractionHandler

PermissionsInfobarBannerInteractionHandler::
    PermissionsInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          PermissionsBannerRequestConfig::RequestSupport()) {}

PermissionsInfobarBannerInteractionHandler::
    ~PermissionsInfobarBannerInteractionHandler() = default;

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/permissions/permissions_infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/permissions/permissions_modal_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PermissionsInfobarModalOverlayMediator ()
// The permissions modal config from the request.
@property(nonatomic, readonly)
    PermissionsInfobarModalOverlayRequestConfig* config;
@end

@implementation PermissionsInfobarModalOverlayMediator

#pragma mark - Accessors

- (PermissionsInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<PermissionsInfobarModalOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return PermissionsInfobarModalOverlayRequestConfig::RequestSupport();
}

@end

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_coordinator.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppLauncherAlertOverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
}

@end

@implementation AppLauncherAlertOverlayCoordinator (Subclassing)

#pragma mark - AlertOverlayCoordinator

- (AlertOverlayMediator*)newMediator {
  return [[AppLauncherAlertOverlayMediator alloc] initWithRequest:self.request];
}

@end

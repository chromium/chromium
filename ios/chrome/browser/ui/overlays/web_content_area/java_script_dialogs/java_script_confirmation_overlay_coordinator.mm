// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JavaScriptConfirmationOverlayCoordinator

#pragma mark - OverlayRequestCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<JavaScriptConfirmationOverlayRequestConfig>();
}

@end

@implementation JavaScriptConfirmationOverlayCoordinator (Subclassing)

- (AlertOverlayMediator*)newMediator {
  return [[JavaScriptConfirmationOverlayMediator alloc]
      initWithRequest:self.request];
}

@end

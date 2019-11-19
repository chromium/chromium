// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JavaScriptPromptOverlayCoordinator

#pragma mark - OverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<JavaScriptPromptOverlayRequestConfig>();
}

@end

@implementation JavaScriptPromptOverlayCoordinator (Subclassing)

- (AlertOverlayMediator*)newMediator {
  return [[JavaScriptPromptOverlayMediator alloc] initWithRequest:self.request];
}

@end

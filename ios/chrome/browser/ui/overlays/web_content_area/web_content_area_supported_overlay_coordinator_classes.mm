// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/web_content_area_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web_content_area {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  return @[ [AlertOverlayCoordinator class] ];
}

}  // web_content_area

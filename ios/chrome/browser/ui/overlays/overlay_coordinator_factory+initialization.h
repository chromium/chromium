// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_COORDINATOR_FACTORY_INITIALIZATION_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_COORDINATOR_FACTORY_INITIALIZATION_H_

#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

@interface OverlayRequestCoordinatorFactory (Initialization)
// Initializer for a factory that supports the OverlayRequestCoordinator types
// in |supportedOverlayClasses|.
- (instancetype)initWithBrowser:(Browser*)browser
    supportedOverlayRequestCoordinatorClasses:
        (NSArray<Class>*)supportedOverlayClasses;
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_COORDINATOR_FACTORY_INITIALIZATION_H_

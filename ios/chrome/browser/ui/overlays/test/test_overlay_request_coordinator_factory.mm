// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/test/test_overlay_request_coordinator_factory.h"

#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory+initialization.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_contained_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_presented_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestOverlayRequestCoordinatorFactory

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBrowser:browser
      supportedOverlayRequestCoordinatorClasses:
          @ [[TestContainedOverlayCoordinator class],
             [TestPresentedOverlayCoordinator class]]];
}

@end

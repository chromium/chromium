// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test/test_overlay_request_coordinator_factory.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_contained_overlay_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/test_modality/test_presented_overlay_coordinator.h"

@implementation TestOverlayRequestCoordinatorFactory

- (Class)coordinatorClassForRequest:(OverlayRequest*)request {
  if ([TestContainedOverlayCoordinator requestSupport]->IsRequestSupported(
          request)) {
    return [TestContainedOverlayCoordinator class];
  } else {
    return [TestPresentedOverlayCoordinator class];
  }
}

@end

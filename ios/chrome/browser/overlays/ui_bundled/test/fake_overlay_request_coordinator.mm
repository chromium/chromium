// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

@implementation FakeOverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return OverlayRequestSupport::All();
}

- (void)startAnimated:(BOOL)animated {
}

- (void)stopAnimated:(BOOL)animated {
}

@end

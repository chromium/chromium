// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OverlayRequestCoordinator

- (void)dealloc {
  // ChromeCoordinator's |-dealloc| calls |-stop|, which defaults to an animated
  // dismissal.  OverlayRequestCoordinators should instead stop without
  // animation so that the OverlayRequestCoordinatorDelegate can be notified of
  // the dismissal immediately.
  [self stopAnimated:NO];
}

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return NO;
}

+ (BOOL)showsOverlayUsingChildViewController {
  return NO;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   request:(OverlayRequest*)request
                                  delegate:(OverlayRequestCoordinatorDelegate*)
                                               delegate {
  DCHECK([[self class] supportsRequest:request]);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _request = request;
    DCHECK(_request);
    _delegate = delegate;
    DCHECK(_delegate);
  }
  return self;
}

#pragma mark - Public

- (void)startAnimated:(BOOL)animated {
  NOTREACHED() << "Subclasses must implement.";
}

- (void)stopAnimated:(BOOL)animated {
  NOTREACHED() << "Subclasses must implement.";
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self startAnimated:YES];
}

- (void)stop {
  [self stopAnimated:YES];
}

@end

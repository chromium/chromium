// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_coordinator.h"

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

@implementation LocationBarBadgeCoordinator {
  LocationBarBadgeViewController* _viewController;
}

- (void)start {
  _viewController = [[LocationBarBadgeViewController alloc] init];
  _mediator = [[LocationBarBadgeMediator alloc] init];
  _mediator.consumer = _viewController;
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;
}

@end

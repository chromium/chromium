// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarCoordinator ()
@property(nonatomic, strong) SecondaryToolbarViewController* viewController;
@end

@implementation SecondaryToolbarCoordinator

@dynamic viewController;

#pragma mark - AdaptiveToolbarCoordinator

- (void)start {
  self.viewController = [[SecondaryToolbarViewController alloc] init];
  self.viewController.buttonFactory = [self buttonFactoryWithType:SECONDARY];
  self.viewController.dispatcher = self.dispatcher;

  [super start];
}

@end

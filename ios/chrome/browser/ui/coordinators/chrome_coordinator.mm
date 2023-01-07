// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  if (self = [super init]) {
    _baseViewController = viewController;
    _childCoordinators = [MutableCoordinatorArray array];
    _browser = browser;
  }
  return self;
}

#pragma mark - Accessors

- (ChromeCoordinator*)activeChildCoordinator {
  // By default the active child is the one most recently added to the child
  // array, but subclasses can override this.
  return self.childCoordinators.lastObject;
}

#pragma mark - Public

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end

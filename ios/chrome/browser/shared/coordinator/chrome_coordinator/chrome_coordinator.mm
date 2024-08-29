// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation ChromeCoordinator {
  base::WeakPtr<Browser> _browser;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  if ((self = [super init])) {
    _baseViewController = viewController;
    _childCoordinators = [MutableCoordinatorArray array];
    if (browser) {
      _browser = browser->AsWeakPtr();
    }
  }
  return self;
}

#pragma mark - Accessors

- (ChromeCoordinator*)activeChildCoordinator {
  // By default the active child is the one most recently added to the child
  // array, but subclasses can override this.
  return self.childCoordinators.lastObject;
}

- (Browser*)browser {
  return _browser.get();
}

#pragma mark - Public

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end

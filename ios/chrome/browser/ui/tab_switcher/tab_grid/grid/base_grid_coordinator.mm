// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_coordinator+subclassing.h"

#import "base/check.h"

@implementation BaseGridCoordinator {
  // Mutator that handle toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate to handle presenting the action sheet.
  __weak id<GridMediatorDelegate> _gridMediatorDelegate;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate {
  CHECK(baseViewController);
  CHECK(browser);
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    CHECK(toolbarsMutator);
    CHECK(delegate);
    _toolbarsMutator = toolbarsMutator;
    _gridMediatorDelegate = delegate;
  }
  return self;
}

#pragma mark - Subclassing properties

- (id<GridToolbarsMutator>)toolbarsMutator {
  return _toolbarsMutator;
}

- (id<GridMediatorDelegate>)gridMediatorDelegate {
  return _gridMediatorDelegate;
}

@end

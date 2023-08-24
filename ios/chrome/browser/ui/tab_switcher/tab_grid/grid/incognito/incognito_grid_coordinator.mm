// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_coordinator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@implementation IncognitoGridCoordinator {
  // Mediator of incognito grid.
  IncognitoGridMediator* _mediator;
  // Mutator that handle toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate to handle presenting the action sheet.
  __weak id<GridMediatorDelegate> _gridMediatorDelegate;
}

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

#pragma mark - Property Implementation.

- (IncognitoGridMediator*)incognitoGridMediator {
  CHECK(_mediator)
      << "IncognitoGridCoordinator's -start should be called before.";
  return _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // TODO(crbug.com/1457146): Init view controller here instead of having a
  // public property.

  _mediator = [[IncognitoGridMediator alloc]
      initWithConsumer:self.incognitoViewController.incognitoTabsConsumer];
  _mediator.browser = self.browser;
  _mediator.delegate = _gridMediatorDelegate;
  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.actionWrangler = self.incognitoViewController;
}

- (void)stop {
  _mediator = nil;
  _toolbarsMutator = nil;
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_coordinator.h"

#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@implementation RegularGridCoordinator {
  // Mediator of regular grid.
  RegularGridMediator* _mediator;
  // Mutator that handles toolbars changes.
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

- (RegularGridMediator*)regularGridMediator {
  CHECK(_mediator)
      << "RegularGridCoordinator's -start should be called before.";
  return _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[RegularGridMediator alloc]
      initWithConsumer:self.regularViewController.regularTabsConsumer];
  _mediator.browser = self.browser;
  _mediator.delegate = _gridMediatorDelegate;
  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.actionWrangler = self.regularViewController;

  // TODO(crbug.com/1457146): As browser state should never be nil, it should be
  // safe to remove the check.
  ChromeBrowserState* regularBrowserState = self.browser->GetBrowserState();
  if (regularBrowserState) {
    _mediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            regularBrowserState);
  }

  self.regularViewController.regularTabsDelegate = _mediator;
  self.regularViewController.regularTabsDragDropHandler = _mediator;
  self.regularViewController.regularTabsShareableItemsProvider = _mediator;
}

- (void)stop {
  _mediator = nil;
}

@end

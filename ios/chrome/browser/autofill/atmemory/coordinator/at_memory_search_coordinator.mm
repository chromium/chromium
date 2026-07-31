// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation AtMemorySearchCoordinator {
  // Search view controller.
  AtMemorySearchViewController* _atMemorySearchViewController;
  // Mediator for the AtMemory search coordinator.
  AtMemorySearchMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _atMemorySearchViewController = [[AtMemorySearchViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _atMemorySearchViewController.searchResultHandler = self.searchResultHandler;
  _mediator = [[AtMemorySearchMediator alloc] init];

  [self.baseNavigationController
      pushViewController:_atMemorySearchViewController
                animated:YES];
}

- (void)stop {
  if (self.baseNavigationController.topViewController ==
      _atMemorySearchViewController) {
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
  _atMemorySearchViewController = nil;
  _mediator = nil;
}

@end

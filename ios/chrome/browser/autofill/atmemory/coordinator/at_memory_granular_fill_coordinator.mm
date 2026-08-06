// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_coordinator.h"

#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation AtMemoryGranularFillCoordinator {
  // View controller for the AtMemory granular fill screen.
  AtMemoryGranularFillViewController* _atMemoryGranularFillViewController;
  // Mediator for the AtMemory granular fill coordinator.
  AtMemoryGranularFillMediator* _mediator;
  // Search result containing attributes to display.
  std::optional<autofill::MemorySearchResult> _result;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                              result:
                                  (const autofill::MemorySearchResult&)result {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _result = result;
  }
  return self;
}

- (void)start {
  _atMemoryGranularFillViewController =
      [[AtMemoryGranularFillViewController alloc]
          initWithStyle:ChromeTableViewStyle()];

  _mediator = [[AtMemoryGranularFillMediator alloc] init];
  _mediator.fillHandler = self.fillHandler;

  [self.baseNavigationController
      pushViewController:_atMemoryGranularFillViewController
                animated:YES];
}

- (void)stop {
  if (self.baseNavigationController.topViewController ==
      _atMemoryGranularFillViewController) {
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
  _atMemoryGranularFillViewController = nil;
  _mediator = nil;
  _result.reset();
}

@end

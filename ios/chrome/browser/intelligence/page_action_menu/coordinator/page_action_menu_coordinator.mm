// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

@implementation PageActionMenuCoordinator {
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PageActionMenuViewController alloc] init];
  _mediator = [[PageActionMenuMediator alloc] init];
  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;

  [super stop];
}

@end

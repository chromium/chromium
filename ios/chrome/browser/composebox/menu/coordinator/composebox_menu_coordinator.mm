// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

@implementation ComposeboxMenuCoordinator {
  ComposeboxMenuViewController* _viewController;
  ComposeboxMenuMediator* _mediator;
}

- (void)start {
  _viewController = [[ComposeboxMenuViewController alloc] init];
  _mediator = [[ComposeboxMenuMediator alloc] init];

  _viewController.sheetPresentationController.prefersGrabberVisible = YES;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator = nil;
}

@end

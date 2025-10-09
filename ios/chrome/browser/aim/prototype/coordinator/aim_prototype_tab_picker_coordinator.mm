// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_coordinator.h"

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_view_controller.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"

@implementation AimPrototypeTabPickerCoordinator {
  /// The tab picker mediator.
  AimPrototypeTabPickerMediator* _mediator;
  /// The tab picker view controller.
  AimPrototypeTabPickerViewController* _viewController;
}

- (void)start {
  _viewController = [[AimPrototypeTabPickerViewController alloc] init];
  _mediator = [[AimPrototypeTabPickerMediator alloc]
      initWithGridConsumer:_viewController.gridViewController];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  [super stop];
}

@end

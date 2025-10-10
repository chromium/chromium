// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_coordinator.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_view_controller.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"

@interface AimPrototypeTabPickerCoordinator ()

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

@end

@implementation AimPrototypeTabPickerCoordinator {
  /// The tab picker mediator.
  AimPrototypeTabPickerMediator* _mediator;
  /// The tab picker view controller.
  AimPrototypeTabPickerViewController* _viewController;
}

- (void)start {
  _viewController = [[AimPrototypeTabPickerViewController alloc] init];

  _mediator = [[AimPrototypeTabPickerMediator alloc]
        initWithGridConsumer:_viewController.gridViewController
           tabPickerConsumer:_viewController
      tabsAttachmentDelegate:self];
  _mediator.browser = self.browser;

  _viewController.mutator = _mediator;
  _viewController.gridViewController.snapshotAndfaviconDataSource = _mediator;
  _viewController.gridViewController.mutator = _mediator;
  _viewController.gridViewController.gridProvider = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - AimPrototypeTabsAttachmentDelegate

- (void)attachSelectedTabs:(AimPrototypeTabPickerMediator*)tabPickerMediator
       selectedIdentifiers:(NSSet<GridItemIdentifier*>*)selectedIdentifiers {
  // TODO(crbug.com/449657332): Pass the selected webstates to the aim mediator.
}

@end

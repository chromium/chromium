// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_coordinator.h"

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_view_controller.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"

@interface ComposeboxTabPickerCoordinator ()

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

@end

@implementation ComposeboxTabPickerCoordinator {
  /// The tab picker mediator.
  ComposeboxTabPickerMediator* _mediator;
  /// The tab picker view controller.
  ComposeboxTabPickerViewController* _viewController;
}

- (void)start {
  _viewController = [[ComposeboxTabPickerViewController alloc] init];

  _mediator = [[ComposeboxTabPickerMediator alloc]
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

#pragma mark - ComposeboxTabsAttachmentDelegate

- (void)attachSelectedTabs:(ComposeboxTabPickerMediator*)tabPickerMediator
       selectedWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs {
  [self.delegate attachSelectedTabsWithWebStateIDs:selectedWebStateIDs];
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

- (std::set<web::WebStateID>)preselectedWebStateIDs {
  return [self.delegate webStateIDsForAttachedTabs];
}

@end

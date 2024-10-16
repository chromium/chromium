// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_coordinator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_view_controller.h"

@implementation RecentActivityCoordinator {
  // A mediator of the recent activity.
  RecentActivityMediator* _mediator;
  // A view controller of the recent activity.
  RecentActivityViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[RecentActivityViewController alloc] init];

  _mediator = [[RecentActivityMediator alloc] init];
  _mediator.consumer = _viewController;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  UISheetPresentationController* sheetPresentationController =
      navigationController.sheetPresentationController;
  sheetPresentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
      YES;
  sheetPresentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
  ];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;
  if (_viewController) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    _viewController = nil;
  }
  [super stop];
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GridContainerViewController

- (void)setContainedViewController:(UIViewController*)viewController {
  if (_containedViewController) {
    [_containedViewController willMoveToParentViewController:nil];
    [_containedViewController.view removeFromSuperview];
    [_containedViewController removeFromParentViewController];
  }
  if (viewController) {
    [self addChildViewController:viewController];
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:viewController.view];
    AddSameConstraints(self.view, viewController.view);
    [viewController didMoveToParentViewController:self];
  }
  _containedViewController = viewController;
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_view_controller.h"

@implementation TabGroupsCoordinator {
  // Mediator for tab groups.
  TabGroupsMediator* _mediator;
  // View controller for tab groups.
  TabGroupsViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group coordinator outside the "
         "Tab Groups experiment.";
  return [super initWithBaseViewController:viewController browser:browser];
}

- (void)start {
  _mediator = [[TabGroupsMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  _viewController = [[TabGroupsViewController alloc] init];
}

- (void)stop {
  _mediator = nil;
  _viewController = nil;
}

@end

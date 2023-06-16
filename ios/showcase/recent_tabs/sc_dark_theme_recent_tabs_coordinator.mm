// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/recent_tabs/sc_dark_theme_recent_tabs_coordinator.h"

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCDarkThemeRecentTabsCoordinator ()
@property(nonatomic, strong) RecentTabsTableViewController* viewController;
@end

@implementation SCDarkThemeRecentTabsCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[RecentTabsTableViewController alloc] init];
  self.viewController.title = @"Dark Theme Recent Tabs";
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

- (void)stop {
  [self.viewController dismissModals];
}

@end

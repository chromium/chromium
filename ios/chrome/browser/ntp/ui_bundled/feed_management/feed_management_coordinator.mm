// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_management/feed_management_coordinator.h"

#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_management/feed_management_view_controller.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"

@interface FeedManagementCoordinator ()

// The navigation controller into which management UI will be placed. This is a
// weak reference because we don't want to keep it in memory if it has been
// dismissed.
@property(nonatomic, weak) TableViewNavigationController* navigationController;

@end

@implementation FeedManagementCoordinator

- (void)start {
  FeedManagementViewController* feedManagementViewController =
      [[FeedManagementViewController alloc]
          initWithStyle:UITableViewStyleInsetGrouped];
  feedManagementViewController.navigationDelegate = self.navigationDelegate;
  TableViewNavigationController* navigationController =
      [[TableViewNavigationController alloc]
          initWithTable:feedManagementViewController];
  self.navigationController = navigationController;
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
  self.navigationController = nil;
}

@end

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_coordinator.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

@interface FeedManagementCoordinator () <FeedManagementFollowDelegate>

// The navigation controller into which management UI will be placed. This is a
// weak reference because we don't want to keep it in memory if it has been
// dismissed.
@property(nonatomic, weak) TableViewNavigationController* navigationController;

// The mediator for the follow management UI.
@property(nonatomic, strong) FollowManagementMediator* followManagementMediator;

@end

@implementation FeedManagementCoordinator

- (void)start {
  FeedManagementViewController* feedManagementViewController =
      [[FeedManagementViewController alloc]
          initWithStyle:UITableViewStyleInsetGrouped];
  feedManagementViewController.followDelegate = self;
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

  [self.followManagementMediator detach];
  self.followManagementMediator = nil;
}

#pragma mark - FeedManagementFollowDelegate

- (void)handleFollowingTapped {
  if (!self.navigationController) {
    // Tapping on the done button and following button simultaneously may result
    // in the navigation controller being dismissed but the tap being
    // registered. In that case, do nothing since the navigation controller has
    // already been dismissed.
    return;
  }

  [self.feedMetricsRecorder recordHeaderMenuManageFollowingTapped];

  if (!self.followManagementMediator) {
    self.followManagementMediator =
        [[FollowManagementMediator alloc] initWithBrowser:self.browser];
  }

  FollowManagementViewController* followManagementViewController =
      [[FollowManagementViewController alloc]
          initWithStyle:UITableViewStyleInsetGrouped];
  followManagementViewController.followedWebChannelsDataSource =
      self.followManagementMediator;
  followManagementViewController.faviconDataSource =
      self.followManagementMediator;
  followManagementViewController.navigationDelegate = self.navigationDelegate;
  followManagementViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  followManagementViewController.followDelegate = self.followManagementMediator;
  [self.followManagementMediator
      addFollowManagementUIUpdater:followManagementViewController];

  [self.navigationController pushViewController:followManagementViewController
                                       animated:YES];
}

#pragma mark - FollowManagementViewDelegate

- (void)followManagementViewControllerWillDismiss:
    (FollowManagementViewController*)viewController {
  [self.followManagementMediator
      removeFollowManagementUIUpdater:viewController];
}

@end

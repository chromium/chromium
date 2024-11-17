// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_coordinator.h"

#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_view_controller.h"

@implementation RecentActivityCoordinator {
  // A mediator of the recent activity.
  RecentActivityMediator* _mediator;
  // A view controller of the recent activity.
  RecentActivityViewController* _viewController;
  // A shared tab group currently displayed.
  base::WeakPtr<const TabGroup> _tabGroup;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  tabGroup:
                                      (base::WeakPtr<const TabGroup>)tabGroup {
  CHECK(IsSharedTabGroupsJoinEnabled(browser->GetProfile()));
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[RecentActivityViewController alloc] init];

  ProfileIOS* profile = self.browser->GetProfile();
  _mediator = [[RecentActivityMediator alloc]
      initWithtabGroup:_tabGroup
      messagingService:collaboration::messaging::
                           MessagingBackendServiceFactory::GetForProfile(
                               profile)
         faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(profile)
           syncService:tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                           profile)];
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

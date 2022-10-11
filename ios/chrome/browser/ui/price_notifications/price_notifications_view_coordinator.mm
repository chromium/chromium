// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PriceNotificationsViewCoordinator ()

// The navigation controller displaying self.tableViewController.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
// The view controller used to display price notifications.
@property(nonatomic, strong)
    PriceNotificationsTableViewController* tableViewController;

@end

@implementation PriceNotificationsViewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  self.tableViewController = [[PriceNotificationsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];

  // Add the "Done" button and hook it up to stop.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  self.tableViewController.navigationItem.rightBarButtonItem = dismissButton;

  self.navigationController = [[TableViewNavigationController alloc]
      initWithTable:self.tableViewController];

  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];

  self.navigationController.navigationBar.prefersLargeTitles = NO;

  if (@available(iOS 15, *)) {
    UISheetPresentationController* sheetPresentationController =
        self.navigationController.sheetPresentationController;
    if (sheetPresentationController) {
      sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
      sheetPresentationController
          .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

      sheetPresentationController.detents = @[
        [UISheetPresentationControllerDetent mediumDetent],
        [UISheetPresentationControllerDetent largeDetent]
      ];
    }
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  self.navigationController = nil;

  [super stop];
}

#pragma mark - Private

- (void)dismissButtonTapped {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      PriceNotificationsCommands) hidePriceNotifications];
}

@end

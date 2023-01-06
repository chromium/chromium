// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"

#import "base/check.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

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
// The mediator being managed by this coordinator.
@property(nonatomic, strong) PriceNotificationsPriceTrackingMediator* mediator;

@end

@implementation PriceNotificationsViewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  self.tableViewController = [[PriceNotificationsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];

  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.browser->GetBrowserState()->GetSharedURLLoaderFactory());
  self.mediator = [[PriceNotificationsPriceTrackingMediator alloc]
      initWithShoppingService:shoppingService
                bookmarkModel:bookmarkModel
                 imageFetcher:std::move(imageFetcher)
                     webState:webState];
  self.mediator.consumer = self.tableViewController;
  self.tableViewController.mutator = self.mediator;
  self.tableViewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);

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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

@implementation PriceNotificationsViewCoordinator {
  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.tableViewController = [[PriceNotificationsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  PrefService* prefService = self.browser->GetBrowserState()->GetPrefs();
  self.tableViewController.hasPreviouslyViewed =
      prefService->GetBoolean(prefs::kPriceNotificationsHasBeenShown);
  if (!self.tableViewController.hasPreviouslyViewed) {
    prefService->SetBoolean(prefs::kPriceNotificationsHasBeenShown, true);
  }

  base::FilePath path = self.browser->GetBrowserState()->GetStatePath();
  BrowserStateInfoCache* infoCache = GetApplicationContext()
                                         ->GetChromeBrowserStateManager()
                                         ->GetBrowserStateInfoCache();
  size_t browserStateIndex = infoCache->GetIndexOfBrowserStateWithPath(path);
  NSString* gaiaID = base::SysUTF8ToNSString(
      infoCache->GetGAIAIdOfBrowserStateAtIndex(browserStateIndex));
  PushNotificationService* pushNotificationService =
      GetApplicationContext()->GetPushNotificationService();
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
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
                     webState:webState
      pushNotificationService:pushNotificationService];
  self.mediator.consumer = self.tableViewController;
  self.mediator.presenter = self;
  self.mediator.handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PriceNotificationsCommands);
  self.mediator.bookmarksHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BookmarksCommands);
  self.mediator.gaiaID = gaiaID;

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

      sheetPresentationController.detents =
          ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
              ? @[ [UISheetPresentationControllerDetent largeDetent] ]
              : @[
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
  [self dismissAlertCoordinator];

  [super stop];
}

#pragma mark - PriceNotificationsAlertPresenter

- (void)presentPushNotificationPermissionAlert {
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }
  __weak PriceNotificationsViewCoordinator* weakSelf = self;

  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_REDIRECT);

  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.tableViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator
      addItemWithTitle:settingsTitle
                action:^{
                  [[UIApplication sharedApplication]
                                openURL:[NSURL URLWithString:settingURL]
                                options:{}
                      completionHandler:nil];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

- (void)presentStartPriceTrackingErrorAlertForItem:
    (PriceNotificationsTableViewItem*)item {
  __weak PriceNotificationsViewCoordinator* weakSelf = self;
  __weak PriceNotificationsPriceTrackingMediator* weakMediator = self.mediator;
  __weak PriceNotificationsTableViewController* weakController =
      self.tableViewController;
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_SUBSCRIBE_ERROR_ALERT_DESCRIPTION);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* tryAgainTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_REATTEMPT);

  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.tableViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakController resetPriceTrackingItem:item];
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:tryAgainTitle
                               action:^{
                                 [weakMediator trackItem:item];
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

- (void)presentStopPriceTrackingErrorAlertForItem:
    (PriceNotificationsTableViewItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakMediator = self.mediator;
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_UNSUBSCRIBE_ERROR_ALERT_DESCRIPTION);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* tryAgainTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_REATTEMPT);

  __weak PriceNotificationsViewCoordinator* weakSelf = self;
  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.tableViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:tryAgainTitle
                               action:^{
                                 [weakMediator stopTrackingItem:item];
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

#pragma mark - Private

- (void)dismissButtonTapped {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      PriceNotificationsCommands) hidePriceNotifications];
}

- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end

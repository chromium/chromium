// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the gaia id used for `profile`.
NSString* GetGaiaIdForProfile(ProfileIOS* profile) {
  const ProfileAttributesIOS attributes =
      GetApplicationContext()
          ->GetProfileManager()
          ->GetProfileAttributesStorage()
          ->GetAttributesForProfileWithName(profile->GetProfileName());

  return base::SysUTF8ToNSString(attributes.GetGaiaId());
}

}  // namespace

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
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
  self.tableViewController.hasPreviouslyViewed =
      prefService->GetBoolean(prefs::kPriceNotificationsHasBeenShown);
  if (!self.tableViewController.hasPreviouslyViewed) {
    prefService->SetBoolean(prefs::kPriceNotificationsHasBeenShown, true);
  }

  NSString* gaiaID = GetGaiaIdForProfile(self.browser->GetProfile());
  PushNotificationService* pushNotificationService =
      GetApplicationContext()->GetPushNotificationService();
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForProfile(
          self.browser->GetProfile());
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(self.browser->GetProfile());
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.browser->GetProfile()->GetSharedURLLoaderFactory());
  self.mediator = [[PriceNotificationsPriceTrackingMediator alloc]
      initWithShoppingService:shoppingService
                bookmarkModel:bookmarkModel
                 imageFetcher:std::move(imageFetcher)
                     webState:webState->GetWeakPtr()
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
  base::RecordAction(
      base::UserMetricsAction("Commerce.PriceTracking.IOS.Track.Failure"));
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
  base::RecordAction(
      base::UserMetricsAction("Commerce.PriceTracking.IOS.Untrack.Failure"));
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

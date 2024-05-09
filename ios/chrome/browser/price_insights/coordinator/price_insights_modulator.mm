// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"

#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PriceInsightsModulator ()

// The mediator to track/untrack a page and open the buying options URL in a new
// tab.
@property(nonatomic, strong) PriceNotificationsPriceTrackingMediator* mediator;
// A weak reference to a PriceInsightsCell.
@property(nonatomic, weak) PriceInsightsCell* priceInsightsCell;

@end

@implementation PriceInsightsModulator {
  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;
}

#pragma mark - Public

- (void)start {
  PushNotificationService* pushNotificationService =
      GetApplicationContext()->GetPushNotificationService();
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.browser->GetBrowserState()->GetSharedURLLoaderFactory());
  self.mediator = [[PriceNotificationsPriceTrackingMediator alloc]
      initWithShoppingService:shoppingService
                 imageFetcher:std::move(imageFetcher)
                     webState:webState
      pushNotificationService:pushNotificationService];
}

- (void)stop {
  self.mediator = nil;
  [self dismissAlertCoordinator];
}

- (UICollectionViewCellRegistration*)cellRegistration {
  __weak __typeof(self) weakSelf = self;
  auto handler =
      ^(PriceInsightsCell* cell, NSIndexPath* indexPath, id identifier) {
        weakSelf.priceInsightsCell = cell;
        [weakSelf configureCell:cell];
      };
  return [UICollectionViewCellRegistration
      registrationWithCellClass:[PriceInsightsCell class]
           configurationHandler:handler];
}

- (PanelBlockData*)panelBlockData {
  return [[PanelBlockData alloc] initWithBlockType:[self blockType]
                                  cellRegistration:[self cellRegistration]];
}

#pragma mark - PriceInsightsConsumer

- (void)didStartPriceTracking {
  [self.priceInsightsCell updateTrackButton:YES];
}

- (void)didStopPriceTracking {
  [self.priceInsightsCell updateTrackButton:NO];
}

- (void)didStartNavigationToWebpage {
}

- (void)presentPushNotificationPermissionAlert {
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }

  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_MESSAGE);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_REDIRECT);

  __weak PriceInsightsModulator* weakSelf = self;
  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
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

- (void)presentStartPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_SUBSCRIBE_ERROR_ALERT_DESCRIPTION);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* tryAgainTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_REATTEMPT);

  __weak PriceInsightsModulator* weakSelf = self;
  __weak PriceNotificationsPriceTrackingMediator* weakMediator = self.mediator;
  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
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
                                 [weakMediator priceInsightsTrackItem:item];
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

- (void)presentStopPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  __weak PriceNotificationsPriceTrackingMediator* weakMediator = self.mediator;
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_UNSUBSCRIBE_ERROR_ALERT_DESCRIPTION);
  NSString* cancelTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CANCEL);
  NSString* tryAgainTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_ERROR_ALERT_REATTEMPT);

  __weak PriceInsightsModulator* weakSelf = self;
  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  [_alertCoordinator addItemWithTitle:cancelTitle
                               action:^{
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator
      addItemWithTitle:tryAgainTitle
                action:^{
                  [weakMediator priceInsightsStopTrackingItem:item];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

#pragma mark - private

// Cell configuration handler helper.
- (void)configureCell:(PriceInsightsCell*)cell {
  cell.viewController = self.baseViewController;
  cell.mutator = self.mediator;
  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  [cell configureWithItem:item];
}

- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end

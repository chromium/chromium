// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"

#import "base/i18n/number_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/payments/core/currency_formatter.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* getFormattedCurrentPrice(int64_t amount_micro,
                                   std::string currency_code,
                                   std::string country_code) {
  float price = static_cast<float>(amount_micro) /
                static_cast<float>(commerce::kToMicroCurrency);
  payments::CurrencyFormatter formatter(currency_code, country_code);
  formatter.SetMaxFractionalDigits(2);
  return base::SysUTF16ToNSString(
      formatter.Format(base::NumberToString(price)));
}

NSDate* getNSDateFromString(std::string date) {
  NSDateFormatter* date_format = [[NSDateFormatter alloc] init];
  [date_format setDateFormat:@"yyyy-MM-dd"];
  NSDate* formated_date =
      [date_format dateFromString:base::SysUTF8ToNSString(date)];
  NSCalendar* calendar = [NSCalendar currentCalendar];
  [calendar setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  NSDate* midnight_date = [calendar startOfDayForDate:formated_date];
  return midnight_date;
}

}  // namespace

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
  [cell configureWithItem:[self getPriceInsightsItemFromConfig]];
}

- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

- (PriceInsightsItem*)getPriceInsightsItemFromConfig {
  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          self.itemConfiguration.get());
  DCHECK(config->product_info.has_value());

  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  item.title = base::SysUTF8ToNSString(config->product_info->title);
  item.variants =
      base::SysUTF8ToNSString(config->product_info->product_cluster_title);
  item.currency = base::SysUTF8ToNSString(config->product_info->currency_code);
  item.canPriceTrack = config->can_price_track;
  item.isPriceTracked = config->is_subscribed;
  item.productURL =
      self.browser->GetWebStateList()->GetActiveWebState()->GetVisibleURL();

  if (!config->price_insights_info.has_value()) {
    return item;
  }

  std::string currencyCode = config->product_info->currency_code;
  std::string countryCode = config->product_info->country_code;
  if (config->price_insights_info->typical_low_price_micros.has_value()) {
    int64_t amountMicro =
        config->price_insights_info->typical_low_price_micros.value();
    item.lowPrice =
        getFormattedCurrentPrice(amountMicro, currencyCode, countryCode);
  }

  if (config->price_insights_info->typical_high_price_micros.has_value()) {
    int64_t amountMicro =
        config->price_insights_info->typical_high_price_micros.value();
    item.highPrice =
        getFormattedCurrentPrice(amountMicro, currencyCode, countryCode);
  }

  NSMutableDictionary* priceHistory = [[NSMutableDictionary alloc] init];
  for (std::tuple<std::string, int64_t> history :
       config->price_insights_info->catalog_history_prices) {
    NSDate* date = getNSDateFromString(std::get<0>(history));
    float amount = static_cast<float>(std::get<1>(history)) /
                   static_cast<float>(commerce::kToMicroCurrency);
    priceHistory[date] = @(amount);
  }
  item.priceHistory = priceHistory;
  item.buyingOptionsURL = config->price_insights_info->jackpot_url.has_value()
                              ? config->price_insights_info->jackpot_url.value()
                              : GURL();
  return item;
}

@end

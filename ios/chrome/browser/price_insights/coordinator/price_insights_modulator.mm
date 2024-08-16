// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"

#import "base/i18n/number_formatting.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/payments/core/currency_formatter.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_price_tracking_mediator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The histogram used to record the current price bucket of the product when the
// user clicks on buying options.
const char kPriceInsightsBuyingOptionsClicked[] =
    "Commerce.PriceInsights.BuyingOptionsClicked";

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
// The service responsible for interacting with commerce's price data
// infrastructure.
@property(nonatomic, assign) commerce::ShoppingService* shoppingService;

@end

@implementation PriceInsightsModulator {
  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;
}

#pragma mark - Public

- (void)start {
  PushNotificationService* pushNotificationService =
      GetApplicationContext()->GetPushNotificationService();
  self.shoppingService = commerce::ShoppingServiceFactory::GetForBrowserState(
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
      initWithShoppingService:self.shoppingService
                bookmarkModel:bookmarkModel
                 imageFetcher:std::move(imageFetcher)
                     webState:webState
      pushNotificationService:pushNotificationService];
  self.mediator.priceInsightsConsumer = self;
}

- (void)stop {
  self.mediator = nil;
  self.shoppingService = nil;
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

- (void)didStartPriceTrackingWithNotification:(BOOL)granted {
  [self.priceInsightsCell updateTrackStatus:YES];
  [self displaySnackbar:granted];
}

- (void)didStopPriceTracking {
  [self.priceInsightsCell updateTrackStatus:NO];
}

- (void)didStartNavigationToWebpageWithPriceBucket:
    (commerce::PriceBucket)bucket {
  base::UmaHistogramEnumeration(kPriceInsightsBuyingOptionsClicked, bucket);
}

- (void)presentPushNotificationPermissionAlertForItem:(PriceInsightsItem*)item {
  NSString* alertTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_INSIGHTS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_TITLE);
  NSString* alertMessage = l10n_util::GetNSString(
      IDS_IOS_PRICE_INSIGHTS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_MESSAGE);
  NSString* closeTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_INSIGHTS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_CLOSE);
  NSString* settingsTitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_INSIGHTS_PRICE_TRACK_PERMISSION_REDIRECT_ALERT_REDIRECT);

  __weak PriceInsightsModulator* weakSelf = self;
  [_alertCoordinator stop];
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:alertTitle
                         message:alertMessage];
  [_alertCoordinator addItemWithTitle:closeTitle
                               action:^{
                                 [weakSelf onPushNotificationCancel:item];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:settingsTitle
                               action:^{
                                 [weakSelf onPushNotificationSettings:item];
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
                                 [weakSelf onStartTrackingRetryForItem:item];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

- (void)presentStopPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
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
                  [weakSelf onStopPriceTrackingRetryForItem:item];
                }
                 style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

#pragma mark - private

// Cell configuration handler helper.
- (void)configureCell:(PriceInsightsCell*)cell {
  cell.viewController = self.baseViewController;
  cell.mutator = self.mediator;
  [cell configureWithItem:[self priceInsightsItemFromConfig]];
}

// Dismisses and removes the current alert coordinator.
- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

/// Creates a PriceInsightsItem object from the current item configuration.
- (PriceInsightsItem*)priceInsightsItemFromConfig {
  PriceInsightsItemConfiguration* config =
      static_cast<PriceInsightsItemConfiguration*>(
          self.itemConfiguration.get());
  DCHECK(config->product_info.has_value());

  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  std::string product_title =
      config->product_info->product_cluster_title.empty()
          ? config->product_info->title
          : config->product_info->product_cluster_title;
  item.title = base::SysUTF8ToNSString(product_title);
  item.currency = config->product_info->currency_code;
  item.country = config->product_info->country_code;
  item.canPriceTrack = config->can_price_track;
  item.productURL =
      self.browser->GetWebStateList()->GetActiveWebState()->GetVisibleURL();

  if (item.canPriceTrack &&
      config->product_info->product_cluster_id.has_value()) {
    item.clusterId = config->product_info->product_cluster_id.value();
    // TODO: b/355423868 - Use the async version of IsSubscribed.
    item.isPriceTracked = self.shoppingService->IsSubscribedFromCache(
        commerce::BuildUserSubscriptionForClusterId(item.clusterId));
  }

  if (!config->price_insights_info.has_value()) {
    return item;
  }

  if (config->price_insights_info->has_multiple_catalogs &&
      config->price_insights_info->catalog_attributes.has_value()) {
    item.variants = base::SysUTF8ToNSString(
        config->price_insights_info->catalog_attributes.value());
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

// Displays a snackbar message to inform the user about the successful price
// tracking, with a button to open the price tracking UI.
- (void)displaySnackbar:(BOOL)granted {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  __weak id<PriceNotificationsCommands> weakPriceNotificationsHandler =
      HandlerForProtocol(dispatcher, PriceNotificationsCommands);
  __weak id<ContextualSheetCommands> weakContextualSheetHandler =
      HandlerForProtocol(dispatcher, ContextualSheetCommands);
  [snackbarHandler
      showSnackbarWithMessage:
          l10n_util::GetNSString(
              granted
                  ? IDS_PRICE_INSIGHTS_SNACKBAR_MESSAGE_TITLE_NOTIFICATION_ENABLED
                  : IDS_PRICE_INSIGHTS_SNACKBAR_MESSAGE_TITLE_NOTIFICATION_DISABLED)
                   buttonText:l10n_util::GetNSString(
                                  IDS_PRICE_INSIGHTS_SNACKBAR_BUTTON_TITLE)
                messageAction:^{
                  base::RecordAction(
                      base::UserMetricsAction("MobileMenuPriceNotifications"));
                  base::UmaHistogramEnumeration(
                      "IOS.ContextualPanel.DismissedReason",
                      ContextualPanelDismissedReason::BlockInteraction);
                  [weakContextualSheetHandler closeContextualSheet];
                  [weakPriceNotificationsHandler showPriceNotifications];
                }
             completionAction:nil];
}

// Callback invoked when the user chooses to retry stopping price tracking after
// an initial error.
- (void)onStopPriceTrackingRetryForItem:(PriceInsightsItem*)item {
  [self.mediator priceInsightsStopTrackingItem:item];
  [self dismissAlertCoordinator];
}

// Callback is invoked when the user chooses to retry starting price tracking
// after an initial error.
- (void)onStartTrackingRetryForItem:(PriceInsightsItem*)item {
  [self.mediator tryPriceInsightsTrackItem:item];
  [self dismissAlertCoordinator];
}

// Callback invoked when the user chooses to close push notifications prompt
// during.
- (void)onPushNotificationCancel:(PriceInsightsItem*)item {
  [self.mediator priceInsightsTrackItem:item notificationsGranted:NO];
  [self dismissAlertCoordinator];
}

// Callback invoked when the user chooses to open settings.
- (void)onPushNotificationSettings:(PriceInsightsItem*)item {
  NSString* settingURL = UIApplicationOpenSettingsURLString;
  if (@available(iOS 15.4, *)) {
    settingURL = UIApplicationOpenNotificationSettingsURLString;
  }

  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:settingURL]
                                     options:{}
                           completionHandler:nil];
  [self.mediator priceInsightsTrackItem:item notificationsGranted:NO];
  [self dismissAlertCoordinator];
}

@end

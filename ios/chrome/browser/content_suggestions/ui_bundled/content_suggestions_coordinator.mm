// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_coordinator.h"

#import <string_view>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service_factory.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/coordinator/app_bundle_promo_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/coordinator/default_browser_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/public/features.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service_factory.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_collection_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_ranking_model.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_constant.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/coordinator/price_tracking_promo_action_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/coordinator/price_tracking_promo_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/coordinator/safety_check_magic_stack_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/send_tab_to_self/coordinator/send_tab_promo_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/coordinator/set_up_list_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/coordinator/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/coordinator/set_up_list_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_item_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_show_more_view_controller.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_tap_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/coordinator/shop_card_action_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/coordinator/shop_card_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/coordinator/shortcuts_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/coordinator/tab_resumption_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/coordinator/tips_magic_stack_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/coordinator/tips_passwords_coordinator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/model/tips_metrics.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_module_state.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/model/large_icon_cache.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_histograms.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp_tiles/model/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/push_notification/ui_bundled/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/price_tracked_items_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Logs the user's decision to opt-in or opt-out of Safety Check notifications
// from the Magic Stack. Determines the source based on the `viaContextMenu`
// flag: if `true`, the action is logged as originating from the long-press
// menu; otherwise, it is logged as originating from the top-right action
// button.
void LogSafetyCheckNotificationOptIn(bool viaContextMenu) {
  if (viaContextMenu) {
    LogSafetyCheckNotificationOptInSource(
        SafetyCheckNotificationsOptInSource::kMagicStackLongPressMenuOptIn,
        SafetyCheckNotificationsOptInSource::kMagicStackLongPressMenuOptOut);
  } else {
    LogSafetyCheckNotificationOptInSource(
        SafetyCheckNotificationsOptInSource::
            kMagicStackTopRightActionButtonOptIn,
        SafetyCheckNotificationsOptInSource::
            kMagicStackTopRightActionButtonOptOut);
  }
}

}  // namespace

using segmentation_platform::TipIdentifier;

@interface ContentSuggestionsCoordinator () <
    ContentSuggestionsCommands,
    ContentSuggestionsViewControllerAudience,
    MagicStackCollectionViewControllerAudience,
    MagicStackModuleContainerDelegate,
    TipsPasswordsCoordinatorDelegate,
    NotificationsOptInAlertCoordinatorDelegate,
    PriceTrackingPromoActionDelegate,
    SetUpListDefaultBrowserPromoCoordinatorDelegate,
    SetUpListTapDelegate,
    ShopCardActionDelegate>

@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;
// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;
// The mediator used by this coordinator.
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;
// Metrics recorder for the content suggestions.
@property(nonatomic, strong)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;
@property(nonatomic, strong) SetUpListMediator* setUpListMediator;

@end

@implementation ContentSuggestionsCoordinator {
  // The coordinator that displays the Default Browser Promo for the Set Up
  // List.
  SetUpListDefaultBrowserPromoCoordinator* _defaultBrowserPromoCoordinator;

  // The Show More Menu presented from the Set Up List in the Magic Stack.
  SetUpListShowMoreViewController* _setUpListShowMoreViewController;

  // The coordinator for displaying tips related to Passwords.
  TipsPasswordsCoordinator* _tipsPasswordsCoordinator;

  // Displays alert giving the user the option to turn notifications
  // on for the app. This is for the third opt in flow where notifications
  // have previously been turned off.
  AlertCoordinator* _priceTrackingPromoAlertCoordinator;

  // The coordinator used to present an alert to enable Tips notifications.
  NotificationsOptInAlertCoordinator* _notificationsOptInAlertCoordinator;

  MagicStackRankingModel* _magicStackRankingModel;

  // Module mediators.
  ShortcutsMediator* _shortcutsMediator;
  SafetyCheckMagicStackMediator* _safetyCheckMediator;
  TipsMagicStackMediator* _tipsMediator;
  AppBundlePromoMediator* _appBundlePromoMediator;
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  TabResumptionMediator* _tabResumptionMediator;
  PriceTrackingPromoMediator* _priceTrackingPromoMediator;
  ShopCardMediator* _shopCardMediator;
  SendTabPromoMediator* _sendTabPromoMediator;
  SigninCoordinator* _signinCoordinator;
  MagicStackCollectionViewController* _magicStackCollectionView;
  DefaultBrowserMediator* _defaultBrowserMediator;

  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
}

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.NTPActionsDelegate);
  if (self.started) {
    // Prevent this coordinator from being started twice in a row.
    return;
  }
  _started = YES;

  ProfileIOS* profile = self.profile;
  PrefService* prefs = profile->GetPrefs();

  _segmentationService =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);

  self.authService = AuthenticationServiceFactory::GetForProfile(profile);

  // Conditionally register for provisional Safety Check notifications if the
  // feature is enabled.
  //
  // TODO(crbug.com/366182129): Move Safety Check provisional notification
  // enrollment to `SafetyCheckNotificationClient` once
  // `ProvisionalPushNotificationService` circular dependencies are fixed.
  if (IsSafetyCheckNotificationsEnabled() &&
      ProvisionalSafetyCheckNotificationsEnabled()) {
    if (ProvisionalPushNotificationService* service =
            ProvisionalPushNotificationServiceFactory::GetForProfile(profile)) {
      service->EnrollUserToProvisionalNotifications(
          ProvisionalPushNotificationService::ClientIdState::kDisabled,
          {PushNotificationClientId::kSafetyCheck});
    }
  }

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForProfile(profile);

  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForProfile(profile);

  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(profile);

  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForProfile(profile);

  self.contentSuggestionsMetricsRecorder =
      [[ContentSuggestionsMetricsRecorder alloc]
          initWithLocalState:GetApplicationContext()->GetLocalState()];

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);

  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForProfile(profile);

  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc] init];

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  NSMutableArray* moduleMediators = [NSMutableArray array];

  _mostVisitedTilesMediator = [[MostVisitedTilesMediator alloc]
      initWithMostVisitedSite:std::move(mostVisitedFactory)
                  prefService:prefs
             largeIconService:largeIconService
               largeIconCache:cache
       URLLoadingBrowserAgent:UrlLoadingBrowserAgent::FromBrowser(self.browser)
        accountManagerService:accountManagerService];
  _mostVisitedTilesMediator.contentSuggestionsDelegate = self.delegate;
  _mostVisitedTilesMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  _mostVisitedTilesMediator.actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:kMenuScenarioHistogramMostVisitedEntry];
  _mostVisitedTilesMediator.snackbarHandler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  _mostVisitedTilesMediator.NTPActionsDelegate = self.NTPActionsDelegate;
  [moduleMediators addObject:_mostVisitedTilesMediator];
  self.contentSuggestionsMediator.mostVisitedTilesMediator =
      _mostVisitedTilesMediator;

  _shortcutsMediator = [[ShortcutsMediator alloc]
      initWithReadingListModel:readingListModel
      featureEngagementTracker:feature_engagement::TrackerFactory::
                                   GetForProfile(profile)
               identityManager:identityManager];
  _shortcutsMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  _shortcutsMediator.NTPActionsDelegate = self.NTPActionsDelegate;
  _shortcutsMediator.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCoordinatorCommands, WhatsNewCommands>>(
      self.browser->GetCommandDispatcher());
  [moduleMediators addObject:_shortcutsMediator];
  self.contentSuggestionsMediator.shortcutsMediator = _shortcutsMediator;

  if (IsTabResumptionEnabled()) {
    _tabResumptionMediator = [[TabResumptionMediator alloc]
              initWithLocalState:GetApplicationContext()->GetLocalState()
                     prefService:prefs
                 identityManager:identityManager
                         browser:self.browser
        optimizationGuideService:OptimizationGuideServiceFactory::GetForProfile(
                                     profile)
          impressionLimitService:
              base::FeatureList::IsEnabled(commerce::kShopCardImpressionLimits)
                  ? ImpressionLimitServiceFactory::GetForProfile(profile)
                  : nil
                 shoppingService:commerce::ShoppingServiceFactory::
                                     GetForProfile(profile)
                   bookmarkModel:ios::BookmarkModelFactory::GetForProfile(
                                     profile)
         pushNotificationService:GetApplicationContext()
                                     ->GetPushNotificationService()
           authenticationService:self.authService];
    _tabResumptionMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    _tabResumptionMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    _tabResumptionMediator.dispatcher = static_cast<
        id<ApplicationCommands, PriceTrackedItemsCommands, SnackbarCommands>>(
        self.browser->GetCommandDispatcher());

    [moduleMediators addObject:_tabResumptionMediator];
  }
  if (IsPriceTrackingPromoCardEnabled(shoppingService, self.authService,
                                      prefs)) {
    _priceTrackingPromoMediator = [[PriceTrackingPromoMediator alloc]
        initWithShoppingService:commerce::ShoppingServiceFactory::GetForProfile(
                                    profile)
                  bookmarkModel:ios::BookmarkModelFactory::GetForProfile(
                                    profile)
                   imageFetcher:std::make_unique<
                                    image_fetcher::ImageDataFetcher>(
                                    profile->GetSharedURLLoaderFactory())
                    prefService:prefs
                     localState:GetApplicationContext()->GetLocalState()
        pushNotificationService:GetApplicationContext()
                                    ->GetPushNotificationService()
          authenticationService:self.authService
                  faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                    profile)];
    _priceTrackingPromoMediator.dispatcher =
        static_cast<id<ApplicationCommands, SnackbarCommands>>(
            self.browser->GetCommandDispatcher());
    _priceTrackingPromoMediator.actionDelegate = self;
    _priceTrackingPromoMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    [moduleMediators addObject:_priceTrackingPromoMediator];
  }
  // Only users that are eligible for ShoppingList are eligible for the
  // ShopCard.
  if (shoppingService->IsShoppingListEligible()) {
    _shopCardMediator = [[ShopCardMediator alloc]
        initWithShoppingService:commerce::ShoppingServiceFactory::GetForProfile(
                                    profile)
                    prefService:prefs
                  bookmarkModel:ios::BookmarkModelFactory::GetForProfile(
                                    profile)
                   imageFetcher:std::make_unique<
                                    image_fetcher::ImageDataFetcher>(
                                    profile->GetSharedURLLoaderFactory())
                  faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                    profile)
         impressionLimitService:ImpressionLimitServiceFactory::GetForProfile(
                                    profile)];
    _shopCardMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    _shopCardMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    [moduleMediators addObject:_shopCardMediator];
    _shopCardMediator.shopCardActionDelegate = self;
  }

    IOSChromeSafetyCheckManager* safetyCheckManager =
        IOSChromeSafetyCheckManagerFactory::GetForProfile(profile);
    _safetyCheckMediator = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:safetyCheckManager
                        localState:GetApplicationContext()->GetLocalState()
                         userState:prefs
                      profileState:self.browser->GetSceneState().profileState];
    _safetyCheckMediator.presentationAudience = self;
    [moduleMediators addObject:_safetyCheckMediator];

  if (send_tab_to_self::
          IsSendTabIOSPushNotificationsEnabledWithMagicStackCard()) {
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile);

    _sendTabPromoMediator =
        [[SendTabPromoMediator alloc] initWithFaviconLoader:faviconLoader
                                                prefService:prefs];
    _sendTabPromoMediator.notificationsDelegate = self;
    [moduleMediators addObject:_sendTabPromoMediator];
  }

  BOOL areTipsCardsEnabled =
      prefs->GetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled);

  if (IsTipsMagicStackEnabled() && areTipsCardsEnabled) {
    _tipsMediator = [[TipsMagicStackMediator alloc]
        initWithIdentifier:TipIdentifier::kUnknown
        profilePrefService:prefs
           shoppingService:commerce::ShoppingServiceFactory::GetForProfile(
                               profile)
             bookmarkModel:ios::BookmarkModelFactory::GetForProfile(profile)
              imageFetcher:std::make_unique<image_fetcher::ImageDataFetcher>(
                               profile->GetSharedURLLoaderFactory())];
    _tipsMediator.presentationAudience = self;
    [moduleMediators addObject:_tipsMediator];
  }

  if (segmentation_platform::features::IsAppBundlePromoEphemeralCardEnabled() &&
      areTipsCardsEnabled) {
    _appBundlePromoMediator = [[AppBundlePromoMediator alloc]
        initWithAppStoreBundleService:AppStoreBundleServiceFactory::
                                          GetForProfile(self.profile)
                   profilePrefService:prefs];
    _appBundlePromoMediator.presentationAudience = self;
    [moduleMediators addObject:_appBundlePromoMediator];
  }
  if (segmentation_platform::features::IsDefaultBrowserMagicStackEnabled() &&
      areTipsCardsEnabled) {
    _defaultBrowserMediator =
        [[DefaultBrowserMediator alloc] initWithProfilePrefService:prefs];
    _defaultBrowserMediator.presentationAudience = self;
    [moduleMediators addObject:_defaultBrowserMediator];
  }

  ContentSuggestionsViewController* viewController =
      [[ContentSuggestionsViewController alloc] init];
  viewController.audience = self;
  viewController.urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  viewController.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  self.contentSuggestionsViewController = viewController;

  BOOL isSetupListEnabled = set_up_list_utils::IsSetUpListActive(
      GetApplicationContext()->GetLocalState(), prefs);
  if (isSetupListEnabled) {
    const TemplateURL* defaultSearchURLTemplate =
        ios::TemplateURLServiceFactory::GetForProfile(profile)
            ->GetDefaultSearchProvider();
    BOOL isDefaultSearchEngine = defaultSearchURLTemplate &&
                                 defaultSearchURLTemplate->prepopulate_id() ==
                                     TemplateURLPrepopulateData::google.id;
    _setUpListMediator = [[SetUpListMediator alloc]
          initWithPrefService:prefs
              identityManager:identityManager
                   sceneState:self.browser->GetSceneState()
        isDefaultSearchEngine:isDefaultSearchEngine
         priceTrackingEnabled:IsPriceTrackingEnabled(self.profile)];
    _setUpListMediator.commandHandler = self;
    _setUpListMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    _setUpListMediator.delegate = self.delegate;
    self.contentSuggestionsMediator.setUpListMediator = _setUpListMediator;
    [moduleMediators addObject:_setUpListMediator];
  }
  _magicStackRankingModel = [[MagicStackRankingModel alloc]
      initWithSegmentationService:_segmentationService
                  shoppingService:commerce::ShoppingServiceFactory::
                                      GetForProfile(profile)
                      authService:self.authService
                      prefService:prefs
                       localState:GetApplicationContext()->GetLocalState()
                  moduleMediators:moduleMediators
                      tipsManager:TipsManagerIOSFactory::GetForProfile(
                                      self.profile)
               templateURLService:ios::TemplateURLServiceFactory::GetForProfile(
                                      self.profile)
            appStoreBundleService:AppStoreBundleServiceFactory::GetForProfile(
                                      self.profile)
                    bookmarkModel:ios::BookmarkModelFactory::GetForProfile(
                                      profile)];
  _magicStackRankingModel.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  self.contentSuggestionsMediator.magicStackRankingModel =
      _magicStackRankingModel;
  _magicStackRankingModel.delegate = self.contentSuggestionsMediator;
  _magicStackRankingModel.homeStartDataSource = self.homeStartDataSource;

  _magicStackCollectionView = [[MagicStackCollectionViewController alloc] init];
  _magicStackCollectionView.audience = self;
  _mostVisitedTilesMediator.consumer = self.contentSuggestionsViewController;

  self.contentSuggestionsMediator.magicStackConsumer =
      _magicStackCollectionView;
  self.contentSuggestionsMediator.consumer =
      self.contentSuggestionsViewController;
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ContentSuggestionsCommands)];
}

- (void)stop {
  _segmentationService = nullptr;
  [_shortcutsMediator disconnect];
  _shortcutsMediator = nil;
  [_safetyCheckMediator disconnect];
  _safetyCheckMediator = nil;
  [_sendTabPromoMediator disconnect];
  _sendTabPromoMediator = nil;
  [_tipsMediator disconnect];
  _tipsMediator = nil;
  [_setUpListMediator disconnect];
  _setUpListMediator = nil;
  [_mostVisitedTilesMediator disconnect];
  _mostVisitedTilesMediator = nil;
  [_tabResumptionMediator disconnect];
  _tabResumptionMediator = nil;
  [_magicStackRankingModel disconnect];
  _magicStackRankingModel = nil;
  [_appBundlePromoMediator disconnect];
  _appBundlePromoMediator = nil;
  [self.contentSuggestionsMediator disconnect];
  self.contentSuggestionsMediator = nil;
  [self.contentSuggestionsMetricsRecorder disconnect];
  self.contentSuggestionsMetricsRecorder = nil;
  self.contentSuggestionsViewController.audience = nil;
  self.contentSuggestionsViewController = nil;
  [self clearPresentedState];
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(ContentSuggestionsCommands)];
  _started = NO;
}

- (ContentSuggestionsViewController*)viewController {
  return self.contentSuggestionsViewController;
}

- (void)clearPresentedState {
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator = nil;
  [_notificationsOptInAlertCoordinator stop];
  _notificationsOptInAlertCoordinator = nil;
  [self stopSigninCoordinator];
}

#pragma mark - Public methods

- (void)refresh {
  [_magicStackCollectionView reset];
  // Refresh in case there are new MVT to show.
  [_mostVisitedTilesMediator refreshMostVisitedTiles];
  [_safetyCheckMediator reset];
  [_priceTrackingPromoMediator reset];
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  // Fetch after resetting ranking since subscriptions could be returned
  // synchronously.
  [_priceTrackingPromoMediator fetchLatestSubscription];
}

#pragma mark - ContentSuggestionsCommands

- (void)showSetUpListSeeMoreMenuExpanded:(BOOL)expanded {
  [_setUpListShowMoreViewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  NSArray<SetUpListItemViewData*>* items = [self.setUpListMediator allItems];

  _setUpListShowMoreViewController =
      [[SetUpListShowMoreViewController alloc] initWithItems:items
                                                 tapDelegate:self];

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:_setUpListShowMoreViewController];
  navController.modalPresentationStyle = UIModalPresentationPageSheet;

  UISheetPresentationController* presentationController =
      navController.sheetPresentationController;

  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  if (expanded) {
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }
  presentationController.preferredCornerRadius = 16;

  [_magicStackCollectionView presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)viewWillDisappear {
  DiscoverFeedServiceFactory::GetForProfile(self.profile)
      ->SetIsShownOnStartSurface(false);
}

- (void)didSelectTip:(segmentation_platform::TipIdentifier)tip {
  CHECK(IsTipsMagicStackEnabled());
  CHECK(_tipsMediator);

  __weak __typeof(self) weakSelf = self;

  ProceduralBlock completion = ^{
    [weakSelf openTipDestination:tip];
  };

  [_tipsMediator removeModuleWithCompletion:completion];
}

// Removes the App Bundle promo from the Magic Stack and opens the App Store
// page to install the Best of Google bundle.
- (void)didSelectAppBundlePromo {
  // Note: The promo modal only works when the `kAppBundlePromoEphemeralCard`
  // feature is enabled. If this card is forced in the
  // #ios-segmentation-ephemeral-card-ranker, tapping the card does NOT do
  // anything. This is because the creation of the AppStorePromoService is gated
  // behind the feature flag.
  CHECK(_appBundlePromoMediator);

  __weak __typeof(self) weakSelf = self;

  ProceduralBlock completion = ^{
    [weakSelf presentAppStoreBundlePage];
  };

  [_appBundlePromoMediator removeModuleWithCompletion:completion];
}

// Presents the Best of Google bundle install page in the App Store.
- (void)presentAppStoreBundlePage {
  [_appBundlePromoMediator
      presentAppStoreBundlePage:self.magicStackCollectionView
                 withCompletion:nil];
}

- (void)didTapDefaultBrowserPromo {
  DefaultBrowserMagicStackIosVariationType variation =
      GetDefaultBrowserMagicStackIosVariation();

  if (variation ==
      DefaultBrowserMagicStackIosVariationType::kTapToDeviceSettings) {
    OpenIOSDefaultBrowserSettingsPage();
  } else if (variation ==
             DefaultBrowserMagicStackIosVariationType::kTapToAppSettings) {
    id<SettingsCommands> settings_handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SettingsCommands);
    [settings_handler
        showDefaultBrowserSettingsFromViewController:nil
                                        sourceForUMA:
                                            DefaultBrowserSettingsPageSource::
                                                kMagicStackCard];
  }
}

- (void)openTipDestination:(segmentation_platform::TipIdentifier)tip {
  CHECK(IsTipsMagicStackEnabled());
  CHECK(_tipsMediator);

  // Log the Tips (Magic Stack) Module that the user tapped on.
  base::UmaHistogramEnumeration(kTipsMagicStackModuleTappedTypeHistogram, tip);
  switch (tip) {
    case TipIdentifier::kUnknown:
      NOTREACHED();
    case TipIdentifier::kLensShop:
    case TipIdentifier::kLensSearch:
    case TipIdentifier::kLensTranslate: {
      LensEntrypoint entryPoint = tip == TipIdentifier::kLensTranslate
                                      ? LensEntrypoint::TranslateOnebox
                                      : LensEntrypoint::NewTabPage;

      if (tip == TipIdentifier::kLensShop &&
          TipsLensShopExperimentTypeEnabled() ==
              TipsLensShopExperimentType::kWithProductImage &&
          _tipsMediator.state.productImageData.length > 0) {
        UIImage* productImage =
            [UIImage imageWithData:_tipsMediator.state.productImageData];

        if (productImage) {
          SearchImageWithLensCommand* command =
              [[SearchImageWithLensCommand alloc] initWithImage:productImage
                                                     entryPoint:entryPoint];

          [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                              LensCommands) searchImageWithLens:command];

          break;
        }
      }

      OpenLensInputSelectionCommand* command =
          [[OpenLensInputSelectionCommand alloc]
                  initWithEntryPoint:entryPoint
                   presentationStyle:LensInputSelectionPresentationStyle::
                                         SlideFromRight
              presentationCompletion:nil];

      command.presentNTPLensIconBubbleOnDismiss = YES;

      [HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands)
          openLensInputSelection:command];

      break;
    }
    case TipIdentifier::kAddressBarPosition:
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          BrowserCoordinatorCommands)
          showOmniboxPositionChoice];
      break;
    case TipIdentifier::kEnhancedSafeBrowsing: {
      if (TipsSafeBrowsingExperimentTypeEnabled() ==
          TipsSafeBrowsingExperimentType::kShowSafeBrowsingSettingsPage) {
        [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            SettingsCommands) showSafeBrowsingSettings];
      } else {
        [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            BrowserCoordinatorCommands)
            showEnhancedSafeBrowsingPromo];
      }

      break;
    }
    case TipIdentifier::kSavePasswords:
    case TipIdentifier::kAutofillPasswords: {
      _tipsPasswordsCoordinator = [[TipsPasswordsCoordinator alloc]
          initWithBaseViewController:self.magicStackCollectionView
                             browser:self.browser
                          identifier:_tipsMediator.state.identifier];

      _tipsPasswordsCoordinator.delegate = self;

      [_tipsPasswordsCoordinator start];

      break;
    }
  }

  [self.NTPActionsDelegate tipsOpened];

  std::optional<std::string_view> name = OutputLabelForTipIdentifier(tip);

  if (name.has_value()) {
    segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetHomeCardRegistryForProfile(self.profile);

    CHECK(registry);

    registry->NotifyCardInteracted(std::string(name.value()).c_str());
  }
}

#pragma mark - MagicStackCollectionViewAudience

- (void)didTapMagicStackEditButton {
  base::RecordAction(base::UserMetricsAction("IOSMagicStackSettingsOpened"));
  [self.delegate openMagicStackCustomizationMenu];
}

- (void)logEphemeralCardVisibility:(ContentSuggestionsModuleType)card {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackTopModuleImpressionHistogram, card);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeCardRegistryForProfile(self.profile);

  switch (card) {
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      registry->NotifyCardShown(
          segmentation_platform::kPriceTrackingNotificationPromo);
      break;
    case ContentSuggestionsModuleType::kSendTabPromo:
      registry->NotifyCardShown(
          segmentation_platform::kSendTabNotificationPromo);
      break;
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips: {
      CHECK(_tipsMediator);
      std::optional<std::string_view> name =
          OutputLabelForTipIdentifier(_tipsMediator.state.identifier);
      if (name.has_value()) {
        registry->NotifyCardShown(std::string(name.value()).c_str());
        // Log the Tips (Magic Stack) Module that was displayed to the user.
        base::UmaHistogramEnumeration(
            kTipsMagicStackModuleDisplayedTypeHistogram,
            _tipsMediator.state.identifier);
        break;
      }
      [[fallthrough]];
    }
    case ContentSuggestionsModuleType::kAppBundlePromo: {
      registry->NotifyCardShown(
          segmentation_platform::kAppBundlePromoEphemeralModule);
      UMA_HISTOGRAM_BOOLEAN(kAppBundlePromoImpression, true);
      break;
    }
    case ContentSuggestionsModuleType::kDefaultBrowser: {
      registry->NotifyCardShown(
          segmentation_platform::kDefaultBrowserPromoEphemeralModule);
      break;
    }
    default:
      NOTREACHED();
  }
}

- (void)logTopModuleImpressionForType:(ContentSuggestionsModuleType)moduleType {
  LogTopModuleImpressionForType(moduleType, self.profile->GetPrefs());
}

#pragma mark - MagicStackModuleContainerDelegate

- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type {
  [self.customizationDelegate dismissCustomizationMenu];
  switch (type) {
    case ContentSuggestionsModuleType::kSafetyCheck:
      [self didSelectSafetyCheckItem:SafetyCheckItemType::kDefault];
      break;
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [self showSetUpListSeeMoreMenuExpanded:NO];
      break;
    case ContentSuggestionsModuleType::kTabResumption:
      [self showMagicStackRecentTabs];
      break;
    case ContentSuggestionsModuleType::kShopCard: {
      id<PriceTrackedItemsCommands> priceNotificationsCommands =
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             PriceTrackedItemsCommands);
      [priceNotificationsCommands showPriceTrackedItems];
      break;
    }
    default:
      break;
  }
}

- (void)neverShowModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kMostVisited:
      [_mostVisitedTilesMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kTabResumption:
      [_tabResumptionMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kSafetyCheck:
      [_safetyCheckMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kPriceTrackingPromo: {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.MagicStackPromo.Hidden"));
      [_priceTrackingPromoMediator disableModule];
      break;
    }
    case ContentSuggestionsModuleType::kSendTabPromo: {
      // The Send Tab Promo has an impression limit of 1, so it's sufficient to
      // just hide the module.
      [_sendTabPromoMediator dismissModule];
      break;
    }
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
    case ContentSuggestionsModuleType::kAppBundlePromo:
    case ContentSuggestionsModuleType::kDefaultBrowser: {
      // Disable all cards with "Chrome Tips" header.
      [self disableTipsModules];
      break;
    }
    case ContentSuggestionsModuleType::kShopCard: {
      [_shopCardMediator disableModule];
      break;
    }
    default:
      break;
  }
}

// Returns the `PushNotificationClientId` associated with the specified `type`.
// Currently, push notifications are exclusively supported by the Set Up List
// and Safety Check modules.
- (PushNotificationClientId)pushNotificationClientId:
    (ContentSuggestionsModuleType)type {
  // This is only supported for Tips, Send Tab, and Safety Check
  // modules.
  CHECK(IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck ||
        type == ContentSuggestionsModuleType::kSendTabPromo);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return PushNotificationClientId::kSafetyCheck;
  }

  if (IsTipsModuleType(type)) {
    return PushNotificationClientId::kTips;
  }

  if (type == ContentSuggestionsModuleType::kSendTabPromo) {
    return PushNotificationClientId::kSendTab;
  }

  NOTREACHED();
}

// Retrieves the message ID for the push notification feature title associated
// with the specified `ContentSuggestionsModuleType`. Currently, push
// notifications are exclusively supported by the Set Up List, Send Tab, and
// Safety Check modules.
- (int)pushNotificationTitleMessageId:(ContentSuggestionsModuleType)type {
  // This is only supported for Tips, Send Tab, and Safety Check
  // modules.
  CHECK(IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck ||
        type == ContentSuggestionsModuleType::kSendTabPromo);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return IDS_IOS_SAFETY_CHECK_TITLE;
  }

  if (IsTipsModuleType(type)) {
    return IDS_IOS_MAGIC_STACK_TIP_TITLE;
  }

  if (type == ContentSuggestionsModuleType::kSendTabPromo) {
    return IDS_IOS_SEND_TAB_PROMO_FEATURE_NAME_FOR_SNACKBAR;
  }

  NOTREACHED();
}

- (NotificationOptInAccessPoint)convertModuleTypeToNotificationOptInAccessPoint:
    (ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kSafetyCheck:
      return NotificationOptInAccessPoint::kSafetyCheck;
    case ContentSuggestionsModuleType::kSendTabPromo:
      return NotificationOptInAccessPoint::kSendTabMagicStackPromo;
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
      return NotificationOptInAccessPoint::kSetUpList;
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
    case ContentSuggestionsModuleType::kAppBundlePromo:
    case ContentSuggestionsModuleType::kDefaultBrowser:
      return NotificationOptInAccessPoint::kTips;
    default:
      NOTREACHED();
  }
}

- (void)enableNotifications:(ContentSuggestionsModuleType)type
             viaContextMenu:(BOOL)viaContextMenu {
  // This is only supported for Tips, Send Tab, and Safety Check
  // modules.
  CHECK(IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck ||
        type == ContentSuggestionsModuleType::kSendTabPromo);

  LogSafetyCheckNotificationOptIn(viaContextMenu);

  // Ask user for permission to opt-in to notifications.
  [_notificationsOptInAlertCoordinator stop];

  _notificationsOptInAlertCoordinator =
      [[NotificationsOptInAlertCoordinator alloc]
          initWithBaseViewController:self.magicStackCollectionView
                             browser:self.browser];
  _notificationsOptInAlertCoordinator.accessPoint =
      [self convertModuleTypeToNotificationOptInAccessPoint:type];
  _notificationsOptInAlertCoordinator.delegate = self;

  const PushNotificationClientId clientId =
      [self pushNotificationClientId:type];

  _notificationsOptInAlertCoordinator.clientIds = std::vector{clientId};

  int featureTitle = [self pushNotificationTitleMessageId:type];

  _notificationsOptInAlertCoordinator.confirmationMessage =
      l10n_util::GetNSStringF(IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE,
                              l10n_util::GetStringUTF16(featureTitle));

  [_notificationsOptInAlertCoordinator start];
}

- (void)disableNotifications:(ContentSuggestionsModuleType)type
              viaContextMenu:(BOOL)viaContextMenu {
  // This is only supported for Tips, Send Tab, and Safety Check
  // modules.
  CHECK(IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck ||
        type == ContentSuggestionsModuleType::kSendTabPromo);

  LogSafetyCheckNotificationOptIn(viaContextMenu);

  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  const PushNotificationClientId clientId =
      [self pushNotificationClientId:type];

  GetApplicationContext()->GetPushNotificationService()->SetPreference(
      identity.gaiaId, clientId, false);

  // Show confirmation snackbar.
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_MANAGE_SETTINGS);

  int featureTitle = [self pushNotificationTitleMessageId:type];

  NSString* message =
      l10n_util::GetNSStringF(IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE_OFF,
                              l10n_util::GetStringUTF16(featureTitle));

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  __weak id<SettingsCommands> weakSettingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);

  [snackbarHandler showSnackbarWithMessage:message
                                buttonText:buttonText
                             messageAction:^{
                               [weakSettingsHandler showNotificationsSettings];
                             }
                          completionAction:nil];
}

- (void)customizeCardsWasTapped {
  [self didTapMagicStackEditButton];
}

#pragma mark - TipsPasswordsCoordinatorDelegate

- (void)tipsPasswordsCoordinatorDidFinish:
    (TipsPasswordsCoordinator*)coordinator {
  CHECK_EQ(coordinator, _tipsPasswordsCoordinator);
  [_tipsPasswordsCoordinator stop];
  _tipsPasswordsCoordinator = nil;
}

#pragma mark - SafetyCheckViewDelegate

// Called when a Safety Check item is selected by the user. Depending on the
// Safety Check item `type`, this method fires a UI command to present the
// Update Chrome page, Password Checkup, or Safety Check half sheet.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type {
  [self.NTPActionsDelegate safetyCheckOpened];
  Browser* browser = self.browser;
  [_magicStackRankingModel logMagicStackEngagementForType:
                               ContentSuggestionsModuleType::kSafetyCheck];

  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForProfile(browser->GetProfile());

  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);

  switch (type) {
    case SafetyCheckItemType::kUpdateChrome: {
      const GURL& chrome_upgrade_url =
          safetyCheckManager->GetChromeAppUpgradeUrl();
      HandleSafetyCheckUpdateChromeTap(chrome_upgrade_url, applicationHandler);
      break;
    }
    case SafetyCheckItemType::kPassword: {
      std::vector<password_manager::CredentialUIEntry> insecure_credentials =
          safetyCheckManager->GetInsecureCredentials();

      password_manager::InsecurePasswordCounts insecure_password_counts =
          safetyCheckManager->GetInsecurePasswordCounts();

      HandleSafetyCheckPasswordTap(insecure_credentials,
                                   insecure_password_counts, applicationHandler,
                                   settingsHandler);

      break;
    }
    case SafetyCheckItemType::kSafeBrowsing:
      [settingsHandler showSafeBrowsingSettings];
      break;
    case SafetyCheckItemType::kAllSafe:
    case SafetyCheckItemType::kRunning:
    case SafetyCheckItemType::kDefault:
      password_manager::PasswordCheckReferrer referrer =
          password_manager::PasswordCheckReferrer::kSafetyCheckMagicStack;
      [settingsHandler showAndStartSafetyCheckForReferrer:referrer];
      break;
  }
}

#pragma mark - SetUpListTapDelegate

- (void)didTapSetUpListItemView:(SetUpListItemView*)view {
  [self didSelectSetUpListItem:view.type];
}

- (void)didSelectSetUpListItem:(SetUpListItemType)type {
  if (set_up_list_utils::ShouldShowCompactedSetUpListModule()) {
    [_magicStackRankingModel
        logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                           kCompactedSetUpList];
  } else {
    [_magicStackRankingModel
        logMagicStackEngagementForType:SetUpListModuleTypeForSetUpListType(
                                           type)];
  }
  [self.contentSuggestionsMetricsRecorder recordSetUpListItemSelected:type];
  [self.NTPActionsDelegate setUpListItemOpened];
  PrefService* localState = GetApplicationContext()->GetLocalState();
  set_up_list_prefs::RecordInteraction(localState);

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  ProceduralBlock completionBlock = ^{
    [weakSelf showUIForSelectedSetUpListItem:type];
  };

  if (_setUpListShowMoreViewController) {
    [_setUpListShowMoreViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completionBlock];
    _setUpListShowMoreViewController = nil;
  } else {
    completionBlock();
  }
}

- (void)dismissSeeMoreViewController {
  DCHECK(_setUpListShowMoreViewController);
  [_setUpListShowMoreViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _setUpListShowMoreViewController = nil;
}

#pragma mark - SetUpList Helpers

// Displays the UI for the given SetUpListItemType.
- (void)showUIForSelectedSetUpListItem:(SetUpListItemType)type {
  switch (type) {
    case SetUpListItemType::kDefaultBrowser:
      [self showDefaultBrowserPromo];
      break;
    case SetUpListItemType::kAutofill:
      [self showCredentialProviderPromo];
      break;
    case SetUpListItemType::kNotifications:
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          BrowserCoordinatorCommands)
          showNotificationsOptInFromAccessPoint:NotificationOptInAccessPoint::
                                                    kSetUpList
                             baseViewController:self.magicStackCollectionView];
      break;
    case SetUpListItemType::kAllSet:
      NOTREACHED();
  }
}

// Shows the Default Browser Promo.
- (void)showDefaultBrowserPromo {
  // Stop the coordinator if it is already running. If the user swipes to
  // dismiss a previous instance and then clicks the item again the
  // previous instance may not have been stopped yet due to the animation.
  [_defaultBrowserPromoCoordinator stop];

  _defaultBrowserPromoCoordinator =
      [[SetUpListDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:self.magicStackCollectionView
                             browser:self.browser
                         application:[UIApplication sharedApplication]];
  _defaultBrowserPromoCoordinator.delegate = self;
  [_defaultBrowserPromoCoordinator start];
}

// Stops the SigninCoordinator.
- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

// Shows the Credential Provider Promo using the SetUpList trigger.
- (void)showCredentialProviderPromo {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      CredentialProviderPromoCommands)
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SetUpList];
}

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_notificationsOptInAlertCoordinator, alertCoordinator);
  std::vector<PushNotificationClientId> clientIds =
      alertCoordinator.clientIds.value();
  if (result != NotificationsOptInAlertResult::kOpenedSettings) {
    [_notificationsOptInAlertCoordinator stop];
    _notificationsOptInAlertCoordinator = nil;
    if (std::find(clientIds.begin(), clientIds.end(),
                  PushNotificationClientId::kSendTab) != clientIds.end()) {
      [_sendTabPromoMediator dismissModule];
    }
  }
}

- (void)notificationsOptInAlertCoordinatorReturnedFromSettings:
    (NotificationsOptInAlertCoordinator*)alertCoordinator {
  CHECK_EQ(_notificationsOptInAlertCoordinator, alertCoordinator);
  std::vector<PushNotificationClientId> clientIds =
      alertCoordinator.clientIds.value();
  [_notificationsOptInAlertCoordinator stop];
  _notificationsOptInAlertCoordinator = nil;
  [PushNotificationUtil getPermissionSettings:^(
                            UNNotificationSettings* settings) {
    if (settings.authorizationStatus == UNAuthorizationStatusAuthorized) {
      for (PushNotificationClientId clientId : clientIds) {
        [self enableNotifications:[self contentSuggestionsModuleType:clientId]
                   viaContextMenu:NO];
      }
    }
  }];
}

#pragma mark - PriceTrackingPromoActionDelegate

// TODO(crbug.com/378554727): Integrate Price Tracking with
// NotificationsOptInAlertCoordinatorDelegate.
- (void)showPriceTrackingPromoAlertCoordinator {
  __weak ContentSuggestionsCoordinator* weakSelf = self;
  _priceTrackingPromoAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.magicStackCollectionView
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SETTINGS_TURN_ON_NOTIFICATIONS_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SETTINGS_TURN_ON_NOTIFICATIONS_TEXT)];
  [_priceTrackingPromoAlertCoordinator
      addItemWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SETTINGS_TURN_ON_NOTIFICATIONS_ACCEPT)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "Commerce.PriceTracking.MagicStackPromo.Reenable.Allow"));
                  NSString* settingURL = UIApplicationOpenSettingsURLString;
                  settingURL = UIApplicationOpenNotificationSettingsURLString;

                  [[UIApplication sharedApplication]
                      openURL:[NSURL URLWithString:settingURL]
                      options:{}
                      completionHandler:^(BOOL res) {
                        [NSNotificationCenter.defaultCenter
                            addObserver:weakSelf
                               selector:@selector(onReturnFromSettings:)
                                   name:UIApplicationDidBecomeActiveNotification
                                 object:nil];
                      }];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_priceTrackingPromoAlertCoordinator
      addItemWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SETTINGS_TURN_ON_NOTIFICATIONS_DENY)
                action:^{
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  base::RecordAction(base::UserMetricsAction(
                      "Commerce.PriceTracking.MagicStackPromo.Reenable.Deny"));
                  [strongSelf dismissAlertCoordinator];
                  [strongSelf->_priceTrackingPromoMediator
                          removePriceTrackingPromo];
                }
                 style:UIAlertActionStyleCancel];
  [_priceTrackingPromoAlertCoordinator start];
}

// TODO(crbug.com/378554727): Integrate Price Tracking with
// NotificationsOptInAlertCoordinatorDelegate.
- (void)onReturnFromSettings:(NSNotification*)notification {
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        if (settings.authorizationStatus == UNAuthorizationStatusAuthorized) {
          [self->_priceTrackingPromoMediator
                  enablePriceTrackingSettingsAndShowSnackbar];
        }
        [self->_priceTrackingPromoMediator removePriceTrackingPromo];
      }];
}

- (void)dismissAlertCoordinator {
  [_priceTrackingPromoAlertCoordinator stop];
  _priceTrackingPromoAlertCoordinator = nil;
}

#pragma mark - SetUpListDefaultBrowserPromoCoordinatorDelegate

- (void)setUpListDefaultBrowserPromoDidFinish:(BOOL)success {
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator = nil;
}

#pragma mark - ShopCardActionDelegate

- (void)openURL:(GURL)url {
  NSInteger new_web_state_index =
      self.browser->GetWebStateList()->GetIndexOfInactiveWebStateWithURL(url);
  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  if (new_web_state_index == WebStateList::kInvalidIndex) {
    urlLoadingBrowserAgent->Load(UrlLoadParams::InNewTab(url));
  } else {
    web::NavigationManager::WebLoadParams webLoadParams =
        web::NavigationManager::WebLoadParams(url);
    UrlLoadParams params = UrlLoadParams::SwitchToTab(webLoadParams);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    urlLoadingBrowserAgent->Load(params);
  }
}

#pragma mark - Helpers

- (void)showMagicStackRecentTabs {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCoordinatorCommands =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [browserCoordinatorCommands showRecentTabs];
}

// Display the notification settings.
- (void)showNotificationSettings {
  [self.contentSuggestionsMetricsRecorder
      recordContentNotificationSnackbarEvent:ContentNotificationSnackbarEvent::
                                                 kActionButtonTapped];
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showNotificationsSettings];
}

// Returns the ContentSuggestionsModuleType associated with `clientId`.
- (ContentSuggestionsModuleType)contentSuggestionsModuleType:
    (PushNotificationClientId)clientId {
  switch (clientId) {
    case PushNotificationClientId::kCommerce:
      return ContentSuggestionsModuleType::kPriceTrackingPromo;
    case PushNotificationClientId::kTips:
      return ContentSuggestionsModuleType::kTips;
    case PushNotificationClientId::kSafetyCheck:
      return ContentSuggestionsModuleType::kSafetyCheck;
    case PushNotificationClientId::kSendTab:
      return ContentSuggestionsModuleType::kSendTabPromo;
    default:
      NOTREACHED();
  }
}

// Disables Magic Stack cards with the "Chrome Tips" header.
- (void)disableTipsModules {
  PrefService* prefs = self.profile->GetPrefs();
  prefs->SetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled, false);
}

@end

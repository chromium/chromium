// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/model/large_icon_cache.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
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
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
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
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/magic_stack_parcel_list_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_action_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_content_notification_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_show_more_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_tap_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using segmentation_platform::TipIdentifier;

@interface ContentSuggestionsCoordinator () <
    ContentSuggestionsCommands,
    ContentSuggestionsViewControllerAudience,
    MagicStackCollectionViewControllerAudience,
    MagicStackHalfSheetTableViewControllerDelegate,
    MagicStackModuleContainerDelegate,
    MagicStackParcelListHalfSheetTableViewControllerDelegate,
    NotificationsConfirmationPresenter,
    NotificationsOptInAlertCoordinatorDelegate,
    NotificationsOptInCoordinatorDelegate,
    PriceTrackingPromoActionDelegate,
    SetUpListContentNotificationPromoCoordinatorDelegate,
    SetUpListDefaultBrowserPromoCoordinatorDelegate,
    SetUpListTapDelegate>

@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;
// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;
// Redefined to not be readonly.
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;
// Metrics recorder for the content suggestions.
@property(nonatomic, strong)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;
// Parcel Tracking Mediator.
@property(nonatomic, strong) ParcelTrackingMediator* parcelTrackingMediator;
@property(nonatomic, strong) SetUpListMediator* setUpListMediator;

@end

@implementation ContentSuggestionsCoordinator {

  // The coordinator that displays the Default Browser Promo for the Set Up
  // List.
  SetUpListDefaultBrowserPromoCoordinator* _defaultBrowserPromoCoordinator;

  // The coordinator that displays the Content Notification Promo for the Set Up
  // List.
  SetUpListContentNotificationPromoCoordinator* _contentNotificationCoordinator;

  // The coordinator that displays the opt-in notification settings view for the
  // Set Up List.
  NotificationsOptInCoordinator* _notificationsOptInCoordinator;

  // The Show More Menu presented from the Set Up List in the Magic Stack.
  SetUpListShowMoreViewController* _setUpListShowMoreViewController;

  // The edit half sheet for toggling all Magic Stack modules.
  MagicStackHalfSheetTableViewController*
      _magicStackHalfSheetTableViewController;
  MagicStackHalfSheetMediator* _magicStackHalfSheetMediator;

  // The parcel list half sheet to see all tracked parcels.
  MagicStackParcelListHalfSheetTableViewController*
      _parcelListHalfSheetTableViewController;

  // The coordinator used to present a modal alert for the parcel tracking
  // module.
  AlertCoordinator* _parcelTrackingAlertCoordinator;

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
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  TabResumptionMediator* _tabResumptionMediator;
  PriceTrackingPromoMediator* _priceTrackingPromoMediator;

  MagicStackCollectionViewController* _magicStackCollectionView;

  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
}

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.NTPActionsDelegate);
  if (self.started) {
    // Prevent this coordinator from being started twice in a row
    return;
  }
  _started = YES;

  ProfileIOS* profile = self.browser->GetProfile();
  PrefService* prefs = ProfileIOS::FromBrowserState(profile)->GetPrefs();

  _segmentationService =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);
  _deviceSwitcherResultDispatcher = segmentation_platform::
      SegmentationPlatformServiceFactory::GetDispatcherForProfile(profile);

  self.authService = AuthenticationServiceFactory::GetForProfile(profile);

  // Conditionally register for provisional Safety Check notifications if the
  // feature is enabled.
  //
  // TODO(crbug.com/366182129): Move Safety Check provisional notification
  // enrollment to `SafetyCheckNotificationClient` once
  // `ProvisionalPushNotificationUtil` circular dependencies are fixed.
  if (IsSafetyCheckNotificationsEnabled()) {
    [ProvisionalPushNotificationUtil
        enrollUserToProvisionalNotificationsForClientIds:
            {PushNotificationClientId::kSafetyCheck}
                             clientEnabledForProvisional:NO
                                         withAuthService:self.authService
                                   deviceInfoSyncService:nil];
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

  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);

  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForProfile(profile);

  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc] init];

  NSMutableArray* moduleMediators = [NSMutableArray array];

  _mostVisitedTilesMediator = [[MostVisitedTilesMediator alloc]
      initWithMostVisitedSite:std::move(mostVisitedFactory)
                  prefService:prefs
             largeIconService:largeIconService
               largeIconCache:cache
       URLLoadingBrowserAgent:UrlLoadingBrowserAgent::FromBrowser(
                                  self.browser)];
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
                   authService:authenticationService];
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
                   browser:self.browser];
    _tabResumptionMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    _tabResumptionMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    [moduleMediators addObject:_tabResumptionMediator];
  }
  if (IsPriceTrackingPromoCardEnabled(shoppingService, authenticationService,
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
          authenticationService:self.authService];
    _priceTrackingPromoMediator.dispatcher =
        static_cast<id<ApplicationCommands, SnackbarCommands>>(
            self.browser->GetCommandDispatcher());
    _priceTrackingPromoMediator.actionDelegate = self;
    _priceTrackingPromoMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    [moduleMediators addObject:_priceTrackingPromoMediator];
  }

  if (IsIOSParcelTrackingEnabled() &&
      !IsParcelTrackingDisabled(
          IsHomeCustomizationEnabled()
              ? prefs
              : GetApplicationContext()->GetLocalState())) {
    _parcelTrackingMediator = [[ParcelTrackingMediator alloc]
        initWithShoppingService:shoppingService
         URLLoadingBrowserAgent:UrlLoadingBrowserAgent::FromBrowser(
                                    self.browser)
                    prefService:IsHomeCustomizationEnabled()
                                    ? prefs
                                    : GetApplicationContext()->GetLocalState()];
    _parcelTrackingMediator.NTPActionsDelegate = self.NTPActionsDelegate;
    [moduleMediators addObject:_parcelTrackingMediator];
  }
  if (IsSafetyCheckMagicStackEnabled()) {
    IOSChromeSafetyCheckManager* safetyCheckManager =
        IOSChromeSafetyCheckManagerFactory::GetForProfile(profile);
    _safetyCheckMediator = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:safetyCheckManager
                        localState:GetApplicationContext()->GetLocalState()
                         userState:prefs
                          appState:self.browser->GetSceneState().appState];
    _safetyCheckMediator.presentationAudience = self;
    [moduleMediators addObject:_safetyCheckMediator];
  }

  if (IsTipsMagicStackEnabled()) {
    _tipsMediator = [[TipsMagicStackMediator alloc]
        initWithIdentifier:TipIdentifier::kUnknown];
    [moduleMediators addObject:_tipsMediator];
  }

  if (!ShouldPutMostVisitedSitesInMagicStack()) {
    ContentSuggestionsViewController* viewController =
        [[ContentSuggestionsViewController alloc] init];
    viewController.audience = self;
    viewController.urlLoadingBrowserAgent =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    viewController.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    self.contentSuggestionsViewController = viewController;
  }
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
                           syncService:syncService
                       identityManager:identityManager
                 authenticationService:authenticationService
                            sceneState:self.browser->GetSceneState()
                 isDefaultSearchEngine:isDefaultSearchEngine
                   segmentationService:_segmentationService
        deviceSwitcherResultDispatcher:_deviceSwitcherResultDispatcher];
    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [_setUpListMediator retrieveUserSegment];
    }
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
                                      self.browser->GetProfile())];
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
  _deviceSwitcherResultDispatcher = nullptr;
  [self.parcelTrackingMediator disconnect];
  self.parcelTrackingMediator = nil;
  [_shortcutsMediator disconnect];
  _shortcutsMediator = nil;
  [_safetyCheckMediator disconnect];
  _safetyCheckMediator = nil;
  _tipsMediator = nil;
  [_setUpListMediator disconnect];
  _setUpListMediator = nil;
  [_mostVisitedTilesMediator disconnect];
  _mostVisitedTilesMediator = nil;
  [_tabResumptionMediator disconnect];
  _tabResumptionMediator = nil;
  [_magicStackRankingModel disconnect];
  _magicStackRankingModel = nil;
  [self.contentSuggestionsMediator disconnect];
  self.contentSuggestionsMediator = nil;
  [self.contentSuggestionsMetricsRecorder disconnect];
  self.contentSuggestionsMetricsRecorder = nil;
  self.contentSuggestionsViewController.audience = nil;
  self.contentSuggestionsViewController = nil;
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator = nil;
  [_magicStackHalfSheetMediator disconnect];
  _magicStackHalfSheetMediator = nil;
  [_magicStackHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _magicStackHalfSheetTableViewController = nil;
  [self dismissParcelListHalfSheet];
  [self dismissParcelTrackingAlertCoordinator];
  [_notificationsOptInAlertCoordinator stop];
  _notificationsOptInAlertCoordinator = nil;
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(ContentSuggestionsCommands)];
  _started = NO;
}

- (ContentSuggestionsViewController*)viewController {
  return self.contentSuggestionsViewController;
}

#pragma mark - Public methods

- (void)refresh {
  [_magicStackCollectionView reset];
  // Refresh in case there are new MVT to show.
  [_mostVisitedTilesMediator refreshMostVisitedTiles];
  [_safetyCheckMediator reset];
  [_parcelTrackingMediator reset];
  [_priceTrackingPromoMediator reset];
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  // Fetch after resetting ranking since parcels could be returned
  // synchronously.
  [_parcelTrackingMediator fetchTrackedParcels];
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
  _setUpListShowMoreViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _setUpListShowMoreViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  if (expanded) {
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }
  presentationController.preferredCornerRadius = 16;
  [_magicStackCollectionView
      presentViewController:_setUpListShowMoreViewController
                   animated:YES
                 completion:nil];
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)viewWillDisappear {
  DiscoverFeedServiceFactory::GetForProfile(self.browser->GetProfile())
      ->SetIsShownOnStartSurface(false);
}

#pragma mark - MagicStackCollectionViewAudience

- (void)didTapMagicStackEditButton {
  base::RecordAction(base::UserMetricsAction("IOSMagicStackSettingsOpened"));
  if (IsHomeCustomizationEnabled()) {
    [self.delegate openMagicStackCustomizationMenu];
  } else {
    _magicStackHalfSheetTableViewController =
        [[MagicStackHalfSheetTableViewController alloc] init];

    _magicStackHalfSheetMediator = [[MagicStackHalfSheetMediator alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()];
    _magicStackHalfSheetMediator.consumer =
        _magicStackHalfSheetTableViewController;
    _magicStackHalfSheetTableViewController.delegate = self;
    _magicStackHalfSheetTableViewController.modelDelegate =
        _magicStackHalfSheetMediator;

    UINavigationController* navViewController = [[UINavigationController alloc]
        initWithRootViewController:_magicStackHalfSheetTableViewController];

    navViewController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        navViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    [_magicStackCollectionView presentViewController:navViewController
                                            animated:YES
                                          completion:nil];
  }
}

- (void)logEphemeralCardVisibility:(ContentSuggestionsModuleType)card {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackTopModuleImpressionHistogram, card);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeCardRegistryForProfile(self.browser->GetProfile());

  switch (card) {
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      registry->NotifyCardShown(
          segmentation_platform::kPriceTrackingNotificationPromo);
      break;
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      registry->NotifyCardShown(segmentation_platform::kTipsEphemeralModule);
      break;
    default:
      NOTREACHED();
  }
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
    case ContentSuggestionsModuleType::kParcelTracking:
      [self showMagicStackParcelList];
      break;
    case ContentSuggestionsModuleType::kTabResumption:
      [self showMagicStackRecentTabs];
      break;
    default:
      break;
  }
}

- (void)neverShowModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
      [_tabResumptionMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kSafetyCheck:
      [_safetyCheckMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [_setUpListMediator disableModule];
      break;
    case ContentSuggestionsModuleType::kParcelTracking: {
      [self presentParcelTrackingAlertCoordinator];
      break;
    }
    case ContentSuggestionsModuleType::kPriceTrackingPromo: {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.MagicStackPromo.Hidden"));
      [_priceTrackingPromoMediator disableModule];
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
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return PushNotificationClientId::kSafetyCheck;
  }

  if (IsSetUpListModuleType(type)) {
    return PushNotificationClientId::kTips;
  }

  NOTREACHED();
}

// Retrieves the message ID for the push notification feature title associated
// with the specified `ContentSuggestionsModuleType`. Currently, push
// notifications are exclusively supported by the Set Up List and Safety Check
// modules.
- (int)pushNotificationTitleMessageId:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return IDS_IOS_SAFETY_CHECK_TITLE;
  }

  if (IsSetUpListModuleType(type)) {
    return content_suggestions::SetUpListTitleStringID();
  }

  NOTREACHED();
}

- (void)enableNotifications:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  // Ask user for permission to opt-in to notifications.
  [_notificationsOptInAlertCoordinator stop];

  _notificationsOptInAlertCoordinator =
      [[NotificationsOptInAlertCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];

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

- (void)disableNotifications:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  const PushNotificationClientId clientId =
      [self pushNotificationClientId:type];

  GetApplicationContext()->GetPushNotificationService()->SetPreference(
      identity.gaiaID, clientId, false);

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

#pragma mark - MagicStackHalfSheetTableViewControllerDelegate

- (void)dismissMagicStackHalfSheet {
  [_magicStackHalfSheetMediator disconnect];
  _magicStackHalfSheetMediator = nil;
  [_magicStackHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _magicStackHalfSheetTableViewController = nil;
}

#pragma mark - MagicStackParcelListHalfSheetTableViewControllerDelegate

- (void)dismissParcelListHalfSheet {
  [_parcelListHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _parcelListHalfSheetTableViewController = nil;
}

- (void)untrackParcel:(NSString*)parcelID carrier:(ParcelType)carrier {
  [self.parcelTrackingMediator untrackParcel:parcelID];

  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarHandler
      showSnackbarWithMessage:
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_UNTRACK_SNACKBAR_TITLE)
                   buttonText:l10n_util::GetNSString(
                                  IDS_IOS_SNACKBAR_ACTION_UNDO)
                messageAction:^{
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  [weakSelf.parcelTrackingMediator trackParcel:parcelID
                                                       carrier:carrier];
                }
             completionAction:nil];
}

#pragma mark - SafetyCheckViewDelegate

// Called when a Safety Check item is selected by the user. Depending on the
// Safety Check item `type`, this method fires a UI command to present the
// Update Chrome page, Password Checkup, or Safety Check half sheet.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type {
  CHECK(IsSafetyCheckMagicStackEnabled());

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
    switch (type) {
      case SetUpListItemType::kSignInSync:
        [weakSelf showSignIn];
        break;
      case SetUpListItemType::kDefaultBrowser:
        [weakSelf showDefaultBrowserPromo];
        break;
      case SetUpListItemType::kAutofill:
        [weakSelf showCredentialProviderPromo];
        break;
      case SetUpListItemType::kNotifications:
        if (IsIOSTipsNotificationsEnabled()) {
          [weakSelf showNotificationsOptInView];
        } else {
          [weakSelf showContentNotificationBottomSheet];
        }
        break;
      case SetUpListItemType::kFollow:
      case SetUpListItemType::kAllSet:
        // TODO(crbug.com/40262090): Add a Follow item to the Set Up List.
        NOTREACHED_IN_MIGRATION();
    }
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

// Shows the Default Browser Promo.
- (void)showDefaultBrowserPromo {
  // Stop the coordinator if it is already running. If the user swipes to
  // dismiss a previous instance and then clicks the item again the
  // previous instance may not have been stopped yet due to the animation.
  [_defaultBrowserPromoCoordinator stop];
  if (IsSegmentedDefaultBrowserPromoEnabled()) {
    _defaultBrowserPromoCoordinator =
        [[SetUpListDefaultBrowserPromoCoordinator alloc]
                initWithBaseViewController:[self viewController]
                                   browser:self.browser
                               application:[UIApplication sharedApplication]
                       segmentationService:_segmentationService
            deviceSwitcherResultDispatcher:_deviceSwitcherResultDispatcher];
  } else {
    _defaultBrowserPromoCoordinator =
        [[SetUpListDefaultBrowserPromoCoordinator alloc]
                initWithBaseViewController:[self viewController]
                                   browser:self.browser
                               application:[UIApplication sharedApplication]
                       segmentationService:nullptr
            deviceSwitcherResultDispatcher:nullptr];
  }
  _defaultBrowserPromoCoordinator.delegate = self;
  [_defaultBrowserPromoCoordinator start];
}

// Shows the SigninSync UI with the SetUpList access point.
- (void)showSignIn {
  ShowSigninCommandCompletionCallback callback =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        if (result == SigninCoordinatorResultSuccess ||
            result == SigninCoordinatorResultCanceledByUser) {
          PrefService* localState = GetApplicationContext()->GetLocalState();
          set_up_list_prefs::MarkItemComplete(localState,
                                              SetUpListItemType::kSignInSync);
        }
      };
  // If there are 0 identities, kInstantSignin requires less taps.
  AuthenticationOperation operation =
      ChromeAccountManagerServiceFactory::GetForProfile(
          self.browser->GetProfile())
              ->HasIdentities()
          ? AuthenticationOperation::kSigninOnly
          : AuthenticationOperation::kInstantSignin;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
              showSignin:command
      baseViewController:self.viewController];
}

// Shows the Credential Provider Promo using the SetUpList trigger.
- (void)showCredentialProviderPromo {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      CredentialProviderPromoCommands)
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SetUpList];
}

- (void)showContentNotificationBottomSheet {
  // Stop the coordinator if it is already running. If the user swipes to
  // dismiss a previous instance and then clicks the item again the
  // previous instance may not have been stopped yet due to the animation.
  [_contentNotificationCoordinator stop];
  _contentNotificationCoordinator =
      [[SetUpListContentNotificationPromoCoordinator alloc]
          initWithBaseViewController:[self viewController]
                             browser:self.browser
                         application:[UIApplication sharedApplication]];
  _contentNotificationCoordinator.delegate = self;
  _contentNotificationCoordinator.messagePresenter = self;
  [_contentNotificationCoordinator start];
}

- (void)showNotificationsOptInView {
  [_notificationsOptInCoordinator stop];
  _notificationsOptInCoordinator = [[NotificationsOptInCoordinator alloc]
      initWithBaseViewController:[self viewController]
                         browser:self.browser];
  _notificationsOptInCoordinator.delegate = self;
  [_notificationsOptInCoordinator start];
}

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_notificationsOptInAlertCoordinator, alertCoordinator);
  [_notificationsOptInAlertCoordinator stop];
  _notificationsOptInAlertCoordinator = nil;
}

#pragma mark - NotificationsOptInCoordinatorDelegate

- (void)notificationsOptInScreenDidFinish:
    (NotificationsOptInCoordinator*)coordinator {
  CHECK_EQ(coordinator, _notificationsOptInCoordinator);
  [_notificationsOptInCoordinator stop];
  _notificationsOptInCoordinator = nil;
}

#pragma mark - PriceTrackingPromoActionDelegate

- (void)showPriceTrackingPromoAlertCoordinator {
  __weak ContentSuggestionsCoordinator* weakSelf = self;
  _priceTrackingPromoAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
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
                  if (@available(iOS 15.4, *)) {
                    settingURL = UIApplicationOpenNotificationSettingsURLString;
                  }

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

#pragma mark - SetUpListContentNotificationPromoCoordinatorDelegate

- (void)setUpListContentNotificationPromoDidFinish {
  [_contentNotificationCoordinator stop];
  _contentNotificationCoordinator = nil;
}

#pragma mark - NotificationsConfirmationPresenter

- (void)presentNotificationsConfirmationMessage {
  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarHandler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_TITLE)
                   buttonText:
                       l10n_util::GetNSString(
                           IDS_IOS_CONTENT_NOTIFICATION_SNACKBAR_ACTION_MANAGE)
                messageAction:^{
                  [weakSelf showNotificationSettings];
                }
             completionAction:nil];

  [self.contentSuggestionsMetricsRecorder
      recordContentNotificationSnackbarEvent:ContentNotificationSnackbarEvent::
                                                 kShown];
}

#pragma mark - Helpers

- (void)showMagicStackParcelList {
  _parcelListHalfSheetTableViewController =
      [[MagicStackParcelListHalfSheetTableViewController alloc]
          initWithParcels:[self.parcelTrackingMediator allParcelTrackingItems]];
  _parcelListHalfSheetTableViewController.delegate = self;

  UINavigationController* navViewController = [[UINavigationController alloc]
      initWithRootViewController:_parcelListHalfSheetTableViewController];

  navViewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      navViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [_magicStackCollectionView presentViewController:navViewController
                                          animated:YES
                                        completion:nil];
}

- (void)showMagicStackRecentTabs {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCoordinatorCommands =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [browserCoordinatorCommands showRecentTabs];
}

// Presents the parcel tracking alert modal.
- (void)presentParcelTrackingAlertCoordinator {
  _parcelTrackingAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_TITLE)
                         message:
                             l10n_util::GetNSStringF(
                                 IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_DESCRIPTION,
                                 base::SysNSStringToUTF16(l10n_util::GetNSString(
                                     IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)))];

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  [_parcelTrackingAlertCoordinator
      addItemWithTitle:
          l10n_util::GetNSStringF(
              IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_DESCRIPTION,
              base::SysNSStringToUTF16(l10n_util::GetNSString(
                  IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)))
                action:^{
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  [weakSelf.parcelTrackingMediator disableModule];
                  [weakSelf dismissParcelTrackingAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_parcelTrackingAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_CANCEL)
                action:^{
                  [weakSelf dismissParcelTrackingAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [_parcelTrackingAlertCoordinator start];
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

// Dismisses the parcel tracking alert modal.
- (void)dismissParcelTrackingAlertCoordinator {
  [_parcelTrackingAlertCoordinator stop];
  _parcelTrackingAlertCoordinator = nil;
}

@end

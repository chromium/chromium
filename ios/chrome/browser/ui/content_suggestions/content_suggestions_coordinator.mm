// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/model/large_icon_cache.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp_tiles/model/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
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
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/magic_stack_parcel_list_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
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
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/push_notification/notifications_confirmation_presenter.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface ContentSuggestionsCoordinator () <
    ContentSuggestionsViewControllerAudience,
    MagicStackCollectionViewControllerAudience,
    MagicStackHalfSheetTableViewControllerDelegate,
    MagicStackModuleContainerDelegate,
    MagicStackParcelListHalfSheetTableViewControllerDelegate,
    NotificationsConfirmationPresenter,
    NotificationsOptInAlertCoordinatorDelegate,
    NotificationsOptInCoordinatorDelegate,
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

  // The coordinator used to present an alert to enable Tips notifications.
  NotificationsOptInAlertCoordinator* _notificationsOptInAlertCoordinator;

  MagicStackRankingModel* _magicStackRankingModel;

  // Module mediators.
  ShortcutsMediator* _shortcutsMediator;
  SafetyCheckMagicStackMediator* _safetyCheckMediator;
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  TabResumptionMediator* _tabResumptionMediator;

  MagicStackCollectionViewController* _magicStackCollectionView;
}

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.NTPMetricsDelegate);

  if (self.started) {
    // Prevent this coordinator from being started twice in a row
    return;
  }

  _started = YES;

  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  PrefService* prefs =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(
          self.browser->GetBrowserState());
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.contentSuggestionsMetricsRecorder =
      [[ContentSuggestionsMetricsRecorder alloc]
          initWithLocalState:GetApplicationContext()->GetLocalState()];

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

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
  _mostVisitedTilesMediator.actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:kMenuScenarioHistogramMostVisitedEntry];
  _mostVisitedTilesMediator.snackbarHandler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  _mostVisitedTilesMediator.NTPMetricsDelegate = self.NTPMetricsDelegate;
  [moduleMediators addObject:_mostVisitedTilesMediator];
  self.contentSuggestionsMediator.mostVisitedTilesMediator =
      _mostVisitedTilesMediator;

  _shortcutsMediator = [[ShortcutsMediator alloc]
      initWithReadingListModel:readingListModel
      featureEngagementTracker:feature_engagement::TrackerFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())
                   authService:authenticationService];
  _shortcutsMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  _shortcutsMediator.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCoordinatorCommands>>(
          self.browser->GetCommandDispatcher());
  [moduleMediators addObject:_shortcutsMediator];
  self.contentSuggestionsMediator.shortcutsMediator = _shortcutsMediator;

  BOOL isSetupListEnabled = set_up_list_utils::IsSetUpListActive(
      GetApplicationContext()->GetLocalState());
  if (isSetupListEnabled) {
    _setUpListMediator = [[SetUpListMediator alloc]
          initWithPrefService:prefs
                  syncService:syncService
              identityManager:identityManager
        authenticationService:authenticationService
                   sceneState:self.browser->GetSceneState()];
    _setUpListMediator.commandHandler = self;
    _setUpListMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    _setUpListMediator.delegate = self.delegate;
    self.contentSuggestionsMediator.setUpListMediator = _setUpListMediator;
    [moduleMediators addObject:_setUpListMediator];
  }

  if (IsTabResumptionEnabled()) {
    _tabResumptionMediator = [[TabResumptionMediator alloc]
        initWithLocalState:GetApplicationContext()->GetLocalState()
               prefService:prefs
           identityManager:identityManager
                   browser:self.browser];
    _tabResumptionMediator.NTPMetricsDelegate = self.NTPMetricsDelegate;
    _tabResumptionMediator.contentSuggestionsMetricsRecorder =
        self.contentSuggestionsMetricsRecorder;
    [moduleMediators addObject:_tabResumptionMediator];
  }
  if (IsIOSParcelTrackingEnabled() &&
      !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState())) {
    _parcelTrackingMediator = [[ParcelTrackingMediator alloc]
        initWithShoppingService:shoppingService
         URLLoadingBrowserAgent:UrlLoadingBrowserAgent::FromBrowser(
                                    self.browser)];
    _parcelTrackingMediator.NTPMetricsDelegate = self.NTPMetricsDelegate;
    [moduleMediators addObject:_parcelTrackingMediator];
  }
  if (IsSafetyCheckMagicStackEnabled()) {
    IOSChromeSafetyCheckManager* safetyCheckManager =
        IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    _safetyCheckMediator = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:safetyCheckManager
                        localState:GetApplicationContext()->GetLocalState()
                          appState:self.browser->GetSceneState().appState];
    _safetyCheckMediator.presentationAudience = self;
    [moduleMediators addObject:_safetyCheckMediator];
  }

  _magicStackRankingModel = [[MagicStackRankingModel alloc]
      initWithSegmentationService:
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetForBrowserState(self.browser->GetBrowserState())
                      prefService:prefs
                       localState:GetApplicationContext()->GetLocalState()
                  moduleMediators:moduleMediators];
  _magicStackRankingModel.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  self.contentSuggestionsMediator.magicStackRankingModel =
      _magicStackRankingModel;
  if (IsIOSMagicStackCollectionViewEnabled()) {
    _magicStackRankingModel.delegate = self.contentSuggestionsMediator;
  }

  self.contentSuggestionsViewController =
      [[ContentSuggestionsViewController alloc] init];
  self.contentSuggestionsViewController.audience = self;
  self.contentSuggestionsViewController.urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.contentSuggestionsViewController.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  self.contentSuggestionsViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.contentSuggestionsViewController.parcelTrackingCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ParcelTrackingOptInCommands);

  if (IsIOSMagicStackCollectionViewEnabled()) {
    _magicStackCollectionView =
        [[MagicStackCollectionViewController alloc] init];
    _magicStackCollectionView.audience = self;
  }

  if (_magicStackRankingModel) {
    _magicStackRankingModel.consumer = self.contentSuggestionsViewController;
  }
  _shortcutsMediator.consumer = self.contentSuggestionsViewController;
  _safetyCheckMediator.consumer = self.contentSuggestionsViewController;
  _mostVisitedTilesMediator.consumer = self.contentSuggestionsViewController;
  _setUpListMediator.consumer = self.contentSuggestionsViewController;

  if (IsIOSMagicStackCollectionViewEnabled()) {
    self.contentSuggestionsMediator.magicStackConsumer =
        _magicStackCollectionView;
  }
  self.contentSuggestionsMediator.consumer =
      self.contentSuggestionsViewController;
}

- (void)stop {
  [self.parcelTrackingMediator disconnect];
  self.parcelTrackingMediator = nil;
  [_shortcutsMediator disconnect];
  _shortcutsMediator = nil;
  [_safetyCheckMediator disconnect];
  _safetyCheckMediator = nil;
  [_setUpListMediator disconnect];
  _setUpListMediator = nil;
  [_mostVisitedTilesMediator disconnect];
  _mostVisitedTilesMediator = nil;
  [_tabResumptionMediator disconnect];
  _tabResumptionMediator = nil;
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
  _started = NO;
}

- (ContentSuggestionsViewController*)viewController {
  return self.contentSuggestionsViewController;
}

#pragma mark - Public methods

- (void)refresh {
  // Refresh in case there are new MVT to show.
  [_mostVisitedTilesMediator refreshMostVisitedTiles];
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)viewWillDisappear {
  DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->SetIsShownOnStartSurface(false);
}

- (void)moduleWasRemoved {
  [self.NTPDelegate updateFeedLayout];
}

- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed {
  return [self.browser->GetSceneState()
              .window.rootViewController.view safeAreaInsets];
}

- (void)didTapMagicStackEditButton {
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
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [self.viewController presentViewController:navViewController
                                    animated:YES
                                  completion:nil];
}

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
  [self.viewController presentViewController:navViewController
                                    animated:YES
                                  completion:nil];
}

- (void)didTapSetUpListItemView:(SetUpListItemView*)view {
  [self didSelectSetUpListItem:view.type];
}

#pragma mark - MagicStackModuleContainerDelegate

- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kSafetyCheck:
      [self didSelectSafetyCheckItem:SafetyCheckItemType::kDefault];
      break;
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [self showSetUpListShowMoreMenu];
      break;
    case ContentSuggestionsModuleType::kParcelTracking:
      [self showMagicStackParcelList];
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
    default:
      break;
  }
}

- (void)enableNotifications:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List modules.
  CHECK(IsSetUpListModuleType(type));

  // Ask user for permission to opt-in notifications.
  [_notificationsOptInAlertCoordinator stop];
  _notificationsOptInAlertCoordinator =
      [[NotificationsOptInAlertCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  _notificationsOptInAlertCoordinator.clientIds =
      std::vector{PushNotificationClientId::kTips};
  _notificationsOptInAlertCoordinator.confirmationMessage =
      l10n_util::GetNSStringF(
          IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE,
          l10n_util::GetStringUTF16(
              content_suggestions::SetUpListTitleStringID()));
  _notificationsOptInAlertCoordinator.delegate = self;
  [_notificationsOptInAlertCoordinator start];
}

- (void)disableNotifications:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List modules.
  CHECK(IsSetUpListModuleType(type));

  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  GetApplicationContext()->GetPushNotificationService()->SetPreference(
      identity.gaiaID, PushNotificationClientId::kTips, false);

  // Show confirmation snackbar.
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_MANAGE_SETTINGS);
  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE_OFF,
      l10n_util::GetStringUTF16(content_suggestions::SetUpListTitleStringID()));
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

  [self.NTPMetricsDelegate safetyCheckOpened];
  Browser* browser = self.browser;
  [_magicStackRankingModel logMagicStackEngagementForType:
                               ContentSuggestionsModuleType::kSafetyCheck];

  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
          browser->GetBrowserState());

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
      std::vector<password_manager::CredentialUIEntry> credentials =
          safetyCheckManager->GetInsecureCredentials();
      HandleSafetyCheckPasswordTap(credentials, applicationHandler,
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
      [settingsHandler showAndStartSafetyCheckInHalfSheet:YES
                                                 referrer:referrer];
      break;
  }
}

#pragma mark - SetUpListTapDelegate

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
  [self.NTPMetricsDelegate setUpListItemOpened];
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
        // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
        NOTREACHED();
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
  _defaultBrowserPromoCoordinator =
      [[SetUpListDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:[self viewController]
                             browser:self.browser
                         application:[UIApplication sharedApplication]];
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationOperation operation =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState)
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

- (void)showSetUpListShowMoreMenu {
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
  presentationController.preferredCornerRadius = 16;
  [self.viewController presentViewController:_setUpListShowMoreViewController
                                    animated:YES
                                  completion:nil];
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

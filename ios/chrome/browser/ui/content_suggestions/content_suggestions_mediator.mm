// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import <vector>

#import <AuthenticationServices/AuthenticationServices.h>
#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/commerce/core/shopping_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/history/core/browser/features.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/search_engines/search_terms_data.h"
#import "components/search_engines/template_url.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/set_up_list.h"
#import "ios/chrome/browser/ntp/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/ntp_tiles/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_tile_saver.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/ui/ntp/feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using credential_provider_promo::IOSCredentialProviderPromoAction;
using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using RequestSource = SearchTermsData::RequestSource;

// The Safety Check (Magic Stack) module runs (at minimum) once every 24 hours.
constexpr base::TimeDelta kSafetyCheckRunThreshold = base::Hours(24);

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

const NSTimeInterval kTwoDays = 2 * 24 * 60 * 60;

// Checks the last action the user took on the Credential Provider Promo to
// determine if it was dismissed.
bool CredentialProviderPromoDismissed(PrefService* local_state) {
  IOSCredentialProviderPromoAction last_action =
      static_cast<IOSCredentialProviderPromoAction>(local_state->GetInteger(
          prefs::kIosCredentialProviderPromoLastActionTaken));
  return last_action == IOSCredentialProviderPromoAction::kNo;
}

}  // namespace

@interface ContentSuggestionsMediator () <AppStateObserver,
                                          AuthenticationServiceObserving,
                                          SyncObserverModelBridge,
                                          IdentityManagerObserverBridgeDelegate,
                                          MostVisitedSitesObserving,
                                          ReadingListModelBridgeObserver,
                                          PrefObserverDelegate,
                                          SafetyCheckManagerObserver,
                                          SceneStateObserver,
                                          SetUpListDelegate,
                                          SyncedSessionsObserver> {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
}

// Whether the contents section should be hidden completely.
// Don't use PrefBackedBoolean or PrefMember as this value needs to be checked
// when the Preference is updated.
@property(nonatomic, assign, readonly) BOOL contentSuggestionsEnabled;

// Don't use PrefBackedBoolean or PrefMember as those values needs to be checked
// when the Preference is updated.
// Whether the suggestions have been disabled in Chrome Settings.
@property(nonatomic, assign)
    const PrefService::Preference* articleForYouEnabled;
// Whether the suggestions have been disabled by a policy.
@property(nonatomic, assign)
    const PrefService::Preference* contentSuggestionsPolicyEnabled;

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedActionItem*>* actionButtonItems;
// Most visited items from the MostVisitedSites service (copied upon receiving
// the callback). Those items are up to date with the model.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* freshMostVisitedItems;
// Section Info for the logo and omnibox section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* logoSectionInfo;
// Section Info for the "Return to Recent Tab" section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* returnToRecentTabSectionInfo;
// Item for the "Return to Recent Tab" tile.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabItem* returnToRecentTabItem;
// Section Info for the Most Visited section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSectionInfo;
// Whether the page impression has been recorded.
@property(nonatomic, assign) BOOL recordedPageImpression;
// Mediator fetching the favicons for the items.
@property(nonatomic, strong) ContentSuggestionsFaviconMediator* faviconMediator;
// Item for the reading list action item.  Reference is used to update the
// reading list count.
@property(nonatomic, strong)
    ContentSuggestionsMostVisitedActionItem* readingListItem;
// Indicates if reading list model is loaded. Readlist cannot be triggered until
// it is.
@property(nonatomic, assign) NSInteger readingListModelIsLoaded;
// Number of unread items in reading list model.
@property(nonatomic, assign) NSInteger readingListUnreadCount;
// YES if the Return to Recent Tab tile is being shown.
@property(nonatomic, assign, getter=mostRecentTabStartSurfaceTileIsShowing)
    BOOL showMostRecentTabStartSurfaceTile;
// Whether the incognito mode is available.
@property(nonatomic, assign) BOOL incognitoAvailable;
// Browser reference.
@property(nonatomic, assign) Browser* browser;
// The SetUpList, a list of tasks a new user might want to complete.
@property(nonatomic, strong) SetUpList* setUpList;

// For testing-only
@property(nonatomic, assign) BOOL hasReceivedMagicStackResponse;

@end

@implementation ContentSuggestionsMediator {
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Local State prefs.
  PrefService* _localState;
  // Used by SetUpList to get the sync status.
  syncer::SyncService* _syncService;
  // Used by SetUpList to get signed-in status.
  AuthenticationService* _authenticationService;
  // Used by the Safety Check (Magic Stack) module for the current Safety Check
  // state.
  SafetyCheckState* _safetyCheckState;
  // Used by SetUpList to observe changes to signed-in status.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  // Observer for Safety Check changes.
  std::unique_ptr<SafetyCheckObserverBridge> _safetyCheckManagerObserver;
  // Helper class for the tab resumption tile.
  std::unique_ptr<TabResumptionHelper> _tabResumptionHelper;
  // Item displayed in the tab resumption tile.
  TabResumptionItem* _tabResumptionItem;
  // The latest module ranking returned from the SegmentationService.
  NSArray<NSNumber*>* _magicStackOrderFromSegmentation;
  // YES if the module ranking has been received from the SegmentationService.
  BOOL _magicStackOrderFromSegmentationReceived;
  // The latest Magic Stack module order sent up to the consumer. This includes
  // any omissions due to filtering from `_magicStackOrderFromSegmentation` (or
  // `magicStackOrder:` if kSegmentationPlatformIosModuleRanker is disabled) and
  // any additions beyond `_magicStackOrderFromSegmentation` (e.g. Set Up List).
  NSArray<NSNumber*>* _latestMagicStackOrder;
  commerce::ShoppingService* _shoppingService;
  NSArray<ParcelTrackingItem*>* _parcelTrackingItems;
}

#pragma mark - Public

- (instancetype)
         initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                 readingListModel:(ReadingListModel*)readingListModel
                      prefService:(PrefService*)prefService
    isGoogleDefaultSearchProvider:(BOOL)isGoogleDefaultSearchProvider
                      syncService:(syncer::SyncService*)syncService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                  shoppingService:(commerce::ShoppingService*)shoppingService
                          browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _localState = GetApplicationContext()->GetLocalState();
    _incognitoAvailable = !IsIncognitoModeDisabled(prefService);
    _articleForYouEnabled =
        prefService->FindPreference(prefs::kArticlesForYouEnabled);
    _contentSuggestionsPolicyEnabled =
        prefService->FindPreference(prefs::kNTPContentSuggestionsEnabled);

    _faviconMediator = [[ContentSuggestionsFaviconMediator alloc]
        initWithLargeIconService:largeIconService
                  largeIconCache:largeIconCache];

    _logoSectionInfo = LogoSectionInformation();
    _mostVisitedSectionInfo = MostVisitedSectionInformation();

    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->AddMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);

    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);

    _authenticationService = authenticationService;
    _syncService = syncService;
    _shoppingService = shoppingService;
    if (IsIOSSetUpListEnabled() &&
        set_up_list_utils::IsSetUpListActive(_localState)) {
      _authServiceObserverBridge =
          std::make_unique<AuthenticationServiceObserverBridge>(
              _authenticationService, self);
      _syncObserverBridge =
          std::make_unique<SyncObserverBridge>(self, _syncService);
      _identityObserverBridge =
          std::make_unique<signin::IdentityManagerObserverBridge>(
              identityManager, self);
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      _prefChangeRegistrar.Init(_localState);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosCredentialProviderPromoLastActionTaken,
          &_prefChangeRegistrar);
      _prefObserverBridge->ObserveChangesForPreference(
          set_up_list_prefs::kDisabled, &_prefChangeRegistrar);
      if (CredentialProviderPromoDismissed(_localState)) {
        set_up_list_prefs::MarkItemComplete(_localState,
                                            SetUpListItemType::kAutofill);
      } else {
        [self checkIfCPEEnabled];
      }
      _setUpList = [SetUpList buildFromPrefs:prefService
                                  localState:_localState
                                 syncService:syncService
                       authenticationService:authenticationService];
    }

    if (IsTabResumptionEnabled() &&
        !tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
      if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
        sync_sessions::SessionSyncService* sessionSyncService =
            SessionSyncServiceFactory::GetForBrowserState(
                browser->GetBrowserState());
        _syncedSessionsObserver =
            std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
                self, sessionSyncService);
      }

      _tabResumptionHelper =
          std::make_unique<TabResumptionHelper>(TabResumptionHelper(browser));
    }

    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(browser)->GetSceneState();

    [sceneState addObserver:self];

    [sceneState.appState addObserver:self];

    _browser = browser;

    if (IsSafetyCheckMagicStackEnabled() &&
        !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
      if (!_prefObserverBridge) {
        _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      }

      _prefChangeRegistrar.Init(_localState);

      // TODO(crbug.com/1481230): Stop observing
      // `kIosSettingsSafetyCheckLastRunTime` changes once the Settings Safety
      // Check is refactored to use the new Safety Check Manager.
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSettingsSafetyCheckLastRunTime, &_prefChangeRegistrar);

      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
          &_prefChangeRegistrar);

      _safetyCheckState = [self initialSafetyCheckState];

      _safetyCheckManagerObserver = std::make_unique<SafetyCheckObserverBridge>(
          self, IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
                    browser->GetBrowserState()));

      if (sceneState.appState.initStage > InitStageNormalUI &&
          sceneState.appState.firstSceneHasInitializedUI &&
          _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
        IOSChromeSafetyCheckManager* safetyCheckManager =
            IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
                browser->GetBrowserState());

        safetyCheckManager->StartSafetyCheck();
      }
    }
  }

  return self;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastRefreshTime, 0);
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastUnseenRefreshTime, 0);
}

- (void)disconnect {
  _mostVisitedBridge.reset();
  _mostVisitedSites.reset();
  _readingListModelBridge.reset();
  _authenticationService = nullptr;
  _authServiceObserverBridge.reset();
  _syncObserverBridge.reset();
  _identityObserverBridge.reset();
  _syncedSessionsObserver.reset();
  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
  [_setUpList disconnect];
  _setUpList = nil;
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState.appState removeObserver:self];
  [sceneState removeObserver:self];
  _localState = nullptr;
}

- (void)refreshMostVisitedTiles {
  // Refresh in case there are new MVT to show.
  _mostVisitedSites->Refresh();
}

- (void)blockMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)allowMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, false);
  [self useFreshMostVisited];
}

- (void)setConsumer:(id<ContentSuggestionsConsumer>)consumer {
  _consumer = consumer;
  self.faviconMediator.consumer = consumer;
  [self configureConsumer];
}

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedTiles;
}

- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel {
  //  The most recent tab tile is part of the tab resume feature.
  if (IsTabResumptionEnabled()) {
    return;
  }

  self.returnToRecentTabSectionInfo = ReturnToRecentTabSectionInformation();
  if (!self.returnToRecentTabItem) {
    self.returnToRecentTabItem =
        [[ContentSuggestionsReturnToRecentTabItem alloc] init];
  }

  // Retrieve favicon associated with the page.
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (driver->FaviconIsValid()) {
    gfx::Image favicon = driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      self.returnToRecentTabItem.icon = favicon.ToUIImage();
    }
  }
  if (!self.returnToRecentTabItem.icon) {
    driver->FetchFavicon(webState->GetLastCommittedURL(), false);
  }

  self.returnToRecentTabItem.title =
      l10n_util::GetNSString(IDS_IOS_RETURN_TO_RECENT_TAB_TITLE);
  self.returnToRecentTabItem.subtitle = [self
      constructReturnToRecentTabSubtitleWithPageTitle:base::SysUTF16ToNSString(
                                                          webState->GetTitle())
                                           timeString:timeLabel];
  self.showMostRecentTabStartSurfaceTile = YES;
  [self.consumer
      showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
}

- (void)hideRecentTabTile {
  if (self.showMostRecentTabStartSurfaceTile) {
    self.showMostRecentTabStartSurfaceTile = NO;
    self.returnToRecentTabItem = nil;
    [self.consumer hideReturnToRecentTabTile];
  }
}

- (void)disableSetUpList {
  set_up_list_prefs::DisableSetUpList(_localState);
}

- (void)disableTabResumption {
  tab_resumption_prefs::DisableTabResumption(_localState);
  [self hideTabResumption];
}

- (void)disableSafetyCheck:(ContentSuggestionsModuleType)type {
  safety_check_prefs::DisableSafetyCheckInMagicStack(_localState);

  MagicStackOrderChange change{MagicStackOrderChange::Type::kRemove};
  change.old_module = type;
  change.index = [self indexForMagicStackModule:type];
  CHECK(change.index != NSNotFound);
  [self.consumer updateMagicStackOrder:change];
}

- (NSArray<ParcelTrackingItem*>*)parcelTrackingItems {
  return _parcelTrackingItems;
}

- (void)disableParcelTracking {
  DisableParcelTracking(_localState);

  // Find all parcel tracking modules and remove them.
  for (NSUInteger i = 0; i < [_latestMagicStackOrder count]; i++) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[_latestMagicStackOrder[i] intValue];
    if (type == ContentSuggestionsModuleType::kParcelTracking ||
        type == ContentSuggestionsModuleType::kParcelTrackingSeeMore) {
      MagicStackOrderChange change{MagicStackOrderChange::Type::kRemove};
      change.old_module = type;
      change.index = [self indexForMagicStackModule:type];
      CHECK(change.index != NSNotFound);
      [self.consumer updateMagicStackOrder:change];
    }
  }
}

- (void)untrackParcel:(NSString*)parcelID {
  _shoppingService->StopTrackingParcel(base::SysNSStringToUTF8(parcelID),
                                       base::BindOnce(^(bool){
                                       }));
}

- (void)trackParcel:(NSString*)parcelID carrier:(ParcelType)carrier {
  commerce::ParcelIdentifier::Carrier carrierValue =
      [self carrierValueForParcelType:carrier];
  _shoppingService->StartTrackingParcels(
      {std::make_pair(carrierValue, base::SysNSStringToUTF8(parcelID))},
      std::string(),
      base::BindOnce(
          ^(bool, std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>){
          }));
}

#pragma mark - AppStateObserver

// Conditionally starts the Safety Check if the upcoming init stage is
// `InitStageFinal` and the Safety Check state indicates it's running.
//
// NOTE: It's safe to call `StartSafetyCheck()` multiple times, because calling
// `StartSafetyCheck()` on an already-running Safety Check is a no-op.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage {
  if (IsSafetyCheckMagicStackEnabled() &&
      !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState) &&
      nextInitStage == InitStageFinal && appState.firstSceneHasInitializedUI &&
      _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
    IOSChromeSafetyCheckManager* safetyCheckManager =
        IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
            _browser->GetBrowserState());

    safetyCheckManager->StartSafetyCheck();
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (IsIOSSetUpListEnabled()) {
        // User has signed in, mark SetUpList item complete. Delayed to allow
        // Signin UI flow to be fully dismissed before starting SetUpList
        // completion animation.
        PrefService* localState = _localState;
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce(^{
              set_up_list_prefs::MarkItemComplete(
                  localState, SetUpListItemType::kSignInSync);
            }),
            base::Seconds(0.5));
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - SetUpListDelegate

- (void)setUpListItemDidComplete:(SetUpListItem*)item {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    if ([weakSelf.setUpList allItemsComplete]) {
      [weakSelf.consumer showSetUpListDoneWithAnimations:^{
        if (!IsMagicStackEnabled()) {
          [self.feedDelegate contentSuggestionsWasUpdated];
        }
      }];
    } else if (IsMagicStackEnabled()) {
      [self.consumer scrollToNextMagicStackModuleForCompletedModule:
                         SetUpListModuleTypeForSetUpListType(item.type)];
    }
  };
  [self.consumer markSetUpListItemComplete:item.type completion:completion];
}

#pragma mark - ContentSuggestionsCommands

- (void)openMostVisitedItem:(NSObject*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  // Checks if the item is a shortcut tile. Does not include Most Visited URL
  // tiles.
  if ([item isKindOfClass:[ContentSuggestionsMostVisitedActionItem class]]) {
    ContentSuggestionsMostVisitedActionItem* mostVisitedItem =
        base::apple::ObjCCastStrict<ContentSuggestionsMostVisitedActionItem>(
            item);
    if (mostVisitedItem.disabled) {
      return;
    }
    [self.NTPMetricsDelegate shortcutTileOpened];
    [self.contentSuggestionsMetricsRecorder
        recordShortcutTileTapped:mostVisitedItem.collectionShortcutType];
    switch (mostVisitedItem.collectionShortcutType) {
      case NTPCollectionShortcutTypeBookmark:
        LogBookmarkUseForDefaultBrowserPromo();
        [self.dispatcher showBookmarksManager];
        break;
      case NTPCollectionShortcutTypeReadingList:
        [self.dispatcher showReadingList];
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        [self.dispatcher showRecentTabs];
        break;
      case NTPCollectionShortcutTypeHistory:
        [self.dispatcher showHistory];
        break;
      case NTPCollectionShortcutTypeWhatsNew:
        SetWhatsNewUsed(self.promosManager);
        [self.dispatcher showWhatsNew];
        break;
      case NTPCollectionShortcutTypeCount:
        NOTREACHED();
        break;
    }
    return;
  }

  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::apple::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  UrlLoadParams params = UrlLoadParams::InCurrentTab(mostVisitedItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)openMostRecentTab {
  [self.NTPMetricsDelegate recentTabTileOpened];
  [self.contentSuggestionsMetricsRecorder recordMostRecentTabOpened];
  [IntentDonationHelper donateIntent:IntentType::kOpenLatestTab];
  [self hideRecentTabTile];
  WebStateList* web_state_list = self.browser->GetWebStateList();
  web::WebState* web_state =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
          ->most_recent_tab();
  if (!web_state) {
    return;
  }
  int index = web_state_list->GetIndexOfWebState(web_state);
  web_state_list->ActivateWebStateAt(index);
}

- (void)openTabResumptionItem {
  switch (_tabResumptionItem.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      // TODO(crbug.com/1478156): Add metrics.
      // TODO(crbug.com/1478156): Derank or hide the tile.
      break;
    case TabResumptionItemType::kMostRecentTab: {
      [self.NTPMetricsDelegate recentTabTileOpened];
      [self.contentSuggestionsMetricsRecorder recordMostRecentTabOpened];
      break;
    }
  }

  web::NavigationManager::WebLoadParams webLoadParams =
      web::NavigationManager::WebLoadParams(_tabResumptionItem.tabURL);
  UrlLoadParams params = UrlLoadParams::SwitchToTab(webLoadParams);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);

  [self hideTabResumption];
}

- (void)loadParcelTrackingPage:(GURL)parcelTrackingURL {
  [self.NTPMetricsDelegate parcelTrackingOpened];
  [self.contentSuggestionsMetricsRecorder
      recordMagicStackModuleEngagementForType:ContentSuggestionsModuleType::
                                                  kParcelTracking];
  UrlLoadingBrowserAgent::FromBrowser(self.browser)
      ->Load(UrlLoadParams::InCurrentTab(parcelTrackingURL));
}

#pragma mark - ContentSuggestionsGestureCommands

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index
                            fromPoint:(CGPoint)point {
  if (incognito &&
      IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:point];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  if (incognito &&
      IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:CGPointZero];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  [self openNewTabWithMostVisitedItem:item
                            incognito:incognito
                              atIndex:item.index];
}

- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  [self.contentSuggestionsMetricsRecorder recordMostVisitedTileRemoved];
  [self blockMostVisitedURL:item.URL];
  [self showMostVisitedUndoForURL:item.URL];
}

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)web_state {
  if (IsTabResumptionEnabled() && _tabResumptionItem) {
    [self hideTabResumption];
  } else {
    [self hideRecentTabTile];
  }
}

- (void)mostRecentTabFaviconUpdatedWithImage:(UIImage*)image {
  if (self.returnToRecentTabItem) {
    self.returnToRecentTabItem.icon = image;
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

- (void)mostRecentTabTitleWasUpdated:(NSString*)title {
  if (self.returnToRecentTabItem) {
    SceneState* scene =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    NSString* time_label = GetRecentTabTileTimeLabelForSceneState(scene);
    self.returnToRecentTabItem.subtitle =
        [self constructReturnToRecentTabSubtitleWithPageTitle:title
                                                   timeString:time_label];
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  if (ShouldHideMVT()) {
    return;
  }

  // This is used by the content widget.
  content_suggestions_tile_saver::SaveMostVisitedToDisk(
      mostVisited, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  self.freshMostVisitedItems = [NSMutableArray array];
  int index = 0;
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    ContentSuggestionsMostVisitedItem* item =
        ConvertNTPTile(tile, self.mostVisitedSectionInfo);
    item.commandHandler = self;
    item.incognitoAvailable = self.incognitoAvailable;
    item.index = index;
    DCHECK(index < kShortcutMinimumIndex);
    index++;
    [self.faviconMediator fetchFaviconForMostVisited:item];
    [self.freshMostVisitedItems addObject:item];
  }

  [self useFreshMostVisited];

  if (mostVisited.size() && !self.recordedPageImpression) {
    self.recordedPageImpression = YES;
    [self recordMostVisitedTilesDisplayed];
    [self.faviconMediator setMostVisitedDataForLogging:mostVisited];
    ntp_tiles::metrics::RecordPageImpression(mostVisited.size());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  // This is used by the content widget.
  content_suggestions_tile_saver::UpdateSingleFavicon(
      siteURL, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  for (ContentSuggestionsMostVisitedItem* item in self.mostVisitedItems) {
    if (item.URL == siteURL) {
      [self.faviconMediator fetchFaviconForMostVisited:item];
      return;
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    if (IsIOSSetUpListEnabled() && _setUpList) {
      [self checkIfCPEEnabled];
    }
  }
}

#pragma mark - SyncedSessionsObserver

- (void)onForeignSessionsChanged {
  DCHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
  [self showTabResumptionTile];
}

#pragma mark - Private

// Creates the initial `SafetyCheckState` based on the previous check states
// stored in Prefs, or (for development builds) the overridden check states via
// Experimental settings.
- (SafetyCheckState*)initialSafetyCheckState {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
          _browser->GetBrowserState());

  // Update Chrome check.
  absl::optional<UpdateChromeSafetyCheckState> overrideUpdateChromeState =
      experimental_flags::GetUpdateChromeSafetyCheckState();

  state.updateChromeState = overrideUpdateChromeState.value_or(
      safetyCheckManager->GetUpdateChromeCheckState());

  // Password check.
  absl::optional<PasswordSafetyCheckState> overridePasswordState =
      experimental_flags::GetPasswordSafetyCheckState();

  state.passwordState = overridePasswordState.value_or(
      safetyCheckManager->GetPasswordCheckState());

  // Safe Browsing check.
  absl::optional<SafeBrowsingSafetyCheckState> overrideSafeBrowsingState =
      experimental_flags::GetSafeBrowsingSafetyCheckState();

  state.safeBrowsingState = overrideSafeBrowsingState.value_or(
      safetyCheckManager->GetSafeBrowsingCheckState());

  // Insecure credentials.
  absl::optional<int> overrideWeakPasswordsCount =
      experimental_flags::GetSafetyCheckWeakPasswordsCount();

  absl::optional<int> overrideReusedPasswordsCount =
      experimental_flags::GetSafetyCheckReusedPasswordsCount();

  absl::optional<int> overrideCompromisedPasswordsCount =
      experimental_flags::GetSafetyCheckCompromisedPasswordsCount();

  bool passwordCountsOverride = overrideWeakPasswordsCount.has_value() ||
                                overrideReusedPasswordsCount.has_value() ||
                                overrideCompromisedPasswordsCount.has_value();

  // NOTE: If any password counts are overriden via Experimental
  // settings, all password counts will be considered overriden.
  if (passwordCountsOverride) {
    state.weakPasswordsCount = overrideWeakPasswordsCount.value_or(0);
    state.reusedPasswordsCount = overrideReusedPasswordsCount.value_or(0);
    state.compromisedPasswordsCount =
        overrideCompromisedPasswordsCount.value_or(0);
  } else {
    std::vector<password_manager::CredentialUIEntry> insecureCredentials =
        safetyCheckManager->GetInsecureCredentials();

    password_manager::InsecurePasswordCounts counts =
        password_manager::CountInsecurePasswordsPerInsecureType(
            insecureCredentials);

    state.weakPasswordsCount = counts.weak_count;
    state.reusedPasswordsCount = counts.reused_count;
    state.compromisedPasswordsCount = counts.compromised_count;
  }

  state.lastRunTime = [self latestSafetyCheckRunTimestamp];

  state.runningState = CanRunSafetyCheck(state.lastRunTime)
                           ? RunningSafetyCheckState::kRunning
                           : RunningSafetyCheckState::kDefault;

  return state;
}

// Returns the last run time of the Safety Check, regardless if the check was
// started from the Safety Check (Magic Stack) module, or the Safety Check
// Settings UI.
- (absl::optional<base::Time>)latestSafetyCheckRunTimestamp {
  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
          _browser->GetBrowserState());

  base::Time lastRunTimeViaModule =
      safetyCheckManager->GetLastSafetyCheckRunTime();

  base::Time lastRunTimeViaSettings =
      _localState->GetTime(prefs::kIosSettingsSafetyCheckLastRunTime);

  // Use the most recent Last Run Time—regardless of where the Safety Check was
  // run—to minimize user confusion.
  base::Time lastRunTime = lastRunTimeViaModule > lastRunTimeViaSettings
                               ? lastRunTimeViaModule
                               : lastRunTimeViaSettings;

  base::TimeDelta lastRunAge = base::Time::Now() - lastRunTime;

  // Only return the Last Run Time if the run happened within the last 24hr.
  return lastRunAge <= kSafetyCheckRunThreshold
             ? absl::optional<base::Time>(lastRunTime)
             : absl::nullopt;
}

- (void)configureConsumer {
  if (!self.consumer) {
    return;
  }
  if (IsMagicStackEnabled()) {
    if (base::FeatureList::IsEnabled(
            segmentation_platform::features::
                kSegmentationPlatformIosModuleRanker)) {
      [self fetchMagicStackModuleRankingFromSegmentationPlatform];
    } else {
      _latestMagicStackOrder = [self magicStackOrder];
      [self.consumer setMagicStackOrder:_latestMagicStackOrder];
    }
    if (IsTabResumptionEnabled()) {
      [self showTabResumptionTile];
    }
  }
  if (self.returnToRecentTabItem) {
    [self.consumer
        showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
  if ([self.mostVisitedItems count] && !ShouldHideMVT()) {
    [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];
  }
  if ([self shouldShowSetUpList]) {
    self.setUpList.delegate = self;
    NSArray<SetUpListItemViewData*>* items = [self setUpListItems];
    if (IsMagicStackEnabled() && [self.setUpList allItemsComplete]) {
      SetUpListItemViewData* allSetItem =
          [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAllSet
                                             complete:NO];
      [self.consumer showSetUpListWithItems:@[ allSetItem ]];
    } else {
      [self.consumer showSetUpListWithItems:items];
    }
    [self.contentSuggestionsMetricsRecorder recordSetUpListShown];
    for (SetUpListItemViewData* item in items) {
      [self.contentSuggestionsMetricsRecorder
          recordSetUpListItemShown:item.type];
    }
  }
  // Show shorcuts if:
  // 1) Magic Stack is enabled (always show shortcuts in Magic Stack).
  // 2) The Set Up List and Magic Stack are not enabled (Set Up List replaced
  // Shortcuts).
  if (!ShouldHideShortcuts() &&
      (IsMagicStackEnabled() || ![self shouldShowSetUpList])) {
    [self.consumer setShortcutTilesWithConfigs:self.actionButtonItems];
  }

  if (IsSafetyCheckMagicStackEnabled() &&
      !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState) &&
      _safetyCheckState.runningState == RunningSafetyCheckState::kDefault) {
    [self.consumer showSafetyCheck:_safetyCheckState];
  }
  if (IsIOSParcelTrackingEnabled()) {
    __weak ContentSuggestionsMediator* weakSelf = self;
    _shoppingService->GetAllParcelStatuses(base::BindOnce(^(
        bool success,
        std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>> parcels) {
      ContentSuggestionsMediator* strongSelf = weakSelf;
      if (!strongSelf || !success) {
        return;
      }
      [strongSelf parcelStatusesSuccessfullyReceived:std::move(parcels)];
    }));
  }
}

// Updates `prefs::kIosSyncSegmentsNewTabPageDisplayCount` with the number of
// remaining New Tab Page displays that include synced history in the Most
// Visited Tiles.
- (void)recordMostVisitedTilesDisplayed {
  const int displayCount =
      _localState->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount) +
      1;

  _localState->SetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount,
                          displayCount);
}

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
  if (ShouldHideMVT()) {
    return;
  }

  if (IsMagicStackEnabled()) {
    const base::Value::List& oldMostVisitedSites =
        _localState->GetList(prefs::kIosLatestMostVisitedSites);
    base::Value::List freshMostVisitedSites;
    for (ContentSuggestionsMostVisitedItem* item in self
             .freshMostVisitedItems) {
      freshMostVisitedSites.Append(item.URL.spec());
    }
    // Don't check for a change in the Most Visited Sites if the device doesn't
    // have any saved sites to begin with. This will not log for users with no
    // top sites that have a new top site, but the benefit of not logging for
    // new installs outweighs it.
    if (!oldMostVisitedSites.empty()) {
      [self lookForNewMostVisitedSite:freshMostVisitedSites
                  oldMostVisitedSites:oldMostVisitedSites];
    }
    _localState->SetList(prefs::kIosLatestMostVisitedSites,
                         std::move(freshMostVisitedSites));
  }

  self.mostVisitedItems = self.freshMostVisitedItems;
  [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];
  [self.feedDelegate contentSuggestionsWasUpdated];
}

// Logs a User Action if `freshMostVisitedSites` has at least one site that
// isn't in `oldMostVisitedSites`.
- (void)
    lookForNewMostVisitedSite:(const base::Value::List&)freshMostVisitedSites
          oldMostVisitedSites:(const base::Value::List&)oldMostVisitedSites {
  for (auto const& freshSiteURLValue : freshMostVisitedSites) {
    BOOL freshSiteInOldList = NO;
    for (auto const& oldSiteURLValue : oldMostVisitedSites) {
      if (freshSiteURLValue.GetString() == oldSiteURLValue.GetString()) {
        freshSiteInOldList = YES;
        break;
      }
    }
    if (!freshSiteInOldList) {
      // Reset impressions since freshness.
      _localState->SetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 0);
      base::RecordAction(
          base::UserMetricsAction("IOSMostVisitedTopSitesChanged"));
      return;
    }
  }
}

// Opens the `URL` in a new tab `incognito` or not. `originPoint` is the origin
// of the new tab animation if the tab is opened in background, in window
// coordinates.
- (void)openNewTabWithURL:(const GURL&)URL
                incognito:(BOOL)incognito
              originPoint:(CGPoint)originPoint {
  // Open the tab in background if it is non-incognito only.
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.SetInBackground(!incognito);
  params.in_incognito = incognito;
  params.append_to = OpenPosition::kCurrentTab;
  params.origin_point = originPoint;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPMetricsDelegate mostVisitedTileOpened];
  [self.contentSuggestionsMetricsRecorder
      recordMostVisitedTileOpened:item
                          atIndex:mostVisitedIndex
                         webState:self.webState];
}

// Shows a snackbar with an action to undo the removal of the most visited item
// with a `URL`.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak ContentSuggestionsMediator* weakSelf = self;
  action.handler = ^{
    ContentSuggestionsMediator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf allowMostVisitedURL:copiedURL];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)];
  message.action = action;
  message.category = @"MostVisitedUndo";
  [self.dispatcher showSnackbarMessage:message];
}

- (NSString*)constructReturnToRecentTabSubtitleWithPageTitle:
                 (NSString*)pageTitle
                                                  timeString:(NSString*)time {
  return [NSString stringWithFormat:@"%@%@", pageTitle, time];
}

- (BOOL)shouldShowWhatsNewActionItem {
  if (WasWhatsNewUsed()) {
    return NO;
  }

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  BOOL isSignedIn =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);

  return !isSignedIn;
}

// Returns an array that represents the order of the modules to be shown in the
// Magic Stack.
- (NSArray<NSNumber*>*)magicStackOrder {
  NSMutableArray* magicStackModules = [NSMutableArray array];
  if (IsTabResumptionEnabled() && _tabResumptionItem) {
    [magicStackModules
        addObject:@(int(ContentSuggestionsModuleType::kTabResumption))];
  }
  if ([self shouldShowSetUpList]) {
    [self addSetUpListToMagicStackOrder:magicStackModules];
  }
  if (ShouldPutMostVisitedSitesInMagicStack()) {
    [magicStackModules
        addObject:@(int(ContentSuggestionsModuleType::kMostVisited))];
  }
  [magicStackModules
      addObject:@(int(ContentSuggestionsModuleType::kShortcuts))];

  if (IsSafetyCheckMagicStackEnabled() &&
      !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
    [self addSafetyCheckToMagicStackOrder:magicStackModules];
  }

  if (IsIOSParcelTrackingEnabled()) {
    if ([_parcelTrackingItems count] > 2) {
      [magicStackModules
          addObject:@(int(
                        ContentSuggestionsModuleType::kParcelTrackingSeeMore))];
    } else {
      for (NSUInteger i = 0; i < [_parcelTrackingItems count]; i++) {
        // Magic Stack will show up to two modules to match the number of
        // parcels tracked.
        [magicStackModules
            addObject:@(int(ContentSuggestionsModuleType::kParcelTracking))];
      }
    }
  }

  return magicStackModules;
}

// Construct the Magic Stack module order from fetched results from
// Segmentation. This method adds on modules not included on the Segmentation
// side (e.g. Set Up List) and also filters out modules not ready or should not
// be presented.
- (NSArray<NSNumber*>*)segmentationMagicStackOrder {
  NSMutableArray<NSNumber*>* magicStackOrder = [NSMutableArray array];
  // Always add Set Up List at the front.
  if ([self shouldShowSetUpList]) {
    [self addSetUpListToMagicStackOrder:magicStackOrder];
  }
  for (NSNumber* moduleNumber : _magicStackOrderFromSegmentation) {
    ContentSuggestionsModuleType moduleType =
        (ContentSuggestionsModuleType)[moduleNumber intValue];
    switch (moduleType) {
      case ContentSuggestionsModuleType::kMostVisited:
        if (ShouldPutMostVisitedSitesInMagicStack()) {
          [magicStackOrder addObject:moduleNumber];
        }
        break;
      case ContentSuggestionsModuleType::kTabResumption:
        if (IsTabResumptionEnabled() &&
            !tab_resumption_prefs::IsTabResumptionDisabled(_localState) &&
            _tabResumptionItem) {
          [magicStackOrder addObject:moduleNumber];
        }
        break;
      case ContentSuggestionsModuleType::kSafetyCheck:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
        if (!IsSafetyCheckMagicStackEnabled() ||
            safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
                _localState)) {
          break;
        }
        // If ShouldHideIrrelevantModules() is enabled and it is not the first
        // ranked module, do not add it to the Magic Stack.
        if (!ShouldHideIrrelevantModules() || [magicStackOrder count] == 0) {
          [self addSafetyCheckToMagicStackOrder:magicStackOrder];
        }
        break;
      case ContentSuggestionsModuleType::kShortcuts:
        [magicStackOrder addObject:moduleNumber];
        break;
      case ContentSuggestionsModuleType::kParcelTracking:
        if (IsIOSParcelTrackingEnabled()) {
          if ([_parcelTrackingItems count] > 2) {
            [magicStackOrder addObject:@(int(ContentSuggestionsModuleType::
                                                 kParcelTrackingSeeMore))];
          } else {
            for (NSUInteger i = 0; i < [_parcelTrackingItems count]; i++) {
              // Magic Stack will show up to two modules to match the number of
              // parcels tracked.
              [magicStackOrder addObject:moduleNumber];
            }
          }
        }
        break;
      default:
        // These module types should not have been added by the logic
        // receiving the order list from Segmentation.
        NOTREACHED();
        break;
    }
  }
  return magicStackOrder;
}

- (void)fetchMagicStackModuleRankingFromSegmentationPlatform {
  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  int mvt_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kMostVisitedTilesFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          mvt_freshness_impression_count));
  int shortcuts_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kShortcutsFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shortcuts_freshness_impression_count));
  int safety_check_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kSafetyCheckFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          safety_check_freshness_impression_count));
  int tab_resumption_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kTabResumptionFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          tab_resumption_freshness_impression_count));
  int parcel_tracking_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kParcelTrackingFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          parcel_tracking_freshness_impression_count));
  __weak ContentSuggestionsMediator* weakSelf = self;
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  self.segmentationService->GetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, options, input_context,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            weakSelf.hasReceivedMagicStackResponse = YES;
            [weakSelf didReceiveSegmentationServiceResult:result];
          }));
}

- (void)didReceiveSegmentationServiceResult:
    (const segmentation_platform::ClassificationResult&)result {
  CHECK(IsMagicStackEnabled());
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return;
  }

  NSMutableArray* magicStackOrder = [NSMutableArray array];
  for (const std::string& label : result.ordered_labels) {
    if (label == segmentation_platform::kMostVisitedTiles) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kMostVisited))];
    } else if (label == segmentation_platform::kShortcuts) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kShortcuts))];
    } else if (label == segmentation_platform::kSafetyCheck) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
    } else if (label == segmentation_platform::kTabResumption) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kTabResumption))];
    } else if (label == segmentation_platform::kParcelTracking) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kParcelTracking))];
    }
  }
  _magicStackOrderFromSegmentationReceived = YES;
  _magicStackOrderFromSegmentation = magicStackOrder;
  _latestMagicStackOrder = [self segmentationMagicStackOrder];
  [self.consumer setMagicStackOrder:_latestMagicStackOrder];
}

- (void)addSetUpListToMagicStackOrder:(NSMutableArray*)order {
  if (set_up_list_utils::ShouldShowCompactedSetUpListModule()) {
    [order addObject:@(int(ContentSuggestionsModuleType::kCompactedSetUpList))];
  } else {
    if ([self.setUpList allItemsComplete]) {
      [order addObject:@(int(ContentSuggestionsModuleType::kSetUpListAllSet))];
    } else {
      for (SetUpListItem* model in self.setUpList.items) {
        [order
            addObject:@(int(SetUpListModuleTypeForSetUpListType(model.type)))];
      }
    }
  }
}

// Adds the Safety Check module to `order` based on the current Safety Check
// state.
- (void)addSafetyCheckToMagicStackOrder:(NSMutableArray*)order {
  CHECK(IsSafetyCheckMagicStackEnabled());

  int checkIssuesCount = CheckIssuesCount(_safetyCheckState);

  if (checkIssuesCount > 2) {
    [order addObject:@(int(ContentSuggestionsModuleType::
                               kSafetyCheckMultiRowOverflow))];
  } else if (checkIssuesCount > 1) {
    [order
        addObject:@(int(ContentSuggestionsModuleType::kSafetyCheckMultiRow))];
  } else {
    [order addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
  }
}

// Returns YES if the conditions are right to display the Set Up List.
- (BOOL)shouldShowSetUpList {
  if (!IsIOSSetUpListEnabled()) {
    return NO;
  }
  if (!set_up_list_utils::IsSetUpListActive(_localState)) {
    return NO;
  }

  SetUpList* setUpList = self.setUpList;
  if (!setUpList || setUpList.items.count == 0) {
    return NO;
  }

  return YES;
}

// Returns an array of all possible items in the Set Up List.
- (NSArray<SetUpListItemViewData*>*)allSetUpListItems {
  NSArray<SetUpListItem*>* items = [self.setUpList allItems];

  NSMutableArray<SetUpListItemViewData*>* allItems =
      [[NSMutableArray alloc] init];
  for (SetUpListItem* model in items) {
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];
    [allItems addObject:item];
  }
  return allItems;
}

// Returns an array of items to display in the Set Up List.
- (NSArray<SetUpListItemViewData*>*)setUpListItems {
  // Map the model objects to view objects.
  NSMutableArray<SetUpListItemViewData*>* items = [[NSMutableArray alloc] init];
  for (SetUpListItem* model in self.setUpList.items) {
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];
    [items addObject:item];
  }
  // For the compacted Set Up List Module in the Magic Stack, there will only be
  // two items shown.
  if (IsMagicStackEnabled() &&
      set_up_list_utils::ShouldShowCompactedSetUpListModule() &&
      [items count] > 2) {
    return [items subarrayWithRange:NSMakeRange(0, 2)];
  }
  return items;
}

// Checks if the CPE is enabled and marks the SetUpList Autofill item complete
// if it is.
- (void)checkIfCPEEnabled {
  __weak __typeof(self) weakSelf = self;
  scoped_refptr<base::SequencedTaskRunner> runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:^(
          ASCredentialIdentityStoreState* state) {
        if (state.isEnabled) {
          // The completion handler sent to ASCredentialIdentityStore is
          // executed on a background thread. Putting it back onto the main
          // thread to update local state prefs.
          runner->PostTask(FROM_HERE, base::BindOnce(^{
                             __typeof(self) strongSelf = weakSelf;
                             if (!strongSelf) {
                               return;
                             }
                             set_up_list_prefs::MarkItemComplete(
                                 strongSelf->_localState,
                                 SetUpListItemType::kAutofill);
                           }));
        }
      }];
}

// Hides the Set Up List with an animation.
- (void)hideSetUpList {
  [self.consumer hideSetUpListWithAnimations:^{
    [self.feedDelegate contentSuggestionsWasUpdated];
  }];
}

// Shows the tab resumption tile if there is a `_tabResumptionItem` to present.
- (void)showTabResumptionTile {
  CHECK(IsTabResumptionEnabled());
  if (!self.consumer ||
      tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
    return;
  }

  // TODO(crbug.com/1478156): Add restrictions.
  if (_tabResumptionItem) {
    [self.consumer showTabResumptionWithItem:_tabResumptionItem];
    return;
  }

  _tabResumptionHelper->SetCanSHowMostRecentItem(
      NewTabPageTabHelper::FromWebState(self.webState)
          ->ShouldShowStartSurface());

  __weak __typeof(self) weakSelf = self;
  _tabResumptionHelper->LastTabResumptionItem(^(TabResumptionItem* item) {
    [weakSelf showTabResumptionWithItem:item];
  });
}

// Shows the tab resumption tile with the given `item` configuration.
- (void)showTabResumptionWithItem:(TabResumptionItem*)item {
  _tabResumptionItem = item;
  _latestMagicStackOrder =
      base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformIosModuleRanker)
          ? [self segmentationMagicStackOrder]
          : [self magicStackOrder];
  if ([self isMagicStackOrderReady]) {
    // Only indicate the need for an explicit insertion if the tab resumption
    // item was received after building the initial Magic Stack order or getting
    // the Magic Stack Order from Segmentation.
    NSUInteger insertionIndex = [self
        indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];
    if (insertionIndex == NSNotFound) {
      return;
    }
    // Only continue on to insert Tab Resumption after `isMagicStackOrderReady`
    // if it is in the Magic Stack order
    MagicStackOrderChange change{MagicStackOrderChange::Type::kInsert,
                                 ContentSuggestionsModuleType::kTabResumption};
    change.index = insertionIndex;
    [self.consumer updateMagicStackOrder:change];
  }
  [self.consumer showTabResumptionWithItem:_tabResumptionItem];
}

// Hides the tab resumption tile.
- (void)hideTabResumption {
  [self.consumer hideTabResumption];
  _tabResumptionItem = nil;
}

// Handles a parcel tracking status fetch result from the
// commerce::ShoppingService.
- (void)parcelStatusesSuccessfullyReceived:
    (std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>)
        parcelStatuses {
  NSMutableArray* parcelItems = [NSMutableArray array];

  for (auto iter = parcelStatuses->begin(); iter != parcelStatuses->end();
       ++iter) {
    ParcelTrackingItem* item = [[ParcelTrackingItem alloc] init];
    item.parcelType = [self parcelTypeforCarrierValue:iter->carrier];
    item.estimatedDeliveryTime = iter->estimated_delivery_time;
    item.parcelID = base::SysUTF8ToNSString(iter->tracking_id);
    item.trackingURL = iter->tracking_url;
    item.status = (ParcelState)iter->state;
    [parcelItems addObject:item];

    NSDate* estimatedDeliveryTime = iter->estimated_delivery_time.ToNSDate();
    if ([estimatedDeliveryTime
            compare:[NSDate dateWithTimeIntervalSinceNow:-kTwoDays]] ==
        NSOrderedAscending) {
      // Parcel was delivered more than two days ago, make this the last time it
      // is shown by stopping tracking.
      _shoppingService->StopTrackingParcel(iter->tracking_id,
                                           base::BindOnce(^(bool){
                                           }));
    }
  }

  if ([parcelItems count] > 0) {
    _latestMagicStackOrder =
        base::FeatureList::IsEnabled(segmentation_platform::features::
                                         kSegmentationPlatformIosModuleRanker)
            ? [self segmentationMagicStackOrder]
            : [self magicStackOrder];
    for (NSUInteger index = 0; index < [_latestMagicStackOrder count];
         index++) {
      ContentSuggestionsModuleType type = (ContentSuggestionsModuleType)
          [_latestMagicStackOrder[index] intValue];
      if (type == ContentSuggestionsModuleType::kParcelTracking ||
          type == ContentSuggestionsModuleType::kParcelTrackingSeeMore) {
        MagicStackOrderChange change{MagicStackOrderChange::Type::kInsert};
        change.new_module = type;
        change.index = index;
        [self.consumer updateMagicStackOrder:change];
      }
    }
    [self.consumer showParcelTrackingItems:parcelItems];
  }
}

// Maps the carrier int value into a ParcelType.
- (ParcelType)parcelTypeforCarrierValue:(int)carrier {
  if (carrier == 1) {
    return ParcelType::kFedex;
  } else if (carrier == 2) {
    return ParcelType::kUPS;
  } else if (carrier == 4) {
    return ParcelType::kUSPS;
  }
  return ParcelType::kUnkown;
}

- (commerce::ParcelIdentifier::Carrier)carrierValueForParcelType:
    (ParcelType)parcelType {
  switch (parcelType) {
    case ParcelType::kUSPS:
      return commerce::ParcelIdentifier::Carrier(4);
    case ParcelType::kUPS:
      return commerce::ParcelIdentifier::Carrier(2);
    case ParcelType::kFedex:
      return commerce::ParcelIdentifier::Carrier(1);
    default:
      return commerce::ParcelIdentifier::Carrier(0);
  }
}

// Returns the index rank of `moduleType`.
// Callers of this need to handle a NSNotFound return case and do nothing in
// that case.
- (NSUInteger)indexForMagicStackModule:
    (ContentSuggestionsModuleType)moduleType {
  NSUInteger index = [_latestMagicStackOrder indexOfObject:@(int(moduleType))];
  if (index == NSNotFound) {
    // It is possible that a feature is enabled but the segmentation model being
    // used didn't return the feature's module (i.e. first browser session after
    // enabling a feature, so the latest model will not be downloaded until the
    // following session since experiment models are tied via finch). It is also
    // possible that the segmentation result has not returned yet.
    CHECK(base::FeatureList::IsEnabled(
        segmentation_platform::features::kSegmentationPlatformIosModuleRanker));
  }
  return index;
}

// Returns NO if client is expecting the order from Segmentation and it has not
// returned yet.
- (BOOL)isMagicStackOrderReady {
  if (base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    return _magicStackOrderFromSegmentationReceived;
  }
  return YES;
}

#pragma mark - Properties

- (NSArray<ContentSuggestionsMostVisitedActionItem*>*)actionButtonItems {
  if (!_actionButtonItems) {
    self.readingListItem = ReadingListActionItem();
    self.readingListItem.count = self.readingListUnreadCount;
    self.readingListItem.disabled = !self.readingListModelIsLoaded;
    _actionButtonItems = @[
      [self shouldShowWhatsNewActionItem] ? WhatsNewActionItem()
                                          : BookmarkActionItem(),
      self.readingListItem, RecentTabsActionItem(), HistoryActionItem()
    ];
    for (ContentSuggestionsMostVisitedActionItem* item in _actionButtonItems) {
      item.accessibilityTraits = UIAccessibilityTraitButton;
    }
  }
  return _actionButtonItems;
}

- (void)setCommandHandler:
    (id<ContentSuggestionsCommands, ContentSuggestionsGestureCommands>)
        commandHandler {
  if (_commandHandler == commandHandler)
    return;

  _commandHandler = commandHandler;

  for (ContentSuggestionsMostVisitedItem* item in self.freshMostVisitedItems) {
    item.commandHandler = commandHandler;
  }
}

- (void)setContentSuggestionsMetricsRecorder:
    (ContentSuggestionsMetricsRecorder*)contentSuggestionsMetricsRecorder {
  CHECK(self.faviconMediator);
  _contentSuggestionsMetricsRecorder = contentSuggestionsMetricsRecorder;
  self.faviconMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
}

- (BOOL)contentSuggestionsEnabled {
  return self.articleForYouEnabled->GetValue()->GetBool() &&
         self.contentSuggestionsPolicyEnabled->GetValue()->GetBool();
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (IsIOSSetUpListEnabled()) {
    if (preferenceName == prefs::kIosCredentialProviderPromoLastActionTaken &&
        CredentialProviderPromoDismissed(_localState)) {
      set_up_list_prefs::MarkItemComplete(_localState,
                                          SetUpListItemType::kAutofill);
    } else if (preferenceName == set_up_list_prefs::kDisabled &&
               set_up_list_prefs::IsSetUpListDisabled(_localState)) {
      [self hideSetUpList];
    }
  }
  if (IsTabResumptionEnabled()) {
    if (_tabResumptionItem &&
        tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
      [self hideTabResumption];
    }
  }

  if (IsSafetyCheckMagicStackEnabled() &&
      (preferenceName == prefs::kIosSettingsSafetyCheckLastRunTime ||
       preferenceName ==
           prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult)) {
    _safetyCheckState.lastRunTime = [self latestSafetyCheckRunTimestamp];

    _safetyCheckState.safeBrowsingState =
        SafeBrowsingSafetyCheckStateForName(
            _localState->GetString(
                prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult))
            .value_or(_safetyCheckState.safeBrowsingState);

    // Trigger a module update when the Last Run Time, or Safe Browsing state,
    // has changed.
    [self runningStateChanged:_safetyCheckState.runningState];
  }
}

#pragma mark - SafetyCheckManagerObserver

- (void)passwordCheckStateChanged:(PasswordSafetyCheckState)state {
  _safetyCheckState.passwordState = state;

  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      safetyCheckManager->GetInsecureCredentials();

  password_manager::InsecurePasswordCounts counts =
      password_manager::CountInsecurePasswordsPerInsecureType(
          insecureCredentials);

  _safetyCheckState.weakPasswordsCount = counts.weak_count;
  _safetyCheckState.reusedPasswordsCount = counts.reused_count;
  _safetyCheckState.compromisedPasswordsCount = counts.compromised_count;
}

- (void)safeBrowsingCheckStateChanged:(SafeBrowsingSafetyCheckState)state {
  _safetyCheckState.safeBrowsingState = state;
}

- (void)updateChromeCheckStateChanged:(UpdateChromeSafetyCheckState)state {
  _safetyCheckState.updateChromeState = state;
}

- (void)runningStateChanged:(RunningSafetyCheckState)state {
  _safetyCheckState.runningState = state;

  if (safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
    // Safety Check can be disabled by long-pressing the module, so
    // SafetyCheckManager can still be running and returning results even after
    // disabling.
    return;
  }

  // Ensures the consumer gets the latest Safety Check state only when the
  // running state changes; this avoids calling the consumer every time an
  // individual check state changes.
  [self.consumer showSafetyCheck:_safetyCheckState];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self readingListModelDidApplyChanges:model];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  self.readingListUnreadCount = model->unread_size();
  self.readingListModelIsLoaded = model->loaded();
  if (self.readingListItem) {
    self.readingListItem.count = self.readingListUnreadCount;
    self.readingListItem.disabled = !self.readingListModelIsLoaded;
    [self.consumer updateShortcutTileConfig:self.readingListItem];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (!_setUpList) {
    return;
  }
  if (_syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      HasManagedSyncDataType(_syncService)) {
    // Sync is now disabled, so mark the SetUpList item complete so that it
    // cannot be used again.
    set_up_list_prefs::MarkItemComplete(_localState,
                                        SetUpListItemType::kSignInSync);
  }
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  if (_setUpList) {
    switch (_authenticationService->GetServiceStatus()) {
      case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      case AuthenticationService::ServiceStatus::SigninAllowed:
        break;
      case AuthenticationService::ServiceStatus::SigninDisabledByUser:
      case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
        // Signin is now disabled, so mark the SetUpList item complete so that
        // it cannot be used again.
        set_up_list_prefs::MarkItemComplete(_localState,
                                            SetUpListItemType::kSignInSync);
    }
  }
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/page_image_service/features.h"
#import "components/page_image_service/image_service.h"
#import "components/page_image_service/mojom/page_image_service.mojom.h"
#import "components/sessions/core/session_id.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/visited_url_ranking/public/url_visit_util.h"
#import "components/visited_url_ranking/public/visited_url_ranking_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/page_image/model/page_image_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_removal_observer_bridge.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_bridge.h"
#import "ios/chrome/browser/tabs/model/tab_sync_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/visited_url_ranking/model/visited_url_ranking_service_factory.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

// Helper function to extract tab data from url aggregate.
// Try first the session tab data, then the tab model tab data.
const visited_url_ranking::URLVisitAggregate::TabData* ExtractTabData(
    const visited_url_ranking::URLVisitAggregate& url_aggregate) {
  const auto& session_iterator = url_aggregate.fetcher_data_map.find(
      visited_url_ranking::Fetcher::kSession);
  if (session_iterator != url_aggregate.fetcher_data_map.end()) {
    const visited_url_ranking::URLVisitAggregate::URLVisitVariant&
        url_visit_variant = session_iterator->second;
    const visited_url_ranking::URLVisitAggregate::TabData* tab_data =
        std::get_if<visited_url_ranking::URLVisitAggregate::TabData>(
            &url_visit_variant);
    if (tab_data) {
      return tab_data;
    }
  }

  const auto& tab_model_iterator = url_aggregate.fetcher_data_map.find(
      visited_url_ranking::Fetcher::kTabModel);
  if (tab_model_iterator != url_aggregate.fetcher_data_map.end()) {
    const visited_url_ranking::URLVisitAggregate::URLVisitVariant&
        url_visit_variant = tab_model_iterator->second;
    const visited_url_ranking::URLVisitAggregate::TabData* tab_data =
        std::get_if<visited_url_ranking::URLVisitAggregate::TabData>(
            &url_visit_variant);
    if (tab_data) {
      return tab_data;
    }
  }
  return nullptr;
}

// Helper function to extract history data from url aggregate.
const visited_url_ranking::URLVisitAggregate::HistoryData* ExtractHistoryData(
    const visited_url_ranking::URLVisitAggregate& url_aggregate) {
  const auto& history_iterator = url_aggregate.fetcher_data_map.find(
      visited_url_ranking::Fetcher::kHistory);
  if (history_iterator != url_aggregate.fetcher_data_map.end()) {
    const visited_url_ranking::URLVisitAggregate::URLVisitVariant&
        url_visit_variant = history_iterator->second;
    const visited_url_ranking::URLVisitAggregate::HistoryData* history_data =
        std::get_if<visited_url_ranking::URLVisitAggregate::HistoryData>(
            &url_visit_variant);
    if (history_data) {
      return history_data;
    }
  }
  return nullptr;
}

// Whether the item should be displayed immediately (before fetching an image).
bool ShouldShowItemImmediately() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTabResumptionShowItemImmediately);
}

// Salient images should come from gstatic.com.
const char kGStatic[] = ".gstatic.com";

// Overrides the reason for testing purpose.
NSString* GetOverridenReason(
    const visited_url_ranking::URLVisitAggregate& url_aggregate) {
  NSString* override_flag =
      experimental_flags::GetTabResumptionDecorationOverride();
  if (![override_flag length]) {
    return nil;
  }
  if ([override_flag isEqualToString:@"MostRecent"]) {
    return base::SysUTF16ToNSString(visited_url_ranking::GetStringForDecoration(
        visited_url_ranking::DecorationType::kMostRecent));
  }
  if ([override_flag isEqualToString:@"FrequentlyVisited"]) {
    return base::SysUTF16ToNSString(visited_url_ranking::GetStringForDecoration(
        visited_url_ranking::DecorationType::kFrequentlyVisited));
  }
  if ([override_flag isEqualToString:@"FrequentlyVisitedAtTime"]) {
    return base::SysUTF16ToNSString(visited_url_ranking::GetStringForDecoration(
        visited_url_ranking::DecorationType::kFrequentlyVisitedAtTime));
  }
  if ([override_flag isEqualToString:@"VisitedSomeTimeAgoRecent"]) {
    return base::SysUTF16ToNSString(visited_url_ranking::GetStringForDecoration(
        visited_url_ranking::DecorationType::kVisitedXAgo, true));
  }
  if ([override_flag isEqualToString:@"VisitedSomeTimeAgoOld"]) {
    return base::SysUTF16ToNSString(
        visited_url_ranking::GetStringForRecencyDecorationWithTime(
            url_aggregate.GetLastVisitTime()));
  }
  return nil;
}

}  // namespace

@interface TabResumptionMediator () <BooleanObserver,
                                     IdentityManagerObserverBridgeDelegate,
                                     MagicStackModuleDelegate,
                                     StartSurfaceRecentTabObserving,
                                     SyncedSessionsObserver,
                                     SyncObserverModelBridge,
                                     TabResumptionCommands>
// readwrite override.
@property(nonatomic, strong, readwrite) TabResumptionItem* itemConfig;

@end

@implementation TabResumptionMediator {
  // Last distant tab resumption item URL.
  GURL _lastDistinctItemURL;
  // Tab identifier of the last distant tab resumption item.
  std::optional<SessionID> _tabId;
  // Session tag of the last distant tab resumption item.
  std::string _sessionTag;
  BOOL _isOffTheRecord;

  // The last item that is returned by the model.
  // The URL/title will be used to not fetch again images if the same item is
  // returned twice, or to ignore update on obsolete items.
  TabResumptionItem* _pendingItem;

  // The owning Browser.
  raw_ptr<Browser> _browser;
  raw_ptr<PrefService> _localState;
  raw_ptr<PrefService> _profilePrefs;
  SceneState* _sceneState;
  // Loads favicons.
  raw_ptr<FaviconLoader> _faviconLoader;
  // Browser Agent that manages the most recent WebState.
  raw_ptr<StartSurfaceRecentTabBrowserAgent> _recentTabBrowserAgent;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> _sessionSyncService;
  // KeyedService responsible for sync state.
  raw_ptr<syncer::SyncService> _syncService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingBrowserAgent;
  raw_ptr<WebStateList> _webStateList;
  // KeyedService for Tab resumption 2.0.
  raw_ptr<visited_url_ranking::VisitedURLRankingService>
      _visitedURLRankingService;
  // KeyedService for Salient images.
  raw_ptr<page_image_service::ImageService> _pageImageService;
  // Observer bridge for mediator to listen to
  // StartSurfaceRecentTabObserverBridge.
  std::unique_ptr<StartSurfaceRecentTabObserverBridge> _startSurfaceObserver;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  // Observer for changes to the user's identity state.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserverBridge;

  // Whether the item is currently presented as Top Module by Magic Stack.
  BOOL _currentlyTopModule;
  PrefBackedBoolean* _tabResumptionDisabled;
}

- (instancetype)initWithLocalState:(PrefService*)localState
                       prefService:(PrefService*)prefService
                   identityManager:(signin::IdentityManager*)identityManager
                           browser:(Browser*)browser {
  self = [super init];
  if (self) {
    CHECK(IsTabResumptionEnabled());
    _localState = localState;
    _profilePrefs = prefService;
    _browser = browser;
    _tabId = SessionID::InvalidValue();
    _sceneState = _browser->GetSceneState();
    _webStateList = _browser->GetWebStateList();
    _isOffTheRecord = _browser->GetProfile()->IsOffTheRecord();

    if (IsHomeCustomizationEnabled()) {
      _tabResumptionDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_profilePrefs
                     prefName:
                         prefs::
                             kHomeCustomizationMagicStackTabResumptionEnabled];
      [_tabResumptionDisabled setObserver:self];
    } else {
      _tabResumptionDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_localState
                     prefName:tab_resumption_prefs::kTabResumptioDisabledPref];
      [_tabResumptionDisabled setObserver:self];
    }

    ProfileIOS* profile = _browser->GetProfile();
    _sessionSyncService = SessionSyncServiceFactory::GetForProfile(profile);
    _syncService = SyncServiceFactory::GetForProfile(profile);
    _faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(profile);
    _recentTabBrowserAgent =
        StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser);
    _URLLoadingBrowserAgent = UrlLoadingBrowserAgent::FromBrowser(_browser);
    _startSurfaceObserver =
        std::make_unique<StartSurfaceRecentTabObserverBridge>(self);
    StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser)->AddObserver(
        _startSurfaceObserver.get());
    if (IsTabResumption1_5Enabled()) {
      _pageImageService = PageImageServiceFactory::GetForProfile(profile);
    }
    _imageFetcher = std::make_unique<image_fetcher::ImageDataFetcher>(
        profile->GetSharedURLLoaderFactory());

    if (IsTabResumption2_0Enabled()) {
      _visitedURLRankingService =
          VisitedURLRankingServiceFactory::GetForProfile(profile);
    }

    if (IsTabResumption2_0Enabled() ||
        !IsTabResumptionEnabledForMostRecentTabOnly()) {
      // Tab resumption 2.0 will get foreign tabs and so needs to register to
      // sync/identity notifications.
      _syncedSessionsObserverBridge.reset(
          new synced_sessions::SyncedSessionsObserverBridge(
              self, _sessionSyncService));

      _syncObserverModelBridge.reset(
          new SyncObserverBridge(self, _syncService));
      _identityManagerObserverBridge.reset(
          new signin::IdentityManagerObserverBridge(identityManager, self));
    }
  }
  return self;
}

- (void)disconnect {
  _syncedSessionsObserverBridge.reset();
  if (_startSurfaceObserver) {
    _recentTabBrowserAgent->RemoveObserver(_startSurfaceObserver.get());
    _startSurfaceObserver.reset();
  }
  _recentTabBrowserAgent = nullptr;
  _syncObserverModelBridge.reset();
  _identityManagerObserverBridge.reset();
  [_tabResumptionDisabled setObserver:nil];
  _tabResumptionDisabled = nil;
}

#pragma mark - Public methods

- (void)openTabResumptionItem:(TabResumptionItem*)item {
  [self.contentSuggestionsMetricsRecorder recordTabResumptionTabOpened];
  tab_resumption_prefs::SetTabResumptionLastOpenedTabURL(item.tabURL,
                                                         _profilePrefs);
  [self.delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                    kTabResumption];

  NSUInteger index = [self.delegate
      indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];
  if (IsTabResumption2_0Enabled() && index == 0 && _visitedURLRankingService) {
    _visitedURLRankingService->RecordAction(visited_url_ranking::kActivated,
                                            self.itemConfig.URLKey,
                                            self.itemConfig.requestID);
  }

  switch (item.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      [self.NTPActionsDelegate distantTabResumptionOpenedAtIndex:index];
      [self openDistantTab:item];
      break;
    case TabResumptionItemType::kMostRecentTab: {
      [self.NTPActionsDelegate recentTabTileOpenedAtIndex:index];
      [IntentDonationHelper donateIntent:IntentType::kOpenLatestTab];
      web::NavigationManager::WebLoadParams webLoadParams =
          web::NavigationManager::WebLoadParams(item.tabURL);
      UrlLoadParams params = UrlLoadParams::SwitchToTab(webLoadParams);
      params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
      _URLLoadingBrowserAgent->Load(params);
      break;
    }
  }
  [self.delegate removeTabResumptionModule];
}

- (void)openDistantTab:(TabResumptionItem*)item {
  ProfileIOS* profile = _browser->GetProfile();
  sync_sessions::OpenTabsUIDelegate* openTabsDelegate =
      SessionSyncServiceFactory::GetForProfile(profile)
          ->GetOpenTabsUIDelegate();
  const sessions::SessionTab* sessionTab = nullptr;
  if (openTabsDelegate->GetForeignTab(_sessionTag, _tabId.value(),
                                      &sessionTab)) {
    bool isNTP = _webStateList->GetActiveWebState()->GetVisibleURL() ==
                 kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(
        _isOffTheRecord, isNTP,
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);

    std::unique_ptr<web::WebState> webState =
        session_util::CreateWebStateWithNavigationEntries(
            profile, sessionTab->current_navigation_index,
            sessionTab->navigations);
    _webStateList->ReplaceWebStateAt(_webStateList->active_index(),
                                     std::move(webState));
  } else {
    web::NavigationManager::WebLoadParams webLoadParams =
        web::NavigationManager::WebLoadParams(item.tabURL);
    UrlLoadParams params = UrlLoadParams::InCurrentTab(webLoadParams);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    _URLLoadingBrowserAgent->Load(params);
  }
}

- (void)disableModule {
  tab_resumption_prefs::DisableTabResumption(
      IsHomeCustomizationEnabled() ? _profilePrefs : _localState);
}

- (void)setDelegate:(id<TabResumptionHelperDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLastTabResumptionItem];
  }
}

#pragma mark - MagicStackModuleDelegate

- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index {
  CHECK(self.itemConfig == magicStackModule);
  _currentlyTopModule = (index == 0);
  if (IsTabResumption2_0Enabled() && index == 0 && _visitedURLRankingService) {
    _visitedURLRankingService->RecordAction(visited_url_ranking::kSeen,
                                            self.itemConfig.URLKey,
                                            self.itemConfig.requestID);
  }
  switch (self.itemConfig.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      [self.NTPActionsDelegate distantTabResumptionDisplayedAtIndex:index];
      break;
    case TabResumptionItemType::kMostRecentTab:
      [self.NTPActionsDelegate recentTabTileDisplayedAtIndex:index];
      break;
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _tabResumptionDisabled) {
    if ((IsHomeCustomizationEnabled() && !observableBoolean.value) ||
        (!IsHomeCustomizationEnabled() && observableBoolean.value)) {
      [self.delegate removeTabResumptionModule];
    }
  }
}

#pragma mark - SyncObserverBridge

- (void)onSyncStateChanged {
  // If tabs are not synced, hide the tab resumption tile.
  if (!_syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    [self.delegate removeTabResumptionModule];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      // If the user is signed out, remove the tab resumption tile.
      [self.delegate removeTabResumptionModule];
      self.itemConfig = nil;
      break;
    }
    default:
      break;
  }
}

#pragma mark - SyncedSessionsObserver

- (void)onForeignSessionsChanged {
  [self fetchLastTabResumptionItem];
}

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)webState {
  if (self.itemConfig && self.itemConfig.itemType == kMostRecentTab) {
    [self.delegate removeTabResumptionModule];
    _currentlyTopModule = NO;
    self.itemConfig = nil;
  }
}

- (void)mostRecentTab:(web::WebState*)webState
    faviconUpdatedWithImage:(UIImage*)image {
}

- (void)mostRecentTab:(web::WebState*)webState
      titleWasUpdated:(NSString*)title {
}

#pragma mark - Private

// Fetches the item to display from the model.
- (void)fetchLastTabResumptionItem {
  if (tab_resumption_prefs::IsTabResumptionDisabled(
          IsHomeCustomizationEnabled() ? _profilePrefs : _localState)) {
    return;
  }
  if (_visitedURLRankingService && IsTabResumption2_0Enabled()) {
    __weak __typeof(self) weakSelf = self;
    _visitedURLRankingService->FetchURLVisitAggregates(
        visited_url_ranking::FetchOptions::
            CreateDefaultFetchOptionsForTabResumption(),
        base::BindOnce(
            ^(visited_url_ranking::ResultStatus status,
              visited_url_ranking::URLVisitsMetadata url_visits_metadata,
              std::vector<visited_url_ranking::URLVisitAggregate> urls) {
              [weakSelf onURLFetched:std::move(urls)
                        withMetadata:url_visits_metadata
                          withStatus:status];
            }));
    return;
  }

  _sessionTag = "";
  _tabId = SessionID::InvalidValue();

  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    // If sync is enabled and `GetOpenTabsUIDelegate()` returns nullptr, that
    // means the `_sessionSyncService` is not fully operational.
    if (_syncService->IsSyncFeatureEnabled() &&
        !_sessionSyncService->GetOpenTabsUIDelegate()) {
      return;
    }
  }

  base::Time mostRecentTabOpenedTime = base::Time::UnixEpoch();
  base::Time lastSyncedTabSyncedTime = base::Time::UnixEpoch();

  web::WebState* mostRecentTab = _recentTabBrowserAgent->most_recent_tab();
  if (mostRecentTab) {
    NSDate* mostRecentTabDate = base::apple::ObjCCastStrict<NSDate>([_sceneState
        sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime]);
    if (mostRecentTabDate != nil) {
      mostRecentTabOpenedTime = base::Time::FromNSDate(mostRecentTabDate);
    }
  }

  const synced_sessions::DistantSession* session = nullptr;
  const synced_sessions::DistantTab* tab = nullptr;
  auto const syncedSessions =
      std::make_unique<synced_sessions::SyncedSessions>(_sessionSyncService);
  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    LastActiveDistantTab lastDistantTab = GetLastActiveDistantTab(
        syncedSessions.get(), TabResumptionForXDevicesTimeThreshold());
    if (lastDistantTab.tab) {
      tab = lastDistantTab.tab;
      if (_lastDistinctItemURL != tab->virtual_url) {
        _lastDistinctItemURL = tab->virtual_url;
        session = lastDistantTab.session;
        lastSyncedTabSyncedTime = tab->last_active_time;
      }
    }
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  bool canShowMostRecentItem =
      activeWebState && NewTabPageTabHelper::FromWebState(activeWebState)
                            ->ShouldShowStartSurface();
  // If both times have not been updated, that means there is no item to return.
  if (mostRecentTabOpenedTime == base::Time::UnixEpoch() &&
      lastSyncedTabSyncedTime == base::Time::UnixEpoch()) {
    return;
  } else if (lastSyncedTabSyncedTime > mostRecentTabOpenedTime) {
    CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
    [self fetchLastSyncedTabItemFromLastActiveDistantTab:tab session:session];
    _sessionTag = session->tag;
    _tabId = tab->tab_id;
  } else if (canShowMostRecentItem) {
    [self fetchMostRecentTabItemFromWebState:mostRecentTab
                                  openedTime:mostRecentTabOpenedTime];
  }
}

// Fetches a relevant image for the `item` to display.
- (void)fetchImageForItem:(TabResumptionItem*)item {
  if ([self isPendingItem:item]) {
    // The item was already fetched or is being fetched, ignore it.
    return;
  }
  _pendingItem = item;
  if (ShouldShowItemImmediately()) {
    [self showItem:item];
  }
  if (item.itemType == kMostRecentTab) {
    [self fetchSnapshotForItem:item];
  } else {
    [self fetchSalientImageForItem:item];
  }
  [self fetchFaviconForItem:item];
}

// Fetches the snapshot of the tab showing `item`.
- (void)fetchSnapshotForItem:(TabResumptionItem*)item {
  if (!IsTabResumptionImagesThumbnailsEnabled()) {
    return [self fetchSalientImageForItem:item];
  }
  BrowserList* browserList =
      BrowserListFactory::GetForProfile(_browser->GetProfile());
  for (Browser* browser : browserList->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* const webStateList = browser->GetWebStateList();
    const int index = webStateList->GetIndexOfWebStateWithURL(item.tabURL);
    if (index == WebStateList::kInvalidIndex) {
      continue;
    }
    web::WebState* webState = webStateList->GetWebStateAt(index);
    if (!webState) {
      continue;
    }
    __weak TabResumptionMediator* weakSelf = self;
    SnapshotTabHelper* snapshotTabHelper =
        SnapshotTabHelper::FromWebState(webState);
    snapshotTabHelper->RetrieveColorSnapshot(^(UIImage* image) {
      [weakSelf snapshotFetched:image forItem:item];
    });
    return;
  }
  return [self fetchSalientImageForItem:item];
}

// The snapshot of the tab showing `item` was fetched.
- (void)snapshotFetched:(UIImage*)image forItem:(TabResumptionItem*)item {
  if (!image) {
    return [self fetchSalientImageForItem:item];
  }
  item.contentImage = image;
  [self showItem:item];
}

// Fetches the salient image for `item`.
- (void)fetchSalientImageForItem:(TabResumptionItem*)item {
  if (!IsTabResumptionImagesSalientEnabled() || !_pageImageService ||
      !base::FeatureList::IsEnabled(page_image_service::kImageService)) {
    return;
  }
  __weak TabResumptionMediator* weakSelf = self;
  page_image_service::mojom::Options options;
  options.optimization_guide_images = true;
  options.suggest_images = false;
  _pageImageService->FetchImageFor(
      page_image_service::mojom::ClientId::NtpTabResumption, item.tabURL,
      options, base::BindOnce(^(const GURL& URL) {
        [weakSelf salientImageURLReceived:URL forItem:item];
      }));
}

// The URL for the salient image has been received. Download the image if it
// is valid or fallbacks to favicon.
- (void)salientImageURLReceived:(const GURL&)URL
                        forItem:(TabResumptionItem*)item {
  __weak TabResumptionMediator* weakSelf = self;
  if (!URL.is_valid() || !URL.SchemeIsCryptographic() ||
      !base::EndsWith(URL.host(), kGStatic)) {
    return;
  }
  _imageFetcher->FetchImageData(
      URL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf salientImageReceived:imageData forItem:item];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Salient image has been received. Display it.
- (void)salientImageReceived:(const std::string&)imageData
                     forItem:(TabResumptionItem*)item {
  UIImage* image =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.c_str()
                                            length:imageData.size()]];
  if (!image) {
    return;
  }
  item.contentImage = image;
  [self showItem:item];
}

// Fetches the favicon for `item`.
- (void)fetchFaviconForItem:(TabResumptionItem*)item {
  __weak TabResumptionMediator* weakSelf = self;

  _faviconLoader->FaviconForPageUrl(
      item.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        [weakSelf faviconReceived:attributes forItem:item];
      });
}

// The favicon has been received. Display it.
- (void)faviconReceived:(FaviconAttributes*)attributes
                forItem:(TabResumptionItem*)item {
  if (!attributes.usesDefaultImage) {
    if ([UIImagePNGRepresentation(item.faviconImage)
            isEqual:UIImagePNGRepresentation(attributes.faviconImage)]) {
      return;
    }
    item.faviconImage = attributes.faviconImage;
    [self showItem:item];
  }
}

// Sends `item` to  TabResumption to be displayed.
- (void)showItem:(TabResumptionItem*)item {
  if (![self isPendingItem:item]) {
    // A new item has been fetched, ignore.
    return;
  }
  if (!self.itemConfig) {
    self.itemConfig = item;
    [self.delegate tabResumptionHelperDidReceiveItem];
    return;
  }

  if (IsTabResumption2_0Enabled() && _currentlyTopModule &&
      _visitedURLRankingService) {
    // If the item is currently displayed, report the display of the new URL.
    if (item.requestID != self.itemConfig.requestID ||
        item.URLKey != self.itemConfig.URLKey) {
      _visitedURLRankingService->RecordAction(visited_url_ranking::kSeen,
                                              self.itemConfig.URLKey,
                                              self.itemConfig.requestID);
    }
  }

  // The item is already used by some view, so it cannot be replaced.
  // Instead the existing config must be updated.
  [self.itemConfig reconfigureWithItem:item];
  [self.delegate tabResumptionHelperDidReconfigureItem];
}

// Creates a TabResumptionItem corresponding to the last synced tab.
- (void)fetchLastSyncedTabItemFromLastActiveDistantTab:
            (const synced_sessions::DistantTab*)tab
                                               session:(const synced_sessions::
                                                            DistantSession*)
                                                           session {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
  TabResumptionItem* item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  item.sessionName = base::SysUTF8ToNSString(session->name);
  item.tabTitle = base::SysUTF16ToNSString(tab->title);
  item.syncedTime = tab->last_active_time;
  item.tabURL = tab->virtual_url;
  item.commandHandler = self;
  item.delegate = self;
  item.shouldShowSeeMore = IsTabResumption1_5SeeMoreEnabled();
  // Fetch the image.
  [self fetchImageForItem:item];
}

// Creates a TabResumptionItem corresponding to the `webState`.
- (void)fetchMostRecentTabItemFromWebState:(web::WebState*)webState
                                openedTime:(base::Time)openedTime {
  TabResumptionItem* item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  item.tabTitle = base::SysUTF16ToNSString(webState->GetTitle());
  item.syncedTime = openedTime;
  item.tabURL = webState->GetLastCommittedURL();
  item.commandHandler = self;
  item.delegate = self;
  item.shouldShowSeeMore = IsTabResumption1_5SeeMoreEnabled();
  // Fetch the image.
  [self fetchImageForItem:item];
}

// Compares `item` and `_pendingItem` on tabURL and tabTitle field.
- (BOOL)isPendingItem:(TabResumptionItem*)item {
  if (_pendingItem == nil) {
    return NO;
  }
  return item.tabURL == _pendingItem.tabURL &&
         [item.tabTitle isEqualToString:_pendingItem.tabTitle];
}

#pragma mark - Private method for Tab resumption 2.0 tab fetch.

// Called when the URLs have been fetched from the different fetcher.
// This method just forwards the URLs to the ranker.
- (void)onURLFetched:(std::vector<visited_url_ranking::URLVisitAggregate>)URLs
        withMetadata:(const visited_url_ranking::URLVisitsMetadata&)metadata
          withStatus:(visited_url_ranking::ResultStatus)status {
  if (status != visited_url_ranking::ResultStatus::kSuccess) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  visited_url_ranking::Config config = {
      .key = visited_url_ranking::kTabResumptionRankerKey};
  _visitedURLRankingService->RankURLVisitAggregates(
      config, std::move(URLs),
      base::BindOnce(
          ^(visited_url_ranking::URLVisitsMetadata local_metadata,
            visited_url_ranking::ResultStatus rankStatus,
            std::vector<visited_url_ranking::URLVisitAggregate> rankedURLs) {
            [weakSelf onURLRanked:std::move(rankedURLs)
                     withMetadata:local_metadata
                       withStatus:rankStatus];
          },
          metadata));
}

// Called when the URLs have been ranked. Select the first one and decorate it.
- (void)onURLRanked:(std::vector<visited_url_ranking::URLVisitAggregate>)URLs
       withMetadata:(const visited_url_ranking::URLVisitsMetadata&)metadata
         withStatus:(visited_url_ranking::ResultStatus)status {
  if (status != visited_url_ranking::ResultStatus::kSuccess ||
      URLs.size() == 0) {
    return;
  }
  if (!IsTabResumption2ReasonEnabled()) {
    return [self onURLDecorated:std::move(URLs) withStatus:status];
  }

  size_t index;
  // Select the first URL with tab data.
  for (index = 0; index < URLs.size(); index++) {
    if (ExtractTabData(URLs[index]) || ExtractHistoryData(URLs[index])) {
      break;
    }
  }
  if (index == URLs.size()) {
    return;
  }

  std::vector<visited_url_ranking::URLVisitAggregate> selectedURLs;
  selectedURLs.push_back(std::move(URLs[index]));

  __weak __typeof(self) weakSelf = self;
  _visitedURLRankingService->DecorateURLVisitAggregates(
      {}, metadata, std::move(selectedURLs),
      base::BindOnce(
          ^(visited_url_ranking::ResultStatus decorateStatus,
            std::vector<visited_url_ranking::URLVisitAggregate> decoratedURLs) {
            [weakSelf onURLDecorated:std::move(decoratedURLs)
                          withStatus:decorateStatus];
          }));
}

// Called when the URLs have been decorated.
- (void)onURLDecorated:(std::vector<visited_url_ranking::URLVisitAggregate>)URLs
            withStatus:(visited_url_ranking::ResultStatus)status {
  if (status != visited_url_ranking::ResultStatus::kSuccess ||
      URLs.size() == 0) {
    return;
  }
  const visited_url_ranking::URLVisitAggregate::TabData* tabData = nullptr;
  const visited_url_ranking::URLVisitAggregate::HistoryData* historyData =
      nullptr;
  const visited_url_ranking::URLVisit* visit = nullptr;

  const visited_url_ranking::URLVisitAggregate* URLAggregate = nullptr;
  for (auto& aggregate : URLs) {
    tabData = ExtractTabData(aggregate);
    if (tabData) {
      URLAggregate = &aggregate;
      visit = &tabData->last_active_tab.visit;
      break;
    }
    historyData = ExtractHistoryData(aggregate);
    if (historyData) {
      URLAggregate = &aggregate;
      visit = &historyData->visit;
      break;
    }
  }
  if (!URLAggregate || !visit) {
    return;
  }

  bool isLocal =
      visit->source != visited_url_ranking::URLVisit::Source::kForeign;
  TabResumptionItemType type =
      (isLocal ? TabResumptionItemType::kMostRecentTab
               : TabResumptionItemType::kLastSyncedTab);
  TabResumptionItem* item = [[TabResumptionItem alloc] initWithItemType:type];
  item.tabTitle = base::SysUTF16ToNSString(visit->title);
  item.syncedTime = visit->last_modified;
  item.tabURL = visit->url;
  item.shouldShowSeeMore = IsTabResumption1_5SeeMoreEnabled();
  item.URLKey = URLAggregate->url_key;
  item.requestID = URLAggregate->request_id;
  if (visit->client_name) {
    item.sessionName = base::SysUTF8ToNSString(visit->client_name.value());
  }
  item.commandHandler = self;
  item.delegate = self;
  if (IsTabResumption2ReasonEnabled()) {
    NSString* overridenReason = GetOverridenReason(*URLAggregate);
    if (overridenReason) {
      item.reason = overridenReason;
    } else if (URLAggregate->decorations.size()) {
      item.reason = base::SysUTF16ToNSString(
          visited_url_ranking::GetMostRelevantDecoration(*URLAggregate)
              .GetDisplayString());
    }
  }
  if (tabData) {
    const visited_url_ranking::URLVisitAggregate::Tab& tab =
        tabData->last_active_tab;
    if (tab.id > 0 && tab.session_tag && !isLocal) {
      item.sessionName = base::SysUTF8ToNSString(tab.session_name.value());
      _sessionTag = tab.session_tag.value();
      _tabId = SessionID::FromSerializedValue(tab.id);
    }
  }

  // Fetch the favicon.
  [self fetchImageForItem:item];
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/session_id.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

@interface TabResumptionMediator () <StartSurfaceRecentTabObserving,
                                     SyncedSessionsObserver,
                                     IdentityManagerObserverBridgeDelegate,
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

  // The owning Browser.
  raw_ptr<Browser> _browser;
  raw_ptr<PrefService> _localState;
  raw_ptr<PrefService> _browserStatePrefs;
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
  // Observer bridge for mediator to listen to
  // StartSurfaceRecentTabObserverBridge.
  std::unique_ptr<StartSurfaceRecentTabObserverBridge> _startSurfaceObserver;

  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  // Observer for changes to the user's identity state.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserverBridge;
}

- (instancetype)initWithLocalState:(PrefService*)localState
                       prefService:(PrefService*)prefService
                   identityManager:(signin::IdentityManager*)identityManager
                           browser:(Browser*)browser {
  self = [super init];
  if (self) {
    CHECK(IsTabResumptionEnabled());
    _localState = localState;
    _browserStatePrefs = prefService;
    _browser = browser;
    _tabId = SessionID::InvalidValue();
    _sceneState = _browser->GetSceneState();
    _webStateList = _browser->GetWebStateList();
    _isOffTheRecord = _browser->GetBrowserState()->IsOffTheRecord();

    ChromeBrowserState* browserState = _browser->GetBrowserState();
    _sessionSyncService =
        SessionSyncServiceFactory::GetForBrowserState(browserState);
    _syncService = SyncServiceFactory::GetForBrowserState(browserState);
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
    _recentTabBrowserAgent =
        StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser);
    _URLLoadingBrowserAgent = UrlLoadingBrowserAgent::FromBrowser(_browser);
    _startSurfaceObserver =
        std::make_unique<StartSurfaceRecentTabObserverBridge>(self);
    StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser)->AddObserver(
        _startSurfaceObserver.get());

    if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
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
}

#pragma mark - Public methods

- (void)openTabResumptionItem:(TabResumptionItem*)item {
  [self.contentSuggestionsMetricsRecorder recordTabResumptionTabOpened];
  tab_resumption_prefs::SetTabResumptionLastOpenedTabURL(item.tabURL,
                                                         _browserStatePrefs);
  [self.delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                    kTabResumption];

  switch (item.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      [self.NTPMetricsDelegate distantTabResumptionOpened];
      [self openDistantTab];
      break;
    case TabResumptionItemType::kMostRecentTab: {
      [self.NTPMetricsDelegate recentTabTileOpened];
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

- (void)openDistantTab {
  ChromeBrowserState* browserState = _browser->GetBrowserState();
  sync_sessions::OpenTabsUIDelegate* openTabsDelegate =
      SessionSyncServiceFactory::GetForBrowserState(browserState)
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
            browserState, sessionTab->current_navigation_index,
            sessionTab->navigations);
    _webStateList->ReplaceWebStateAt(_webStateList->active_index(),
                                     std::move(webState));
  }
}

- (void)disableModule {
  tab_resumption_prefs::DisableTabResumption(_localState);
  [self.delegate removeTabResumptionModule];
}

- (void)setDelegate:(id<TabResumptionHelperDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLastTabResumptionItem];
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
  }
}

- (void)mostRecentTab:(web::WebState*)webState
    faviconUpdatedWithImage:(UIImage*)image {
}

- (void)mostRecentTab:(web::WebState*)webState
      titleWasUpdated:(NSString*)title {
}

#pragma mark - Private

- (void)fetchLastTabResumptionItem {
  if (tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
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

- (void)fetchFaviconForItem:(TabResumptionItem*)item {
  __weak TabResumptionMediator* weakSelf = self;
  _faviconLoader->FaviconForPageUrl(
      item.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        TabResumptionMediator* strongSelf = weakSelf;
        if (strongSelf && !attributes.usesDefaultImage) {
          item.faviconImage = attributes.faviconImage;
          strongSelf.itemConfig = item;
          [strongSelf.delegate tabResumptionHelperDidReceiveItem];
        }
      });
}

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

  // Fetch the favicon.
  [self fetchFaviconForItem:item];
}

// Creates a TabResumptionItem corresponding to the last synced tab then
// asynchronously invokes `item_block_handler` and exits.
- (void)fetchMostRecentTabItemFromWebState:(web::WebState*)webState
                                openedTime:(base::Time)openedTime {
  TabResumptionItem* item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  item.tabTitle = base::SysUTF16ToNSString(webState->GetTitle());
  item.syncedTime = openedTime;
  item.tabURL = webState->GetLastCommittedURL();
  item.commandHandler = self;

  // Fetch the favicon.
  [self fetchFaviconForItem:item];
}

@end

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#import "base/debug/dump_without_crashing.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_observing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

namespace {

// Returns whether the user needs to enter a passphrase or enable sync to make
// tab sync work.
bool UserActionIsRequiredToHaveTabSyncWork(syncer::SyncService* sync_service) {
  if (!sync_service->GetDisableReasons().empty()) {
    return true;
  }

  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    return true;
  }

  switch (sync_service->GetUserActionableError()) {
    // No error.
    case syncer::SyncService::UserActionableError::kNone:
      return false;

    // These errors effectively amount to disabled sync or effectively paused.
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return true;

    // This error doesn't stop tab sync.
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return false;

    // These errors don't actually stop sync.
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return false;
  }

  NOTREACHED();
}

}  // namespace

@interface RecentTabsMediator () <IdentityManagerObserverBridgeDelegate,
                                  SyncedSessionsObserver,
                                  TabGridModeObserving,
                                  TabGridToolbarsGridDelegate> {
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  SessionsSyncUserState _userState;
  // Current scene state.
  SceneState* _sceneState;
  // YES if remote grid is disabled by policy.
  BOOL _isDisabled;
  // Last active page.
  TabGridPage _lastActivePage;
  // Whether this screen is selected in the TabGrid.
  BOOL _selectedGrid;
  // Feature engagement tracker for notifying promo events.
  raw_ptr<feature_engagement::Tracker> _engagementTracker;
  // Time to ensure that the updates to the consumer are only happening once all
  // the updates are complete.
  std::unique_ptr<base::RetainingOneShotTimer> _timer;
  // Holder for the current mode of the tab grid.
  TabGridModeHolder* _modeHolder;
}

// Return the user's current sign-in and chrome-sync state.
- (SessionsSyncUserState)userSignedInState;
// Reload the panel.
- (void)refreshSessionsView;
@property(nonatomic, assign)
    sync_sessions::SessionSyncService* sessionSyncService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) sessions::TabRestoreService* restoreService;
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@property(nonatomic, assign) syncer::SyncService* syncService;
@property(nonatomic, assign) BrowserList* browserList;
@end

@implementation RecentTabsMediator

- (instancetype)
    initWithSessionSyncService:
        (sync_sessions::SessionSyncService*)sessionSyncService
               identityManager:(signin::IdentityManager*)identityManager
                restoreService:(sessions::TabRestoreService*)restoreService
                 faviconLoader:(FaviconLoader*)faviconLoader
                   syncService:(syncer::SyncService*)syncService
                   browserList:(BrowserList*)browserList
                    sceneState:(SceneState*)sceneState
              disabledByPolicy:(BOOL)disabled
             engagementTracker:(feature_engagement::Tracker*)engagementTracker
                    modeHolder:(TabGridModeHolder*)modeHolder {
  self = [super init];
  if (self) {
    _sessionSyncService = sessionSyncService;
    _identityManager = identityManager;
    _restoreService = restoreService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    _browserList = browserList;
    _sceneState = sceneState;
    _isDisabled = disabled;
    _engagementTracker = engagementTracker;
    __weak __typeof(self) weakSelf = self;
    _timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE, base::Milliseconds(100), base::BindRepeating(^{
          [weakSelf updateConsumerTabs];
        }));
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
  }
  return self;
}

#pragma mark - Public Interface

- (void)initObservers {
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, self.sessionSyncService);
  }
  if (!_identityManagerObserver) {
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            self.identityManager, self);
  }
  if (!_closedTabsObserver) {
    _closedTabsObserver =
        std::make_unique<recent_tabs::ClosedTabsObserverBridge>(self);
    if (self.restoreService) {
      self.restoreService->AddObserver(_closedTabsObserver.get());
    }
    [self.consumer setTabRestoreService:self.restoreService];
  }
}

- (void)disconnect {
  _syncedSessionsObserver.reset();
  _identityManagerObserver.reset();

  if (_closedTabsObserver) {
    if (self.restoreService) {
      self.restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
    _sessionSyncService = nullptr;
    _identityManager = nullptr;
    _restoreService = nullptr;
    _faviconLoader = nullptr;
    _syncService = nullptr;
  }

  _sceneState = nil;

  [_modeHolder removeObserver:self];
  _modeHolder = nil;
}

- (void)configureConsumer {
  [self refreshSessionsView];
}

#pragma mark - SyncedSessionsObserver

- (void)onForeignSessionsChanged {
  [self refreshSessionsView];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Sign-in could happen without onForeignSessionsChanged (e.g. if the user
      // signed-in without opting in to history sync; maybe also if sync ran
      // into an encryption error). The sign-in promo must still be updated in
      // that case, so handle it here.
      [self refreshSessionsView];
      break;
  }
}

#pragma mark - ClosedTabsObserving

- (void)tabRestoreServiceChanged:(sessions::TabRestoreService*)service {
  _timer->Reset();
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.consumer setTabRestoreService:nullptr];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

#pragma mark - Private

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  if (!_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return SessionsSyncUserState::USER_SIGNED_OUT;
  }

  if (UserActionIsRequiredToHaveTabSyncWork(_syncService)) {
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF;
  }

  DCHECK(self.sessionSyncService);
  sync_sessions::OpenTabsUIDelegate* delegate =
      self.sessionSyncService->GetOpenTabsUIDelegate();
  if (!delegate) {
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS;
  }

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  return delegate->GetAllForeignSessions(&sessions)
             ? SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS
             : SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
}

// Creates and send a tab grid toolbar configuration with button that should be
// displayed when recent grid is selected.
- (void)configureToolbarsButtons {
  if (!_selectedGrid) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];
  if (_isDisabled) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageRemoteTabs]];
    return;
  }

  // Done button is enabled if there is at least one tab in the last active
  // page.
  BOOL tabsInOtherGrid = NO;
  if (_lastActivePage == TabGridPageRegularTabs) {
    Browser* regularBrowser =
        _sceneState.browserProviderInterface.mainBrowserProvider.browser;
    tabsInOtherGrid =
        regularBrowser && !regularBrowser->GetWebStateList()->empty();
  } else if (_lastActivePage == TabGridPageIncognitoTabs &&
             _sceneState.browserProviderInterface.hasIncognitoBrowserProvider) {
    Browser* incognitoBrowser =
        _sceneState.browserProviderInterface.incognitoBrowserProvider.browser;
    tabsInOtherGrid =
        incognitoBrowser && !incognitoBrowser->GetWebStateList()->empty();
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] initWithPage:TabGridPageRemoteTabs];
  toolbarsConfiguration.doneButton = tabsInOtherGrid;
  toolbarsConfiguration.searchButton = YES;
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

// Update consumer tabs.
- (void)updateConsumerTabs {
  self.restoreService->LoadTabsFromLastSession();
  [self.consumer refreshRecentlyClosedTabs];
}

#pragma mark - RecentTabsTableViewControllerDelegate

- (void)refreshSessionsView {
  // This method is called from three places: 1) when this mediator observes a
  // change in the synced session state,  2) when the UI layer recognizes
  // that the signin process has completed, and 3) when the history & tabs sync
  // opt-in screen is dismissed.
  // The 2 latter calls are necessary because they can happen much more
  // immediately than the former call.
  [self.consumer refreshUserState:[self userSignedInState]];
}

#pragma mark - TabGridModeObserving

- (void)tabGridModeDidChange:(TabGridModeHolder*)modeHolder {
  [self configureToolbarsButtons];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selectedGrid = selected;

  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectRemotePanel"));
    default_browser::NotifyRemoteTabsGridViewed(_engagementTracker);

    [self configureToolbarsButtons];
  }
}

- (void)setPageAsActive {
  NOTREACHED() << "Should not be called in remote tabs.";
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

- (void)doneButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
  [self.tabGridHandler exitTabGrid];
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

- (void)selectAllButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

- (void)searchButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridSearchTabs"));
  _modeHolder.mode = TabGridMode::kSearch;
}

- (void)cancelSearchButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridCancelSearchTabs"));
  _modeHolder.mode = TabGridMode::kNormal;
}

- (void)closeSelectedTabs:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

- (void)shareSelectedTabs:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

- (void)selectTabsButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in remote tabs.";
}

#pragma mark - TabGridActivityObserver

- (void)updateLastActiveTabPage:(TabGridPage)page {
  _lastActivePage = page;
}

@end

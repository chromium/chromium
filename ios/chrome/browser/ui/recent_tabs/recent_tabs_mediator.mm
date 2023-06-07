// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#import "base/debug/dump_without_crashing.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsMediator () <SyncedSessionsObserver,
                                  WebStateListObserving> {
  std::unique_ptr<AllWebStateListObservationRegistrar> _registrar;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  SessionsSyncUserState _userState;
  // The list of web state list currently processing batch operations (e.g.
  // Closing All, or Undoing a Close All).
  std::set<WebStateList*> _webStateListsWithBatchOperations;
}

// Return the user's current sign-in and chrome-sync state.
- (SessionsSyncUserState)userSignedInState;
// Utility functions for -userSignedInState so these can be mocked out
// easily for unit tests.
- (BOOL)hasSyncConsent;
- (BOOL)isSyncTabsEnabled;
- (BOOL)hasForeignSessions;
- (BOOL)isSyncCompleted;
// Reload the panel.
- (void)refreshSessionsView;
@property(nonatomic, assign)
    sync_sessions::SessionSyncService* sessionSyncService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) sessions::TabRestoreService* restoreService;
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@property(nonatomic, assign) SyncSetupService* syncSetupService;
@property(nonatomic, assign) BrowserList* browserList;

@end

@implementation RecentTabsMediator

- (instancetype)
    initWithSessionSyncService:
        (sync_sessions::SessionSyncService*)sessionSyncService
               identityManager:(signin::IdentityManager*)identityManager
                restoreService:(sessions::TabRestoreService*)restoreService
                 faviconLoader:(FaviconLoader*)faviconLoader
              syncSetupService:(SyncSetupService*)syncSetupService
                   browserList:(BrowserList*)browserList {
  self = [super init];
  if (self) {
    _sessionSyncService = sessionSyncService;
    _identityManager = identityManager;
    _restoreService = restoreService;
    _faviconLoader = faviconLoader;
    _syncSetupService = syncSetupService;
    _browserList = browserList;
  }
  return self;
}

#pragma mark - Public Interface

- (void)initObservers {
  if (!_registrar) {
    _registrar = std::make_unique<AllWebStateListObservationRegistrar>(
        _browserList, std::make_unique<WebStateListObserverBridge>(self),
        AllWebStateListObservationRegistrar::Mode::REGULAR);
  }
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, self.identityManager, self.sessionSyncService);
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
  _registrar.reset();
  _syncedSessionsObserver.reset();

  if (_closedTabsObserver) {
    if (self.restoreService) {
      self.restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
    _sessionSyncService = nullptr;
    _identityManager = nullptr;
    _restoreService = nullptr;
    _faviconLoader = nullptr;
    _syncSetupService = nullptr;
  }
}

- (void)configureConsumer {
  [self refreshSessionsView];
}

#pragma mark - SyncedSessionsObserver

- (void)reloadSessions {
  [self refreshSessionsView];
}

- (void)onSyncStateChanged {
  [self refreshSessionsView];
}

#pragma mark - ClosedTabsObserving

- (void)tabRestoreServiceChanged:(sessions::TabRestoreService*)service {
  self.restoreService->LoadTabsFromLastSession();
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). To properly batch
  // process TabRestoreService changes, those changes must be executed after the
  // WebStateList batch operation ended. This allows RecentTabs to ignore
  // individual tabRestoreServiceChanged calls that correspond to a WebStateList
  // batch operation. The consumer is updated once after all batch operations
  // have completed.
  if (_webStateListsWithBatchOperations.empty()) {
    [self.consumer refreshRecentlyClosedTabs];
  }
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.consumer setTabRestoreService:nullptr];
}

#pragma mark - WebStateListObserving

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  _webStateListsWithBatchOperations.insert(webStateList);
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  _webStateListsWithBatchOperations.erase(webStateList);
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). Individual
  // TabRestoreService updates are ignored between
  // `-webStateListWillBeginBatchOperation:` and
  // `-webStateListBatchOperationEnded:` for all observed WebStateLists. The
  // consumer is updated once after all batch operations have completed.
  if (_webStateListsWithBatchOperations.empty()) {
    [self.consumer refreshRecentlyClosedTabs];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  if (_webStateListsWithBatchOperations.contains(webStateList)) {
    // This means a WebStateList was in a batch operation (received
    // `-webStateListWillBeginBatchOperation:`) that didn't finish (didn't
    // receive `-webStateListBatchOperationEnded:`). This is not supposed to
    // happen, but if it did, handle it by removing the web state list from the
    // set and dump without crashing.
    base::debug::DumpWithoutCrashing();
    _webStateListsWithBatchOperations.erase(webStateList);
    if (_webStateListsWithBatchOperations.empty()) {
      [self.consumer refreshRecentlyClosedTabs];
    }
  }
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

- (BOOL)hasSyncConsent {
  return _syncedSessionsObserver->HasSyncConsent();
}

- (BOOL)isSyncTabsEnabled {
  DCHECK([self hasSyncConsent]);
  return !self.syncSetupService->UserActionIsRequiredToHaveTabSyncWork();
}

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  if (![self hasSyncConsent])
    return SessionsSyncUserState::USER_SIGNED_OUT;
  if (![self isSyncTabsEnabled])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF;
  if (![self isSyncCompleted])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS;
  if ([self hasForeignSessions])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS;
  return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
}

- (BOOL)isSyncCompleted {
  DCHECK(self.sessionSyncService);
  return self.sessionSyncService->GetOpenTabsUIDelegate() != nullptr;
}

- (BOOL)hasForeignSessions {
  DCHECK(self.sessionSyncService);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      self.sessionSyncService->GetOpenTabsUIDelegate();
  DCHECK(openTabs);
  std::vector<const sync_sessions::SyncedSession*> sessions;
  return openTabs->GetAllForeignSessions(&sessions);
}

#pragma mark - RecentTabsTableViewControllerDelegate

- (void)refreshSessionsView {
  // This method is called from two places: 1) when this mediator observes a
  // change in the synced session state, and 2) when the UI layer recognizes
  // that the signin process has completed. The latter call is necessary because
  // it can happen much more immediately than the former call.
  [self.consumer refreshUserState:[self userSignedInState]];
}

@end

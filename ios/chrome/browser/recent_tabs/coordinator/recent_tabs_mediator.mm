// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_mediator.h"

#import "base/debug/dump_without_crashing.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/timer/timer.h"
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
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_consumer.h"
#import "ios/chrome/browser/recent_tabs/ui/sessions_sync_user_state.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
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
    case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
      return false;

    // TODO(crbug.com/370026230): Update this case upon UI implementation.
    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      return false;
  }

  NOTREACHED();
}

}  // namespace

@interface RecentTabsMediator () <IdentityManagerObserverBridgeDelegate,
                                  SyncedSessionsObserver> {
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  // Time to ensure that the updates to the consumer are only happening once all
  // the updates are complete.
  std::unique_ptr<base::RetainingOneShotTimer> _timer;
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
@end

@implementation RecentTabsMediator

- (instancetype)
    initWithSessionSyncService:
        (sync_sessions::SessionSyncService*)sessionSyncService
               identityManager:(signin::IdentityManager*)identityManager
                restoreService:(sessions::TabRestoreService*)restoreService
                 faviconLoader:(FaviconLoader*)faviconLoader
                   syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _sessionSyncService = sessionSyncService;
    _identityManager = identityManager;
    _restoreService = restoreService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    __weak __typeof(self) weakSelf = self;
    _timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE, base::Milliseconds(100), base::BindRepeating(^{
          [weakSelf updateConsumerTabs];
        }));
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
}

- (void)configureConsumer {
  [self refreshSessionsView];
}

- (void)refreshSessionsView {
  // This method is called from three places: 1) when this mediator observes a
  // change in the synced session state,  2) when the UI layer recognizes
  // that the signin process has completed, and 3) when the history & tabs sync
  // opt-in screen is dismissed.
  // The 2 latter calls are necessary because they can happen much more
  // immediately than the former call.
  [self.consumer refreshUserState:[self userSignedInState]];
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
               completion:(void (^)(FaviconAttributes* attributes,
                                    bool cached))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        completion(attributes, cached);
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

// Update consumer tabs.
- (void)updateConsumerTabs {
  self.restoreService->LoadTabsFromLastSession();
  [self.consumer refreshRecentlyClosedTabs];
}

@end

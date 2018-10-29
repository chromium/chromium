// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Desired width and height of favicon.
const CGFloat kFaviconWidthHeight = 24;
// Minimum favicon size to retrieve.
const CGFloat kFaviconMinWidthHeight = 16;
}  // namespace

@interface RecentTabsMediator ()<SyncedSessionsObserver> {
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  SessionsSyncUserState _userState;
}

// Return the user's current sign-in and chrome-sync state.
- (SessionsSyncUserState)userSignedInState;
// Utility functions for -userSignedInState so these can be mocked out
// easily for unit tests.
- (BOOL)isSignedIn;
- (BOOL)isSyncTabsEnabled;
- (BOOL)hasForeignSessions;
- (BOOL)isSyncCompleted;
// Reload the panel.
- (void)refreshSessionsView;

@end

@implementation RecentTabsMediator
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;

#pragma mark - Public Interface

- (void)initObservers {
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, _browserState);
  }
  if (!_closedTabsObserver) {
    _closedTabsObserver =
        std::make_unique<recent_tabs::ClosedTabsObserverBridge>(self);
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
    if (restoreService)
      restoreService->AddObserver(_closedTabsObserver.get());
    [self.consumer setTabRestoreService:restoreService];
  }
}

- (void)disconnect {
  _syncedSessionsObserver.reset();

  if (_closedTabsObserver) {
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
    if (restoreService) {
      restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
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
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
  restoreService->LoadTabsFromLastSession();
  [self.consumer refreshRecentlyClosedTabs];
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.consumer setTabRestoreService:nullptr];
}

#pragma mark - RecentTabsImageDataSource

- (FaviconAttributes*)faviconForURL:(const GURL&)URL
                         completion:(void (^)(FaviconAttributes*))completion {
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(self.browserState);
  FaviconAttributes* cachedAttributes = faviconLoader->FaviconForUrl(
      URL, kFaviconMinWidthHeight, kFaviconWidthHeight,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
  DCHECK(cachedAttributes);

  return cachedAttributes;
}

#pragma mark - Private

- (BOOL)isSignedIn {
  return _syncedSessionsObserver->IsSignedIn();
}

- (BOOL)isSyncTabsEnabled {
  DCHECK([self isSignedIn]);
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
  return !service->UserActionIsRequiredToHaveSyncWork();
}

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  if (![self isSignedIn])
    return SessionsSyncUserState::USER_SIGNED_OUT;
  if (![self isSyncTabsEnabled])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF;
  if ([self hasForeignSessions])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS;
  if (![self isSyncCompleted])
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS;
  return SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
}

- (BOOL)hasForeignSessions {
  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  DCHECK(service);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      service->GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;
  return openTabs ? openTabs->GetAllForeignSessions(&sessions) : NO;
}

- (BOOL)isSyncCompleted {
  return _syncedSessionsObserver->IsFirstSyncCycleCompleted();
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

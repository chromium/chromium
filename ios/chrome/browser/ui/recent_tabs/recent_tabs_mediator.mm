// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
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

@interface RecentTabsMediator () <SyncedSessionsObserver,
                                  WebStateListObserving> {
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
// YES if Tabs are being updated in batch. (e.g. Closing All, or Undoing a Close
// All).
@property(nonatomic, assign) BOOL processingBatchOperation;

@end

@implementation RecentTabsMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

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

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

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
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). To properly batch
  // process TabRestoreService changes, those changes must be executed inside
  // the WebStateList batch operation callback. This allows RecentTabs to ignore
  // individual tabRestoreServiceChanged calls that correspond to the
  // WebStateList batch operation. The consumer is updated once after the batch
  // operation is completed.
  if (!self.processingBatchOperation)
    [self.consumer refreshRecentlyClosedTabs];
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.consumer setTabRestoreService:nullptr];
}

#pragma mark - WebStateListObserving

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  self.processingBatchOperation = YES;
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  self.processingBatchOperation = NO;
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). Individual
  // TabRestoreService updates are ignored between
  // |-webStateListWillBeginBatchOperation:| and
  // |-webStateListBatchOperationEnded:|. The consumer is updated once after the
  // batch operation is complete.
  [self.consumer refreshRecentlyClosedTabs];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(const GURL&)URL
           completion:(void (^)(FaviconAttributes*))completion {
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(self.browserState);
  faviconLoader->FaviconForPageUrl(
      URL, kFaviconWidthHeight, kFaviconMinWidthHeight,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

#pragma mark - Setters/Getters

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList)
    _webStateList->RemoveObserver(_webStateListObserver.get());

  _webStateList = webStateList;

  if (_webStateList)
    _webStateList->AddObserver(_webStateListObserver.get());
}

#pragma mark - Private

- (BOOL)isSignedIn {
  return _syncedSessionsObserver->IsSignedIn();
}

- (BOOL)isSyncTabsEnabled {
  DCHECK([self isSignedIn]);
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
  return !service->UserActionIsRequiredToHaveTabSyncWork();
}

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  if (![self isSignedIn])
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
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForBrowserState(_browserState);
  DCHECK(service);
  return service->GetOpenTabsUIDelegate() != nullptr;
}

- (BOOL)hasForeignSessions {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForBrowserState(_browserState);
  DCHECK(service);
  sync_sessions::OpenTabsUIDelegate* openTabs =
      service->GetOpenTabsUIDelegate();
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

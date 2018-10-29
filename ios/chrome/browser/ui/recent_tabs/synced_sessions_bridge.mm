// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/synced_sessions_bridge.h"

#include "components/browser_sync/profile_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "services/identity/public/cpp/identity_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace synced_sessions {

#pragma mark - SyncedSessionsObserverBridge

SyncedSessionsObserverBridge::SyncedSessionsObserverBridge(
    id<SyncedSessionsObserver> owner,
    ios::ChromeBrowserState* browserState)
    : SyncObserverBridge(
          owner,
          ProfileSyncServiceFactory::GetForBrowserState(browserState)),
      owner_(owner),
      identity_manager_(
          IdentityManagerFactory::GetForBrowserState(browserState)),
      browser_state_(browserState),
      identity_manager_observer_(this) {
  identity_manager_observer_.Add(identity_manager_);
}

SyncedSessionsObserverBridge::~SyncedSessionsObserverBridge() {}

#pragma mark - SyncObserverBridge

void SyncedSessionsObserverBridge::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  [owner_ reloadSessions];
}

void SyncedSessionsObserverBridge::OnForeignSessionUpdated(
    syncer::SyncService* sync) {
  [owner_ reloadSessions];
}

bool SyncedSessionsObserverBridge::IsFirstSyncCycleCompleted() {
  return SyncSetupServiceFactory::GetForBrowserState(browser_state_)
      ->IsDataTypeActive(syncer::SESSIONS);
}

#pragma mark - identity::IdentityManager::Observer

void SyncedSessionsObserverBridge::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  [owner_ reloadSessions];
}

#pragma mark - Signin and syncing status

bool SyncedSessionsObserverBridge::IsSignedIn() {
  return identity_manager_->HasPrimaryAccount();
}

}  // namespace synced_sessions

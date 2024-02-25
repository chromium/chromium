// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/synced_sessions_bridge.h"

#import "base/functional/bind.h"
#import "components/sync_sessions/session_sync_service.h"

namespace synced_sessions {

SyncedSessionsObserverBridge::SyncedSessionsObserverBridge(
    id<SyncedSessionsObserver> owner,
    sync_sessions::SessionSyncService* sync_service)
    : owner_(owner),
      // base::Unretained() is safe below because the subscription itself is a
      // class member field and handles destruction well.
      foreign_session_updated_subscription_(
          sync_service->SubscribeToForeignSessionsChanged(base::BindRepeating(
              &SyncedSessionsObserverBridge::OnForeignSessionChanged,
              base::Unretained(this)))) {}

void SyncedSessionsObserverBridge::OnForeignSessionChanged() {
  [owner_ onForeignSessionsChanged];
}

}  // namespace synced_sessions

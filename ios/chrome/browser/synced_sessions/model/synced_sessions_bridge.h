// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/callback_list.h"

namespace sync_sessions {
class SessionSyncService;
}

@protocol SyncedSessionsObserver
// Notifies when foreign sessions change.
- (void)onForeignSessionsChanged;
@end

namespace synced_sessions {

// Bridge class that will notify the panel when the remote sessions content
// change.
class SyncedSessionsObserverBridge {
 public:
  SyncedSessionsObserverBridge(id<SyncedSessionsObserver> owner,
                               sync_sessions::SessionSyncService* sync_service);

  SyncedSessionsObserverBridge(const SyncedSessionsObserverBridge&) = delete;
  SyncedSessionsObserverBridge& operator=(const SyncedSessionsObserverBridge&) =
      delete;

  ~SyncedSessionsObserverBridge() = default;

 private:
  void OnForeignSessionChanged();

  const __weak id<SyncedSessionsObserver> owner_;
  const base::CallbackListSubscription foreign_session_updated_subscription_;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_BRIDGE_H_

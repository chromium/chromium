// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ChromeBrowserState;

namespace signin {
class IdentityManager;
}

@protocol SyncedSessionsObserver
// Reloads the session data.
- (void)reloadSessions;
@end

namespace synced_sessions {

// Bridge class that will notify the panel when the remote sessions content
// change.
class SyncedSessionsObserverBridge : public signin::IdentityManager::Observer {
 public:
  SyncedSessionsObserverBridge(id<SyncedSessionsObserver> owner,
                               ChromeBrowserState* browserState);

  SyncedSessionsObserverBridge(const SyncedSessionsObserverBridge&) = delete;
  SyncedSessionsObserverBridge& operator=(const SyncedSessionsObserverBridge&) =
      delete;

  ~SyncedSessionsObserverBridge() override;
  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // Returns true if user has granted sync consent.
  bool HasSyncConsent();

 private:
  void OnForeignSessionChanged();

  __weak id<SyncedSessionsObserver> owner_ = nil;
  signin::IdentityManager* identity_manager_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::CallbackListSubscription foreign_session_updated_subscription_;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_

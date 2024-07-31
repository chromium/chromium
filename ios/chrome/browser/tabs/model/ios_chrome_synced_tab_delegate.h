// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "ios/web/public/web_state_user_data.h"

class IOSChromeSyncedTabDelegate
    : public sync_sessions::SyncedTabDelegate,
      public web::WebStateUserData<IOSChromeSyncedTabDelegate> {
 public:
  IOSChromeSyncedTabDelegate(const IOSChromeSyncedTabDelegate&) = delete;
  IOSChromeSyncedTabDelegate& operator=(const IOSChromeSyncedTabDelegate&) =
      delete;

  // Resets the cached last_active_time value, allowing the next call to
  // GetLastActiveTime() to return the actual value.
  void ResetCachedLastActiveTime();

  ~IOSChromeSyncedTabDelegate() override;

  // SyncedTabDelegate:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsBeingDestroyed() const override;
  base::Time GetLastActiveTime() override;
  std::string GetExtensionAppId() const override;
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  bool ProfileHasChildAccount() const override;
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool IsPlaceholderTab() const override;
  bool ShouldSync(sync_sessions::SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      sync_sessions::SyncSessionsClient* sessions_client) override;

 private:
  friend class web::WebStateUserData<IOSChromeSyncedTabDelegate>;

  explicit IOSChromeSyncedTabDelegate(web::WebState* web_state);

  // The associated WebState.
  const raw_ptr<web::WebState> web_state_;

  // Cached value of last_active_time, sometimes returned instead of the
  // last_active_time from the WebState.
  std::optional<base::Time> cached_last_active_time_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

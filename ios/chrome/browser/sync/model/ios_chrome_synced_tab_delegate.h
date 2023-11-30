// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#import "ios/web/public/web_state_user_data.h"

@class CRWSessionStorage;

class IOSChromeSyncedTabDelegate
    : public sync_sessions::SyncedTabDelegate,
      public web::WebStateUserData<IOSChromeSyncedTabDelegate> {
 public:
  IOSChromeSyncedTabDelegate(const IOSChromeSyncedTabDelegate&) = delete;
  IOSChromeSyncedTabDelegate& operator=(const IOSChromeSyncedTabDelegate&) =
      delete;

  ~IOSChromeSyncedTabDelegate() override;

  // SyncedTabDelegate:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsBeingDestroyed() const override;
  base::Time GetLastActiveTime() const override;
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
  std::unique_ptr<SyncedTabDelegate> CreatePlaceholderTabSyncedTabDelegate()
      override;

 private:
  friend class web::WebStateUserData<IOSChromeSyncedTabDelegate>;

  explicit IOSChromeSyncedTabDelegate(web::WebState* web_state);

  // Returns whether the navigation data must be read from session storage.
  // Can only be used if placeholder tabs support is not enabled. If this
  // method returns true, then `session_storage_` must be used to get the
  // navigation information.
  bool GetSessionStorageIfNeeded() const;

  // The associated WebState.
  web::WebState* const web_state_;

  // The session storage for the WebState. Used only when the support for
  // placeholder tabs is not enabled. Invalid to use otherwise.
  mutable CRWSessionStorage* session_storage_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

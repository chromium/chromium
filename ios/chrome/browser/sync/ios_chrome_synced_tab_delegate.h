// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNCED_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#import "ios/web/public/web_state_user_data.h"

@class CRWSessionStorage;
class IOSTaskTabHelper;

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
  std::string GetExtensionAppId() const override;
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  std::string GetPageLanguageAtIndex(int i) const override;
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

 private:
  explicit IOSChromeSyncedTabDelegate(web::WebState* web_state);
  const IOSTaskTabHelper* ios_task_tab_helper() const;
  friend class web::WebStateUserData<IOSChromeSyncedTabDelegate>;

  // Whether navigation data should be taken from session storage.
  // Storage must be used if slim navigation is enabled and the tab has not be
  // displayed.
  // If the session storage must be used and was not fetched yet, bet it from
  // `web_state_`.
  bool GetSessionStorageIfNeeded() const;

  web::WebState* web_state_;
  mutable CRWSessionStorage* session_storage_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNCED_TAB_DELEGATE_H_

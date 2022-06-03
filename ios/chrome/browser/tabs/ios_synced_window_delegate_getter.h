// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_
#define IOS_CHROME_BROWSER_TABS_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_

#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace browser_sync {
class SyncedWindowDelegate;
}

class IOSSyncedWindowDelegatesGetter
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  IOSSyncedWindowDelegatesGetter();
  // Not copyable or moveable
  IOSSyncedWindowDelegatesGetter(const IOSSyncedWindowDelegatesGetter&) =
      delete;
  IOSSyncedWindowDelegatesGetter& operator=(
      const IOSSyncedWindowDelegatesGetter&) = delete;
  ~IOSSyncedWindowDelegatesGetter() override;

  // sync_sessions::SyncedWindowDelegatesGetter:
  SyncedWindowDelegateMap GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(
      SessionID session_id) override;
};

#endif  // IOS_CHROME_BROWSER_TABS_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_

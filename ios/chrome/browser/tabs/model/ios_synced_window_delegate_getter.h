// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_

#import "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

class BrowserList;
namespace browser_sync {
class SyncedWindowDelegate;
}

class IOSSyncedWindowDelegatesGetter
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  explicit IOSSyncedWindowDelegatesGetter(BrowserList* browser_list);

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

 private:
  raw_ptr<BrowserList> const browser_list_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_IOS_SYNCED_WINDOW_DELEGATE_GETTER_H_

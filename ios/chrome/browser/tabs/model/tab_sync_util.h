// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_SYNC_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_SYNC_UTIL_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace synced_sessions {
struct DistantTab;
struct DistantSession;
class SyncedSessions;
}  // namespace synced_sessions

// Represents the last active distant tab.
struct LastActiveDistantTab {
  // The last active distant tab.
  const raw_ptr<const synced_sessions::DistantTab> tab;
  // The session of the last active distant tab.
  const raw_ptr<const synced_sessions::DistantSession> session;
};

// Checks that distant sessions and tabs are sorted by their `modified_time`.
void CheckDistantTabsOrder(synced_sessions::SyncedSessions* synced_sessions);

// Returns the latest active tab that was used below the given `time_threshold`.
LastActiveDistantTab GetLastActiveDistantTab(
    synced_sessions::SyncedSessions* synced_sessions,
    base::TimeDelta time_threshold);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_SYNC_UTIL_H_

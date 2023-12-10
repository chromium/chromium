// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_sync_util.h"

#import "base/time/time.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"

namespace {

// The maxium number of sessions to look at.
const size_t kMaxSessionLength = 5;

// The maxium number of tabs to look at.
const unsigned int kMaxTabLength = 50;

}  // namespace

void CheckDistantTabsOrder(synced_sessions::SyncedSessions* synced_sessions) {
  base::Time previous_session_modified_time = base::Time::Now();
  for (size_t session_index = 0;
       session_index < synced_sessions->GetSessionCount(); ++session_index) {
    const synced_sessions::DistantSession* session =
        synced_sessions->GetSession(session_index);
    CHECK(previous_session_modified_time >= session->modified_time);
    previous_session_modified_time = session->modified_time;

    base::Time previous_modified_time = base::Time::Now();

    for (const auto& distant_tab : session->tabs) {
      CHECK(previous_modified_time >= distant_tab->modified_time);
      CHECK(distant_tab->modified_time >= distant_tab->last_active_time);
      previous_modified_time = distant_tab->modified_time;
    }
  }
}

LastActiveDistantTab GetLastActiveDistantTab(
    synced_sessions::SyncedSessions* synced_sessions,
    base::TimeDelta delta_threshold) {
  base::Time time_threshold = base::Time::Now() - delta_threshold;
  const synced_sessions::DistantTab* last_active_tab = nullptr;
  const synced_sessions::DistantSession* last_active_session = nullptr;

  const size_t max_sessions_to_consider =
      std::min(synced_sessions->GetSessionCount(), kMaxSessionLength);
  for (size_t session_index = 0; session_index < max_sessions_to_consider;
       ++session_index) {
    const synced_sessions::DistantSession* session =
        synced_sessions->GetSession(session_index);

    // Skip the session if its `modified_time` value doesn't meet the time
    // threshold.
    if (session->modified_time < time_threshold) {
      break;
    }

    unsigned int tab_index = 0;
    for (const auto& distant_tab : session->tabs) {
      if (tab_index++ == kMaxTabLength) {
        break;
      }

      // Tabs in sessions are sorted by their `modified_time` value.
      // Skip the session if the `modified_time` of the tab doesn't meet the
      // time threshold.
      if (distant_tab->modified_time < time_threshold) {
        break;
      }

      if (distant_tab->last_active_time > time_threshold) {
        time_threshold = distant_tab->last_active_time;
        last_active_tab = distant_tab.get();
        last_active_session = session;
      }
    }
  }

  CHECK_EQ(!!last_active_tab, !!last_active_session);
  return LastActiveDistantTab{
      .tab = last_active_tab,
      .session = last_active_session,
  };
}

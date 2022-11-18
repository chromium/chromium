// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_session.h"
#include "url/gurl.h"

namespace sync_sessions {
class SessionSyncService;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}

namespace synced_sessions {

// Data holder that contains the data of the distant tabs to show in the UI.
struct DistantTab {
  DistantTab();

  DistantTab(const DistantTab&) = delete;
  DistantTab& operator=(const DistantTab&) = delete;

  // Uniquely identifies the distant session this DistantTab belongs to.
  std::string session_tag;
  // Uniquely identifies this tab in its distant session.
  SessionID tab_id;
  // The title of the page shown in this DistantTab.
  std::u16string title;
  // The url shown in this DistantTab.
  GURL virtual_url;
  // Returns a hash the fields `virtual_url` and `title`.
  // By design, two tabs in the same distant session can have the same
  // `hashOfUserVisibleProperties`.
  size_t hashOfUserVisibleProperties();
};

// Data holder that contains a set of distant tabs to show in the UI.
struct DistantTabsSet {
  DistantTabsSet();
  ~DistantTabsSet();

  DistantTabsSet(const DistantTabsSet&);

  // The tag of the DistantSession which owns the tabs referenced in `tabs`.
  std::string session_tag;
  // A selection of `DistantTab`s from the session with tag `session_tag`. A
  // null value for `filtered_tabs` represents that the session's tabs are
  // not filtered. This shortcut representation prevents having to copy over
  // pointers to each tab within a session when every tab is included.
  absl::optional<std::vector<DistantTab*>> filtered_tabs;
};

// Data holder that contains the data of the distant sessions and their tabs to
// show in the UI.

class DistantSession {
 public:
  DistantSession();
  // Initialize with the session tagged with `tag` and obtained with
  // `sync_service`. `sync_service` must not be null.
  DistantSession(sync_sessions::SessionSyncService* sync_service,
                 const std::string& tag);

  DistantSession(const DistantSession&) = delete;
  DistantSession& operator=(const DistantSession&) = delete;

  ~DistantSession();

  void InitWithSyncedSession(
      const sync_sessions::SyncedSession* synced_session,
      sync_sessions::OpenTabsUIDelegate* open_tabs_delegate);

  std::string tag;
  std::string name;
  base::Time modified_time;
  std::vector<std::unique_ptr<DistantTab>> tabs;
};

// Class containing distant sessions.
class SyncedSessions {
 public:
  // Initialize with no distant sessions.
  SyncedSessions();
  // Initialize with all the distant sessions obtained from `sync_service`.
  // `sync_service` must not be null.
  explicit SyncedSessions(sync_sessions::SessionSyncService* sync_service);
  SyncedSessions(sync_sessions::SessionSyncService* sync_service,
                 const std::string& tag);

  SyncedSessions(const SyncedSessions&) = delete;
  SyncedSessions& operator=(const SyncedSessions&) = delete;

  ~SyncedSessions();
  DistantSession const* GetSession(size_t index) const;
  DistantSession const* GetSessionWithTag(const std::string& tag) const;
  size_t GetSessionCount() const;
  void EraseSessionWithTag(const std::string& tag);

  // Used by tests only.
  void AddDistantSessionForTest(
      std::unique_ptr<const DistantSession> distant_session);

 private:
  std::vector<std::unique_ptr<const DistantSession>> sessions_;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_

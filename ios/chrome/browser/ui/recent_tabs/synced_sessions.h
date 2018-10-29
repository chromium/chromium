// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_session.h"
#include "url/gurl.h"

namespace syncer {
class SyncService;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}

namespace synced_sessions {

// Data holder that contains the data of the distant tabs to show in the UI.
struct DistantTab {
  DistantTab();
  // Uniquely identifies the distant session this DistantTab belongs to.
  std::string session_tag;
  // Uniquely identifies this tab in its distant session.
  SessionID tab_id;
  // The title of the page shown in this DistantTab.
  base::string16 title;
  // The url shown in this DistantTab.
  GURL virtual_url;
  // Returns a hash the fields |virtual_url| and |title|.
  // By design, two tabs in the same distant session can have the same
  // |hashOfUserVisibleProperties|.
  size_t hashOfUserVisibleProperties();

  DISALLOW_COPY_AND_ASSIGN(DistantTab);
};

// Data holder that contains the data of the distant sessions and their tabs to
// show in the UI.

class DistantSession {
 public:
  DistantSession();
  // Initialize with the session tagged with |tag| and obtained with
  // |sync_service|. |sync_service| must not be null.
  DistantSession(syncer::SyncService* sync_service, const std::string& tag);
  ~DistantSession();
  void InitWithSyncedSession(const sync_sessions::SyncedSession* synced_session,
                             sync_sessions::OpenTabsUIDelegate* open_tabs);
  std::string tag;
  std::string name;
  base::Time modified_time;
  sync_pb::SyncEnums::DeviceType device_type;
  std::vector<std::unique_ptr<DistantTab>> tabs;

  DISALLOW_COPY_AND_ASSIGN(DistantSession);
};

// Class containing distant sessions.
class SyncedSessions {
 public:
  // Initialize with no distant sessions.
  SyncedSessions();
  // Initialize with all the distant sessions obtained from |sync_service|.
  // |sync_service| must not be null.
  explicit SyncedSessions(syncer::SyncService* sync_service);
  SyncedSessions(syncer::SyncService* sync_service, const std::string& tag);
  ~SyncedSessions();
  DistantSession const* GetSession(size_t index) const;
  DistantSession const* GetSessionWithTag(const std::string& tag) const;
  size_t GetSessionCount() const;
  void EraseSession(size_t index);

  // Used by tests only.
  void AddDistantSessionForTest(
      std::unique_ptr<const DistantSession> distant_session);

 private:
  std::vector<std::unique_ptr<const DistantSession>> sessions_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessions);
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_H_

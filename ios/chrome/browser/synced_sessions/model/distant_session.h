// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_SESSION_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_SESSION_H_

#import <string>

#import "components/sync_device_info/device_info.h"

namespace base {
class Time;
}  // namespace base

namespace sync_sessions {
class OpenTabsUIDelegate;
class SessionSyncService;
struct SyncedSession;
}  // namespace sync_sessions

namespace synced_sessions {

struct DistantTab;

// Data holder that contains the data of the distant sessions and their tabs to
// show in the UI.
struct DistantSession {
  DistantSession();
  // Initializes with the session tagged with `tag` and obtained with
  // `sync_service`. `sync_service` must not be null.
  DistantSession(sync_sessions::SessionSyncService* sync_service,
                 const std::string& tag);

  DistantSession(const DistantSession&) = delete;
  DistantSession& operator=(const DistantSession&) = delete;

  ~DistantSession();

  // Loads information from `synced_session` to a distant session using
  // `open_tabs_delegate`.
  void InitWithSyncedSession(
      const sync_sessions::SyncedSession* synced_session,
      sync_sessions::OpenTabsUIDelegate* open_tabs_delegate);

  // Unique identifier of a session.
  std::string tag;
  // Session name.
  std::string name;
  // Time the session is last modified.
  base::Time modified_time;
  // A list of tabs opened in this session.
  std::vector<std::unique_ptr<DistantTab>> tabs;
  // The form factor of the device in which the session is created.
  syncer::DeviceInfo::FormFactor form_factor;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_SESSION_H_

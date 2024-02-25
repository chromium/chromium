// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_H_

#import <memory>
#import <string>
#import <vector>

namespace sync_sessions {
class SessionSyncService;
}

namespace synced_sessions {

struct DistantSession;
class FakeSyncedSessions;

// Class containing distant sessions.
class SyncedSessions {
 public:
  // Initializes with no distant sessions.
  SyncedSessions();
  // Initializes with all the distant sessions obtained from `sync_service`.
  // `sync_service` must not be null.
  explicit SyncedSessions(sync_sessions::SessionSyncService* sync_service);

  SyncedSessions(const SyncedSessions&) = delete;
  SyncedSessions& operator=(const SyncedSessions&) = delete;

  ~SyncedSessions();
  // Returns the distant session with `index` in the list of sessions.
  DistantSession const* GetSession(size_t index) const;
  // Returns the session with the unique identifier `tag`.
  DistantSession const* GetSessionWithTag(const std::string& tag) const;
  // Returns the number of distant sessions.
  size_t GetSessionCount() const;
  // Removes the session with the unique identifier `tag` from this session.
  void EraseSessionWithTag(const std::string& tag);

 private:
  friend class FakeSyncedSessions;

  std::vector<std::unique_ptr<const DistantSession>> sessions_;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_H_

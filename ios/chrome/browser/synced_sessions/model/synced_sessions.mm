// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"

#import <functional>
#import <memory>

#import "base/check_op.h"
#import "base/time/time.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"

namespace {

// Comparator function for sorting the sessions_ vector so that the most
// recently used sessions are at the beginning.
bool CompareSessionsByTime(
    std::unique_ptr<const synced_sessions::DistantSession>& a,
    std::unique_ptr<const synced_sessions::DistantSession>& b) {
  return a->modified_time > b->modified_time;
}

}  // namespace

namespace synced_sessions {

SyncedSessions::SyncedSessions() = default;

SyncedSessions::SyncedSessions(
    sync_sessions::SessionSyncService* sync_service) {
  DCHECK(sync_service);
  // Reload Sync open tab sessions.
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      sync_service->GetOpenTabsUIDelegate();
  if (open_tabs) {
    // Iterating through all remote sessions, then retrieving the tabs to
    // display to the user.
    std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
        sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const sync_sessions::SyncedSession* session : sessions) {
      auto distant_session = std::make_unique<DistantSession>();
      distant_session->InitWithSyncedSession(session, open_tabs);
      // Don't display sessions with no tabs.
      if (!distant_session->tabs.empty()) {
        sessions_.push_back(std::move(distant_session));
      }
    }
  }
  std::sort(sessions_.begin(), sessions_.end(), CompareSessionsByTime);
}

SyncedSessions::~SyncedSessions() = default;

// Returns the session at index `index`.
DistantSession const* SyncedSessions::GetSession(size_t index) const {
  DCHECK_LE(index, GetSessionCount());
  return sessions_[index].get();
}

const DistantSession* SyncedSessions::GetSessionWithTag(
    const std::string& tag) const {
  for (auto const& distant_session : sessions_) {
    if (distant_session->tag == tag) {
      return distant_session.get();
    }
  }
  return nullptr;
}

// Returns the number of distant sessions.
size_t SyncedSessions::GetSessionCount() const {
  return sessions_.size();
}

// Deletes the session at index `index`.
void SyncedSessions::EraseSessionWithTag(const std::string& tag) {
  size_t index = GetSessionCount();
  for (size_t i = 0; i < GetSessionCount(); i++) {
    if (GetSession(i)->tag == tag) {
      index = i;
      break;
    }
  }

  DCHECK_LE(index, GetSessionCount());
  sessions_.erase(sessions_.begin() + index);
}

}  // namespace synced_sessions

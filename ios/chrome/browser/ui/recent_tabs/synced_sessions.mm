// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"

#import <functional>
#import <memory>

#import "base/check_op.h"
#import "base/strings/utf_string_conversions.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Comparator function for sorting the sessions_ vector so that the most
// recently used sessions are at the beginning.
bool SortSessionsByTime(
    std::unique_ptr<const synced_sessions::DistantSession>& a,
    std::unique_ptr<const synced_sessions::DistantSession>& b) {
  return a->modified_time > b->modified_time;
}

// Helper to extract the relevant content from a SessionTab and add it to a
// DistantSession.
void AddTabToDistantSession(const sessions::SessionTab& session_tab,
                            const std::string& session_tag,
                            synced_sessions::DistantSession* distant_session) {
  if (session_tab.navigations.size() > 0) {
    distant_session->tabs.push_back(
        std::make_unique<synced_sessions::DistantTab>());
    synced_sessions::DistantTab& distant_tab = *distant_session->tabs.back();
    distant_tab.session_tag = session_tag;
    distant_tab.tab_id = session_tab.tab_id;
    int index = session_tab.current_navigation_index;
    if (index < 0)
      index = 0;
    if (index > (int)session_tab.navigations.size() - 1)
      index = session_tab.navigations.size() - 1;
    const sessions::SerializedNavigationEntry* navigation =
        &session_tab.navigations[index];
    distant_tab.title = navigation->title();
    distant_tab.virtual_url = navigation->virtual_url();
    if (distant_tab.title.empty()) {
      std::string url = navigation->virtual_url().spec();
      distant_tab.title = base::UTF8ToUTF16(url);
    }
  }
}

}  // namespace

namespace synced_sessions {

DistantTab::DistantTab() : tab_id(SessionID::InvalidValue()) {}

size_t DistantTab::hashOfUserVisibleProperties() {
  std::stringstream ss;
  ss << title << std::endl << virtual_url.spec();
  return std::hash<std::string>()(ss.str());
}

DistantTabsSet::DistantTabsSet() = default;

DistantTabsSet::~DistantTabsSet() = default;

DistantTabsSet::DistantTabsSet(const DistantTabsSet&) = default;

DistantSession::DistantSession() = default;

DistantSession::DistantSession(sync_sessions::SessionSyncService* sync_service,
                               const std::string& tag) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      sync_service->GetOpenTabsUIDelegate();

  if (open_tabs) {
    std::vector<const sync_sessions::SyncedSession*> sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const auto* session : sessions) {
      if (tag == session->GetSessionTag()) {
        this->InitWithSyncedSession(session, open_tabs);
      }
    }
  }
}

DistantSession::~DistantSession() = default;

void DistantSession::InitWithSyncedSession(
    const sync_sessions::SyncedSession* synced_session,
    sync_sessions::OpenTabsUIDelegate* open_tabs_delegate) {
  tag = synced_session->GetSessionTag();
  name = synced_session->GetSessionName();
  modified_time = synced_session->GetModifiedTime();

  std::vector<const sessions::SessionTab*> open_tabs;
  open_tabs_delegate->GetForeignSessionTabs(tag, &open_tabs);
  for (const sessions::SessionTab* session_tab : open_tabs) {
    AddTabToDistantSession(*session_tab, tag, this);
  }
}

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
    std::vector<const sync_sessions::SyncedSession*> sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const auto* session : sessions) {
      auto distant_session = std::make_unique<DistantSession>();
      distant_session->InitWithSyncedSession(session, open_tabs);
      // Don't display sessions with no tabs.
      if (!distant_session->tabs.empty())
        sessions_.push_back(std::move(distant_session));
    }
  }
  std::sort(sessions_.begin(), sessions_.end(), SortSessionsByTime);
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

void SyncedSessions::AddDistantSessionForTest(
    std::unique_ptr<const DistantSession> distant_session) {
  sessions_.push_back(std::move(distant_session));
}

}  // synced_sessions namespace

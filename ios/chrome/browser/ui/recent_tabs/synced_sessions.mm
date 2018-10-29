// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"

#include <functional>
#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"

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

DistantSession::DistantSession() {}

DistantSession::DistantSession(syncer::SyncService* sync_service,
                               const std::string& tag) {
  if (sync_service->CanSyncFeatureStart() &&
      sync_service->GetOpenTabsUIDelegate()) {
    sync_sessions::OpenTabsUIDelegate* open_tabs =
        sync_service->GetOpenTabsUIDelegate();

    std::vector<const sync_sessions::SyncedSession*> sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const auto* session : sessions) {
      if (tag == session->session_tag) {
        this->InitWithSyncedSession(session, open_tabs);
      }
    }
  }
}

DistantSession::~DistantSession() {}

void DistantSession::InitWithSyncedSession(
    const sync_sessions::SyncedSession* synced_session,
    sync_sessions::OpenTabsUIDelegate* open_tabs) {
  tag = synced_session->session_tag;
  name = synced_session->session_name;
  modified_time = synced_session->modified_time;
  device_type = synced_session->device_type;

  // Order tabs by their visual position within window.
  for (const auto& kv : synced_session->windows) {
    for (const auto& session_tab : kv.second->wrapped_window.tabs) {
      AddTabToDistantSession(*session_tab, synced_session->session_tag, this);
    }
  }
}

SyncedSessions::SyncedSessions() {}

SyncedSessions::SyncedSessions(syncer::SyncService* sync_service) {
  DCHECK(sync_service);
  // Reload Sync open tab sesions.
  if (sync_service->CanSyncFeatureStart() &&
      sync_service->GetOpenTabsUIDelegate()) {
    sync_sessions::OpenTabsUIDelegate* open_tabs =
        sync_service->GetOpenTabsUIDelegate();

    // Iterating through all remote sessions, then all remote windows, then all
    // remote tabs to retrieve the tabs to display to the user. This will
    // flatten all of those data into a sequential vector of tabs.

    std::vector<const sync_sessions::SyncedSession*> sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const auto* session : sessions) {
      std::unique_ptr<DistantSession> distant_session(new DistantSession());
      distant_session->InitWithSyncedSession(session, open_tabs);
      // Don't display sessions with no tabs.
      if (distant_session->tabs.size() > 0)
        sessions_.push_back(std::move(distant_session));
    }
  }
  std::sort(sessions_.begin(), sessions_.end(), SortSessionsByTime);
}

SyncedSessions::~SyncedSessions() {}

// Returns the session at index |index|.
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

// Deletes the session at index |index|.
void SyncedSessions::EraseSession(size_t index) {
  DCHECK_LE(index, GetSessionCount());
  sessions_.erase(sessions_.begin() + index);
}

void SyncedSessions::AddDistantSessionForTest(
    std::unique_ptr<const DistantSession> distant_session) {
  sessions_.push_back(std::move(distant_session));
}

}  // synced_sessions namespace

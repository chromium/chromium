// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/distant_session.h"

#import "base/strings/utf_string_conversions.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"

namespace {

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
    distant_tab.modified_time = session_tab.timestamp;
    distant_tab.last_active_time = session_tab.last_active_time;
    int index = session_tab.current_navigation_index;
    if (index < 0) {
      index = 0;
    }
    if (index > (int)session_tab.navigations.size() - 1) {
      index = session_tab.navigations.size() - 1;
    }
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

DistantSession::DistantSession() = default;

DistantSession::DistantSession(sync_sessions::SessionSyncService* sync_service,
                               const std::string& tag) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      sync_service->GetOpenTabsUIDelegate();

  if (open_tabs) {
    std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
        sessions;
    open_tabs->GetAllForeignSessions(&sessions);
    for (const sync_sessions::SyncedSession* session : sessions) {
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
  form_factor = synced_session->GetDeviceFormFactor();

  std::vector<const sessions::SessionTab*> open_tabs;
  open_tabs_delegate->GetForeignSessionTabs(tag, &open_tabs);
  for (const sessions::SessionTab* session_tab : open_tabs) {
    AddTabToDistantSession(*session_tab, tag, this);
  }
}

}  // namespace synced_sessions

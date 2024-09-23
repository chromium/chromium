// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "components/sessions/core/live_tab.h"
#import "components/sessions/core/tab_restore_types.h"

FakeTabRestoreService::FakeTabRestoreService() = default;

FakeTabRestoreService::~FakeTabRestoreService() = default;

// static
FakeTabRestoreService::TestingFactory
FakeTabRestoreService::GetTestingFactory() {
  return base::BindRepeating(
      [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
        return std::make_unique<FakeTabRestoreService>();
      });
}

void FakeTabRestoreService::AddObserver(
    sessions::TabRestoreServiceObserver* observer) {
}

void FakeTabRestoreService::RemoveObserver(
    sessions::TabRestoreServiceObserver* observer) {
}

std::optional<SessionID> FakeTabRestoreService::CreateHistoricalTab(
    sessions::LiveTab* live_tab,
    int index) {
  auto tab = std::make_unique<sessions::tab_restore::Tab>();
  int entry_count =
      live_tab->IsInitialBlankNavigation() ? 0 : live_tab->GetEntryCount();
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    sessions::SerializedNavigationEntry entry = live_tab->GetEntryAtIndex(i);
    tab->navigations[i] = entry;
  }
  entries_.push_front(std::move(tab));
  return std::nullopt;
}

void FakeTabRestoreService::BrowserClosing(sessions::LiveTabContext* context) {
  NOTREACHED_IN_MIGRATION();
}

void FakeTabRestoreService::BrowserClosed(sessions::LiveTabContext* context) {
  NOTREACHED_IN_MIGRATION();
}

void FakeTabRestoreService::CreateHistoricalGroup(
    sessions::LiveTabContext* context,
    const tab_groups::TabGroupId& group) {
  NOTREACHED_IN_MIGRATION();
}

void FakeTabRestoreService::GroupClosed(const tab_groups::TabGroupId& group) {
  NOTREACHED_IN_MIGRATION();
}

void FakeTabRestoreService::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  NOTREACHED_IN_MIGRATION();
}

void FakeTabRestoreService::ClearEntries() {
}

void FakeTabRestoreService::DeleteNavigationEntries(
    const DeletionPredicate& predicate) {
  NOTREACHED_IN_MIGRATION();
}

const FakeTabRestoreService::Entries& FakeTabRestoreService::entries() const {
  return entries_;
}

std::vector<sessions::LiveTab*> FakeTabRestoreService::RestoreMostRecentEntry(
    sessions::LiveTabContext* context) {
  NOTREACHED_IN_MIGRATION();
  return std::vector<sessions::LiveTab*>();
}

void FakeTabRestoreService::RemoveEntryById(SessionID session_id) {
  FakeTabRestoreService::Entries::iterator it =
      GetEntryIteratorById(session_id);
  if (it == entries_.end()) {
    return;
  }
  entries_.erase(it);
}

std::vector<sessions::LiveTab*> FakeTabRestoreService::RestoreEntryById(
    sessions::LiveTabContext* context,
    SessionID session_id,
    WindowOpenDisposition disposition) {
  NOTREACHED_IN_MIGRATION();
  return std::vector<sessions::LiveTab*>();
}

void FakeTabRestoreService::LoadTabsFromLastSession() {
  NOTREACHED_IN_MIGRATION();
}

bool FakeTabRestoreService::IsLoaded() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void FakeTabRestoreService::DeleteLastSession() {
}

bool FakeTabRestoreService::IsRestoring() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

FakeTabRestoreService::Entries::iterator
FakeTabRestoreService::GetEntryIteratorById(SessionID session_id) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == session_id) {
      return i;
    }
  }
  return entries_.end();
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_FAKE_TAB_RESTORE_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_FAKE_TAB_RESTORE_SERVICE_H_

#import "components/sessions/core/tab_restore_service.h"

// A Fake restore service that just store and returns tabs.
class FakeTabRestoreService : public sessions::TabRestoreService {
 public:
  explicit FakeTabRestoreService();
  ~FakeTabRestoreService() override;

  void AddObserver(sessions::TabRestoreServiceObserver* observer) override;
  void RemoveObserver(sessions::TabRestoreServiceObserver* observer) override;
  absl::optional<SessionID> CreateHistoricalTab(sessions::LiveTab* live_tab,
                                                int index) override;
  void BrowserClosing(sessions::LiveTabContext* context) override;
  void BrowserClosed(sessions::LiveTabContext* context) override;
  void CreateHistoricalGroup(sessions::LiveTabContext* context,
                             const tab_groups::TabGroupId& group) override;
  void GroupClosed(const tab_groups::TabGroupId& group) override;
  void GroupCloseStopped(const tab_groups::TabGroupId& group) override;
  void ClearEntries() override;
  void DeleteNavigationEntries(const DeletionPredicate& predicate) override;
  const Entries& entries() const override;
  std::vector<sessions::LiveTab*> RestoreMostRecentEntry(
      sessions::LiveTabContext* context) override;
  void RemoveTabEntryById(SessionID session_id) override;
  std::vector<sessions::LiveTab*> RestoreEntryById(
      sessions::LiveTabContext* context,
      SessionID session_id,
      WindowOpenDisposition disposition) override;
  void LoadTabsFromLastSession() override;
  bool IsLoaded() const override;
  void DeleteLastSession() override;
  bool IsRestoring() const override;

 private:
  // Returns an iterator to the entry with id `session_id`.
  Entries::iterator GetEntryIteratorById(SessionID session_id);

  Entries entries_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_FAKE_TAB_RESTORE_SERVICE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_FAKE_TAB_RESTORE_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_FAKE_TAB_RESTORE_SERVICE_H_

#import "base/functional/callback_forward.h"
#import "components/sessions/core/tab_restore_service.h"

namespace web {
class BrowserState;
}

// A Fake restore service that just store and returns tabs.
class FakeTabRestoreService : public sessions::TabRestoreService {
 public:
  // Type of the factory returned by GetTestingFactory(). Can be registered
  // with TestProfileIOS::Builder::AddTestingFactory().
  using TestingFactory = base::RepeatingCallback<std::unique_ptr<KeyedService>(
      web::BrowserState*)>;

  explicit FakeTabRestoreService();
  ~FakeTabRestoreService() override;

  // Returns a factory that creates new instance of FakeTabRestoreService.
  static TestingFactory GetTestingFactory();

  // sessions::TabRestoreService implementation.
  void AddObserver(sessions::TabRestoreServiceObserver* observer) override;
  void RemoveObserver(sessions::TabRestoreServiceObserver* observer) override;
  std::optional<SessionID> CreateHistoricalTab(sessions::LiveTab* live_tab,
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
  void RemoveEntryById(SessionID session_id) override;
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

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_FAKE_TAB_RESTORE_SERVICE_H_

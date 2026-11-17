// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver.h"

#import "components/history/core/common/pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver_delegate.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FakeBrowsingHistoryDriverDelegate
    : public IOSBrowsingHistoryDriverDelegate {
 public:
  FakeBrowsingHistoryDriverDelegate() = default;
  ~FakeBrowsingHistoryDriverDelegate() override = default;

  // IOSBrowsingHistoryDriverDelegate:
  void HistoryQueryCompleted(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override {}

  void HistoryWasDeleted() override {}

  void ShowNoticeAboutOtherFormsOfBrowsingHistory(
      BOOL should_show_notice) override {}
};

class IOSBrowsingHistoryDriverTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAllowDeletingBrowserHistory, true);
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeBrowsingHistoryDriverDelegate delegate_;
};

// Tests that `AllowHistoryDeletions()` returns true when the
// `kAllowDeletingBrowserHistory` pref is enabled.
TEST_F(IOSBrowsingHistoryDriverTest, AllowHistoryDeletionsEnabled) {
  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, true);
  IOSBrowsingHistoryDriver driver(&pref_service_, &delegate_);
  EXPECT_TRUE(driver.AllowHistoryDeletions());
}

// Tests that `AllowHistoryDeletions()` returns false when the
// `kAllowDeletingBrowserHistory` pref is disabled.
TEST_F(IOSBrowsingHistoryDriverTest, AllowHistoryDeletionsDisabled) {
  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  IOSBrowsingHistoryDriver driver(&pref_service_, &delegate_);
  EXPECT_FALSE(driver.AllowHistoryDeletions());
}

}  // namespace

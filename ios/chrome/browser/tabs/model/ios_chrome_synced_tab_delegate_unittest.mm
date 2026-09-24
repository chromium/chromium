// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"

#import <memory>

#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/test_synced_window_delegates_getter.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

namespace {

using IOSChromeSyncedTabDelegateTest = PlatformTest;

// The minimum time between two sync updates of `last_active_time` when the tab
// hasn't changed.
constexpr base::TimeDelta kSyncActiveTimeThreshold = base::Minutes(10);

// Regression test for crbug.com/980841: Verifies that the browser does not
// crash if the pending item is null.
TEST_F(IOSChromeSyncedTabDelegateTest, ShouldHandleNullItem) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetPendingItemIndex(0);

  ASSERT_EQ(0, navigation_manager->GetPendingItemIndex());
  ASSERT_EQ(nullptr, navigation_manager->GetPendingItem());

  web::FakeWebState web_state;
  const SessionID window_id = SessionID::NewUnique();
  web_state.SetNavigationManager(std::move(navigation_manager));
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state, window_id);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  EXPECT_EQ(GURL(), tab_delegate->GetVirtualURLAtIndex(0));
}

// Tests that GetLastActiveTime() is returning the cached value if less time
// than a threshold has passed, and is returning the WebState last active time
// if more time has passed.
TEST_F(IOSChromeSyncedTabDelegateTest, CachedLastActiveTime) {
  web::FakeWebState web_state;
  const SessionID window_id = SessionID::NewUnique();
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state, window_id);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  base::Time original_time = base::Time::Now();
  web_state.SetLastActiveTime(original_time);

  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  // If not enough time has passed, the cached time should be returned.
  base::Time before_threshold =
      original_time + kSyncActiveTimeThreshold - base::Minutes(1);
  web_state.SetLastActiveTime(before_threshold);
  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  // After the threshold has passed, the new value should be returned.
  base::Time after_threshold =
      original_time + kSyncActiveTimeThreshold + base::Minutes(1);
  web_state.SetLastActiveTime(after_threshold);
  EXPECT_EQ(after_threshold, tab_delegate->GetLastActiveTime());
}

// Tests that the resetting the cached value of last_active_time allows to
// return the value from the WebState even if less time than the threshold has
// passed.
TEST_F(IOSChromeSyncedTabDelegateTest, ResetCachedLastActiveTime) {
  web::FakeWebState web_state;
  const SessionID window_id = SessionID::NewUnique();
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state, window_id);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  base::Time original_time = base::Time::Now();
  web_state.SetLastActiveTime(original_time);

  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  tab_delegate->ResetCachedLastActiveTime();

  // Even if the threshold is not passed, the cached value has been reset so the
  // new time should be returned.
  base::Time before_threshold =
      original_time + kSyncActiveTimeThreshold - base::Minutes(1);
  web_state.SetLastActiveTime(before_threshold);
  EXPECT_EQ(before_threshold, tab_delegate->GetLastActiveTime());
}

class FakeSyncSessionsClient : public sync_sessions::SyncSessionsClient {
 public:
  FakeSyncSessionsClient() = default;
  ~FakeSyncSessionsClient() override = default;

  sync_sessions::SessionSyncPrefs* GetSessionSyncPrefs() override {
    return nullptr;
  }
  syncer::RepeatingDataTypeStoreFactory GetStoreFactory() override {
    return syncer::RepeatingDataTypeStoreFactory();
  }
  void ClearAllOnDemandFavicons() override {}
  bool ShouldSyncURL(const GURL& url) const override { return true; }
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override {
    return true;
  }
  sync_sessions::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return &window_delegates_getter_;
  }
  sync_sessions::LocalSessionEventRouter* GetLocalSessionEventRouter()
      override {
    return nullptr;
  }
  base::WeakPtr<SyncSessionsClient> AsWeakPtr() override { return nullptr; }
  std::optional<std::string> GetSessionDisplayNameFromDeviceInfo(
      const std::string& session_tag) const override {
    return std::nullopt;
  }

  sync_sessions::TestSyncedWindowDelegatesGetter window_delegates_getter_;
};

TEST_F(IOSChromeSyncedTabDelegateTest, ShouldSync) {
  // Create a navigation entry (so that there's something to sync).
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->AddItem(GURL("https://example.com/"),
                              ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* navigation_item = navigation_manager->GetItemAtIndex(0);
  navigation_manager->SetLastCommittedItem(navigation_item);

  // Create a WebState aka "a tab".
  web::FakeWebState web_state;
  web_state.SetNavigationManager(std::move(navigation_manager));
  web_state.SetNavigationItemCount(1);
  const SessionID window_id = SessionID::NewUnique();
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state, window_id);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  FakeSyncSessionsClient client;
  // If the window is not known, it shouldn't sync.
  EXPECT_FALSE(tab_delegate->ShouldSync(&client));

  client.window_delegates_getter_.AddWindow(
      sync_pb::SyncEnums_BrowserType_TYPE_TABBED, window_id);
  EXPECT_TRUE(tab_delegate->ShouldSync(&client));
}

}  // namespace

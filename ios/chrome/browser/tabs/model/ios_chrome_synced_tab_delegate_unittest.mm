// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/features.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/test_synced_window_delegates_getter.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
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
  web_state.SetNavigationManager(std::move(navigation_manager));
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  EXPECT_EQ(GURL(), tab_delegate->GetVirtualURLAtIndex(0));
}

// Tests that GetLastActiveTime() is returning the cached value if less time
// than a threshold has passed, and is returning the WebState last active time
// if more time has passed.
TEST_F(IOSChromeSyncedTabDelegateTest, CachedLastActiveTime) {
  web::FakeWebState web_state;
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

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
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

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

  sync_sessions::TestSyncedWindowDelegatesGetter window_delegates_getter_;
};

TEST_F(IOSChromeSyncedTabDelegateTest,
       SyncOnlyTabsActiveAfterSigninForManagedAccount) {
  base::test::ScopedFeatureList scoped_feature_list{kIdentityDiscAccountMenu};

  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;

  // Create a BrowserState with the necessary services.
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(AuthenticationServiceFactory::GetInstance(),
                            AuthenticationServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      profile.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

  const base::Time pre_signin_time = base::Time::Now();

  // Sign in with a managed account.
  id<SystemIdentity> identity = [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  // Create a navigation entry (so that there's something to sync).
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->AddItem(GURL("https://example.com/"),
                              ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* navigation_item = navigation_manager->GetItemAtIndex(0);
  navigation_item->SetTimestamp(pre_signin_time - base::Minutes(1));
  navigation_manager->SetLastCommittedItem(navigation_item);

  // Create a WebState aka "a tab" plus the necessary helpers.
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile.get());
  web_state.SetNavigationManager(std::move(navigation_manager));
  web_state.SetNavigationItemCount(1);
  IOSChromeSessionTabHelper::CreateForWebState(&web_state);
  const SessionID window_id = SessionID::NewUnique();
  IOSChromeSessionTabHelper::FromWebState(&web_state)->SetWindowID(window_id);
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  FakeSyncSessionsClient client;
  client.window_delegates_getter_.AddWindow(
      sync_pb::SyncEnums_BrowserType_TYPE_TABBED, window_id);

  // A tab that was last active before the sign-in happened should *not* sync.
  web_state.SetLastActiveTime(pre_signin_time - base::Minutes(1));
  EXPECT_FALSE(tab_delegate->ShouldSync(&client));

  // Once the tab gets reactivated again after the signin, it should sync.
  web_state.SetLastActiveTime(pre_signin_time + base::Minutes(1));
  EXPECT_TRUE(tab_delegate->ShouldSync(&client));

  // Alternatively, if the tab does not get reactivated (because the user never
  // left the tab), but a navigation happens, that should also make it sync.
  web_state.SetLastActiveTime(pre_signin_time - base::Minutes(1));
  navigation_item->SetTimestamp(pre_signin_time + base::Minutes(1));
  EXPECT_TRUE(tab_delegate->ShouldSync(&client));
}

}  // namespace

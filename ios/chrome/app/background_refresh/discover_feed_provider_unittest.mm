// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/discover_feed_provider.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Fake C++ DiscoverFeedService that implements pure virtual methods
// and allows controlling the returned earliest background refresh date.
class FakeDiscoverFeedService : public DiscoverFeedService {
 public:
  FakeDiscoverFeedService() = default;
  ~FakeDiscoverFeedService() override = default;

  // DiscoverFeedRefresher implementation.
  void RefreshFeed(FeedRefreshTrigger trigger) override {}
  void PerformBackgroundRefreshes(void (^completion)(BOOL)) override {}
  void HandleBackgroundRefreshTaskExpiration() override {}
  NSDate* GetEarliestBackgroundRefreshBeginDate() override {
    return earliest_date_;
  }

  // DiscoverFeedService implementation.
  void CreateFeedModel() override {}
  void SetIsShownOnStartSurface(bool shown_on_start_surface) override {}
  FeedMetricsRecorder* GetFeedMetricsRecorder() override { return nil; }
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) override {
    return nil;
  }
  void RemoveFeedViewController(
      UIViewController* feed_view_controller) override {}
  void UpdateFeedViewVisibilityState(
      UICollectionView* collection_view,
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state) override {}
  void UpdateTheme() override {}

  void SetEarliestBackgroundRefreshBeginDate(NSDate* date) {
    earliest_date_ = date;
  }

 private:
  NSDate* earliest_date_ = nil;
};

// Controls whether CreateFakeDiscoverFeedService returns a service or nullptr.
bool g_return_null_service = false;

std::unique_ptr<KeyedService> CreateFakeDiscoverFeedService(
    ProfileIOS* profile) {
  if (g_return_null_service) {
    return nullptr;
  }
  return std::make_unique<FakeDiscoverFeedService>();
}

}  // namespace

class DiscoverFeedProviderTest : public PlatformTest {
 public:
  DiscoverFeedProviderTest() {
    g_return_null_service = false;
    scoped_feature_list_.InitAndEnableFeature(
        kEnableDiscoverBackgroundRefresh);

    TestProfileIOS::Builder builder;
    builder.SetName("Default");
    builder.AddTestingFactory(
        DiscoverFeedServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeDiscoverFeedService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    GetApplicationContext()->GetLocalState()->SetString(
        prefs::kLastUsedProfile, "Default");

    provider_ = [[DiscoverFeedProvider alloc] init];
  }

  ~DiscoverFeedProviderTest() override {
    g_return_null_service = false;
  }

  FakeDiscoverFeedService* GetFakeService() {
    return static_cast<FakeDiscoverFeedService*>(
        DiscoverFeedServiceFactory::GetForProfile(profile_));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  DiscoverFeedProvider* provider_;
};

// Tests that when the feature is disabled, the provider is not due and returns Max interval.
TEST_F(DiscoverFeedProviderTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      kEnableDiscoverBackgroundRefresh);

  EXPECT_FALSE([provider_ isDue]);
  EXPECT_EQ([provider_ refreshInterval], base::TimeDelta::Max());
  EXPECT_TRUE([provider_ task] == nil);
}

// Tests that when no service exists (e.g. factory returns nullptr),
// the provider safely handles it without crashing.
TEST_F(DiscoverFeedProviderTest, NullServiceHandledSafely) {
  g_return_null_service = true;
  TestProfileIOS::Builder builder;
  builder.SetName("NoServiceProfile");
  builder.AddTestingFactory(
      DiscoverFeedServiceFactory::GetInstance(),
      base::BindRepeating(&CreateFakeDiscoverFeedService));
  profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
  GetApplicationContext()->GetLocalState()->SetString(
      prefs::kLastUsedProfile, "NoServiceProfile");

  EXPECT_FALSE([provider_ isDue]);
  EXPECT_EQ([provider_ refreshInterval],
            kDiscoverFeedBackgroundRefreshNoServiceInterval.Get());
  EXPECT_TRUE([provider_ task] == nil);
}

// Tests the scenario from b/562865525:
// 1. Provider is used while signed in.
// 2. Profile changes to one without DiscoverFeedService (e.g. during account removal / signout).
// 3. App transitions to background and queries refreshInterval.
// Verifies that the provider does not crash on a dangling pointer and returns the fallback interval.
TEST_F(DiscoverFeedProviderTest, AccountRemovalDoesNotCrashOnRefreshInterval) {
  // Step 1: Initial query while service is present.
  GetFakeService()->SetEarliestBackgroundRefreshBeginDate(
      [NSDate dateWithTimeIntervalSinceNow:3600]);

  EXPECT_FALSE([provider_ isDue]);
  EXPECT_GT([provider_ refreshInterval].InSeconds(), 0);

  // Step 2: Switch to a profile state where DiscoverFeedService is null.
  g_return_null_service = true;
  TestProfileIOS::Builder builder;
  builder.SetName("LoggedOutProfile");
  builder.AddTestingFactory(
      DiscoverFeedServiceFactory::GetInstance(),
      base::BindRepeating(&CreateFakeDiscoverFeedService));
  profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
  GetApplicationContext()->GetLocalState()->SetString(
      prefs::kLastUsedProfile, "LoggedOutProfile");

  // Step 3: App goes to background after account removal, triggering refreshInterval.
  EXPECT_FALSE([provider_ isDue]);
  EXPECT_EQ([provider_ refreshInterval],
            kDiscoverFeedBackgroundRefreshNoServiceInterval.Get());
  EXPECT_TRUE([provider_ task] == nil);
}

// Tests that when the service returns a nil earliest date, it returns the no-date interval.
TEST_F(DiscoverFeedProviderTest, NilEarliestDateReturnsNoDateInterval) {
  GetFakeService()->SetEarliestBackgroundRefreshBeginDate(nil);

  EXPECT_FALSE([provider_ isDue]);
  EXPECT_EQ([provider_ refreshInterval],
            kDiscoverFeedBackgroundRefreshNoDateInterval.Get());
}

// Tests that when the service returns a valid future date, interval is calculated properly.
TEST_F(DiscoverFeedProviderTest, ValidFutureDateCalculatesInterval) {
  NSDate* futureDate = [NSDate dateWithTimeIntervalSinceNow:3600];
  GetFakeService()->SetEarliestBackgroundRefreshBeginDate(futureDate);

  EXPECT_FALSE([provider_ isDue]);
  base::TimeDelta interval = [provider_ refreshInterval];
  EXPECT_NEAR(interval.InSeconds(), 3600, 5);
}

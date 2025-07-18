// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/chrome_profile_download_service_tracker.h"

#import "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kProfileFoo[] = "foo";
constexpr char kProfileBar[] = "bar";
constexpr char kProfileBaz[] = "baz";

download::BackgroundDownloadService* GetBackgroundDownloadServiceForProfile(
    ProfileIOS* profile) {
  return BackgroundDownloadServiceFactory::GetForProfile(profile);
}

}  // namespace

namespace optimization_guide {

class ChromeProfileDownloadServiceTrackerIOSTest : public PlatformTest {
 public:
  ChromeProfileDownloadServiceTrackerIOSTest() = default;

  ChromeProfileDownloadServiceTrackerIOSTest(
      const ChromeProfileDownloadServiceTrackerIOSTest&) = delete;
  ChromeProfileDownloadServiceTrackerIOSTest& operator=(
      const ChromeProfileDownloadServiceTrackerIOSTest&) = delete;

 protected:
  ProfileIOS* CreateTestingProfile(const std::string& name) {
    TestProfileIOS::Builder builder;
    builder.SetName(name);
    return profile_manager_.AddProfileWithBuilder(std::move(builder));
  }

  void DeleteProfile(ChromeProfileDownloadServiceTracker* service_tracker,
                     ProfileIOS* profile) {
    service_tracker->OnProfileUnloaded(&profile_manager_, profile);
    service_tracker->OnProfileMarkedForPermanentDeletion(&profile_manager_,
                                                         profile);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
};

TEST_F(ChromeProfileDownloadServiceTrackerIOSTest, OneProfile) {
  ChromeProfileDownloadServiceTracker service_tracker;
  ProfileIOS* foo_profile = CreateTestingProfile(kProfileFoo);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));
}

TEST_F(ChromeProfileDownloadServiceTrackerIOSTest, TwoProfiles) {
  ChromeProfileDownloadServiceTracker service_tracker;
  ProfileIOS* foo_profile = CreateTestingProfile(kProfileFoo);
  ProfileIOS* bar_profile = CreateTestingProfile(kProfileBar);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));

  // Simulate foo profile deletion, the download manager should be picked from
  // bar profile.
  DeleteProfile(&service_tracker, foo_profile);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(bar_profile));

  // When another profile is created, it should still pick the bar profile.
  CreateTestingProfile(kProfileBaz);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(bar_profile));
}

TEST_F(ChromeProfileDownloadServiceTrackerIOSTest,
       ServiceTrackerCreatedAfterProfile) {
  ProfileIOS* foo_profile = CreateTestingProfile(kProfileFoo);
  task_environment_.RunUntilIdle();

  ChromeProfileDownloadServiceTracker service_tracker;

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));
}

}  // namespace optimization_guide

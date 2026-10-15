// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/chrome_profile_download_service_tracker.h"

#import "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

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
  ProfileIOS* CreatePersonalTestingProfile() {
    CHECK(profile_manager_.GetLoadedProfiles().empty());
    return profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

  ProfileIOS* CreateManagedTestingProfile(id<SystemIdentity> managed_identity) {
    return AddManagedProfileWithBuilder(profile_manager_, managed_identity);
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
  ChromeProfileDownloadServiceTracker service_tracker(&profile_manager_);
  ProfileIOS* personal_profile = CreatePersonalTestingProfile();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(personal_profile));
}

TEST_F(ChromeProfileDownloadServiceTrackerIOSTest, TwoProfiles) {
  ChromeProfileDownloadServiceTracker service_tracker(&profile_manager_);
  ProfileIOS* personal_profile = CreatePersonalTestingProfile();
  ProfileIOS* managed_profile =
      CreateManagedTestingProfile([FakeSystemIdentity fakeManagedIdentity]);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(personal_profile));

  // Simulate personal profile deletion, the download manager should be picked
  // from `managed_profile`.
  DeleteProfile(&service_tracker, personal_profile);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(managed_profile));

  // When another managed profile is created, it should still pick
  // `managed_profile`.
  CreateManagedTestingProfile(
      [FakeSystemIdentity identityWithEmail:@"baz@foo.com"]);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(managed_profile));
}

TEST_F(ChromeProfileDownloadServiceTrackerIOSTest,
       ServiceTrackerCreatedAfterProfile) {
  ProfileIOS* personal_profile = CreatePersonalTestingProfile();
  task_environment_.RunUntilIdle();

  ChromeProfileDownloadServiceTracker service_tracker(&profile_manager_);

  EXPECT_EQ(service_tracker.GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(personal_profile));
}

}  // namespace optimization_guide

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"

#import "base/base64url.h"
#import "base/hash/sha1.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/browser/identifiers/identifiers_prefs.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_delegate_ios_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise {

namespace {

constexpr char kFakeDeviceID[] = "fake-id";

}  // namespace

class ProfileIdServiceFactoryIOSTest : public PlatformTest {
 public:
  ProfileIdServiceFactoryIOSTest() {
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetClientId(kFakeDeviceID);
    profile_ = AddTestProfile(/*name=*/std::string());
    service_ = ProfileIdServiceFactoryIOS::GetForProfile(profile_);
  }

 protected:
  std::string GetTestProfileId(const ProfileIOS* profile) {
    std::string encoded_string;
    std::string device_id = kFakeDeviceID;
    base::Base64UrlEncode(
        base::SHA1HashString(profile->GetPrefs()->GetString(kProfileGUIDPref) +
                             device_id),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &encoded_string);
    return encoded_string;
  }

  void SetProfileIdService(ProfileIOS* profile) {
    service_ = ProfileIdServiceFactoryIOS::GetForProfile(profile);
  }

  TestProfileIOS::Builder CreateProfileBuilder(const std::string& name) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ProfileIdServiceFactoryIOS::GetInstance(),
                              ProfileIdServiceFactoryIOS::GetDefaultFactory());
    if (!name.empty()) {
      builder.SetName(name);
    }
    return builder;
  }

  // Adds a pre-configured test profile to the manager.
  TestProfileIOS* AddTestProfile(const std::string& name) {
    return profile_manager_.AddProfileWithBuilder(CreateProfileBuilder(name));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<ProfileIdService> service_;
  policy::FakeBrowserDMTokenStorage storage_;
};

// Tests multiple calls to get the profile identifier for the same profile has
// the same profile identifiers each time.
TEST_F(ProfileIdServiceFactoryIOSTest, GetProfileId_MultipleCalls) {
  auto profile_id1 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_), profile_id1.value());
  auto profile_id2 = service_->GetProfileId();
  EXPECT_EQ(profile_id2.value(), profile_id1.value());
  auto profile_id3 = service_->GetProfileId();
  EXPECT_EQ(profile_id3.value(), profile_id2.value());
}

// Tests that multiple profiles have different profile identifiers.
TEST_F(ProfileIdServiceFactoryIOSTest, GetProfileId_MultipleProfiles) {
  // The original profile is the profile set in BaseTest.
  auto profile_id_1 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_), profile_id_1.value());
  auto* profile_2 = AddTestProfile("profile-2");
  SetProfileIdService(profile_2);
  auto profile_id_2 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_2), profile_id_2.value());
  EXPECT_FALSE(profile_id_1.value() == profile_id_2.value());
}

// Tests that no profile identifier is created and no profile GUID is
// persisted(in the case that a profile guid did not previously exist).
TEST_F(ProfileIdServiceFactoryIOSTest, GetProfileId_Guest_Profile) {
  profile_->GetPrefs()->SetString(kProfileGUIDPref, "");
  SetProfileIdService(profile_);
  std::string profile_guid = profile_->GetPrefs()->GetString(kProfileGUIDPref);
  EXPECT_TRUE(profile_guid.empty());
}

// Tests that no service is created in OTR profiles since the factory has a
// profile preference for only creating the service for regular profiles.
TEST_F(ProfileIdServiceFactoryIOSTest, GetProfileId_Incognito_Profile) {
  auto* otr_profile = profile_->GetOffTheRecordProfile();
  SetProfileIdService(otr_profile);
  EXPECT_FALSE(service_);
}

}  // namespace enterprise

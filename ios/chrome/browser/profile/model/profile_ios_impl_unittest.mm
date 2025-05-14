// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_ios_impl.h"

#import "base/files/file_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/profile/model/test_with_profile.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"

using ProfileIOSImplTest = TestWithProfile;

// Tests that ProfileIOSImpl uses the profile name as storage identifier when
// the profile name is a valid UUID.
TEST_F(ProfileIOSImplTest, GetWebKitStorageID) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::string name = uuid.AsLowercaseString();

  ScopedProfileKeepAliveIOS keep_alive = CreateProfile(name);
  ProfileIOS* profile = keep_alive.profile();
  ASSERT_TRUE(profile);

  // The profile storage identifier should be equal to its name as an UUID.
  EXPECT_EQ(profile->GetWebKitStorageID(), uuid);
}

// Tests that ProfileIOSImpl uses a randomly generated UUID for the storage
// and store it in preference for pre-M-133 profiles.
TEST_F(ProfileIOSImplTest, GetWebKitStorageID_PreM133) {
  const std::string name = "Default";

  // As the profile name is not an UUID and the profile directory does not
  // exists, this is considered a M-128+ profile and will use a random UUID
  // stored in the pref service.
  const base::FilePath path = profile_data_dir().Append(name);
  ASSERT_FALSE(base::DirectoryExists(path));

  ScopedProfileKeepAliveIOS keep_alive = CreateProfile(name);
  ProfileIOS* profile = keep_alive.profile();
  ASSERT_TRUE(profile);

  // The profile storage identifier should be an invalid base::Uuid.
  const base::Uuid uuid = profile->GetWebKitStorageID();
  EXPECT_TRUE(uuid.is_valid());

  // Check that the string has been saved in the preference.
  EXPECT_EQ(
      uuid.AsLowercaseString(),
      profile->GetPrefs()->GetString(prefs::kBrowserStateStorageIdentifier));
}

// Tests that ProfileIOSImpl uses the default storage for pre M-128 profiles.
TEST_F(ProfileIOSImplTest, GetWebKitStorageID_PreM128) {
  const std::string name = "Default";

  // Create the directory so that the ProfileIOSImpl does not think this is
  // a new profile, but do not store anything in the PrefService so that it
  // is identifier as a pre M-128 profile.
  const base::FilePath path = profile_data_dir().Append(name);
  ASSERT_TRUE(base::CreateDirectory(path));

  ScopedProfileKeepAliveIOS keep_alive = CreateProfile(name);
  ProfileIOS* profile = keep_alive.profile();
  ASSERT_TRUE(profile);

  // The profile storage identifier should be an invalid base::Uuid.
  EXPECT_FALSE(profile->GetWebKitStorageID().is_valid());
}

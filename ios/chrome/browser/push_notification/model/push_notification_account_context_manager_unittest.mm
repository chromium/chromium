// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager+testing.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

struct TestCase {
  std::string gaia;
};

// Updates information about a Profile into `storage`.
void UpdateProfileAuthInfo(ProfileAttributesStorageIOS* storage,
                           const std::string& profile_name,
                           const std::string& gaia) {
  storage->UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce(
                        [](const std::string& gaia, ProfileAttributesIOS attr) {
                          attr.SetAuthenticationInfo(gaia, std::string());
                          return attr;
                        },
                        gaia));
}

// Iterates through the testcases and creates a new Profile for each
// testcase's gaia ID in storage and adds it to the AccountContextManager. In
// addition, this function validates that the testcases were added to the
// AccountContextManager.
template <unsigned long N>
void AddTestCasesToManagerAndValidate(
    PushNotificationAccountContextManager* manager,
    const TestCase (&test_cases)[N],
    ProfileAttributesStorageIOS* storage,
    const std::string& profile_name) {
  // Construct the Profiles with the given gaia id and add the gaia id into
  // the AccountContextManager.
  for (const TestCase& test_case : test_cases) {
    UpdateProfileAuthInfo(storage, profile_name, test_case.gaia);
    [manager addAccount:test_case.gaia];
  }

  ASSERT_EQ([manager accountIDs].count, N);

  // Validate that the given testcases exist inside the AccountContextManager.
  bool entries_are_valid = true;
  for (const TestCase& test_case : test_cases) {
    if (![manager preferenceMapForAccount:test_case.gaia]) {
      entries_are_valid = false;
    }
  }

  ASSERT_EQ(entries_are_valid, true);
}

}  // namespace

class PushNotificationAccountContextManagerTest : public PlatformTest {
 public:
  PushNotificationAccountContextManagerTest() {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());

    manager_ = [[PushNotificationAccountContextManager alloc]
        initWithProfileManager:&profile_manager_];
  }

  ProfileAttributesStorageIOS* profile_attributes_storage() const {
    return GetApplicationContext()
        ->GetProfileManager()
        ->GetProfileAttributesStorage();
  }

  const std::string& profile_name() const { return profile_->GetProfileName(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  PushNotificationAccountContextManager* manager_;
};

// This test ensures that the AccountContextManager can store a new account ID.
TEST_F(PushNotificationAccountContextManagerTest, AddAccount) {
  static const TestCase kTestCase[] = {{"0"}};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());
}

// This test ensures that the AccountContextManager can store multiple new
// account IDs.
TEST_F(PushNotificationAccountContextManagerTest, AddMultipleAccounts) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());
}

// This test ensures that new entries in the context map are not added for
// duplicates and that the occurence counter is properly incremented.
TEST_F(PushNotificationAccountContextManagerTest, AddDuplicates) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  static const TestCase kNoDuplicatesTestCase[] = {{"1"}, {"2"}, {"2"}};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());

  for (const TestCase& test_case : kNoDuplicatesTestCase) {
    UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                          test_case.gaia);
    [manager_ addAccount:test_case.gaia];
  }

  // Validate again that the original testcases are inside the
  // AccountContextManager.
  bool entries_are_valid = true;
  for (const TestCase& test_case : kTestCase) {
    if (![manager_ preferenceMapForAccount:test_case.gaia]) {
      entries_are_valid = false;
      break;
    }
  }
  ASSERT_EQ(entries_are_valid, true);

  // Validate the occurence counter has increased.
  ASSERT_EQ(
      [manager_ registrationCountForAccount:kNoDuplicatesTestCase[0].gaia], 2u);
  ASSERT_EQ(
      [manager_ registrationCountForAccount:kNoDuplicatesTestCase[1].gaia], 3u);
}

// This test ensures that the AccountContextManager can remove an account ID.
TEST_F(PushNotificationAccountContextManagerTest, RemoveAccount) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  const TestCase kRemovalTestCase = {"5"};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());

  // Add the testcase we would like to check for its removal into the manager.
  UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                        kRemovalTestCase.gaia);
  [manager_ addAccount:kRemovalTestCase.gaia];

  // Remove the testcase
  ASSERT_EQ([manager_ removeAccount:kRemovalTestCase.gaia], true);

  // Validate again that the original testcases are inside the
  // AccountContextManager.
  bool entries_are_valid = true;
  for (const TestCase& test_case : kTestCase) {
    if (![manager_ preferenceMapForAccount:test_case.gaia]) {
      entries_are_valid = false;
    }
  }

  ASSERT_EQ(entries_are_valid, true);
}

// This test ensures that the AccountContextManager can remove multiple account
// IDs.
TEST_F(PushNotificationAccountContextManagerTest, RemoveMultipleAccounts) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  static const TestCase kRemovalTestCase[] = {{"5"}, {"6"}, {"7"}};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());

  for (const TestCase& test_case : kRemovalTestCase) {
    UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                          test_case.gaia);
    [manager_ addAccount:test_case.gaia];
  }
  for (const TestCase& test_case : kRemovalTestCase) {
    // Remove the testcase
    ASSERT_EQ([manager_ removeAccount:test_case.gaia], true);
  }

  // Validate again that the original testcases are inside the
  // AccountContextManager.
  bool entries_are_valid = true;
  for (const TestCase& test_case : kTestCase) {
    if (![manager_ preferenceMapForAccount:test_case.gaia]) {
      entries_are_valid = false;
    }
  }

  ASSERT_EQ(entries_are_valid, true);
}

// This test ensures AccountContextManager can remove duplicate values.
TEST_F(PushNotificationAccountContextManagerTest, AddDuplicateThenRemove) {
  static const TestCase kTestCase[] = {{"0"}};
  const TestCase kRemovalTestCase = {"5"};
  static const TestCase kNoDuplicatesTestCase[] = {kRemovalTestCase};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());

  UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                        kRemovalTestCase.gaia);
  [manager_ addAccount:kRemovalTestCase.gaia];

  for (const TestCase& test_case : kNoDuplicatesTestCase) {
    UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                          test_case.gaia);
    [manager_ addAccount:test_case.gaia];
  }

  // Validate the occurence counter has increased.
  ASSERT_EQ(
      [manager_ registrationCountForAccount:kNoDuplicatesTestCase[0].gaia], 2u);
  // Remove the duplicate testcase twice.
  [manager_ removeAccount:kRemovalTestCase.gaia];
  ASSERT_EQ([manager_ removeAccount:kRemovalTestCase.gaia], true);

  // Validate again that the original testcases are inside the
  // AccountContextManager.
  bool entries_are_valid = true;
  for (const TestCase& test_case : kTestCase) {
    if (![manager_ preferenceMapForAccount:test_case.gaia]) {
      entries_are_valid = false;
      break;
    }
  }
  ASSERT_EQ(entries_are_valid, true);
}

// This test ensures the AccountContextManager can update its values. This test
// depends on the existence of the chosen push notification enabled feature to
// toggle.
TEST_F(PushNotificationAccountContextManagerTest, UpdatePreferences) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  AddTestCasesToManagerAndValidate(
      manager_, kTestCase, profile_attributes_storage(), profile_name());

  static const TestCase kUpdateTestCase[] = {{"0"}, {"2"}, {"4"}};

  PushNotificationClientId clientID = PushNotificationClientId::kCommerce;

  for (const TestCase& test_case : kTestCase) {
    UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                          test_case.gaia);
    [manager_ enablePushNotification:clientID forAccount:test_case.gaia];
    ASSERT_EQ([manager_ isPushNotificationEnabledForClient:clientID
                                                forAccount:test_case.gaia],
              YES);
  }

  for (const TestCase& test_case : kUpdateTestCase) {
    UpdateProfileAuthInfo(profile_attributes_storage(), profile_name(),
                          test_case.gaia);
    [manager_ disablePushNotification:clientID forAccount:test_case.gaia];
    ASSERT_EQ([manager_ isPushNotificationEnabledForClient:clientID
                                                forAccount:test_case.gaia],
              NO);
  }
}

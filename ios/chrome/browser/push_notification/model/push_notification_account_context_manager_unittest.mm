// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager+testing.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

struct TestCase {
  std::string gaia;
};

// Adds or updates information about a ChromeBrowserState into `cache`.
void AddOrUpdateBrowserStateAuthInfo(BrowserStateInfoCache* cache,
                                     const std::string& browser_state_name,
                                     const std::string& gaia) {
  if (const size_t index =
          cache->GetIndexOfBrowserStateWithName(browser_state_name);
      index != std::string::npos) {
    cache->SetAuthInfoOfBrowserStateAtIndex(index, gaia, std::string());
  } else {
    cache->AddBrowserState(browser_state_name, gaia, std::string());
  }
}

// Iterates through the testcases and creates a new BrowserState for each
// testcase's gaia ID in the info_cache and adds it to the
// AccountContextManager. In addition, this function validates that the
// testcases were added to the AccountContextManager.
template <unsigned long N>
void AddTestCasesToManagerAndValidate(
    PushNotificationAccountContextManager* manager,
    const TestCase (&test_cases)[N],
    BrowserStateInfoCache* info_cache,
    const std::string& browser_state_name) {
  // Construct the BrowserStates with the given gaia id and add the gaia id into
  // the AccountContextManager.
  for (const TestCase& test_case : test_cases) {
    AddOrUpdateBrowserStateAuthInfo(info_cache, browser_state_name,
                                    test_case.gaia);
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
    browser_state_ = browser_state_manager_.AddBrowserStateWithBuilder(
        TestChromeBrowserState::Builder());

    browser_state_info()->RemoveBrowserState(browser_state_name());
    manager_ = [[PushNotificationAccountContextManager alloc]
        initWithChromeBrowserStateManager:&browser_state_manager_];
  }

  BrowserStateInfoCache* browser_state_info() const {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetBrowserStateInfoCache();
  }

  const std::string& browser_state_name() const {
    return browser_state_->GetBrowserStateName();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestChromeBrowserStateManager browser_state_manager_;
  raw_ptr<ChromeBrowserState> browser_state_;
  PushNotificationAccountContextManager* manager_;
};

// This test ensures that the AccountContextManager can store a new account ID.
TEST_F(PushNotificationAccountContextManagerTest, AddAccount) {
  static const TestCase kTestCase[] = {{"0"}};

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());
}

// This test ensures that the AccountContextManager can store multiple new
// account IDs.
TEST_F(PushNotificationAccountContextManagerTest, AddMultipleAccounts) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());
}

// This test ensures that new entries in the context map are not added for
// duplicates and that the occurence counter is properly incremented.
TEST_F(PushNotificationAccountContextManagerTest, AddDuplicates) {
  static const TestCase kTestCase[] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};

  static const TestCase kNoDuplicatesTestCase[] = {{"1"}, {"2"}, {"2"}};

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());

  for (const TestCase& test_case : kNoDuplicatesTestCase) {
    AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
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

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());

  // Add the testcase we would like to check for its removal into the manager.
  AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
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

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());

  for (const TestCase& test_case : kRemovalTestCase) {
    AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
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

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());

  AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
                                  kRemovalTestCase.gaia);
  [manager_ addAccount:kRemovalTestCase.gaia];

  for (const TestCase& test_case : kNoDuplicatesTestCase) {
    AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
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

  AddTestCasesToManagerAndValidate(manager_, kTestCase, browser_state_info(),
                                   browser_state_name());

  static const TestCase kUpdateTestCase[] = {{"0"}, {"2"}, {"4"}};

  PushNotificationClientId clientID = PushNotificationClientId::kCommerce;

  for (const TestCase& test_case : kTestCase) {
    AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
                                    test_case.gaia);
    [manager_ enablePushNotification:clientID forAccount:test_case.gaia];
    ASSERT_EQ([manager_ isPushNotificationEnabledForClient:clientID
                                                forAccount:test_case.gaia],
              YES);
  }

  for (const TestCase& test_case : kUpdateTestCase) {
    AddOrUpdateBrowserStateAuthInfo(browser_state_info(), browser_state_name(),
                                    test_case.gaia);
    [manager_ disablePushNotification:clientID forAccount:test_case.gaia];
    ASSERT_EQ([manager_ isPushNotificationEnabledForClient:clientID
                                                forAccount:test_case.gaia],
              NO);
  }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns app upgrade details for an outdated application.
UpgradeRecommendedDetails OutdatedAppDetails() {
  UpgradeRecommendedDetails details;

  details.is_up_to_date = false;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://orgForName.org");

  return details;
}

}  // namespace

class SafetyCheckNotificationClientTest : public PlatformTest {
 public:
  void SetUp() override {
    SetupMockNotificationCenter();

    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);

    update->Set(kSafetyCheckNotificationKey, true);

    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    BrowserList* list = BrowserListFactory::GetForProfile(profile);

    mock_scene_state_ = OCMClassMock([SceneState class]);

    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);

    browser_ = std::make_unique<TestBrowser>(profile, mock_scene_state_);

    list->AddBrowser(browser_.get());

    pref_service_ = profile->GetPrefs();

    local_pref_service_ =
        TestingApplicationContext::GetGlobal()->GetLocalState();

    safety_check_manager_ =
        IOSChromeSafetyCheckManagerFactory::GetForProfile(profile);

    notification_client_ = std::make_unique<SafetyCheckNotificationClient>(
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    safety_check_manager_->StopSafetyCheck();
  }

  // Sets up a mock notification center, so notification requests can be
  // tested.
  void SetupMockNotificationCenter() {
    mock_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    // Swizzle in the mock notification center.
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return mock_notification_center_;
        };
    notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
  }

  // Stubs the notification center's completion callback for
  // getPendingNotificationRequestsWithCompletionHandler.
  void StubGetPendingRequests(NSArray<UNNotificationRequest*>* requests) {
    auto completionCaller =
        ^BOOL(void (^completion)(NSArray<UNNotificationRequest*>* requests)) {
          completion(requests);
          return YES;
        };
    OCMStub([mock_notification_center_
        getPendingNotificationRequestsWithCompletionHandler:
            [OCMArg checkWithBlock:completionCaller]]);
  }

  // Returns an `OCMArg` that verifies a `UNNotificationRequest` was passed
  // matching `notification_id`.
  id NotificationRequestArg(NSString* notification_id) {
    return [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* request) {
      EXPECT_NSEQ(request.identifier, notification_id);
      return YES;
    }];
  }

  // Sets up an OCMock expectation that a notification matching
  // `notification_id` is requested.
  void ExpectNotificationRequest(NSString* notification_id) {
    ExpectNotificationRequest(NotificationRequestArg(notification_id));
  }

  void ExpectNotificationRequest(id request) {
    auto completionCaller = ^BOOL(void (^completion)(NSError* error)) {
      if (completion) {
        completion(nil);
      }
      return YES;
    };

    OCMExpect([mock_notification_center_
        addNotificationRequest:request
         withCompletionHandler:[OCMArg checkWithBlock:completionCaller]]);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<SafetyCheckNotificationClient> notification_client_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<PrefService> local_pref_service_;
};

#pragma mark - Test cases

// Tests that HandleNotificationReception does nothing and returns "NoData".
TEST_F(SafetyCheckNotificationClientTest,
       HandleNotificationReceptionReturnsNoData) {
  EXPECT_EQ(notification_client_->HandleNotificationReception(nil),
            std::nullopt);
}

// Tests that RegisterActionalableNotifications returns an empty array.
TEST_F(SafetyCheckNotificationClientTest,
       RegisterActionableNotificationsReturnsEmptyArray) {
  EXPECT_EQ(notification_client_->RegisterActionableNotifications().count, 0u);
}

// Tests that a Safe Browsing notification is correctly scheduled when the user
// turns off Safe Browsing.
TEST_F(SafetyCheckNotificationClientTest, SchedulesSafeBrowsingNotification) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  StubGetPendingRequests(nil);
  ExpectNotificationRequest(kSafetyCheckSafeBrowsingNotificationID);

  base::RunLoop run_loop;

  notification_client_->OnSceneActiveForegroundBrowserReady(
      run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that a Update Chrome notification is correctly scheduled when the user
// has an available app update.
TEST_F(SafetyCheckNotificationClientTest, SchedulesUpdateChromeNotification) {
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(kSafetyCheckUpdateChromeNotificationID);

  // Simulate an available app update.
  //
  // Note: We need `task_environment_.RunUntilIdle()` because
  // `IOSChromeSafetyCheckManager`'s internals aren't exposed, and we must
  // ensure all pending tasks complete before proceeding.
  safety_check_manager_->StartOmahaCheckForTesting();
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);
  safety_check_manager_->HandleOmahaResponse(OutdatedAppDetails());
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;

  notification_client_->OnSceneActiveForegroundBrowserReady(
      run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that a Password notification is correctly scheduled when the user
// has a compromised credential.
TEST_F(SafetyCheckNotificationClientTest, SchedulesPasswordNotification) {
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(kSafetyCheckPasswordNotificationID);

  password_manager::InsecurePasswordCounts counts = {
      /* compromised */ 1, /* dismissed */ 0, /* reused */ 0,
      /* weak */ 0};

  safety_check_manager_->SetInsecurePasswordCountsForTesting(counts);

  safety_check_manager_->SetPasswordCheckStateForTesting(
      PasswordSafetyCheckState::kUnmutedCompromisedPasswords);

  base::RunLoop run_loop;

  notification_client_->OnSceneActiveForegroundBrowserReady(
      run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

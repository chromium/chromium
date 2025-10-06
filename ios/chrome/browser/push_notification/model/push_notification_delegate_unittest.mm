// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/run_loop.h"
#import "base/test/scoped_run_loop_timeout.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ScopedRunLoopTimeout;

@interface PushNotificationDelegate (Testing) <AppStateObserver>
@end

// Test fixture for PushNotificationDelegate.
// class PushNotificationDelegateTest : public TestWithProfile {
class PushNotificationDelegateTest : public PlatformTest {
 protected:
  PushNotificationDelegateTest() {}

  ~PushNotificationDelegateTest() override {}

  void SetUp() override {
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    [app_state_ startInitialization];
    profile_ = BuildProfile();
    scene_state_ = [[FakeSceneState alloc] initWithAppState:app_state_
                                                    profile:profile_];
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_;
    [app_state_ profileStateCreated:profile_state_];
    scene_state_.profileState = profile_state_;
    [profile_state_ sceneStateConnected:scene_state_];

    CreateUserNotificationCenter();
    delegate_ = [[PushNotificationDelegate alloc]
              initWithAppState:app_state_
        userNotificationCenter:user_notification_center_];
    [delegate_ appState:app_state_ sceneConnected:scene_state_];
  }

  void TearDown() override {
    [scene_state_ shutdown];
    profile_state_.profile = nullptr;
  }

  // Returns a mock UNNotification object with the given `identifier`.

  id MockNotification(NSString* identifier) {
    UNNotificationRequest* request = [UNNotificationRequest
        requestWithIdentifier:identifier
                      content:[[UNNotificationContent alloc] init]
                      trigger:nil];
    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification request]).andReturn(request);
    return mock_notification;
  }

  // Creates a mock UNUserNotificationCenter that returns an empty array for
  // getDeliveredNotificationsWithCompletionHandler.
  void CreateUserNotificationCenter() {
    user_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    id block = ^(void (^completionHandler)(NSArray<UNNotification*>*)) {
      completionHandler(@[]);
      return YES;
    };
    OCMStub([user_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            [OCMArg checkWithBlock:block]]);

    // Swizzle `currentNotificationCenter` because there is other code besides
    // the PushNotificationDelegate that accesses it.
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return user_notification_center_;
        };
    user_notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
  }

  // Builds a TestProfileIOS with all the required factories.
  TestProfileIOS* BuildProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
    builder.AddTestingFactory(
        IOSChromePasswordCheckManagerFactory::GetInstance(),
        IOSChromePasswordCheckManagerFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeSafetyCheckManagerFactory::GetInstance(),
        IOSChromeSafetyCheckManagerFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        PushNotificationProfileServiceFactory::GetInstance(),
        PushNotificationProfileServiceFactory::GetDefaultFactory());
    return profile_manager_.AddProfileWithBuilder(std::move(builder));
  }

  // Creates a TestBrowser object and adds it to the Profile's BrowserList.
  void CreateBrowser() {
    browser_ = std::make_unique<TestBrowser>(profile_, scene_state_);
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
    browser_list->AddBrowser(browser_.get());
  }

  // Advances the AppState's initStage to `kFinal`.
  void SimulateAppStateFinal() {
    while (app_state_.initStage != AppInitStage::kFinal) {
      [app_state_ queueTransitionToNextInitStage];
    }
    EXPECT_EQ(app_state_.initStage, AppInitStage::kFinal);
  }

  // Advances the ProfileState's initStage to `kFinal`.
  void SimulateProfileStateFinal() {
    while (profile_state_.initStage != ProfileInitStage::kFinal) {
      [profile_state_ queueTransitionToNextInitStage];
    }
    EXPECT_EQ(profile_state_.initStage, ProfileInitStage::kFinal);
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  AppState* app_state_;
  FakeSceneState* scene_state_;
  ProfileState* profile_state_;
  UNUserNotificationCenter* user_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> user_notification_center_swizzler_;
  PushNotificationDelegate* delegate_;
};

// Tests that willPresentNotification runs after the app is foreground active
// in order to avoid crashing because something is not fully loaded yet.
TEST_F(PushNotificationDelegateTest, WillPresentNotification) {
  ScopedRunLoopTimeout scoped_timeout(FROM_HERE, base::Seconds(5));
  __block bool completion_handler_called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit = run_loop.QuitClosure();
  UNNotification* notification = MockNotification(@"identifier");

  // Call willPresentNotification: before the app is fully initialized.
  // The completion handler should be queued.
  [delegate_
       userNotificationCenter:user_notification_center_
      willPresentNotification:notification
        withCompletionHandler:^(UNNotificationPresentationOptions options) {
          completion_handler_called = true;
          std::move(quit).Run();
        }];

  // The handler should not have been called yet.
  EXPECT_FALSE(completion_handler_called);

  // Simulate the AppState and ProfileState going to final, and SceneState
  // foreground active.
  CreateBrowser();
  SimulateAppStateFinal();
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SimulateProfileStateFinal();

  // Wait for the completion block to run.
  run_loop.Run();
  EXPECT_TRUE(completion_handler_called);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_run_loop_timeout.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/change_profile_commands.h"
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
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ScopedRunLoopTimeout;

@interface PushNotificationDelegate (Testing) <AppStateObserver>
@end

// Fake implementation of ChangeProfileCommands to intercept switch profile
// requests.
@interface FakeChangeProfileCommandHandler : NSObject <ChangeProfileCommands>
@property(nonatomic, assign) BOOL changeProfileCalled;
@property(nonatomic, assign) std::string lastProfileName;
@property(nonatomic, strong) SceneState* lastSceneState;
@property(nonatomic, assign) ChangeProfileReason lastReason;
@end

@implementation FakeChangeProfileCommandHandler
- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  self.changeProfileCalled = YES;
  self.lastProfileName = std::string(profileName);
  self.lastSceneState = sceneState;
  self.lastReason = reason;
  if (!continuation.is_null()) {
    std::move(continuation).Run(sceneState, base::DoNothing());
  }
}

- (void)deleteProfile:(std::string_view)profileName {
}
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

// Tests that when multi-profile push notification handling is enabled and the
// notification response doesn't contain a direct profile name but contains a
// Chime Gaia ID, the delegate maps it to the corresponding profile name and
// calls the ChangeProfileCommands handler.
TEST_F(PushNotificationDelegateTest,
       HandleNotificationResponseWithChimeGaiaID) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kSeparateProfilesForManagedAccounts, kIOSPushNotificationMultiProfile,
       kContentPushNotifications},
      {});

  // Assign Gaia ID "12345" to the test profile.
  profile_manager_.GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profile_->GetProfileName(),
          base::BindOnce([](ProfileAttributesIOS& attr) {
            ProfileAttributesIOS::GaiaIdSet gaia_ids;
            gaia_ids.insert(GaiaId("12345"));
            attr.SetAttachedGaiaIds(gaia_ids);
          }));

  // Setup the fake ChangeProfileCommands handler.
  FakeChangeProfileCommandHandler* fake_handler =
      [[FakeChangeProfileCommandHandler alloc] init];
  [app_state_.appCommandDispatcher
      startDispatchingToTarget:fake_handler
                   forProtocol:@protocol(ChangeProfileCommands)];

  // Create a mock UNNotificationResponse with nested Chime payload holding
  // target user's Gaia ID.
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.userInfo = @{@"$" : @{@"u" : @"12345"}};
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:@"identifier"
                                           content:content
                                           trigger:nil];
  id mock_response = OCMClassMock([UNNotificationResponse class]);
  id mock_notification = OCMClassMock([UNNotification class]);
  OCMStub([mock_response notification]).andReturn(mock_notification);
  OCMStub([mock_notification request]).andReturn(request);

  // Set the scene activation level to foreground active.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  CreateBrowser();
  SimulateAppStateFinal();
  SimulateProfileStateFinal();

  // Stub foregroundActiveScene on the delegate to return our scene_state_.
  id mock_delegate = OCMPartialMock(delegate_);
  OCMStub([mock_delegate foregroundActiveScene]).andReturn(scene_state_);

  // Trigger the delegate's didReceiveNotificationResponse method.
  [mock_delegate userNotificationCenter:user_notification_center_
         didReceiveNotificationResponse:mock_response
                  withCompletionHandler:^{
                  }];

  // Verify that changeProfile was called on the fake handler with the mapped
  // profile name.
  EXPECT_TRUE(fake_handler.changeProfileCalled);
  EXPECT_EQ(fake_handler.lastProfileName, profile_->GetProfileName());
  EXPECT_EQ(fake_handler.lastSceneState, scene_state_);
  EXPECT_EQ(fake_handler.lastReason,
            ChangeProfileReason::kHandlePushNotification);
}

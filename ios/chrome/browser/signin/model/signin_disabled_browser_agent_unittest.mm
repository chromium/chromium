// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_disabled_browser_agent.h"

#import "base/functional/bind.h"
#import "base/test/run_until.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

@interface FakeChangeProfileHandler : NSObject <ChangeProfileCommands>
@property(nonatomic, assign, readonly) std::string lastProfileName;
@property(nonatomic, weak, readonly) SceneState* lastSceneState;
@property(nonatomic, assign, readonly) std::optional<ChangeProfileReason>
    lastReason;
@end

@implementation FakeChangeProfileHandler {
  std::string _lastProfileName;
  __weak SceneState* _lastSceneState;
  std::optional<ChangeProfileReason> _lastReason;
}

- (std::string)lastProfileName {
  return _lastProfileName;
}

- (SceneState*)lastSceneState {
  return _lastSceneState;
}

- (std::optional<ChangeProfileReason>)lastReason {
  return _lastReason;
}

- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  _lastProfileName = std::string(profileName);
  _lastSceneState = sceneState;
  _lastReason = reason;
}

- (void)deleteProfile:(std::string_view)profileName {
}

@end

namespace {

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
  sync_preferences::PrefServiceMockFactory factory;
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
      factory.CreateSyncable(registry.get());
  RegisterProfilePrefs(registry.get());
  return prefs;
}

}  // namespace

class SigninDisabledBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
  }

  void TearDown() override {
    @autoreleasepool {
      [scene_state_ shutdown];
      scene_state_ = nil;
      profile_state_ = nil;
      app_state_ = nil;
    }
    PlatformTest::TearDown();
  }

  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  Browser* browser() {
    return scene_state_.browserProviderInterface.mainBrowserProvider.browser;
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  AppState* app_state_;
  ProfileState* profile_state_;
};

// Tests that disabling `prefs::kSigninAllowedOnDevice` signs out the user when
// signed in to the personal profile.
TEST_F(SigninDisabledBrowserAgentTest, SignOutWhenPrefChangesToFalse) {
  SigninDisabledBrowserAgent::CreateForBrowser(browser());
  SignIn();
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentity());

  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, false);

  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity());
}

// Tests that if `prefs::kSigninAllowedOnDevice` is already false when the
// agent is created, the user is signed out immediately.
TEST_F(SigninDisabledBrowserAgentTest, SignOutOnCreationWhenPrefAlreadyFalse) {
  SignIn();
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentity());
  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, false);

  SigninDisabledBrowserAgent::CreateForBrowser(browser());

  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity());
}

// Tests that disabling `prefs::kSigninAllowedOnDevice` on a non-personal
// profile triggers switching to the personal profile.
TEST_F(SigninDisabledBrowserAgentTest,
       SwitchToPersonalProfileOnManagedProfile) {
  TestProfileIOS::Builder managed_builder;
  managed_builder.SetName("managed");
  managed_builder.SetPrefService(CreatePrefService());
  managed_builder.AddTestingFactory(
      AuthenticationServiceFactory::GetInstance(),
      AuthenticationServiceFactory::GetDefaultFactory());
  managed_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateTestSyncService));
  TestProfileIOS* managed_profile =
      profile_manager_.AddProfileWithBuilder(std::move(managed_builder));

  FakeSceneState* managed_scene_state =
      [[FakeSceneState alloc] initWithProfile:managed_profile];
  managed_scene_state.profileState = profile_state_;

  SigninDisabledBrowserAgent::CreateForBrowser(
      managed_scene_state.browserProviderInterface.mainBrowserProvider.browser);

  FakeChangeProfileHandler* change_profile_handler =
      [[FakeChangeProfileHandler alloc] init];
  [app_state_.appCommandDispatcher
      startDispatchingToTarget:change_profile_handler
                   forProtocol:@protocol(ChangeProfileCommands)];

  GetLocalState()->SetBoolean(prefs::kSigninAllowedOnDevice, false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return change_profile_handler.lastReason.has_value(); }));

  std::string personal_profile_name =
      profile_manager_.GetProfileAttributesStorage()->GetPersonalProfileName();
  EXPECT_EQ(change_profile_handler.lastProfileName, personal_profile_name);
  EXPECT_EQ(change_profile_handler.lastSceneState, managed_scene_state);
  EXPECT_EQ(change_profile_handler.lastReason,
            ChangeProfileReason::kManagedAccountSignOut);

  [managed_scene_state shutdown];
}

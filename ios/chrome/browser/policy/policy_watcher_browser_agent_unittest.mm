// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"

#import "base/apple/backup_util.h"
#import "base/mac/foundation_util.h"
#import "base/path_service.h"
#import "base/test/ios/wait_util.h"
#import "build/build_config.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/pref_names.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;
using web::WebTaskEnvironment;

class PolicyWatcherBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    // Set the initial pref value.
    GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                                static_cast<int>(BrowserSigninMode::kEnabled));

    // Set up the test browser and attach the browser agents.
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    // Browser Agent under test.
    PolicyWatcherBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = PolicyWatcherBrowserAgent::FromBrowser(browser_.get());

    // SceneState Browser Agent.
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:chrome_browser_state_.get()];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    PrefServiceMockFactory factory;
    scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable);
    std::unique_ptr<PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  // Sign in in the authentication service with a fake identity.
  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForBrowserState(
        chrome_browser_state_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  PolicyWatcherBrowserAgent* agent_;
  std::unique_ptr<Browser> browser_;
  FakeSceneState* scene_state_;
  // Keep app_state_ alive as it is a weak property of the scene state.
  AppState* app_state_;
};

#pragma mark - Tests.

// Tests that the browser agent doesn't monitor the pref if Initialize hasn't
// been called.
TEST_F(PolicyWatcherBrowserAgentTest, NoObservationIfNoInitialize) {
  // Set the initial pref value.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kEnabled));

  // Set up the test browser and attach the browser agent under test.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(chrome_browser_state_.get());
  PolicyWatcherBrowserAgent::CreateForBrowser(browser.get());

  // Set up the mock observer handler as strict mock. Calling it will fail the
  // test.
  id mockObserver =
      OCMStrictProtocolMock(@protocol(PolicyWatcherBrowserAgentObserving));
  PolicyWatcherBrowserAgentObserverBridge bridge(mockObserver);
  agent_->AddObserver(&bridge);

  // Action: disable browser sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  agent_->RemoveObserver(&bridge);
}

// Tests that the browser agent monitors the kBrowserSigninPolicy pref and
// notifies its observers when it changes.
TEST_F(PolicyWatcherBrowserAgentTest, ObservesSigninAllowedByPolicy) {
  // Set the initial pref value.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kEnabled));
  // Set up the mock observer handler.
  id mockObserver =
      OCMStrictProtocolMock(@protocol(PolicyWatcherBrowserAgentObserving));
  PolicyWatcherBrowserAgentObserverBridge bridge(mockObserver);
  agent_->AddObserver(&bridge);
  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);

  // Setup the expectation after the Initialize to make sure that the observers
  // are notified when the pref is updated and not during Initialize().
  OCMExpect(
      [mockObserver policyWatcherBrowserAgentNotifySignInDisabled:agent_]);

  // Action: disable browser sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  // Verify the forceSignOut command was dispatched by the browser agent.
  EXPECT_OCMOCK_VERIFY(mockObserver);

  agent_->RemoveObserver(&bridge);
}

// Tests that the pref change doesn't trigger a command if the user isn't signed
// in.
TEST_F(PolicyWatcherBrowserAgentTest, NoCommandIfNotSignedIn) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());

  ASSERT_FALSE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Strict mock, will fail if a method is called.
  id mockHandler = OCMStrictProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);

  // Action: disable browser sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
}

// Tests that the pref change triggers a command if the user is signed
// in.
TEST_F(PolicyWatcherBrowserAgentTest, CommandIfSignedIn) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());

  SignIn();

  ASSERT_TRUE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  OCMExpect([mockHandler showForceSignedOutPrompt]).andDo(^(NSInvocation*) {
    run_loop_ptr->Quit();
  });

  // Action: disable browser sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
  run_loop.Run();

  // Verify the forceSignOut command was dispatched by the browser agent.
  EXPECT_OCMOCK_VERIFY(mockHandler);
  EXPECT_FALSE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that the pref change doesn't trigger a command if the scene isn't
// active.
TEST_F(PolicyWatcherBrowserAgentTest, NoCommandIfNotActive) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());

  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  SignIn();

  ASSERT_TRUE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Strict mock, will fail if a method is called.
  id mockHandler = OCMStrictProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);

  // Action: disable browser sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        base::RunLoop().RunUntilIdle();
        return scene_state_.appState.shouldShowForceSignOutPrompt;
      }));
  EXPECT_FALSE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that the handler is called and the user signed out if the policy is
// updated while the app is not running.
TEST_F(PolicyWatcherBrowserAgentTest, SignOutIfPolicyChangedAtColdStart) {
  // Create another Agent from a new browser to simulate a behaviour of "the
  // pref changed in background.

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  SignIn();

  // Update the pref and Sign in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  // Set up the test browser and attach the browser agents.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(chrome_browser_state_.get());

  // Browser Agent under test.
  PolicyWatcherBrowserAgent::CreateForBrowser(browser.get());
  PolicyWatcherBrowserAgent* agent =
      PolicyWatcherBrowserAgent::FromBrowser(browser.get());

  FakeSceneState* scene_state =
      [[FakeSceneState alloc] initWithAppState:app_state_
                                  browserState:chrome_browser_state_.get()];
  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  SceneStateBrowserAgent::CreateForBrowser(browser.get(), scene_state);

  // The SignOut will occur when the handler is set.
  ASSERT_TRUE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  OCMExpect([mockHandler showForceSignedOutPrompt]).andDo(^(NSInvocation*) {
    run_loop_ptr->Quit();
  });
  agent->Initialize(mockHandler);
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mockHandler);
  EXPECT_FALSE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that the command to show the UI isn't sent if the authentication
// service is still signing out the user.
TEST_F(PolicyWatcherBrowserAgentTest, UINotShownWhileSignOut) {
  AuthenticationService* authentication_service =
      static_cast<AuthenticationService*>(
          AuthenticationServiceFactory::GetForBrowserState(
              chrome_browser_state_.get()));

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(identity);
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_TRUE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  // Strict protocol: method calls will fail until the method is stubbed.
  id mockHandler = OCMStrictProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);

  // As the SignOut callback hasn't been called yet, this shouldn't trigger a UI
  // update.
  agent_->SignInUIDismissed();

  OCMExpect([mockHandler showForceSignedOutPrompt]);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Once the SignOut callback is executed, the command should be sent.
  EXPECT_OCMOCK_VERIFY(mockHandler);
}

// Tests that the command to show the UI is sent when the Browser Agent is
// notified of the UI being dismissed.
TEST_F(PolicyWatcherBrowserAgentTest, CommandSentWhenUIIsDismissed) {
  SignIn();

  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  // Strict protocol: method calls will fail until the method is stubbed.
  id mockHandler = OCMStrictProtocolMock(@protocol(PolicyChangeCommands));
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  OCMExpect([mockHandler showForceSignedOutPrompt]).andDo(^(NSInvocation*) {
    run_loop_ptr->Quit();
  });
  agent_->Initialize(mockHandler);
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mockHandler);

  // Reset the expectation for the SignInUIDismissed call.
  OCMExpect([mockHandler showForceSignedOutPrompt]);

  agent_->SignInUIDismissed();

  EXPECT_OCMOCK_VERIFY(mockHandler);
}

// Tests that the handler is called and the alert shown as expected.
TEST_F(PolicyWatcherBrowserAgentTest, AlertIfSyncDisabledChanges) {
  // Make sure shown if off.
  NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
  [standard_defaults setBool:NO forKey:kSyncDisabledAlertShownKey];
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      syncer::prefs::internal::kSyncManaged, false);

  // Browser Agent under test.
  // Set up the test browser and attach the browser agents.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(chrome_browser_state_.get());

  // Browser Agent under test.
  PolicyWatcherBrowserAgent::CreateForBrowser(browser.get());
  PolicyWatcherBrowserAgent* agent =
      PolicyWatcherBrowserAgent::FromBrowser(browser.get());

  @autoreleasepool {
    FakeSceneState* scene_state =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:chrome_browser_state_.get()];
    scene_state.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser.get(), scene_state);

    id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
    OCMExpect([mockHandler showSyncDisabledPrompt]);
    agent->Initialize(mockHandler);

    // Update the pref.
    browser_->GetBrowserState()->GetPrefs()->SetBoolean(
        syncer::prefs::internal::kSyncManaged, true);

    EXPECT_OCMOCK_VERIFY(mockHandler);
    EXPECT_TRUE([standard_defaults boolForKey:kSyncDisabledAlertShownKey]);

    [[mockHandler reject] showSyncDisabledPrompt];

    // Update the pref.
    browser_->GetBrowserState()->GetPrefs()->SetBoolean(
        syncer::prefs::internal::kSyncManaged, false);

    EXPECT_OCMOCK_VERIFY(mockHandler);
    EXPECT_FALSE([standard_defaults boolForKey:kSyncDisabledAlertShownKey]);
  }
}

// Tests that the handler is called and the alert shown at startup as expected.
TEST_F(PolicyWatcherBrowserAgentTest, AlertIfSyncDisabledChangedAtColdStart) {
  // Make sure shown if off.
  NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
  [standard_defaults setBool:NO forKey:kSyncDisabledAlertShownKey];
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      syncer::prefs::internal::kSyncManaged, true);

  // Browser Agent under test.
  // Set up the test browser and attach the browser agents.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(chrome_browser_state_.get());

  // Browser Agent under test.
  PolicyWatcherBrowserAgent::CreateForBrowser(browser.get());
  PolicyWatcherBrowserAgent* agent =
      PolicyWatcherBrowserAgent::FromBrowser(browser.get());

  @autoreleasepool {
    FakeSceneState* scene_state =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:chrome_browser_state_.get()];
    scene_state.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser.get(), scene_state);

    id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
    OCMExpect([mockHandler showSyncDisabledPrompt]);
    agent->Initialize(mockHandler);

    base::RunLoop().RunUntilIdle();

    EXPECT_OCMOCK_VERIFY(mockHandler);
    EXPECT_TRUE([standard_defaults boolForKey:kSyncDisabledAlertShownKey]);

    [[mockHandler reject] showSyncDisabledPrompt];

    // Update the pref.
    browser_->GetBrowserState()->GetPrefs()->SetBoolean(
        syncer::prefs::internal::kSyncManaged, false);

    EXPECT_OCMOCK_VERIFY(mockHandler);
    EXPECT_FALSE([standard_defaults boolForKey:kSyncDisabledAlertShownKey]);
  }
}

// Tests that disabling the backup-allowed preference marks the app container
// as excluded from backup, and enabling the preference clears this exclusion.
TEST_F(PolicyWatcherBrowserAgentTest, BackupPreventionChanged) {
  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);
  base::FilePath storage_dir = base::mac::GetUserLibraryPath();

  // Ensure that backups are allowed initially.
  ASSERT_TRUE(base::apple::ClearBackupExclusion(storage_dir));

  // Disallow backups.
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kAllowChromeDataInBackups, false);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::apple::GetBackupExclusion(storage_dir));

  // Allow backups.
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kAllowChromeDataInBackups, true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::apple::GetBackupExclusion(storage_dir));
}

// Tests that disabling the backup-allowed preference marks the app container
// as excluded from backup at startup.
TEST_F(PolicyWatcherBrowserAgentTest, BackupDisallowedAtColdStart) {
  base::FilePath storage_dir = base::mac::GetUserLibraryPath();

  // Ensure that backups are allowed initially.
  ASSERT_TRUE(base::apple::ClearBackupExclusion(storage_dir));

  // Disallow backups
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kAllowChromeDataInBackups, false);

  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::apple::GetBackupExclusion(storage_dir));
}

// Tests that enabling the backup-allowed preference marks the app container
// as no longer excluded from backup at startup.
TEST_F(PolicyWatcherBrowserAgentTest, BackupAllowedAtColdStart) {
  base::FilePath storage_dir = base::mac::GetUserLibraryPath();

  // Ensure that backups are disallowed initially.
  ASSERT_TRUE(base::apple::SetBackupExclusion(storage_dir));

  // Allow backups
  browser_->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kAllowChromeDataInBackups, true);

  id mockHandler = OCMProtocolMock(@protocol(PolicyChangeCommands));
  agent_->Initialize(mockHandler);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::apple::GetBackupExclusion(storage_dir));
}

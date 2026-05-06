// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/signin_account_capabilities_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_coordinator.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_manager.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_target.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SigninAccountCapabilitiesSceneAgentTest : public PlatformTest {
 public:
  SigninAccountCapabilitiesSceneAgentTest() : PlatformTest() {
    feature_list_.InitAndEnableFeature(
        switches::kEnforceCanSignInToChromeCapability);

    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    app_state_ = OCMClassMock([AppState class]);
    SceneState* scene_state = [[SceneState alloc] initWithAppState:app_state_];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state addAgent:layout_guide_scene_agent];
    scene_state_ = OCMPartialMock(scene_state);

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    stub_browser_interface_provider_ =
        [[StubBrowserProviderInterface alloc] init];
    stub_browser_interface_provider_.mainBrowserProvider.browser =
        browser_.get();
    OCMStub([scene_state_ browserProviderInterface])
        .andReturn(stub_browser_interface_provider_);

    scene_ui_provider_ = OCMProtocolMock(@protocol(SceneUIProvider));

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    scene_state_.profileState = profile_state_;

    agent_ = [[SigninAccountCapabilitiesSceneAgent alloc]
        initWithSceneUIProvider:scene_ui_provider_];
    agent_.sceneState = scene_state_;
  }

  ~SigninAccountCapabilitiesSceneAgentTest() override {
    [agent_ sceneStateDidDisableUI:scene_state_];
  }

  void AddIdentity(FakeSystemIdentity* identity) {
    fake_system_identity_manager_->AddIdentity(identity);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    auto options =
        signin::AccountAvailabilityOptionsBuilder().WithGaiaId(identity.gaiaId);
    signin::MakeAccountAvailable(
        identity_manager,
        options.Build(base::SysNSStringToUTF8(identity.userEmail)));
  }

  void SetPrimaryIdentity(FakeSystemIdentity* identity) {
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    authentication_service->SignIn(identity,
                                   signin_metrics::AccessPoint::kSettings);
  }

  void RemoveIdentity(FakeSystemIdentity* identity) {
    fake_system_identity_manager_->ForgetIdentityFromOtherApplication(identity);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    signin::RemoveRefreshTokenForAccount(
        identity_manager, identity_manager
                              ->FindExtendedAccountInfoByEmailAddress(
                                  base::SysNSStringToUTF8(identity.userEmail))
                              .account_id);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  id<SceneUIProvider> scene_ui_provider_;
  ProfileState* profile_state_;
  AppState* app_state_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  StubBrowserProviderInterface* stub_browser_interface_provider_;
  SigninAccountCapabilitiesSceneAgent* agent_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
};

// Tests that the agent handles the sign-in request from the coordinator.
TEST_F(SigninAccountCapabilitiesSceneAgentTest, TestWantsToSignIn) {
  // Mock the SceneCommands handler.
  id<SceneCommands> scene_commands_mock =
      OCMProtocolMock(@protocol(SceneCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:scene_commands_mock
                   forProtocol:@protocol(SceneCommands)];

  // Create a mock coordinator.
  id coordinator_mock = OCMClassMock([AgeMismatchSignoutCoordinator class]);
  [agent_ setValue:coordinator_mock forKey:@"ageMismatchSignoutCoordinator"];

  // Expect the sign-in command to be shown.
  OCMExpect([scene_commands_mock showSignin:[OCMArg any]
                         baseViewController:[OCMArg any]]);

  // Call the delegate method.
  id<AgeMismatchSignoutCoordinatorDelegate> delegate =
      (id<AgeMismatchSignoutCoordinatorDelegate>)agent_;
  [delegate ageMismatchSignoutCoordinatorWantsToSignIn:coordinator_mock];

  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // Clean up.
  [agent_ setValue:nil forKey:@"ageMismatchSignoutCoordinator"];

  // Verify that the sign-in command was shown.
  EXPECT_OCMOCK_VERIFY((id)scene_commands_mock);
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
}


// Tests that the agent signs out the account if the `CanSignInToChrome`
// capability is explicitly set to false.
TEST_F(SigninAccountCapabilitiesSceneAgentTest,
       TestSignoutOnCanSignInToChromeCapabilityFalse) {
  base::HistogramTester histogram_tester;

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  AddIdentity(identity);
  SetPrimaryIdentity(identity);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  AccountInfo account = identity_manager->FindExtendedAccountInfoByEmailAddress(
      base::SysNSStringToUTF8(identity.userEmail));

  account = signin::WithGeneratedUserInfo(account, "Test");
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_sign_in_to_chrome(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account);

  base::HistogramTester* histogram_tester_ptr = &histogram_tester;
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return histogram_tester_ptr->GetBucketCount(
                   "Signin.SignoutProfile",
                   signin_metrics::ProfileSignout::
                       kSignoutFromCanSignInToChromeCapability) == 1;
      }));

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(authentication_service->HasPrimaryIdentity());

  // Wait for the sign-out completion block to finish.
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that isSignoutInProgress returns YES during sign-out and while the
// coordinator is running.
TEST_F(SigninAccountCapabilitiesSceneAgentTest, TestIsSignoutInProgress) {
  EXPECT_FALSE(agent_.isSignoutInProgress);

  // Set sign-out in progress.
  [agent_ setValue:@YES forKey:@"isAgeMismatchSignoutInProgress"];
  EXPECT_TRUE(agent_.isSignoutInProgress);

  // Set coordinator running.
  id coordinator_mock = OCMClassMock([AgeMismatchSignoutCoordinator class]);
  [agent_ setValue:coordinator_mock forKey:@"ageMismatchSignoutCoordinator"];
  [agent_ setValue:@NO forKey:@"isAgeMismatchSignoutInProgress"];
  EXPECT_TRUE(agent_.isSignoutInProgress);

  // Set both.
  [agent_ setValue:@YES forKey:@"isAgeMismatchSignoutInProgress"];
  EXPECT_TRUE(agent_.isSignoutInProgress);

  // Clean up.
  [agent_ setValue:@NO forKey:@"isAgeMismatchSignoutInProgress"];
  [agent_ setValue:nil forKey:@"ageMismatchSignoutCoordinator"];
  EXPECT_FALSE(agent_.isSignoutInProgress);
}

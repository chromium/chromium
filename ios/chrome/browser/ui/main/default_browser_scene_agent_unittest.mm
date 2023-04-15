// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/default_browser/utils_test_support.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class DefaultBrowserSceneAgentTest : public PlatformTest {
 public:
  DefaultBrowserSceneAgentTest() : PlatformTest() {}

 protected:
  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    std::unique_ptr<Browser> browser_ =
        std::make_unique<TestBrowser>(browser_state_.get());
    id browser_launcher_mock_ =
        [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    id main_application_delegate_ =
        [OCMockObject mockForClass:[MainApplicationDelegate class]];
    app_state_ =
        [[AppState alloc] initWithBrowserLauncher:browser_launcher_mock_
                               startupInformation:startup_information_
                              applicationDelegate:main_application_delegate_];
    app_state_.mainBrowserState = browser_state_.get();
    promos_manager_ = std::make_unique<MockPromosManager>();
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:browser_state_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    dispatcher_ = [[CommandDispatcher alloc] init];
    agent_ = [[DefaultBrowserSceneAgent alloc]
        initWithCommandDispatcher:dispatcher_];
    agent_.sceneState = scene_state_;
    agent_.promosManager = promos_manager_.get();
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

  void TearDown() override {
    browser_state_.reset();
    ClearDefaultBrowserPromoData();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }
  void EnableDefaultBrowserPromoRefactoringFlag() {
    scoped_feature_list_.InitWithFeatures(
        {kDefaultBrowserRefactoringPromoManager}, {});
  }
  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(identity);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  DefaultBrowserSceneAgent* agent_;
  FakeSceneState* scene_state_;
  AppState* app_state_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id dispatcher_;
};

// Tests that DefaultBrowser was registered with the promo manager when a
// condition is met for a tailored promo.
TEST_F(DefaultBrowserSceneAgentTest,
       TestPromoRegistrationLikelyInterestedTailored) {
  EnableDefaultBrowserPromoRefactoringFlag();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  SignIn();
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(1);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that DefaultBrowser was registered with the promo manager when the
// condition is met for a default promo.
TEST_F(DefaultBrowserSceneAgentTest,
       TestPromoRegistrationLikelyInterestedDefault) {
  EnableDefaultBrowserPromoRefactoringFlag();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  SignIn();
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(1);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that DefaultBrowser was registered to the promo manager when the
// conditions (ShouldRegisterPromoWithPromoManager) are met when
// kDefaultBrowserRefactoringPromoManager is enabled.
TEST_F(DefaultBrowserSceneAgentTest,
       TesChromeLikelyDefaultBrowserNoPromoRegistration) {
  EnableDefaultBrowserPromoRefactoringFlag();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  LogOpenHTTPURLFromExternalURL();
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that DefaultBrowser was not registered to the promo manager due to the
// last shutbown not being clean when kDefaultBrowserRefactoringPromoManager is
// enabled.
TEST_F(DefaultBrowserSceneAgentTest,
       TestLastShutdownNotCleanNoPromoRegistration) {
  EnableDefaultBrowserPromoRefactoringFlag();
  SignIn();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that DefaultBrowser was not registered to the promo manager because the
// user previously interacted with a default browser tailored fullscreen promo
// when kDefaultBrowserRefactoringPromoManager is enabled.
TEST_F(DefaultBrowserSceneAgentTest,
       TestInteractedTailoredPromoNoPromoRegistration) {
  EnableDefaultBrowserPromoRefactoringFlag();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  SignIn();
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that DefaultBrowser was not registered to the promo manager because the
// user previously interacted with a default browser fullscreen promo when
// kDefaultBrowserRefactoringPromoManager is enabled.
TEST_F(DefaultBrowserSceneAgentTest,
       TestInteractedDefaultPromoNoPromoRegistration) {
  EnableDefaultBrowserPromoRefactoringFlag();
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  SignIn();
  LogUserInteractionWithFullscreenPromo();
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the DefaultPromoTypeMadeForIOS tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserSceneAgentTest, TestDefaultPromoTypeMadeForIOSShown) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher_ startDispatchingToTarget:mockDefaultPromoCommandsHandler
                            forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  app_state_.shouldShowDefaultBrowserPromo = true;
  app_state_.defaultBrowserPromoTypeToShow = DefaultPromoTypeMadeForIOS;

  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoMadeForIOS]);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeStaySafe tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserSceneAgentTest, TestDefaultPromoTypeStaySafeShown) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher_ startDispatchingToTarget:mockDefaultPromoCommandsHandler
                            forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  app_state_.shouldShowDefaultBrowserPromo = true;
  app_state_.defaultBrowserPromoTypeToShow = DefaultPromoTypeStaySafe;

  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoStaySafe]);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeAllTabs tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserSceneAgentTest, TestDefaultPromoTypeAllTabsShown) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher_ startDispatchingToTarget:mockDefaultPromoCommandsHandler
                            forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  app_state_.shouldShowDefaultBrowserPromo = true;
  app_state_.defaultBrowserPromoTypeToShow = DefaultPromoTypeAllTabs;

  OCMExpect([mockDefaultPromoCommandsHandler showTailoredPromoAllTabs]);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

// Tests that the DefaultPromoTypeGeneral tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserSceneAgentTest, TestDefaultPromoTypeGeneralShown) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::DefaultBrowser))
      .Times(0);

  id mockDefaultPromoCommandsHandler =
      OCMProtocolMock(@protocol(DefaultPromoCommands));
  [dispatcher_ startDispatchingToTarget:mockDefaultPromoCommandsHandler
                            forProtocol:@protocol(DefaultPromoCommands)];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  app_state_.shouldShowDefaultBrowserPromo = true;
  app_state_.defaultBrowserPromoTypeToShow = DefaultPromoTypeGeneral;

  OCMExpect(
      [mockDefaultPromoCommandsHandler showDefaultBrowserFullscreenPromo]);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mockDefaultPromoCommandsHandler);
}

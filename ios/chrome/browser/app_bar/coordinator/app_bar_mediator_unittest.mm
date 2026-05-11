// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_impl.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_lock_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@protocol TestAppBarConsumer <AppBarConsumer,
                              FullscreenUIElement,
                              FullscreenBrowserAgentObserving>
@end

namespace {

MenuScenarioHistogram kTestMenuScenario = kMenuScenarioHistogramToolbarMenu;

}  // namespace

@interface AppBarMediator (Test)
- (void)updateConsumer;
- (void)updateAssistantButton;
- (void)addNewTabInCurrentTabGroup;
@end

class AppBarMediatorTest : public PlatformTest {
 protected:
  AppBarMediatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {kChromeNextIa, kFullscreenRefactoring}, {});
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(GeminiServiceFactory::GetInstance(),
                              GeminiServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    // Initialize VariationsService with a default country to prevent crashes
    // in IsGeminiLocationEligible().
    scoped_variations_service_.Get()->OverrideStoredPermanentCountry("us");

    regular_profile_ = std::move(builder).Build();
    incognito_profile_ = TestProfileIOS::Builder().Build();

    // IdentityTestEnvironment requires the SigninClient, which is created
    // when the profile is built. But it can also be used as a member.
    // We don't need to initialize it with the profile's manager on iOS
    // as they share the same global SystemIdentityManager.

    auth_service_ =
        AuthenticationServiceFactory::GetForProfile(regular_profile_.get());
    gemini_service_ptr_ = std::make_unique<GeminiServiceImpl>(
        regular_profile_.get(), auth_service_,
        IdentityManagerFactory::GetForProfile(regular_profile_.get()),
        regular_profile_->GetTestingPrefService(),
        OptimizationGuideServiceFactory::GetForProfile(regular_profile_.get()));
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(
            regular_profile_.get());

    regular_browser_ = std::make_unique<TestBrowser>(regular_profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(incognito_profile_.get());

    FullscreenBrowserAgent::CreateForBrowser(regular_browser_.get());
    FullscreenBrowserAgent::CreateForBrowser(incognito_browser_.get());

    mock_fullscreen_handler_ = OCMProtocolMock(@protocol(FullscreenCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_fullscreen_handler_
                     forProtocol:@protocol(FullscreenCommands)];
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    mock_browser_coordinator_handler_ =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    mock_qr_scanner_handler_ = OCMProtocolMock(@protocol(QRScannerCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_qr_scanner_handler_
                     forProtocol:@protocol(QRScannerCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_qr_scanner_handler_
                     forProtocol:@protocol(QRScannerCommands)];

    mock_lens_handler_ = OCMProtocolMock(@protocol(LensCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_handler_
                     forProtocol:@protocol(LensCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_handler_
                     forProtocol:@protocol(LensCommands)];

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(regular_browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(regular_browser_.get());

    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(regular_browser_.get()));

    tab_grid_state_ = [[TabGridState alloc] init];
    incognito_state_ = [[IncognitoState alloc] initWithSceneState:nil];
    regular_web_state_list_ = regular_browser_->GetWebStateList();
    incognito_web_state_list_ = incognito_browser_->GetWebStateList();

    TestFullscreenController::CreateForBrowser(regular_browser_.get());
    TestFullscreenController::CreateForBrowser(incognito_browser_.get());

    BrowserActionFactory* regular_action_factory_ =
        [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                             scenario:kTestMenuScenario];
    BrowserActionFactory* incognito_action_factory_ =
        [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                             scenario:kTestMenuScenario];

    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    GeminiBrowserAgent::CreateForBrowser(regular_browser_.get());

    mediator_ = [[AppBarMediator alloc]
            initWithRegularWebStateList:regular_web_state_list_.get()
                  incognitoWebStateList:incognito_web_state_list_.get()
            regularFullscreenController:TestFullscreenController::FromBrowser(
                                            regular_browser_.get())
          incognitoFullscreenController:TestFullscreenController::FromBrowser(
                                            incognito_browser_.get())
          regularFullscreenBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                            regular_browser_.get())
        incognitoFullscreenBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                            incognito_browser_.get())
                   regularActionFactory:regular_action_factory_
                 incognitoActionFactory:incognito_action_factory_
                            prefService:regular_profile_
                                            ->GetTestingPrefService()
                     templateURLService:search_engines_test_environment_
                                            .template_url_service()
                  authenticationService:auth_service_
                          geminiService:gemini_service_ptr_.get()
                     geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                            regular_browser_.get())
                              URLLoader:url_loader_
                           tabGridState:tab_grid_state_
                         incognitoState:incognito_state_];

    consumer_ = OCMProtocolMock(@protocol(TestAppBarConsumer));
    mediator_.consumer = consumer_;
    mediator_.sceneHandler = mock_scene_handler_;
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    mediator_.settingsHandler = mock_settings_handler_;
    mediator_.lensHandler = mock_lens_handler_;
    mock_gemini_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mediator_.geminiHandler = mock_gemini_handler_;
    mock_tab_groups_handler_ = OCMProtocolMock(@protocol(TabGroupsCommands));
    mediator_.regularTabGroupsCommands = mock_tab_groups_handler_;
    mediator_.incognitoTabGroupsCommands = mock_tab_groups_handler_;
  }

  ~AppBarMediatorTest() override {
    [mediator_ disconnect];
    mediator_ = nil;
  }

  void SignInAndSetCapability(bool capability) {
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(regular_profile_.get());

    signin::AccountAvailabilityOptionsBuilder builder;
    builder.WithGaiaId(identity.gaiaId)
        .AsPrimary(signin::ConsentLevel::kSignin);

    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager,
        builder.Build(base::SysNSStringToUTF8(identity.userEmail)));

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(capability);
    mutator.set_can_use_gemini_in_chrome(capability);

    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  // Sets the location eligibility.
  void SetLocationEligible(bool eligible) {
    if (eligible) {
      scoped_variations_service_.Get()->OverrideStoredPermanentCountry("us");
      TestingApplicationContext::GetGlobal()
          ->GetApplicationLocaleStorage()
          ->Set("en-US");
    } else {
      scoped_variations_service_.Get()->OverrideStoredPermanentCountry("fr");
      TestingApplicationContext::GetGlobal()
          ->GetApplicationLocaleStorage()
          ->Set("fr-FR");
    }
  }

  // Wrapper for `InvokeFloaty`.
  void InvokeFloaty(GeminiBrowserAgent* agent, GeminiConfiguration* config) {
    agent->InvokeFloaty(config);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  IOSChromeScopedTestingVariationsService scoped_variations_service_;
  std::unique_ptr<TestProfileIOS> regular_profile_;
  std::unique_ptr<TestProfileIOS> incognito_profile_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  AppBarMediator* __strong mediator_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  raw_ptr<WebStateList> regular_web_state_list_;
  raw_ptr<WebStateList> incognito_web_state_list_;
  TabGridState* tab_grid_state_;
  IncognitoState* incognito_state_;
  raw_ptr<AuthenticationService> auth_service_;
  std::unique_ptr<GeminiService> gemini_service_ptr_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  id<TestAppBarConsumer> consumer_;
  id mock_fullscreen_handler_;
  id mock_scene_handler_;
  id mock_browser_coordinator_handler_;
  id mock_lens_handler_;
  id mock_qr_scanner_handler_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
  id mock_tab_groups_handler_;
};

// Tests that the consumer is updated when a web state is added.
TEST_F(AppBarMediatorTest, TestDidAddWebState) {
  OCMExpect([consumer_ updateTabCount:1]);
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when a web state is detached.
TEST_F(AppBarMediatorTest, TestDidDetachWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  OCMExpect([consumer_ updateTabCount:0]);
  regular_web_state_list_->DetachWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching to incognito.
TEST_F(AppBarMediatorTest, TestSwitchToIncognitoNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;

  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular.
TEST_F(AppBarMediatorTest, TestSwitchToRegularNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ updateTabCount:0]);
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  incognito_state_.incognitoContentVisible = NO;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching to incognito.
TEST_F(AppBarMediatorTest, TestSwitchToIncognitoTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular.
TEST_F(AppBarMediatorTest, TestSwitchToRegularTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  OCMExpect([consumer_ updateTabCount:0]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  OCMExpect([consumer_ updateTabCount:1]);
  tab_grid_state_.currentPage = TabGridPageRegularTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests creating a new tab from outside of the tab grid.
TEST_F(AppBarMediatorTest, TestCreateNewTabNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;

  // Try to open a new tab.
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);
  [mediator_ createNewTabFromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}

// Tests creating a new tab from inside of the tab grid.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  EXPECT_FALSE(url_loader_->last_params.in_incognito);
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}

// Tests creating a new tab from inside of the tab grid incognito.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGridIncognito) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  EXPECT_TRUE(url_loader_->last_params.in_incognito);
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}

// Tests creating a new tab in a group from inside of the tab grid.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGridInGroup) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Create a group.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  const TabGroup* group = regular_web_state_list_->CreateGroup(
      {0},
      tab_groups::TabGroupVisualData(u"Group",
                                     tab_groups::TabGroupColorId::kGrey),
      tab_groups::TabGroupId::GenerateNew());

  tab_grid_state_.visibleTabGroup = group;

  // Expect tab grid to prepare to exit.
  id mock_tab_grid_handler = OCMProtocolMock(@protocol(TabGridCommands));
  mediator_.tabGridHandler = mock_tab_grid_handler;
  OCMExpect([mock_tab_grid_handler prepareToExitTabGrid]);
  OCMExpect([mock_tab_groups_handler_ hideTabGroup]);
  // We don't expect exitTabGrid because FakeUrlLoadingBrowserAgent doesn't
  // mutate the web state list, so addNewTabIncognito returns false.

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  EXPECT_FALSE(url_loader_->last_params.in_incognito);
  EXPECT_FALSE(url_loader_->last_params.load_in_group);
  EXPECT_EQ(nullptr, url_loader_->last_params.tab_group.get());
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);

  EXPECT_OCMOCK_VERIFY(mock_tab_grid_handler);
  EXPECT_OCMOCK_VERIFY(mock_tab_groups_handler_);
}

// Tests that adding a new tab in the current group from the tab grid
// correctly updates the URL loader with the group info and prepares to exit the
// grid.
TEST_F(AppBarMediatorTest, TestAddNewTabInCurrentTabGroup) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Create a group.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  const TabGroup* group = regular_web_state_list_->CreateGroup(
      {0},
      tab_groups::TabGroupVisualData(u"Group",
                                     tab_groups::TabGroupColorId::kGrey),
      tab_groups::TabGroupId::GenerateNew());

  tab_grid_state_.visibleTabGroup = group;
  [mediator_ updateConsumer];

  // Expect tab grid to prepare to exit.
  id mock_tab_grid_handler = OCMProtocolMock(@protocol(TabGridCommands));
  mediator_.tabGridHandler = mock_tab_grid_handler;
  OCMExpect([mock_tab_grid_handler prepareToExitTabGrid]);

  // Try to open a new tab in group.
  [mediator_ addNewTabInCurrentTabGroup];

  EXPECT_FALSE(url_loader_->last_params.in_incognito);
  EXPECT_TRUE(url_loader_->last_params.load_in_group);
  EXPECT_EQ(group, url_loader_->last_params.tab_group.get());
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);

  EXPECT_OCMOCK_VERIFY(mock_tab_grid_handler);
}

// Tests creating a new tab in a group from inside of the tab grid when disabled
// by policy.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGridInGroupDisabledByPolicy) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Create a group.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  const TabGroup* group = regular_web_state_list_->CreateGroup(
      {0},
      tab_groups::TabGroupVisualData(u"Group",
                                     tab_groups::TabGroupColorId::kGrey),
      tab_groups::TabGroupId::GenerateNew());

  tab_grid_state_.visibleTabGroup = group;

  // Disable adding regular tabs by policy (forcing incognito).
  regular_profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));

  // We don't expect prepareToExitTabGrid or exitTabGrid to be called.
  id mock_tab_grid_handler = OCMProtocolMock(@protocol(TabGridCommands));
  mediator_.tabGridHandler = mock_tab_grid_handler;

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  // Verify that Load was NOT called.
  EXPECT_EQ(0, url_loader_->load_new_tab_call_count);
}

// Tests that buttons are enabled/disabled based on policy.
TEST_F(AppBarMediatorTest, TestSetButtonsEnabledByPolicy) {
  tab_grid_state_.currentPage = TabGridPageRegularTabs;
  tab_grid_state_.tabGridVisible = YES;

  // Disable incognito by policy.
  regular_profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));

  // Switch to incognito page: buttons should be disabled.
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch to regular page: buttons should be enabled.
  OCMExpect([consumer_ setButtonsEnabled:NO]);
  tab_grid_state_.currentPage = TabGridPageRegularTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when opening the app on a regular tabs
// while having the TabGrid in incognito but not visible.
TEST_F(AppBarMediatorTest, TestLaunchInRegularTabNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  incognito_state_.incognitoContentVisible = NO;
  incognito_state_.lockState = IncognitoLockState::kReauth;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  id consumer = OCMProtocolMock(@protocol(AppBarConsumer));
  OCMExpect([consumer setButtonsEnabled:YES]);
  mediator_.consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the consumer is updated when switching to incognito while having
// the incognito lock.
TEST_F(AppBarMediatorTest, TestSwitchToIncognitoNonTabGridWithAuthentication) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;
  incognito_state_.lockState = IncognitoLockState::kReauth;

  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setButtonsEnabled:NO]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular while
// having the incognito lock.
TEST_F(AppBarMediatorTest, TestSwitchToRegularNonTabGridWithAuthentication) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;
  incognito_state_.lockState = IncognitoLockState::kReauth;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ updateTabCount:0]);
  OCMExpect([consumer_ setButtonsEnabled:NO]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setButtonsEnabled:YES]);
  incognito_state_.incognitoContentVisible = NO;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that buttons are disabled when incognito authentication is required.
TEST_F(AppBarMediatorTest, TestSetButtonsDisabledOnAuthenticationRequired) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  OCMExpect([consumer_ setButtonsEnabled:NO]);
  incognito_state_.lockState = IncognitoLockState::kReauth;
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setButtonsEnabled:YES]);
  tab_grid_state_.currentPage = TabGridPageRegularTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when the active web state is in a group.
TEST_F(AppBarMediatorTest, TestInTabGroup) {
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  // Not in a group initially.
  OCMExpect([consumer_ setInTabGroup:NO]);
  [mediator_ updateConsumer];
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Create a group and add the web state to it.
  OCMExpect([consumer_ setInTabGroup:YES]);
  regular_web_state_list_->CreateGroup(
      {0},
      tab_groups::TabGroupVisualData(u"Group",
                                     tab_groups::TabGroupColorId::kGrey),
      tab_groups::TabGroupId::GenerateNew());
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Remove from group.
  OCMExpect([consumer_ setInTabGroup:NO]);
  regular_web_state_list_->RemoveFromGroups({0});
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated with the incognito state.
TEST_F(AppBarMediatorTest, TestIncognitoState) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;

  // Initial state should be non-incognito.
  OCMExpect([consumer_ setIncognito:NO]);
  [mediator_ updateConsumer];
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch to incognito.
  OCMExpect([consumer_ setIncognito:YES]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ setIncognito:NO]);
  incognito_state_.incognitoContentVisible = NO;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated with the incognito state in the tab grid.
TEST_F(AppBarMediatorTest, TestIncognitoStateTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Initial state in regular tab grid should be non-incognito.
  OCMExpect([consumer_ setIncognito:NO]);
  [mediator_ updateConsumer];
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch to incognito page in tab grid.
  OCMExpect([consumer_ setIncognito:YES]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer receives fullscreen events.
TEST_F(AppBarMediatorTest, TestFullscreenEvent) {
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(regular_browser_.get());

  // Expect the consumer to be notified.
  OCMExpect([consumer_ fullscreenWillUpdateObscuredInsetRange:agent]);

  // Simulate the event.
  agent->InvalidateInsetRange();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is correctly updated when the Gemini
// floaty invocation state changes and Gemini is available.
TEST_F(AppBarMediatorTest, TestAssistantButtonHighlighted_GeminiAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu, kGeminiCopresence},
                                       {});

  GeminiBrowserAgent* agent =
      GeminiBrowserAgent::FromBrowser(regular_browser_.get());

  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  web_state->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("https://example.com"));

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  // Expect highlighted to be YES when floaty is invoked.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:YES
                                       enabled:YES]);

  InvokeFloaty(agent, [[GeminiConfiguration alloc] init]);

  EXPECT_OCMOCK_VERIFY(consumer_);

  // Expect highlighted to be NO when floaty is dismissed.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES]);

  agent->DismissFloaty();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is correctly updated when the Gemini
// floaty invocation state changes and Gemini is NOT available.
TEST_F(AppBarMediatorTest, TestAssistantButtonHighlighted_GeminiNotAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu, kGeminiCopresence},
                                       {});

  GeminiBrowserAgent* agent =
      GeminiBrowserAgent::FromBrowser(regular_browser_.get());

  // Add active WebState with GeminiTabHelper, but an ineligible URL.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("chrome://settings"));

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  // Expect highlighted to remain NO and enabled to remain NO when floaty is
  // invoked.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:NO]);

  InvokeFloaty(agent, [[GeminiConfiguration alloc] init]);

  EXPECT_OCMOCK_VERIFY(consumer_);

  // Expect highlighted to remain NO and enabled to remain NO when floaty is
  // dismissed.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:NO]);

  agent->DismissFloaty();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the signed out state when not signed
// in and not location eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateLensFallback) {
  SetLocationEligible(false);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the ask state when location is
// eligible, even if not signed in.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAskLocationEligible) {
  SetLocationEligible(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button remains in the Lens state when signed in but
// not location eligible (fallback state).
TEST_F(AppBarMediatorTest, TestAssistantButtonStateLensFallbackSignedIn) {
  SetLocationEligible(false);
  SignInAndSetCapability(false);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the ask state when signed in and
// is sufficiently eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAsk) {
  SetLocationEligible(true);
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is enabled when Gemini is available.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAsk_GeminiAvailable) {
  SetLocationEligible(true);
  SignInAndSetCapability(true);

  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  web_state->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("https://google.com"));

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is disabled when Gemini is not available.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAsk_GeminiNotAvailable) {
  SetLocationEligible(true);
  SignInAndSetCapability(true);

  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("chrome://settings"));

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping the assistant button in the Lens state dispatches
// the Lens command.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedLens) {
  OCMExpect([mock_lens_handler_
      openLensInputSelection:[OCMArg
                                 checkWithBlock:^BOOL(
                                     OpenLensInputSelectionCommand* command) {
                                   return command.entryPoint ==
                                          LensEntrypoint::AppBar;
                                 }]]);
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kLens];
  EXPECT_OCMOCK_VERIFY(mock_lens_handler_);
}

// Tests that tapping the assistant button in the ask state dispatches the
// Gemini command.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedEligible) {
  SignInAndSetCapability(true);
  [mediator_ updateAssistantButton];

  OCMExpect([mock_gemini_handler_
      startGeminiFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                  GeminiStartupState* state) {
        return state.entryPoint == gemini::EntryPoint::AppBar;
      }]]);
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAsk];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Tests that the assistant button is in the kAIM state when the correct
// features are enabled.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAIM) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kAssistantContainer, kAimCobrowse, kGeminiKillSwitch},
      {kPageActionMenu});

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping the assistant button in the kAIM state dispatches the
// assistant command.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedAIM) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kAssistantContainer, kGeminiKillSwitch},
                                {kPageActionMenu});
  [mediator_ updateAssistantButton];

  OCMExpect([mock_scene_handler_ showAssistant]);
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAIM];
  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}

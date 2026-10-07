// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>
#import <string_view>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_lock_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/lens_overlay_state_notifier.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
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
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/mock_network_change_notifier.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

@protocol TestAppBarConsumer <AppBarConsumer,
                              FullscreenUIElement,
                              FullscreenBrowserAgentObserving>
@end

@interface AppBarMediator (Testing)
@property(nonatomic, assign) BOOL overrideLensAvailabilityForTesting;
@end

namespace {

MenuScenarioHistogram kTestMenuScenario = kMenuScenarioHistogramToolbarMenu;

// Indexes of the entries of the assistant button menu when all of them are
// available, as the unavailable entries are hidden.
constexpr NSUInteger kAssistantButtonMenuAskGeminiIndex = 0;
constexpr NSUInteger kAssistantButtonMenuLensIndex = 1;
constexpr NSUInteger kAssistantButtonMenuAccountIndex = 2;
// Number of entries of the assistant button menu when all of them are
// available.
constexpr NSUInteger kAssistantButtonMenuEntryCount = 3;

// Value of the preferred assistant button state outside of the enum range.
constexpr int kInvalidAssistantButtonState =
    static_cast<int>(AppBarAssistantButtonPreferredState::kMaxValue) + 1;

// User actions recorded when selecting the entries of the assistant button
// menu.
constexpr std::string_view kAssistantButtonMenuAskGeminiUserAction =
    "MobileToolbarAssistantCustomizationAskGeminiSelected";
constexpr std::string_view kAssistantButtonMenuLensUserAction =
    "MobileToolbarAssistantCustomizationLensSelected";
constexpr std::string_view kAssistantButtonMenuAccountUserAction =
    "MobileToolbarAssistantCustomizationAccountSelected";

// Expected entry of the assistant button menu.
struct ExpectedAssistantButtonMenuEntry {
  // ID of the title string of the entry.
  int title_id;
  // Whether the entry is checked.
  bool checked;
};

// Expects `menu` to contain exactly `expected_entries`, in this order, all of
// them being enabled.
void ExpectAssistantButtonMenuEntries(
    UIMenu* menu,
    const std::vector<ExpectedAssistantButtonMenuEntry>& expected_entries) {
  ASSERT_TRUE(menu);
  ASSERT_EQ(expected_entries.size(), menu.children.count);
  for (size_t index = 0; index < expected_entries.size(); ++index) {
    const ExpectedAssistantButtonMenuEntry& expected = expected_entries[index];
    NSString* title = l10n_util::GetNSString(expected.title_id);
    SCOPED_TRACE(base::SysNSStringToUTF8(title));
    UIAction* action =
        base::apple::ObjCCastStrict<UIAction>(menu.children[index]);
    EXPECT_NSEQ(title, action.title);
    EXPECT_EQ(expected.checked ? UIMenuElementStateOn : UIMenuElementStateOff,
              action.state);
    EXPECT_FALSE(action.attributes & UIMenuElementAttributesDisabled);
  }
}

// Expects `tester` to have recorded the user actions of the "Ask Gemini",
// "Lens" and "Account" entries of the assistant button menu respectively
// `ask_gemini_count`, `lens_count` and `account_count` times.
void ExpectAssistantButtonMenuUserActionCounts(
    const base::UserActionTester& tester,
    int ask_gemini_count,
    int lens_count,
    int account_count) {
  EXPECT_EQ(ask_gemini_count,
            tester.GetActionCount(kAssistantButtonMenuAskGeminiUserAction));
  EXPECT_EQ(lens_count,
            tester.GetActionCount(kAssistantButtonMenuLensUserAction));
  EXPECT_EQ(account_count,
            tester.GetActionCount(kAssistantButtonMenuAccountUserAction));
}

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
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    // Initialize VariationsService with a default country to prevent crashes
    // in IsGeminiLocationEligible().
    SetLocationEligible(true);

    regular_profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(builder));
    incognito_profile_ = regular_profile_->GetOffTheRecordProfile();

    // IdentityTestEnvironment requires the SigninClient, which is created
    // when the profile is built. But it can also be used as a member.
    // We don't need to initialize it with the profile's manager on iOS
    // as they share the same global SystemIdentityManager.

    auth_service_ =
        AuthenticationServiceFactory::GetForProfile(regular_profile_);
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(regular_profile_));
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(regular_profile_);

    regular_browser_ = std::make_unique<TestBrowser>(regular_profile_);
    incognito_browser_ = std::make_unique<TestBrowser>(incognito_profile_);

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

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(regular_browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(regular_browser_.get());

    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(regular_browser_.get()));

    tab_grid_state_ = [[TabGridState alloc] init];
    incognito_state_ = [[IncognitoState alloc] initWithSceneState:nil];
    lens_overlay_state_ = [[LensOverlayStateNotifier alloc] init];
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

    aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
        *regular_profile_->GetTestingPrefService(),
        search_engines_test_environment_.template_url_service(),
        regular_profile_->GetSharedURLLoaderFactory(),
        IdentityManagerFactory::GetForProfile(regular_profile_.get()));
    ON_CALL(*aim_eligibility_service_, IsAimEligible())
        .WillByDefault(testing::Return(false));
    ON_CALL(*aim_eligibility_service_, IsServerEligibilityEnabled())
        .WillByDefault(testing::Return(false));

    // Lens is eligible without any tab, so disable it by default to let tests
    // control its availability with `overrideLensAvailabilityForTesting`.
    regular_profile_->GetTestingPrefService()->SetInteger(
        lens::prefs::kLensOverlaySettings,
        static_cast<int>(
            lens::prefs::LensOverlaySettingsPolicyValue::kDisabled));

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
                                profile:regular_profile_.get()
                            prefService:regular_profile_
                                            ->GetTestingPrefService()
                     templateURLService:search_engines_test_environment_
                                            .template_url_service()
                  authenticationService:auth_service_
                        identityManager:IdentityManagerFactory::GetForProfile(
                                            regular_profile_.get())
                          geminiService:fake_gemini_service_
                     geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                            regular_browser_.get())
                  aimEligibilityService:aim_eligibility_service_.get()
                              URLLoader:url_loader_
                           tabGridState:tab_grid_state_
                         incognitoState:incognito_state_
               lensOverlayStateNotifier:lens_overlay_state_];

    consumer_ = OCMProtocolMock(@protocol(TestAppBarConsumer));
    mediator_.consumer = consumer_;
    mediator_.sceneHandler = mock_scene_handler_;
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    mediator_.settingsHandler = mock_settings_handler_;
    mock_lens_overlay_handler_ =
        OCMProtocolMock(@protocol(LensOverlayCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_overlay_handler_
                     forProtocol:@protocol(LensOverlayCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_lens_overlay_handler_
                     forProtocol:@protocol(LensOverlayCommands)];
    mediator_.lensOverlayHandler = mock_lens_overlay_handler_;
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    mediator_.geminiHandler = mock_gemini_handler_;
    mock_tab_groups_handler_ = OCMProtocolMock(@protocol(TabGroupsCommands));
    mediator_.regularTabGroupsCommands = mock_tab_groups_handler_;
    mediator_.incognitoTabGroupsCommands = mock_tab_groups_handler_;
    mock_delegate_ = OCMProtocolMock(@protocol(AppBarMediatorDelegate));
    mediator_.delegate = mock_delegate_;
  }

  ~AppBarMediatorTest() override {
    [mediator_ disconnect];
    mediator_ = nil;
    aim_eligibility_service_.reset();
    fake_gemini_service_ = nullptr;
    regular_web_state_list_ = nullptr;
    incognito_web_state_list_ = nullptr;
    url_loader_ = nullptr;
    regular_browser_.reset();
    incognito_browser_.reset();
    regular_profile_ = nullptr;
    incognito_profile_ = nullptr;
    auth_service_ = nullptr;
    account_manager_service_ = nullptr;
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

    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(capability);
    mutator.set_can_use_gemini_in_chrome(capability);

    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void SignInWithTriboolCapability(signin::Tribool capability) {
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

    AccountCapabilitiesTestMutator mutator(&account_info);
    if (capability == signin::Tribool::kTrue) {
      mutator.set_can_use_model_execution_features(true);
      mutator.set_can_use_gemini_in_chrome(true);
    } else if (capability == signin::Tribool::kFalse) {
      mutator.set_can_use_model_execution_features(false);
      mutator.set_can_use_gemini_in_chrome(false);
    }

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

  // Sets whether AI Mode is eligible.
  void SetAimEligible(bool eligible) {
    ON_CALL(*aim_eligibility_service_, IsAimEligible())
        .WillByDefault(testing::Return(eligible));
  }

  // Stores `state` as the assistant button state chosen by the user.
  void SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState state) {
    regular_profile_->GetTestingPrefService()->SetInteger(
        prefs::kAppBarAssistantButtonPreferredState, static_cast<int>(state));
  }

  // Updates the assistant button, verifies the expectations of the consumer and
  // returns the menu set on the consumer for the assistant button.
  UIMenu* UpdateAssistantButtonAndGetMenu() {
    __block UIMenu* assistant_button_menu = nil;
    OCMExpect([consumer_ setMenu:[OCMArg checkWithBlock:^BOOL(UIMenu* menu) {
                           assistant_button_menu = menu;
                           return YES;
                         }]
                   forButtonType:AppBarButtonTypeAssistant]);
    [mediator_ updateAssistantButton];
    EXPECT_OCMOCK_VERIFY(consumer_);
    return assistant_button_menu;
  }

  // Selects the entry at `index` of the assistant button menu.
  void SelectAssistantButtonMenuEntry(NSUInteger index) {
    UIMenu* menu = UpdateAssistantButtonAndGetMenu();
    ASSERT_EQ(kAssistantButtonMenuEntryCount, menu.children.count);
    UIAction* action =
        base::apple::ObjCCastStrict<UIAction>(menu.children[index]);
    [action performWithSender:nil target:nil];
  }

  // Replaces `mediator_` with a new mediator, which records the on-load
  // metrics when its consumer is set.
  void RecreateMediator() {
    [mediator_ disconnect];
    mediator_ = CreateMediatorWithCustomGeminiService(fake_gemini_service_);
  }

  AppBarMediator* CreateMediatorWithCustomGeminiService(
      GeminiService* gemini_service) {
    BrowserActionFactory* regular_action_factory =
        [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                             scenario:kTestMenuScenario];
    BrowserActionFactory* incognito_action_factory =
        [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                             scenario:kTestMenuScenario];
    AppBarMediator* mediator = [[AppBarMediator alloc]
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
                   regularActionFactory:regular_action_factory
                 incognitoActionFactory:incognito_action_factory
                                profile:regular_profile_.get()
                            prefService:regular_profile_
                                            ->GetTestingPrefService()
                     templateURLService:search_engines_test_environment_
                                            .template_url_service()
                  authenticationService:auth_service_
                        identityManager:IdentityManagerFactory::GetForProfile(
                                            regular_profile_.get())
                          geminiService:gemini_service
                     geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                            regular_browser_.get())
                  aimEligibilityService:aim_eligibility_service_.get()
                              URLLoader:url_loader_
                           tabGridState:tab_grid_state_
                         incognitoState:incognito_state_
               lensOverlayStateNotifier:lens_overlay_state_];
    mediator.consumer = consumer_;
    mediator.sceneHandler = mock_scene_handler_;
    mediator.settingsHandler = mock_settings_handler_;
    mediator.lensOverlayHandler = mock_lens_overlay_handler_;
    mediator.geminiHandler = mock_gemini_handler_;
    mediator.regularTabGroupsCommands = mock_tab_groups_handler_;
    mediator.incognitoTabGroupsCommands = mock_tab_groups_handler_;
    mediator.delegate = mock_delegate_;
    return mediator;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  IOSChromeScopedTestingVariationsService scoped_variations_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<MockAimEligibilityService> aim_eligibility_service_;
  raw_ptr<TestProfileIOS> regular_profile_;
  raw_ptr<ProfileIOS> incognito_profile_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  AppBarMediator* __strong mediator_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  raw_ptr<WebStateList> regular_web_state_list_;
  raw_ptr<WebStateList> incognito_web_state_list_;
  TabGridState* tab_grid_state_;
  IncognitoState* incognito_state_;
  LensOverlayStateNotifier* lens_overlay_state_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<FakeGeminiService> fake_gemini_service_ = nullptr;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  id<TestAppBarConsumer> consumer_;
  id mock_fullscreen_handler_;
  id mock_scene_handler_;
  id mock_browser_coordinator_handler_;
  id mock_qr_scanner_handler_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
  id mock_tab_groups_handler_;
  id mock_lens_overlay_handler_;
  id mock_delegate_;
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

// Tests that the consumer tab count is updated when entering the regular tab
// grid from incognito.
TEST_F(AppBarMediatorTest, TestEnterTabGridCrossModeFromIncognitoToRegular) {
  tab_grid_state_.tabGridVisible = NO;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  incognito_state_.incognitoContentVisible = YES;

  // 1 tab in incognito, 2 tabs in regular.
  auto incognito_web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(incognito_web_state));

  auto regular_web_state1 = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(regular_web_state1));
  auto regular_web_state2 = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(regular_web_state2));

  // The active page transitions to regular before tabGridVisible becomes YES.
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Entering tab grid should update the consumer with regular tab count (2).
  OCMExpect([consumer_ updateTabCount:2]);
  tab_grid_state_.tabGridVisible = YES;
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
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  GeminiBrowserAgent* agent =
      GeminiBrowserAgent::FromBrowser(regular_browser_.get());

  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  web_state->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("https://example.com"));
  web_state->WasShown();

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  // Expect highlighted to be YES when floaty is invoked.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:YES
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  InvokeFloaty(agent, [[GeminiConfiguration alloc] init]);

  EXPECT_OCMOCK_VERIFY(consumer_);

  // Expect highlighted to be NO when floaty is dismissed.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  agent->DismissFloaty();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the ask state when location is
// eligible, even if not signed in.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAskLocationEligible) {
  SetLocationEligible(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
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
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
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
  web_state->WasShown();

  regular_web_state_list_->InsertWebState(std::move(web_state));
  regular_web_state_list_->ActivateWebStateAt(0);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when account capability is unknown and device is offline,
// the button optimistically falls back to kAsk.
TEST_F(AppBarMediatorTest, TestOptimisticFallback_WhenCapabilityIsUnknown) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kUnknown);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when account capability is explicitly false (e.g. child account),
// the button is optimistically shown and enabled as kAsk.
TEST_F(AppBarMediatorTest, TestNoFallback_WhenCapabilityExplicitlyFalse) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kFalse);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when online and workspace check explicitly returns disabled,
// optimistic fallback is blocked.
TEST_F(AppBarMediatorTest, TestNoFallback_WhenWorkspaceExplicitlyDisabled) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:[OCMArg any]
                     signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when offline, a failed/false workspace check is bypassed by
// optimistic fallback and shows kAsk.
TEST_F(AppBarMediatorTest,
       TestOptimisticFallback_WhenOfflineAndWorkspaceDisabledDueToNetwork) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that local enterprise policy disablement overrides optimistic fallback.
TEST_F(AppBarMediatorTest, TestNoFallback_WhenLocalEnterprisePolicyDisabled) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kUnknown);

  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:[OCMArg any]
                     signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that in incognito mode, the assistant button stays in kAsk state.
TEST_F(AppBarMediatorTest, TestGeminiButtonDisabled_WhenInIncognito) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kUnknown);
  incognito_state_.incognitoContentVisible = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the ask state and enabled when signed
// in with an unverified primary identity.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAsk_UnverifiedIdentity) {
  SetLocationEligible(true);
  SignInAndSetCapability(false);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(regular_profile_.get());
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when account capability is pending (kUnknown) online, the button
// defaults to kAsk.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateAsk_AccountCapabilitiesPending) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kUnknown);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when workspace policy check is pending, the button defaults to
// kAsk.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAsk_WorkspacePolicyPending) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  fake_gemini_service_->SetWorkspacePolicyCheckPending(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when workspace policy check completes and returns disabled,
// the button transitions from kAsk to fallback (kAccount).
TEST_F(AppBarMediatorTest,
       TestAssistantButtonState_TransitionsToFallbackWhenWorkspaceDisabled) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  fake_gemini_service_->SetWorkspacePolicyCheckPending(true);

  // Initial update configures button for pending state (kAsk).
  [mediator_ updateAssistantButton];

  // When policy resolves as disabled, expect transition to kAccount.
  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:[OCMArg any]
                     signedIn:YES]);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);
  fake_gemini_service_->SetWorkspacePolicyCheckPending(false);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is updated when the network connection
// changes.
TEST_F(AppBarMediatorTest, TestAssistantButtonUpdatedOnNetworkChange) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network =
      net::test::MockNetworkChangeNotifier::Create();
  mock_network->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);
  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  // Re-instantiate the mediator so that it registers with the mock network.
  [mediator_ disconnect];
  mediator_ = CreateMediatorWithCustomGeminiService(fake_gemini_service_);
  mediator_.consumer = consumer_;

  // Initial update configures button for offline fallback (kAsk).
  [mediator_ updateAssistantButton];

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  // Expect the consumer to be notified when the network reconnects.
  // Because Workspace policy is restricted, it transitions from optimistic
  // offline fallback (kAsk) to kAccount when online.
  OCMExpect([consumer_
                setAssistantButtonState:AppBarAssistantButtonState::kAccount
                            highlighted:NO
                                enabled:YES
                                 avatar:[OCMArg any]
                               signedIn:YES])
      .andDo(^(NSInvocation* invocation) {
        quit_closure.Run();
      });

  mock_network->SetConnectionTypeAndNotifyObservers(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(AppBarMediatorTest, TestAssistantButtonTappedEligible) {
  SignInAndSetCapability(true);
  [mediator_ updateAssistantButton];

  OCMExpect([mock_gemini_handler_
      startGeminiEntryFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                       GeminiStartupState*
                                                           state) {
        return state.entryPoint == gemini::EntryPoint::AppBar;
      }]
                        baseViewController:[OCMArg any]
                  showSnackbarOnCompletion:YES
                                completion:[OCMArg any]]);
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAsk
                                   fromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAsk, 1);
}

// Tests that tapping the assistant button in an incognito tab notifies the
// delegate.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedInIncognitoTab) {
  SignInAndSetCapability(true);
  [mediator_ updateAssistantButton];

  incognito_state_.incognitoContentVisible = YES;

  OCMExpect([mock_delegate_ appBarMediatorDidTapAssistantInIncognito]);

  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAsk
                                   fromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAsk, 1);
}

// Tests that tapping the assistant button in incognito tab grid notifies the
// delegate.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedInIncognitoTabGrid) {
  SignInAndSetCapability(true);
  [mediator_ updateAssistantButton];

  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;

  OCMExpect([mock_delegate_ appBarMediatorDidTapAssistantInIncognito]);

  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAsk
                                   fromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAsk, 1);
}

// Tests that the assistant button is enabled in incognito tab when state is
// kAsk.
TEST_F(AppBarMediatorTest, TestAssistantButtonEnabledInIncognitoForAskState) {
  SignInAndSetCapability(true);
  incognito_state_.incognitoContentVisible = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is disabled in incognito tab when state is
// not kAsk.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonDisabledInIncognitoForAccountState) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  incognito_state_.incognitoContentVisible = YES;

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:NO
                       avatar:[OCMArg any]
                     signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is disabled in incognito tab grid when state
// is not kAsk.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonDisabledInIncognitoTabGridForAccountState) {
  SetLocationEligible(true);
  SignInWithTriboolCapability(signin::Tribool::kTrue);

  gemini::IneligibilityReasons reasons;
  reasons.workspace = true;
  fake_gemini_service_->SetIneligibilityReasons(reasons);

  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:NO
                       avatar:[OCMArg any]
                     signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the kAIM state when the correct
// features are enabled.
// Tests that the assistant button state is kLens when the AIM is disabled
// by enterprise policy and Lens is available.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAIM_DisabledByPolicyLens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiKillSwitch}, {kPageActionMenu});

  regular_profile_->GetTestingPrefService()->SetInteger(
      omnibox::kAIModeSettings, 1);
  mediator_.overrideLensAvailabilityForTesting = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is kAccount when the AIM is
// disabled by enterprise policy and Lens is NOT available.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateAIM_DisabledByPolicyAccount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiKillSwitch}, {kPageActionMenu});

  regular_profile_->GetTestingPrefService()->SetInteger(
      omnibox::kAIModeSettings, 1);
  mediator_.overrideLensAvailabilityForTesting = NO;
  SetLocationEligible(false);

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(AppBarMediatorTest, TestAssistantButtonStateAIM) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiKillSwitch}, {kPageActionMenu});

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping the AIM button on the App Bar navigates to the AIM SRP
// URL in the current tab, rather than triggering the Assistant Container flow.
TEST_F(AppBarMediatorTest, TestAimButtonTappedOpensAimSrp) {
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAIM
                                   fromView:nil];
  EXPECT_EQ(1, url_loader_->load_current_tab_call_count);
  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_valid());
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAIM, 1);
}

// Tests that tapping the AIM button in Incognito mode navigates to the AIM SRP
// URL in the current tab.
TEST_F(AppBarMediatorTest, TestAimButtonTappedOpensAimSrpIncognito) {
  incognito_state_.incognitoContentVisible = YES;
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAIM
                                   fromView:nil];
  EXPECT_EQ(1, url_loader_->load_current_tab_call_count);
  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_valid());
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAIM, 1);
}

// Tests that the assistant button is in the kLens state when location is
// ineligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateLensWhenIneligible) {
  SetLocationEligible(false);
  mediator_.overrideLensAvailabilityForTesting = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button remains ineligible for kAsk when the country
// is in the EEA (e.g., France), even if the PageActionMenu feature is enabled.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateEEACountryGated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  // Set country to France ("fr"), which is in EEA.
  scoped_variations_service_.Get()->OverrideStoredPermanentCountry("fr");
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "en-US");

  SignInAndSetCapability(true);
  mediator_.overrideLensAvailabilityForTesting = YES;

  // Expect state to be kLens instead of kAsk.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button remains ineligible for kAsk when the country
// is Japan ("jp"), even if the PageActionMenu feature is enabled.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateJapanCountryGated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  // Set country to Japan ("jp").
  scoped_variations_service_.Get()->OverrideStoredPermanentCountry("jp");
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "en-US");

  SignInAndSetCapability(true);
  mediator_.overrideLensAvailabilityForTesting = YES;

  // Expect state to be kLens instead of kAsk.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is in the kAsk state when the country is
// not in the EEA and not Japan (e.g., Brazil), and the PageActionMenu feature
// is enabled.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateNonEEANonJapanEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  // Set country to Brazil ("br"), which is not in EEA or Japan.
  scoped_variations_service_.Get()->OverrideStoredPermanentCountry("br");
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "en-US");

  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button remains in the kLens state when signed in but
// not location eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateLensWhenIneligibleSignedIn) {
  // Tear down mediator and agent before changing location eligibility.
  [mediator_ disconnect];
  mediator_ = nil;
  regular_browser_->RemoveUserData(GeminiBrowserAgent::UserDataKey());

  SetLocationEligible(false);
  SignInAndSetCapability(false);

  // Recreate agent and mediator with the new location eligibility.
  GeminiBrowserAgent::CreateForBrowser(regular_browser_.get());

  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:aim_eligibility_service_.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];
  mediator_.consumer = consumer_;
  mediator_.sceneHandler = mock_scene_handler_;
  mediator_.settingsHandler = mock_settings_handler_;
  mediator_.lensOverlayHandler = mock_lens_overlay_handler_;

  mediator_.overrideLensAvailabilityForTesting = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping the assistant button in the kLens state dispatches
// the Lens Overlay command.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedLens) {
  OCMExpect([mock_lens_overlay_handler_
      createAndShowLensUI:YES
               entrypoint:LensOverlayEntrypoint::kAppBar
               completion:[OCMArg any]]);
  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kLens
                                   fromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_lens_overlay_handler_);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kLens, 1);
}

// Tests that the assistant button is in the kAccount state by default.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAccountDefault) {
  SetLocationEligible(false);
  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is disabled in the kAccount state when
// sign-in is disabled.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAccount_SigninDisabled) {
  SetLocationEligible(false);
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice, false);

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:NO
                       avatar:nil
                     signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that tapping the assistant button in the kAccount state calls the
// delegate to show sign-in when signed out.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedAccountSignedOut) {
  id mock_delegate = OCMProtocolMock(@protocol(AppBarMediatorDelegate));
  mediator_.delegate = mock_delegate;

  UIView* dummy_view = [[UIView alloc] init];
  OCMExpect([mock_delegate showSignin:dummy_view]);

  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAccount
                                   fromView:dummy_view];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAccount, 1);
}

// Tests that tapping the assistant button in the kAccount state calls the
// delegate to show account menu when signed in.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedAccountSignedIn) {
  SignInAndSetCapability(true);

  id mock_delegate = OCMProtocolMock(@protocol(AppBarMediatorDelegate));
  mediator_.delegate = mock_delegate;

  UIView* dummy_view = [[UIView alloc] init];
  OCMExpect([mock_delegate showAccountMenu:dummy_view]);

  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAccount
                                   fromView:dummy_view];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAccount, 1);
}

// Tests that tapping the assistant button in the kAccount state does nothing
// when sign-in is disabled, even if there is a primary identity.
TEST_F(AppBarMediatorTest, TestAssistantButtonTappedAccountSigninDisabled) {
  SignInAndSetCapability(true);
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice, false);

  id mock_delegate = OCMProtocolMock(@protocol(AppBarMediatorDelegate));
  mediator_.delegate = mock_delegate;

  UIView* dummy_view = [[UIView alloc] init];
  OCMReject([mock_delegate showSignin:[OCMArg any]]);
  OCMReject([mock_delegate showAccountMenu:[OCMArg any]]);

  [mediator_ assistantButtonTappedWithState:AppBarAssistantButtonState::kAccount
                                   fromView:dummy_view];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
  histogram_tester_.ExpectUniqueSample(kAppBarAssistantButtonTappedHistogram,
                                       AppBarAssistantButtonState::kAccount, 1);
}

// Tests that the assistant button is in the kAccount state with an avatar when
// signed in.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAccountWithAvatar) {
  SetLocationEligible(false);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(regular_profile_.get());
  GeminiBrowserAgent* agent =
      GeminiBrowserAgent::FromBrowser(regular_browser_.get());
  if (identity_manager && agent) {
    identity_manager->RemoveObserver(agent);
  }

  SignInAndSetCapability(true);

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:[OCMArg checkWithBlock:^BOOL(id value) {
                         return value != nil;
                       }]
                     signedIn:YES]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is kAccount when Gemini is
// disabled by enterprise policy, even if location is eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAccountWhenPolicyDisabled) {
  SetLocationEligible(true);

  // Set policy to disabled.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is kAccount when GenAi is
// disabled by enterprise policy, even if location is eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateAccountGenAiPolicy) {
  SetLocationEligible(true);

  // Disable GenAi using the GenAiDefaultSettings enterprise policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that selecting a NTP web state updates the consumer with NTP visible
// and isStartSurface status.
TEST_F(AppBarMediatorTest, TestWebStateSelectionNTPUpdatesConsumer) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("chrome://newtab"));
  NewTabPageTabHelper::CreateForWebState(web_state.get());
  NewTabPageTabHelper::FromWebState(web_state.get())
      ->SetShowStartSurface(false);

  OCMExpect([consumer_ setNTPVisible:YES isStartSurface:NO]);

  regular_web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  EXPECT_OCMOCK_VERIFY(consumer_);

  // Test Start Surface NTP
  auto start_web_state = std::make_unique<web::FakeWebState>();
  start_web_state->SetVisibleURL(GURL("chrome://newtab"));
  NewTabPageTabHelper::CreateForWebState(start_web_state.get());
  NewTabPageTabHelper::FromWebState(start_web_state.get())
      ->SetShowStartSurface(true);

  OCMExpect([consumer_ setNTPVisible:YES isStartSurface:YES]);

  regular_web_state_list_->InsertWebState(
      std::move(start_web_state),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when GeminiService's eligibility changes, the mediator receives
// the notification and updates the assistant button's state on the consumer.
TEST_F(AppBarMediatorTest, TestGeminiEligibilityChangeUpdatesAssistantButton) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});

  FakeGeminiService fake_gemini_service;
  fake_gemini_service.SetIsEligible(false);

  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  AppBarMediator* mediator = [[AppBarMediator alloc]
          initWithRegularWebStateList:regular_web_state_list_
                incognitoWebStateList:incognito_web_state_list_
          regularFullscreenController:TestFullscreenController::FromBrowser(
                                          regular_browser_.get())
        incognitoFullscreenController:TestFullscreenController::FromBrowser(
                                          incognito_browser_.get())
        regularFullscreenBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                          regular_browser_.get())
      incognitoFullscreenBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                          incognito_browser_.get())
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:&fake_gemini_service
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:aim_eligibility_service_.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  id consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  mediator.consumer = consumer;

  // Change from ineligible (no kAsk button or disabled state) to eligible.
  // When eligible, it should update state to kAsk.
  OCMExpect([consumer setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                  highlighted:NO
                                      enabled:YES
                                       avatar:nil
                                     signedIn:NO]);

  fake_gemini_service.SetIsEligible(true);

  EXPECT_OCMOCK_VERIFY(consumer);

  [mediator disconnect];
}

// Tests that when AimEligibilityService's eligibility changes, the mediator
// receives the notification and updates the assistant button's state on the
// consumer.
TEST_F(AppBarMediatorTest, TestAimEligibilityChangeUpdatesAssistantButton) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiKillSwitch}, {kPageActionMenu});

  base::RepeatingClosure aim_callback;
  auto mock_aim_service = std::make_unique<MockAimEligibilityService>(
      *regular_profile_->GetTestingPrefService(),
      search_engines_test_environment_.template_url_service(),
      regular_profile_->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(regular_profile_.get()));

  EXPECT_CALL(*mock_aim_service, RegisterEligibilityChangedCallback(testing::_))
      .WillOnce([&](base::RepeatingClosure callback) {
        aim_callback = callback;
        return base::CallbackListSubscription();
      });

  FakeGeminiService fake_gemini_service;
  fake_gemini_service.SetIsEligible(false);

  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  AppBarMediator* mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:&fake_gemini_service
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:mock_aim_service.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  id consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  mediator.consumer = consumer;

  // Change from ineligible to eligible.
  EXPECT_CALL(*mock_aim_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  OCMExpect([consumer setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                  highlighted:NO
                                      enabled:YES
                                       avatar:nil
                                     signedIn:NO]);

  ASSERT_FALSE(aim_callback.is_null());
  aim_callback.Run();

  EXPECT_OCMOCK_VERIFY(consumer);

  [mediator disconnect];
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

// Tests that the Gemini button is enabled when the user is signed out
// but the GeminiSettings policy allows it.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateAsk_EnabledByPolicyWhenSignedOut) {
  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());
  web_state->SetContentsMimeType("text/html");
  GeminiTabHelper::CreateForWebState(web_state.get());
  web_state->SetVisibleURL(GURL("https://example.com"));
  web_state->WasShown();

  regular_web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  SetLocationEligible(true);

  // Ensure GeminiSettings enterprise policy allows Gemini.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kAllowed));

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the Gemini button is not shown when the GeminiSettings enterprise
// policy explicitly disables Gemini.
TEST_F(AppBarMediatorTest, TestAssistantButtonState_HiddenWhenPolicyDisabled) {
  SetLocationEligible(true);

  // Disable Gemini using the GeminiSettings enterprise policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // Force Lens to deterministically check the state.
  mediator_.overrideLensAvailabilityForTesting = YES;

  // When Gemini is disabled by policy, the button state is set to Lens.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the Gemini button is not shown when the GenAiDefaultSettings
// enterprise policy explicitly disables Gemini.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonState_HiddenWhenGenAiPolicyDisabled) {
  SetLocationEligible(true);

  // Disable GenAi using the GenAiDefaultSettings enterprise policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  // Force Lens to deterministically check the state.
  mediator_.overrideLensAvailabilityForTesting = YES;
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:YES]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is kAIM when Gemini is disabled
// by GeminiSettings policy, but AIM features are allowed.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateAIM_WhenGeminiDisabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu}, {kGeminiKillSwitch});

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  SetLocationEligible(true);

  // Disable Gemini using the GeminiSettings enterprise policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button state is kAIM when Gemini is disabled
// by GenAiDefaultSettings policy, but AIM features are allowed.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateAIM_WhenGenAiDisabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu}, {kGeminiKillSwitch});

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  SetLocationEligible(true);

  // Disable GenAi using the GenAiDefaultSettings enterprise policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests assistant button on-load metrics.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateOnLoadMetric) {
  histogram_tester_.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAsk, 1);
}

// Tests the Lens state on-load metric. Recreates the mediator to reset SetUp's
// recording.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateOnLoadMetric_Lens) {
  // Disable Gemini via policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // Recreate the mediator.
  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  base::HistogramTester local_histogram_tester;
  AppBarMediator* local_mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:aim_eligibility_service_.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  local_mediator.overrideLensAvailabilityForTesting = YES;

  id local_consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  OCMExpect([local_consumer
      setAssistantButtonState:AppBarAssistantButtonState::kLens
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);

  local_mediator.consumer = local_consumer;
  EXPECT_OCMOCK_VERIFY(local_consumer);

  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kLens, 1);

  [local_mediator disconnect];
}

// Tests the Account state on-load metric. Recreates the mediator to reset
// SetUp's recording.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateOnLoadMetric_Account) {
  // Disable Gemini via policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // Recreate the mediator.
  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  base::HistogramTester local_histogram_tester;
  AppBarMediator* local_mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:aim_eligibility_service_.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  local_mediator.overrideLensAvailabilityForTesting = NO;
  SetLocationEligible(false);

  id local_consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  OCMExpect([local_consumer
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);

  local_mediator.consumer = local_consumer;
  EXPECT_OCMOCK_VERIFY(local_consumer);

  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAccount, 1);

  [local_mediator disconnect];
}

// Tests the AIM state on-load metric.
TEST_F(AppBarMediatorTest, TestAssistantButtonStateOnLoadMetric_AIM) {
  // Disable Gemini via policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // Enable AIM features.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {});

  // Recreate the mediator to test the initial on-load recording.
  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  base::HistogramTester local_histogram_tester;
  AppBarMediator* local_mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:aim_eligibility_service_.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  // We expect the consumer to be updated with kAIM.
  id local_consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  OCMExpect([local_consumer
      setAssistantButtonState:AppBarAssistantButtonState::kAIM
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);

  // Setting the consumer triggers updateAssistantButton and should record the
  // metric.
  local_mediator.consumer = local_consumer;

  EXPECT_OCMOCK_VERIFY(local_consumer);

  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAIM, 1);

  [local_mediator disconnect];
}

// Tests that the AIM on-load metric is deferred while AIM eligibility is
// pending on startup, and recorded when the eligibility changed callback
// executes.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateOnLoadMetric_AIM_DeferredUntilCallback) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  // Disable Gemini via policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  base::RepeatingClosure aim_callback;
  auto mock_aim_service = std::make_unique<MockAimEligibilityService>(
      *regular_profile_->GetTestingPrefService(),
      search_engines_test_environment_.template_url_service(),
      regular_profile_->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(regular_profile_.get()));

  ON_CALL(*mock_aim_service, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsServerEligibilityEnabled())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsAimEligible())
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(*mock_aim_service, RegisterEligibilityChangedCallback(testing::_))
      .WillOnce([&](base::RepeatingClosure callback) {
        aim_callback = callback;
        return base::CallbackListSubscription();
      });

  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  base::HistogramTester local_histogram_tester;
  AppBarMediator* local_mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:mock_aim_service.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  local_mediator.overrideLensAvailabilityForTesting = NO;
  SetLocationEligible(false);

  id local_consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  local_mediator.consumer = local_consumer;

  // On initial load, AIM check is pending, so 0 metrics are recorded.
  local_histogram_tester.ExpectTotalCount(
      kAppBarAssistantButtonStateOnLoadHistogram, 0);
  local_histogram_tester.ExpectTotalCount(
      kAppBarAssistantButtonPreferredStateOnLoadHistogram, 0);

  // Simulate response arriving with AIM eligible.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  std::string response_string;
  response.SerializeToString(&response_string);
  mock_aim_service->SetEligibilityResponseForDebugging(
      base::Base64Encode(response_string));
  EXPECT_CALL(*mock_aim_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  OCMExpect([local_consumer
      setAssistantButtonState:AppBarAssistantButtonState::kAIM
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);

  ASSERT_FALSE(aim_callback.is_null());
  aim_callback.Run();

  EXPECT_OCMOCK_VERIFY(local_consumer);

  // Exactly 1 metric sample should now be logged for kAIM.
  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAIM, 1);

  // Calling callback or updateAssistantButton again does not log duplicate
  // metrics.
  aim_callback.Run();
  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAIM, 1);
  // The preferred state is logged once, along with the state.
  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonPreferredStateOnLoadHistogram,
      AppBarAssistantButtonPreferredState::kDefault, 1);

  [local_mediator disconnect];
}

// Tests the AIM state on-load metric when AIM eligibility is already cached in
// prefs on startup.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonStateOnLoadMetric_AIM_CachedInPrefs) {
  // Disable Gemini via policy.
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // Populate cached response in prefs.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  std::string response_string;
  response.SerializeToString(&response_string);
  regular_profile_->GetTestingPrefService()->SetString(
      "aim_eligibility_service.aim_eligibility_response",
      base::Base64Encode(response_string));

  auto mock_aim_service = std::make_unique<MockAimEligibilityService>(
      *regular_profile_->GetTestingPrefService(),
      search_engines_test_environment_.template_url_service(),
      regular_profile_->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(regular_profile_.get()));

  ON_CALL(*mock_aim_service, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_service, IsServerEligibilityEnabled())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(*mock_aim_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  BrowserActionFactory* regular_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:regular_browser_.get()
                                           scenario:kTestMenuScenario];
  BrowserActionFactory* incognito_action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:incognito_browser_.get()
                                           scenario:kTestMenuScenario];

  base::HistogramTester local_histogram_tester;
  AppBarMediator* local_mediator = [[AppBarMediator alloc]
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
                 regularActionFactory:regular_action_factory
               incognitoActionFactory:incognito_action_factory
                              profile:regular_profile_.get()
                          prefService:regular_profile_->GetTestingPrefService()
                   templateURLService:search_engines_test_environment_
                                          .template_url_service()
                authenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          regular_profile_.get())
                        geminiService:fake_gemini_service_
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          regular_browser_.get())
                aimEligibilityService:mock_aim_service.get()
                            URLLoader:url_loader_
                         tabGridState:tab_grid_state_
                       incognitoState:incognito_state_
             lensOverlayStateNotifier:lens_overlay_state_];

  id local_consumer = OCMProtocolMock(@protocol(TestAppBarConsumer));
  OCMExpect([local_consumer
      setAssistantButtonState:AppBarAssistantButtonState::kAIM
                  highlighted:NO
                      enabled:YES
                       avatar:nil
                     signedIn:NO]);

  // Setting the consumer triggers updateAssistantButton and records metric
  // immediately.
  local_mediator.consumer = local_consumer;

  EXPECT_OCMOCK_VERIFY(local_consumer);

  // Since response was in prefs, metric is logged immediately.
  local_histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAIM, 1);

  [local_mediator disconnect];
}

// Tests the priority chain: Gemini (kAsk) has priority over AIM (kAIM) and Lens
// (kLens).
TEST_F(AppBarMediatorTest, TestAssistantButtonStatePriority_GeminiOverAll) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {});

  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;

  // Gemini is eligible, AIM is eligible, Lens is eligible.
  // Gemini (kAsk) should be chosen.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the priority chain: AIM (kAIM) has priority over Lens (kLens) when
// Gemini is ineligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStatePriority_AIMOverLens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {});

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  // Keep location eligible for AIM, but disable Gemini via policy.
  SetLocationEligible(true);
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  mediator_.overrideLensAvailabilityForTesting = YES;

  // Gemini is ineligible, AIM is eligible, Lens is eligible.
  // AIM (kAIM) should be chosen.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the priority chain: Lens (kLens) has priority over Account (kAccount)
// when Gemini and AIM are ineligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonStatePriority_LensOverAccount) {
  // Make Gemini ineligible via country gating.
  SetLocationEligible(false);
  mediator_.overrideLensAvailabilityForTesting = YES;

  // Gemini is ineligible, AIM is ineligible (disabled by default), Lens is
  // eligible. Lens (kLens) should be chosen.
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);

  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button shows Lens when the user chose it, even if
// Gemini is eligible.
TEST_F(AppBarMediatorTest, TestAssistantButtonCustomization_PreferredLens) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kLens);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the user choice is kept while it isn't eligible, so that the
// assistant button shows it again once it becomes eligible.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_UnavailableChoiceIsKept) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = NO;
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kLens);
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_EQ(static_cast<int>(AppBarAssistantButtonPreferredState::kLens),
            regular_profile_->GetTestingPrefService()->GetInteger(
                prefs::kAppBarAssistantButtonPreferredState));

  mediator_.overrideLensAvailabilityForTesting = YES;
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that choosing "Ask Gemini" uses the default priority order, so that the
// assistant button shows AI Mode when Gemini isn't eligible.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_PreferredAskShowsAimWhenNoGemini) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kAsk);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the user choice also applies in incognito, so that the assistant
// button shows the same entry point as in regular mode, and that there is no
// assistant button menu in incognito.
TEST_F(AppBarMediatorTest, TestAssistantButtonCustomization_Incognito) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);
  incognito_state_.incognitoContentVisible = YES;

  // All the entry points but Gemini are disabled in incognito.
  OCMExpect([consumer_
      setAssistantButtonState:AppBarAssistantButtonState::kAccount
                  highlighted:NO
                      enabled:NO
                       avatar:nil
                     signedIn:NO]);
  EXPECT_FALSE(UpdateAssistantButtonAndGetMenu());
}

// Tests that the assistant button keeps showing Lens, disabled, when the user
// chose Lens and there is no tab, so that the entry point is the same in
// regular and incognito mode.
TEST_F(AppBarMediatorTest, TestAssistantButtonCustomization_LensWithoutTab) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = NO;
  regular_profile_->GetTestingPrefService()->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kLens);
  ASSERT_TRUE(regular_web_state_list_->empty());
  ASSERT_TRUE(incognito_web_state_list_->empty());

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:NO
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);

  incognito_state_.incognitoContentVisible = YES;
  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:NO
                                        avatar:nil
                                      signedIn:NO]);
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the content of the assistant button menu when all the entries are
// available and the user didn't choose any.
TEST_F(AppBarMediatorTest, TestAssistantButtonCustomization_Menu) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;

  UIMenu* menu = UpdateAssistantButtonAndGetMenu();
  // "Ask Gemini" is checked as the assistant button shows Gemini.
  ExpectAssistantButtonMenuEntries(
      menu, {{IDS_IOS_APP_BAR_ASK_GEMINI, /*checked=*/true},
             {IDS_IOS_LENS_PRODUCT_NAME, /*checked=*/false},
             {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/false}});
}

// Tests that the assistant button menu hides the unavailable entries, and
// checks "Lens" as the assistant button shows Lens when neither Gemini nor AI
// Mode is available.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_MenuHidesUnavailableEntries) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(false);
  mediator_.overrideLensAvailabilityForTesting = YES;

  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_LENS_PRODUCT_NAME, /*checked=*/true},
       {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/false}});
}

// Tests that the assistant button menu shows and checks "Ask Gemini" when
// Gemini isn't available but AI Mode is, as the assistant button shows AI
// Mode.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_MenuShowsAskGeminiWhenAimAvailable) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_APP_BAR_ASK_GEMINI, /*checked=*/true},
       {IDS_IOS_LENS_PRODUCT_NAME, /*checked=*/false},
       {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/false}});
}

// Tests that there is an assistant button menu when only AI Mode and Account
// are available, so that the user can switch between them.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_MenuWithAimAndAccountOnly) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(true);
  mediator_.overrideLensAvailabilityForTesting = NO;
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);

  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_APP_BAR_ASK_GEMINI, /*checked=*/false},
       {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/true}});
}

// Tests that the assistant button menu checks the entry of the state shown by
// the assistant button, and not the one chosen by the user, when the chosen one
// isn't available.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_MenuChecksDisplayedEntry) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = NO;
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kLens);

  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_APP_BAR_ASK_GEMINI, /*checked=*/true},
       {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/false}});
}

// Tests that there is no assistant button menu when neither Gemini, AI Mode nor
// Lens is available, as Account would be the only choice.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_NoMenuWhenOnlyAccountAvailable) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(false);
  mediator_.overrideLensAvailabilityForTesting = NO;

  EXPECT_FALSE(UpdateAssistantButtonAndGetMenu());
}

// Tests that the assistant button uses the default priority order when the user
// chose Account while signed out and sign-in is disabled, and that the menu
// hides the Account entry. Gemini is also unavailable to signed-out users when
// sign-in is disabled, so the button shows AI Mode.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_PreferredAccountSigninDisabled) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  SetAimEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice, false);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kAIM
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_APP_BAR_ASK_GEMINI, /*checked=*/true},
       {IDS_IOS_LENS_PRODUCT_NAME, /*checked=*/false}});
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the Account entry stays available when sign-in is disabled but the
// user is signed in, as the account menu can still be shown.
TEST_F(
    AppBarMediatorTest,
    TestAssistantButtonCustomization_PreferredAccountSignedInSigninDisabled) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SignInAndSetCapability(true);
  SetLocationEligible(false);
  SetAimEligible(false);
  mediator_.overrideLensAvailabilityForTesting = YES;
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice, false);

  ExpectAssistantButtonMenuEntries(
      UpdateAssistantButtonAndGetMenu(),
      {{IDS_IOS_LENS_PRODUCT_NAME, /*checked=*/false},
       {IDS_IOS_APP_BAR_ACCOUNT, /*checked=*/true}});
}

// Tests that selecting an entry of the assistant button menu stores the user
// choice and updates the assistant button.
TEST_F(AppBarMediatorTest, TestAssistantButtonCustomization_SelectMenuEntry) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  UIMenu* menu = UpdateAssistantButtonAndGetMenu();
  ASSERT_EQ(kAssistantButtonMenuEntryCount, menu.children.count);
  UIAction* lens_action = base::apple::ObjCCastStrict<UIAction>(
      menu.children[kAssistantButtonMenuLensIndex]);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:YES
                                        avatar:nil
                                      signedIn:NO]);
  [lens_action performWithSender:nil target:nil];
  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_EQ(static_cast<int>(AppBarAssistantButtonPreferredState::kLens),
            regular_profile_->GetTestingPrefService()->GetInteger(
                prefs::kAppBarAssistantButtonPreferredState));
}

// Tests that selecting each entry of the assistant button menu records its own
// user action. "Ask Gemini" is selected first, while it is already checked as
// the user didn't choose any entry, to verify that it is recorded anyway.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonCustomization_SelectionRecordsUserActions) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  mediator_.overrideLensAvailabilityForTesting = YES;
  base::UserActionTester user_action_tester;

  ASSERT_NO_FATAL_FAILURE(
      SelectAssistantButtonMenuEntry(kAssistantButtonMenuAskGeminiIndex));
  ExpectAssistantButtonMenuUserActionCounts(user_action_tester,
                                            /*ask_gemini_count=*/1,
                                            /*lens_count=*/0,
                                            /*account_count=*/0);

  ASSERT_NO_FATAL_FAILURE(
      SelectAssistantButtonMenuEntry(kAssistantButtonMenuLensIndex));
  ExpectAssistantButtonMenuUserActionCounts(user_action_tester,
                                            /*ask_gemini_count=*/1,
                                            /*lens_count=*/1,
                                            /*account_count=*/0);

  ASSERT_NO_FATAL_FAILURE(
      SelectAssistantButtonMenuEntry(kAssistantButtonMenuAccountIndex));
  ExpectAssistantButtonMenuUserActionCounts(user_action_tester,
                                            /*ask_gemini_count=*/1,
                                            /*lens_count=*/1,
                                            /*account_count=*/1);
}

// Tests that the preferred state on-load metric records "Default" when the user
// didn't choose any state.
TEST_F(AppBarMediatorTest, TestAssistantButtonPreferredStateOnLoadMetric) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonPreferredStateOnLoadHistogram,
      AppBarAssistantButtonPreferredState::kDefault, 1);
}

// Tests that the preferred state on-load metric records "Ask Gemini" when the
// user chose it, distinctly from the absence of choice.
TEST_F(AppBarMediatorTest, TestAssistantButtonPreferredStateOnLoadMetric_Ask) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetPreferredAssistantButtonState(AppBarAssistantButtonPreferredState::kAsk);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonPreferredStateOnLoadHistogram,
      AppBarAssistantButtonPreferredState::kAsk, 1);
}

// Tests that the preferred state on-load metric records "Default" when the
// stored state is outside of the enum range, as it is treated as no choice.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonPreferredStateOnLoadMetric_InvalidPreference) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  regular_profile_->GetTestingPrefService()->SetInteger(
      prefs::kAppBarAssistantButtonPreferredState,
      kInvalidAssistantButtonState);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonPreferredStateOnLoadHistogram,
      AppBarAssistantButtonPreferredState::kDefault, 1);
}

// Tests that the default state on-load metric isn't recorded when the user
// didn't choose any state.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonDefaultStateOnLoadMetric_NoChoice) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectTotalCount(kAppBarAssistantButtonStateOnLoadHistogram,
                                    1);
  histogram_tester.ExpectTotalCount(
      kAppBarAssistantButtonDefaultStateOnLoadHistogram, 0);
}

// Tests that the default state on-load metric records Gemini, the highest
// priority state, when the user chose Account and Gemini is eligible, while the
// assistant button shows Account.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonDefaultStateOnLoadMetric_ChosenAccount) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(true);
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAccount, 1);
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonDefaultStateOnLoadHistogram,
      AppBarAssistantButtonState::kAsk, 1);
}

// Tests that the default state on-load metric uses the whole priority order: it
// records AI Mode when the user chose Account and Gemini isn't eligible.
TEST_F(AppBarMediatorTest,
       TestAssistantButtonDefaultStateOnLoadMetric_PriorityOrder) {
  base::test::ScopedFeatureList feature_list(kAppBarAssistantCustomization);
  SetLocationEligible(false);
  SetAimEligible(true);
  SetPreferredAssistantButtonState(
      AppBarAssistantButtonPreferredState::kAccount);
  base::HistogramTester histogram_tester;

  RecreateMediator();
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonStateOnLoadHistogram,
      AppBarAssistantButtonState::kAccount, 1);
  histogram_tester.ExpectUniqueSample(
      kAppBarAssistantButtonDefaultStateOnLoadHistogram,
      AppBarAssistantButtonState::kAIM, 1);
}

// Tests that the assistant button is disabled when Lens Overlay is visible.
TEST_F(AppBarMediatorTest, TestAssistantButtonDisabledWhenLensOverlayVisible) {
  SetLocationEligible(false);
  mediator_.overrideLensAvailabilityForTesting = YES;

  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(regular_profile_.get());

  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  fake_navigation_manager->AddItem(GURL("https://example.com"),
                                   ui::PAGE_TRANSITION_LINK);
  fake_navigation_manager->SetVisibleItem(
      fake_navigation_manager->GetItemAtIndex(0));
  web_state->SetNavigationManager(std::move(fake_navigation_manager));

  LensOverlayTabHelper::CreateForWebState(web_state.get());
  LensOverlayTabHelper* helper =
      LensOverlayTabHelper::FromWebState(web_state.get());

  regular_web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  helper->SetLensOverlayUIAttachedAndAlive(true);

  OCMExpect([consumer_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                   highlighted:NO
                                       enabled:NO
                                        avatar:[OCMArg any]
                                      signedIn:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation* invocation) {
        BOOL highlighted;
        [invocation getArgument:&highlighted atIndex:3];
        BOOL enabled;
        [invocation getArgument:&enabled atIndex:4];
        UIImage* avatar;
        [invocation getArgument:&avatar atIndex:5];
        BOOL signedIn;
        [invocation getArgument:&signedIn atIndex:6];

        EXPECT_FALSE(highlighted);
        EXPECT_FALSE(enabled);
        EXPECT_EQ(nil, avatar);
        EXPECT_FALSE(signedIn);
      });
  [lens_overlay_state_ lensOverlayDidPrepare];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that WebStateList updates during batch operations are deferred until
// the batch ends.
TEST_F(AppBarMediatorTest, TestWebStateListBatchOperation) {
  WebStateList* web_state_list = regular_browser_->GetWebStateList();

  // Reject intermediate tab count updates during batch insertion.
  OCMReject([consumer_ updateTabCount:1]);
  OCMReject([consumer_ updateTabCount:2]);

  // Expect only the final tab count update when the batch operation ends.
  OCMExpect([consumer_ updateTabCount:3]);

  {
    WebStateList::ScopedBatchOperation batch =
        web_state_list->StartBatchOperation();
    for (int i = 0; i < 3; ++i) {
      auto web_state = std::make_unique<web::FakeWebState>();
      web_state_list->InsertWebState(std::move(web_state));
    }
  }

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that consumer updates are deferred while SceneState.UIEnabled is NO
// and performed once SceneState.UIEnabled transitions to YES.
TEST_F(AppBarMediatorTest, TestConsumerUpdatesDeferredUntilSceneUIEnabled) {
  SceneState* scene_state = [[SceneState alloc] init];
  ASSERT_FALSE(scene_state.UIEnabled);
  mediator_.sceneState = scene_state;

  // Use a strict mock to verify no consumer methods are invoked while
  // UIEnabled is NO (including when setting consumer or inserting a WebState).
  id deferred_consumer = OCMStrictProtocolMock(@protocol(TestAppBarConsumer));
  mediator_.consumer = deferred_consumer;

  auto web_state = std::make_unique<web::FakeWebState>();
  regular_browser_->GetWebStateList()->InsertWebState(std::move(web_state));
  [mediator_ updateAssistantButton];
  EXPECT_OCMOCK_VERIFY(deferred_consumer);

  // Switch back to consumer_ (still while UIEnabled is NO, so no update yet),
  // then enable UI and verify updateConsumer synchronizes the state.
  mediator_.consumer = consumer_;
  OCMExpect([consumer_ updateTabCount:1]);
  scene_state.UIEnabled = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/buttons/toolbar_buttons_mediator.h"

#import "base/i18n/language_tag.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/menu/public/menu_histograms.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// Exposes the Gemini observation and the history menu navigation in tests.
@interface ToolbarButtonsMediator (Test)
- (void)geminiAvailabilityChanged:(BOOL)available;
- (void)navigateToPageForItem:(web::NavigationItem*)item;
@end

namespace {

MenuScenarioHistogram kTestMenuScenario = kMenuScenarioHistogramToolbarMenu;

// `FakeNavigationManager` subclass that records calls to `GoToIndex`.
class RecordingNavigationManager : public web::FakeNavigationManager {
 public:
  void GoToIndex(int index) override {
    last_navigated_index_ = index;
    SetLastCommittedItemIndex(index);
  }
  int last_navigated_index() const { return last_navigated_index_; }

 private:
  int last_navigated_index_ = -1;
};

}  // namespace

// Fixture for testing ToolbarButtonsMediator.
class ToolbarButtonsMediatorTest : public PlatformTest {
 protected:
  ToolbarButtonsMediatorTest() {
    scoped_feature_list_.InitAndEnableFeature(kChromeNextIa);
    SetLocationEligible(true);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindOnce(&IdentityTestEnvironmentBrowserStateAdaptor::
                           BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    // The Gemini browser agent observes fullscreen.
    TestFullscreenController::CreateForBrowser(browser_.get());
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    GeminiBrowserAgent::CreateForBrowser(browser_.get());
    gemini_browser_agent_ = GeminiBrowserAgent::FromBrowser(browser_.get());

    // The menus' actions need these handlers.
    scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    browser_coordinator_handler_ =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:browser_coordinator_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];
    page_side_swipe_handler_ =
        OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:page_side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];

    mediator_ = CreateMediator(/*incognito=*/NO);

    consumer_ = OCMProtocolMock(@protocol(ToolbarButtonsConsumer));
    [mediator_ addConsumer:consumer_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  ToolbarButtonsMediator* CreateMediator(BOOL incognito) {
    BrowserActionFactory* action_factory =
        [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                             scenario:kTestMenuScenario];
    ToolbarButtonsMediator* mediator = [[ToolbarButtonsMediator alloc]
            initWithIncognito:incognito
                 webStateList:browser_->GetWebStateList()
                actionFactory:action_factory
                      profile:profile_.get()
                  prefService:profile_->GetPrefs()
        authenticationService:auth_service_
           geminiBrowserAgent:gemini_browser_agent_];
    mediator.navigationBrowserAgent =
        WebNavigationBrowserAgent::FromBrowser(browser_.get());
    return mediator;
  }

  // Returns a new fake web state and sets the fake navigation manager to the
  // navigation manager used here.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetVisibleURL(GURL("https://example.com"));
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = navigation_manager.get();
    navigation_manager->AddItem(GURL("https://example.com/1"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL("https://example.com/2"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL("https://example.com/3"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItemIndex(2);
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(profile_.get());
    return web_state;
  }

  // Inserts and activates a new web state.
  void InsertActiveWebState() {
    browser_->GetWebStateList()->InsertWebState(
        CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());
  }

  void SignInAndSetCapability(bool capability) {
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());

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

  void SetLocationEligible(bool eligible) {
    scoped_variations_service_.Get()->OverrideStoredPermanentCountry(
        eligible ? "us" : "fr");
    TestingApplicationContext::GetGlobal()
        ->GetApplicationLocaleStorage()
        ->SetTag(eligible ? base::i18n::GetKnownLanguageTag("en-US")
                          : base::i18n::GetKnownLanguageTag("fr-FR"));
  }

  // Returns the tab grid menu that `mediator` gives a new consumer when it is
  // added.
  UIMenu* TabGridMenuOnActivation(ToolbarButtonsMediator* mediator) {
    id consumer = OCMProtocolMock(@protocol(ToolbarButtonsConsumer));
    __block UIMenu* captured_menu = nil;
    OCMExpect([consumer setMenu:[OCMArg checkWithBlock:^BOOL(id obj) {
                          captured_menu = obj;
                          return YES;
                        }]
                  forButtonType:ToolbarButtonTypeTabGrid]);
    [mediator addConsumer:consumer];
    EXPECT_OCMOCK_VERIFY(consumer);
    return captured_menu;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  IOSChromeScopedTestingVariationsService scoped_variations_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ToolbarButtonsMediator* mediator_;
  id consumer_;
  id scene_handler_;
  id page_side_swipe_handler_;
  id browser_coordinator_handler_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<GeminiBrowserAgent> gemini_browser_agent_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
};

// Tests that inserting web states updates the consumer tab count.
TEST_F(ToolbarButtonsMediatorTest, TestTabCountAndGroupUpdates) {
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setInTabGroup:NO]);
  InsertActiveWebState();
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Group the active tab.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setInTabGroup:YES]);
  browser_->GetWebStateList()->CreateGroup(
      {0},
      tab_groups::TabGroupVisualData(u"Group",
                                     tab_groups::TabGroupColorId::kBlue),
      tab_groups::TabGroupId::GenerateNew());
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Add a second web state, not in the group. The active tab is still the
  // grouped one, so the count is the group's.
  OCMExpect([consumer_ updateTabCount:1]);
  OCMExpect([consumer_ setInTabGroup:YES]);
  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(1));
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Add the second web state to the group.
  OCMExpect([consumer_ updateTabCount:2]);
  OCMExpect([consumer_ setInTabGroup:YES]);
  const TabGroup* group = browser_->GetWebStateList()->GetGroupOfWebStateAt(0);
  browser_->GetWebStateList()->MoveToGroup({1}, group);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Insert and activate a third web state, not in a group.
  OCMExpect([consumer_ updateTabCount:3]);
  OCMExpect([consumer_ setInTabGroup:NO]);
  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(2).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Activate the grouped tab again.
  OCMExpect([consumer_ updateTabCount:2]);
  OCMExpect([consumer_ setInTabGroup:YES]);
  browser_->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that selecting a web state updates the consumer.
TEST_F(ToolbarButtonsMediatorTest, TestWebStateSelectionUpdatesConsumer) {
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO animated:NO]);
  OCMExpect([consumer_ setShareEnabled:YES]);
  OCMExpect([consumer_ setNTPVisible:NO
                      isStartSurface:NO
                           isLoading:NO
                     loadingProgress:0.0]);
  InsertActiveWebState();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that `setShareEnabled:` is disabled on internal `chrome://` URLs except
// for downloaded and external files.
TEST_F(ToolbarButtonsMediatorTest, TestShareEnabledForChromeURLs) {
  std::unique_ptr<web::FakeWebState> version_web_state = CreateWebState();
  version_web_state->SetVisibleURL(GURL("chrome://version"));
  OCMExpect([consumer_ setShareEnabled:NO]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(version_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);

  std::unique_ptr<web::FakeWebState> downloads_web_state = CreateWebState();
  downloads_web_state->SetVisibleURL(GURL("chrome://downloads/fileName"));
  OCMExpect([consumer_ setShareEnabled:YES]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(downloads_web_state),
      WebStateList::InsertionParams::AtIndex(1).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that selecting a NTP web state updates the consumer with NTP visible
// and isStartSurface status.
TEST_F(ToolbarButtonsMediatorTest, TestWebStateSelectionNTPUpdatesConsumer) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web_state->SetVisibleURL(GURL("chrome://newtab"));
  NewTabPageTabHelper::CreateForWebState(web_state.get());
  NewTabPageTabHelper::FromWebState(web_state.get())
      ->SetShowStartSurface(false);
  OCMExpect([consumer_ setNTPVisible:YES
                      isStartSurface:NO
                           isLoading:NO
                     loadingProgress:0.0]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);

  std::unique_ptr<web::FakeWebState> start_web_state = CreateWebState();
  start_web_state->SetVisibleURL(GURL("chrome://newtab"));
  NewTabPageTabHelper::CreateForWebState(start_web_state.get());
  NewTabPageTabHelper::FromWebState(start_web_state.get())
      ->SetShowStartSurface(true);
  OCMExpect([consumer_ setNTPVisible:YES
                      isStartSurface:YES
                           isLoading:NO
                     loadingProgress:0.0]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(start_web_state),
      WebStateList::InsertionParams::AtIndex(1).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests update of the consumer when the webpage triggers a navigation.
TEST_F(ToolbarButtonsMediatorTest, TestWebStateUpdates) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  OCMExpect([consumer_ setNTPVisible:NO
                      isStartSurface:NO
                           isLoading:YES
                     loadingProgress:0.0]);
  fake_web_state->SetLoading(true);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setNTPVisible:NO
                      isStartSurface:NO
                           isLoading:NO
                     loadingProgress:0.0]);
  fake_web_state->SetLoading(false);
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES animated:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setCanGoForward:YES animated:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES animated:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO animated:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that a deactivated consumer stops getting updates.
TEST_F(ToolbarButtonsMediatorTest, TestDeactivatedConsumerGetsNoUpdates) {
  [mediator_ removeConsumer:consumer_];

  OCMReject([consumer_ setCanGoBack:OCMOCK_ANY]);
  OCMReject([consumer_ setShareEnabled:OCMOCK_ANY]);
  OCMReject([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  InsertActiveWebState();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that a consumer is synced with the current state when it becomes
// active, and only then.
TEST_F(ToolbarButtonsMediatorTest, TestActivationSyncsConsumer) {
  InsertActiveWebState();

  id consumer = OCMProtocolMock(@protocol(ToolbarButtonsConsumer));
  OCMExpect([consumer setCanGoBack:YES]);
  OCMExpect([consumer setCanGoForward:NO animated:NO]);
  OCMExpect([consumer setShareEnabled:YES]);
  OCMExpect([consumer setNTPVisible:NO
                     isStartSurface:NO
                          isLoading:NO
                    loadingProgress:0.0]);
  OCMExpect([consumer setMenu:[OCMArg any]
                forButtonType:ToolbarButtonTypeTabGrid]);
  OCMExpect([consumer updateTabCount:1]);
  OCMExpect([consumer setInTabGroup:NO]);
  [mediator_ addConsumer:consumer];
  EXPECT_OCMOCK_VERIFY(consumer);

  // Activating an active consumer doesn't sync it again.
  OCMReject([consumer setCanGoBack:OCMOCK_ANY]);
  [mediator_ addConsumer:consumer];
  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that removing an inactive consumer, or removing a consumer twice, is a
// no-op, and that a removed consumer is synced again when added back.
TEST_F(ToolbarButtonsMediatorTest, TestRemoveAndAddBackConsumer) {
  InsertActiveWebState();
  id other_consumer = OCMStrictProtocolMock(@protocol(ToolbarButtonsConsumer));
  [mediator_ removeConsumer:other_consumer];
  EXPECT_OCMOCK_VERIFY(other_consumer);

  [mediator_ removeConsumer:consumer_];
  [mediator_ removeConsumer:consumer_];
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  [mediator_ addConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that `-updateConsumer:withWebState:animated:` only updates the given
// consumer, so that snapshotting a toolbar with another WebState doesn't update
// the other active consumers.
TEST_F(ToolbarButtonsMediatorTest, TestUpdateConsumerOnlyUpdatesThatConsumer) {
  InsertActiveWebState();
  id other_consumer = OCMProtocolMock(@protocol(ToolbarButtonsConsumer));
  [mediator_ addConsumer:other_consumer];

  auto snapshot_web_state = std::make_unique<web::FakeWebState>();
  snapshot_web_state->SetVisibleURL(GURL("chrome://version"));
  snapshot_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  snapshot_web_state->SetBrowserState(profile_.get());

  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setShareEnabled:NO]);
  OCMExpect([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  OCMReject([other_consumer setCanGoBack:OCMOCK_ANY]);
  OCMReject([other_consumer setShareEnabled:OCMOCK_ANY]);
  OCMReject([other_consumer setMenu:[OCMArg any]
                      forButtonType:ToolbarButtonTypeTabGrid]);
  [mediator_ updateConsumer:consumer_
               withWebState:snapshot_web_state.get()
                   animated:NO];
  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_OCMOCK_VERIFY(other_consumer);
}

// Tests that `-updateConsumer:withWebState:animated:` does nothing for a
// consumer that isn't active.
TEST_F(ToolbarButtonsMediatorTest, TestUpdateConsumerIgnoresInactiveConsumer) {
  InsertActiveWebState();
  [mediator_ removeConsumer:consumer_];

  OCMReject([consumer_ setCanGoBack:OCMOCK_ANY]);
  OCMReject([consumer_ setShareEnabled:OCMOCK_ANY]);
  OCMReject([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  [mediator_ updateConsumer:consumer_
               withWebState:browser_->GetWebStateList()->GetActiveWebState()
                   animated:NO];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the active consumers get the same menus, built once per update.
TEST_F(ToolbarButtonsMediatorTest, TestMenusBuiltOnceForAllConsumers) {
  id other_consumer = OCMProtocolMock(@protocol(ToolbarButtonsConsumer));
  [mediator_ addConsumer:other_consumer];

  __block UIMenu* menu = nil;
  __block UIMenu* other_menu = nil;
  OCMExpect([consumer_ setMenu:[OCMArg checkWithBlock:^BOOL(id obj) {
                         menu = obj;
                         return YES;
                       }]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  OCMExpect([other_consumer setMenu:[OCMArg checkWithBlock:^BOOL(id obj) {
                              other_menu = obj;
                              return YES;
                            }]
                      forButtonType:ToolbarButtonTypeTabGrid]);
  InsertActiveWebState();
  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_OCMOCK_VERIFY(other_consumer);
  ASSERT_NE(nil, menu);
  EXPECT_EQ(menu, other_menu);
}

// Tests that the mutator handles back action.
TEST_F(ToolbarButtonsMediatorTest, TestMutatorGoBack) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  ASSERT_EQ(
      2, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());

  [mediator_ goBack];

  EXPECT_EQ(
      1, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());
}

// Tests that the mutator handles forward action.
TEST_F(ToolbarButtonsMediatorTest, TestMutatorGoForward) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web_navigation_util::GoBack(fake_web_state);
  ASSERT_EQ(
      1, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());

  [mediator_ goForward];

  EXPECT_EQ(
      2, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());
}

// Tests that back and forward buttons are disabled on the consumer and mutator
// and history-menu navigations are blocked while the active tab is
// `kActorControlled`, then restored when returning to `kInactive`.
TEST_F(ToolbarButtonsMediatorTest,
       TestBackForwardDisabledAndBlockedWhenActorControlled) {
  constexpr char kTestURL0[] = "https://example.com/0";
  constexpr char kTestURL1[] = "https://example.com/1";
  constexpr char kTestURL2[] = "https://example.com/2";
  constexpr int kFirstIndex = 0;
  constexpr int kMiddleIndex = 1;
  constexpr int kLastIndex = 2;
  constexpr int kNoNavigatedIndex = -1;

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL(kTestURL1));
  web_state->SetBrowserState(profile_.get());
  auto nav_manager = std::make_unique<RecordingNavigationManager>();
  RecordingNavigationManager* raw_nav_manager = nav_manager.get();
  nav_manager->AddItem(GURL(kTestURL0), ui::PAGE_TRANSITION_TYPED);
  nav_manager->AddItem(GURL(kTestURL1), ui::PAGE_TRANSITION_TYPED);
  nav_manager->AddItem(GURL(kTestURL2), ui::PAGE_TRANSITION_TYPED);
  nav_manager->SetLastCommittedItemIndex(kMiddleIndex);
  web::NavigationItem* target_item =
      raw_nav_manager->GetItemAtIndex(kLastIndex);
  web_state->SetNavigationManager(std::move(nav_manager));

  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* actor_tab_helper =
      ActorTabHelper::FromWebState(web_state.get());
  ASSERT_NE(nullptr, actor_tab_helper);

  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES animated:NO]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setCanGoForward:NO animated:NO]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_OCMOCK_VERIFY(consumer_);

  [mediator_ goBack];
  EXPECT_EQ(kMiddleIndex, raw_nav_manager->GetLastCommittedItemIndex());

  [mediator_ goForward];
  EXPECT_EQ(kMiddleIndex, raw_nav_manager->GetLastCommittedItemIndex());

  [mediator_ navigateToPageForItem:target_item];
  EXPECT_EQ(kNoNavigatedIndex, raw_nav_manager->last_navigated_index());
  EXPECT_EQ(kMiddleIndex, raw_nav_manager->GetLastCommittedItemIndex());

  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES animated:NO]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kInactive);
  EXPECT_OCMOCK_VERIFY(consumer_);

  [mediator_ goBack];
  EXPECT_EQ(kFirstIndex, raw_nav_manager->GetLastCommittedItemIndex());

  [mediator_ navigateToPageForItem:target_item];
  EXPECT_EQ(kLastIndex, raw_nav_manager->last_navigated_index());
  EXPECT_EQ(kLastIndex, raw_nav_manager->GetLastCommittedItemIndex());
}

// Tests that actor control state changes don't update inactive consumers.
TEST_F(ToolbarButtonsMediatorTest, TestActorControlDoesNotUpdateInactive) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* actor_tab_helper =
      ActorTabHelper::FromWebState(web_state.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  [mediator_ removeConsumer:consumer_];

  OCMReject([consumer_ setActorControlled:OCMOCK_ANY]);
  OCMReject([consumer_ setCanGoBack:OCMOCK_ANY]);
  OCMReject([consumer_ setCanGoForward:OCMOCK_ANY animated:OCMOCK_ANY]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that `ToolbarButtonsMediator` pushes `setActorControlled:` to active
// consumers when the active tab's actor control state changes and resets it to
// `NO` when the active tab is closed.
TEST_F(ToolbarButtonsMediatorTest, TestActorControlledUpdatesConsumer) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* actor_tab_helper =
      ActorTabHelper::FromWebState(web_state.get());
  ASSERT_NE(nullptr, actor_tab_helper);

  OCMExpect([consumer_ setActorControlled:NO]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setActorControlled:YES]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setActorControlled:NO]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kInactive);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setActorControlled:YES]);
  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Closing the active tab clears `actorControlled` on the consumer.
  fake_navigation_manager_ = nullptr;
  OCMExpect([consumer_ setActorControlled:NO]);
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the mutator handles reload action.
TEST_F(ToolbarButtonsMediatorTest, TestMutatorReload) {
  InsertActiveWebState();
  ASSERT_FALSE(fake_navigation_manager_->ReloadWasCalled());

  [mediator_ reload];

  EXPECT_TRUE(fake_navigation_manager_->ReloadWasCalled());
}

// Tests that the mutator handles stop action.
TEST_F(ToolbarButtonsMediatorTest, TestMutatorStop) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  ASSERT_FALSE(fake_web_state->was_stopped());

  [mediator_ stop];

  EXPECT_TRUE(fake_web_state->was_stopped());
}

// Tests the user actions recorded when the tools menu is tapped.
TEST_F(ToolbarButtonsMediatorTest, TestRecordUserActionsForToolsMenuTapped) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  [mediator_ recordUserActionsForToolsMenuTapped];
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileToolbarShowMenu"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("MobileToolbarShowMenuOnNTP"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "MobileToolbarShowMenuFromReaderMode"));

  fake_web_state->SetVisibleURL(GURL("chrome://newtab"));
  [mediator_ recordUserActionsForToolsMenuTapped];
  EXPECT_EQ(2, user_action_tester.GetActionCount("MobileToolbarShowMenu"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileToolbarShowMenuOnNTP"));
}

// Tests that assistantButtonTapped: calls geminiHandler to start entry flow.
TEST_F(ToolbarButtonsMediatorTest, TestAssistantButtonTapped) {
  id mock_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  mediator_.geminiHandler = mock_gemini_handler;

  gemini_browser_agent_->SetContainerInvoked(false);
  OCMExpect([mock_gemini_handler
      startGeminiEntryFlowWithStartupState:[OCMArg any]
                        baseViewController:nil
                  showSnackbarOnCompletion:YES
                                completion:nil]);
  [mediator_ assistantButtonTapped];

  // The floaty is dismissed if the assistant button is tapped while it is
  // showing.
  gemini_browser_agent_->SetContainerInvoked(true);
  OCMExpect([mock_gemini_handler dismissGeminiFlowWithCompletion:nil]);
  [mediator_ assistantButtonTapped];

  EXPECT_OCMOCK_VERIFY(mock_gemini_handler);
}

// Tests that assistantButtonTapped: in incognito delegates to the coordinator.
TEST_F(ToolbarButtonsMediatorTest, TestAssistantButtonTappedInIncognito) {
  ToolbarButtonsMediator* incognito_mediator =
      CreateMediator(/*incognito=*/YES);
  id mock_delegate = OCMProtocolMock(@protocol(ToolbarButtonsMediatorDelegate));
  incognito_mediator.delegate = mock_delegate;

  OCMExpect([mock_delegate
      toolbarButtonsMediatorDidTapAssistantInIncognito:incognito_mediator]);
  [incognito_mediator assistantButtonTapped];
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [incognito_mediator disconnect];
}

// Tests that the TabGrid button menu's "New Incognito Tab" action is disabled
// when incognito mode is disabled by policy.
TEST_F(ToolbarButtonsMediatorTest, TestTabGridMenu_IncognitoDisabled) {
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));
  InsertActiveWebState();

  UIMenu* menu = TabGridMenuOnActivation(mediator_);
  ASSERT_NE(nil, menu);
  // "New Tab", "New Incognito Tab" (disabled) and "Close Current Tab".
  ASSERT_EQ(3U, menu.children.count);

  UIAction* open_new_tab_action = (UIAction*)menu.children[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_TAB),
              open_new_tab_action.title);

  UIAction* open_new_incognito_tab_action = (UIAction*)menu.children[1];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB),
              open_new_incognito_tab_action.title);
  EXPECT_EQ(UIMenuElementAttributesDisabled,
            open_new_incognito_tab_action.attributes);

  UIAction* close_tab_action = (UIAction*)menu.children[2];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLOSE_TAB),
              close_tab_action.title);
}

// Tests that the TabGrid button menu contains "Close Tab", "New Tab", and "New
// Incognito Tab" actions, and that none are disabled when incognito is allowed.
TEST_F(ToolbarButtonsMediatorTest, TestTabGridMenu_IncognitoEnabled) {
  InsertActiveWebState();

  UIMenu* menu = TabGridMenuOnActivation(mediator_);
  ASSERT_NE(nil, menu);
  ASSERT_EQ(3U, menu.children.count);

  UIAction* open_new_tab_action = (UIAction*)menu.children[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_TAB),
              open_new_tab_action.title);
  EXPECT_EQ(0U, open_new_tab_action.attributes);

  UIAction* open_new_incognito_tab_action = (UIAction*)menu.children[1];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB),
              open_new_incognito_tab_action.title);
  EXPECT_EQ(0U, open_new_incognito_tab_action.attributes);

  UIAction* close_tab_action = (UIAction*)menu.children[2];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLOSE_TAB),
              close_tab_action.title);
  EXPECT_EQ(UIMenuElementAttributesDestructive, close_tab_action.attributes);
}

// Tests that the assistant button is visible and enabled when signed in and
// location is eligible (not EEA / Japan), and PageActionMenu is enabled.
TEST_F(ToolbarButtonsMediatorTest,
       TestAssistantButtonVisible_PageActionMenuEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  InsertActiveWebState();
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonVisible:YES enabled:YES]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is visible and enabled for signed-out users
// when location is eligible and sign-in is allowed.
TEST_F(ToolbarButtonsMediatorTest, TestAssistantButtonVisible_SignedOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  InsertActiveWebState();

  OCMExpect([consumer_ setAssistantButtonVisible:YES enabled:YES]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button remains enabled on NTP web pages when
// visible.
TEST_F(ToolbarButtonsMediatorTest, TestAssistantButtonVisible_NTP) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  std::unique_ptr<web::FakeWebState> ntp_web_state = CreateWebState();
  ntp_web_state->SetVisibleURL(GURL("chrome://newtab"));
  NewTabPageTabHelper::CreateForWebState(ntp_web_state.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(ntp_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonVisible:YES enabled:YES]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is NOT visible when the country is in the
// EEA (e.g., France), even if PageActionMenu is enabled.
TEST_F(ToolbarButtonsMediatorTest,
       TestAssistantButtonNotVisible_EEACountryGated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  scoped_variations_service_.Get()->OverrideStoredPermanentCountry("fr");
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->SetTag(
      base::i18n::GetKnownLanguageTag("en-US"));
  InsertActiveWebState();
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonVisible:NO enabled:NO]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is NOT visible when the country is Japan
// ("jp"), even if PageActionMenu is enabled.
TEST_F(ToolbarButtonsMediatorTest,
       TestAssistantButtonNotVisible_JapanCountryGated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  scoped_variations_service_.Get()->OverrideStoredPermanentCountry("jp");
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->SetTag(
      base::i18n::GetKnownLanguageTag("en-US"));
  InsertActiveWebState();
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonVisible:NO enabled:NO]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the assistant button is visible and enabled in incognito when
// Gemini is eligible for the original profile.
TEST_F(ToolbarButtonsMediatorTest, TestAssistantButtonVisibleInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {});
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web_state->SetBrowserState(otr_profile);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  SignInAndSetCapability(true);

  OCMExpect([consumer_ setAssistantButtonVisible:YES enabled:YES]);
  [mediator_ geminiAvailabilityChanged:YES];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

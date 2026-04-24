// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/banner_promo/model/fake_default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_histograms.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

MenuScenarioHistogram kTestMenuScenario = kMenuScenarioHistogramToolbarMenu;

}  // namespace

// Fixture for testing ToolbarMediator.
class ToolbarMediatorTest : public PlatformTest,
                            public testing::WithParamInterface<BOOL> {
 protected:
  ToolbarMediatorTest() {
    scoped_feature_list_.InitAndEnableFeature(kChromeNextIa);
    mock_app_agent_ = OCMClassMock([DefaultBrowserBannerPromoAppAgent class]);
    settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());

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

    BrowserActionFactory* action_factory_ =
        [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                             scenario:kTestMenuScenario];
    mediator_ = [[ToolbarMediator alloc]
                initWithWebStateList:browser_->GetWebStateList()
                       actionFactory:action_factory_
                fullscreenController:TestFullscreenController::FromBrowser(
                                         browser_.get())
                         topPosition:GetParam()
        defaultBrowserBannerAppAgent:GetParam() ? mock_app_agent_ : nil];
    mediator_.navigationBrowserAgent =
        WebNavigationBrowserAgent::FromBrowser(browser_.get());
    mediator_.settingsHandler = settings_handler_;

    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
    [mediator_ setConsumer:consumer_];
  }

  // Returns a new fake web state and set the fake navigation manager to the
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

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ToolbarMediator* mediator_;
  id consumer_;
  id page_side_swipe_handler_;
  id scene_handler_;
  id browser_coordinator_handler_;
  id mock_app_agent_;
  id settings_handler_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
};

// Tests that selecting a web state updates the consumer.
TEST_P(ToolbarMediatorTest, TestWebStateSelectionUpdatesConsumer) {
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO]);
  OCMExpect([consumer_ setShareEnabled:YES]);
  OCMExpect([consumer_ setIsLoading:NO]);

  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests update of the consumer when the webpage triggers a navigation.
TEST_P(ToolbarMediatorTest, TestWebStateUpdates) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Test loading state.
  OCMExpect([consumer_ setIsLoading:YES]);
  fake_web_state->SetLoading(true);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setIsLoading:NO]);
  fake_web_state->SetLoading(false);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Test back-forward state.
  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the mutator handles back action.
TEST_P(ToolbarMediatorTest, TestMutatorGoBack) {
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
TEST_P(ToolbarMediatorTest, TestMutatorGoForward) {
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

// Tests that the mutator handles reload action.
TEST_P(ToolbarMediatorTest, TestMutatorReload) {
  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_FALSE(fake_navigation_manager_->ReloadWasCalled());

  [mediator_ reload];

  EXPECT_TRUE(fake_navigation_manager_->ReloadWasCalled());
}

// Tests that the mutator handles stop action.
TEST_P(ToolbarMediatorTest, TestMutatorStop) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_FALSE(fake_web_state->was_stopped());

  [mediator_ stop];

  EXPECT_TRUE(fake_web_state->was_stopped());
}

// Tests that the consumer is updated when the bottom omnibox pref changes.
TEST_P(ToolbarMediatorTest, TestBottomOmniboxEnabled) {
  if (!IsBottomOmniboxAvailable()) {
    return;
  }
  BOOL top_position = GetParam();

  OCMExpect([consumer_ setVisible:top_position]);
  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      omnibox::kIsOmniboxInBottomPosition, false);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setVisible:!top_position]);
  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      omnibox::kIsOmniboxInBottomPosition, true);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is not updated when the bottom omnibox pref changes
// if the bottom toolbar is not available.
TEST_P(ToolbarMediatorTest, TestBottomOmniboxNotEnabled) {
  if (IsBottomOmniboxAvailable()) {
    return;
  }

  OCMReject([consumer_ setVisible:YES]).ignoringNonObjectArgs();

  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      omnibox::kIsOmniboxInBottomPosition, false);
  TestingApplicationContext::GetGlobal()->GetLocalState()->SetBoolean(
      omnibox::kIsOmniboxInBottomPosition, true);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that displayPromoFromAppAgent: calls showBannerPromo on the consumer.
TEST_P(ToolbarMediatorTest, TestDisplayPromo) {
  if (!GetParam()) {
    // Promo is only supported on top position.
    return;
  }
  FakeDefaultBrowserBannerPromoAppAgent* fake_app_agent =
      [[FakeDefaultBrowserBannerPromoAppAgent alloc] init];

  BrowserActionFactory* action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                           scenario:kTestMenuScenario];

  ToolbarMediator* local_mediator = [[ToolbarMediator alloc]
              initWithWebStateList:browser_->GetWebStateList()
                     actionFactory:action_factory
              fullscreenController:TestFullscreenController::FromBrowser(
                                       browser_.get())
                       topPosition:GetParam()
      defaultBrowserBannerAppAgent:fake_app_agent];

  id local_consumer = OCMProtocolMock(@protocol(ToolbarConsumer));
  [local_mediator setConsumer:local_consumer];

  OCMExpect([local_consumer showBannerPromo]);

  [fake_app_agent forceDisplayPromo];

  EXPECT_OCMOCK_VERIFY(local_consumer);
  [local_mediator disconnect];
}

// Tests that hidePromoFromAppAgent: calls hideBannerPromo on the consumer.
TEST_P(ToolbarMediatorTest, TestHidePromo) {
  if (!GetParam()) {
    // Promo is only supported on top position.
    return;
  }
  FakeDefaultBrowserBannerPromoAppAgent* fake_app_agent =
      [[FakeDefaultBrowserBannerPromoAppAgent alloc] init];

  BrowserActionFactory* action_factory =
      [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                           scenario:kTestMenuScenario];

  ToolbarMediator* local_mediator = [[ToolbarMediator alloc]
              initWithWebStateList:browser_->GetWebStateList()
                     actionFactory:action_factory
              fullscreenController:TestFullscreenController::FromBrowser(
                                       browser_.get())
                       topPosition:GetParam()
      defaultBrowserBannerAppAgent:fake_app_agent];

  id local_consumer = OCMProtocolMock(@protocol(ToolbarConsumer));
  [local_mediator setConsumer:local_consumer];

  OCMExpect([local_consumer hideBannerPromo]);

  [fake_app_agent forceHidePromo];

  EXPECT_OCMOCK_VERIFY(local_consumer);
  [local_mediator disconnect];
}

// Tests that bannerPromoWasTapped: calls settingsHandler and appAgent.
TEST_P(ToolbarMediatorTest, TestBannerPromoWasTapped) {
  if (!GetParam()) {
    // Promo is only supported on top position.
    return;
  }
  OCMExpect([settings_handler_
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kBannerPromo]);
  OCMExpect([mock_app_agent_ promoTapped]);

  [mediator_ bannerPromoWasTapped:nil];

  EXPECT_OCMOCK_VERIFY(settings_handler_);
  EXPECT_OCMOCK_VERIFY(mock_app_agent_);
}

// Tests that bannerPromoCloseButtonWasTapped: calls appAgent.
TEST_P(ToolbarMediatorTest, TestBannerPromoCloseButtonWasTapped) {
  if (!GetParam()) {
    // Promo is only supported on top position.
    return;
  }
  OCMExpect([mock_app_agent_ promoCloseButtonTapped]);

  [mediator_ bannerPromoCloseButtonWasTapped:nil];

  EXPECT_OCMOCK_VERIFY(mock_app_agent_);
}

// Tests that tabGroupIndicatorVisibilityUpdated: updates the app agent.
TEST_P(ToolbarMediatorTest, TestTabGroupIndicatorVisibilityUpdated) {
  if (!GetParam()) {
    // Promo is only supported on top position.
    return;
  }
  OCMExpect([mock_app_agent_ setUICurrentlySupportsPromo:NO]);
  [mediator_ tabGroupIndicatorVisibilityUpdated:YES];
  EXPECT_OCMOCK_VERIFY(mock_app_agent_);

  OCMExpect([mock_app_agent_ setUICurrentlySupportsPromo:YES]);
  [mediator_ tabGroupIndicatorVisibilityUpdated:NO];
  EXPECT_OCMOCK_VERIFY(mock_app_agent_);
}

INSTANTIATE_TEST_SUITE_P(ToolbarMediatorTest,
                         ToolbarMediatorTest,
                         testing::Bool());

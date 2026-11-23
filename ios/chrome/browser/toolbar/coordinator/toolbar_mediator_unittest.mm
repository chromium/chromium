// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/i18n/language_tag.h"
#import "base/test/scoped_feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/banner_promo/model/fake_default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/menu/public/menu_histograms.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/voice_search_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/toolbar/coordinator/buttons/toolbar_buttons_mediator.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Mirrors `MainToolbarCoordinator`: only the toolbar with the omnibox is an
// active consumer of the buttons mediator.
@interface FakeToolbarMediatorDelegate : NSObject <ToolbarMediatorDelegate>
@property(nonatomic, weak) ToolbarButtonsMediator* buttonsMediator;
@end

@implementation FakeToolbarMediatorDelegate

- (void)toolbarMediator:(ToolbarMediator*)mediator
    didUpdateHasOmnibox:(BOOL)hasOmnibox {
  if (hasOmnibox) {
    [self.buttonsMediator addConsumer:mediator.consumer];
  } else {
    [self.buttonsMediator removeConsumer:mediator.consumer];
  }
}

@end

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

    voice_search_handler_ = OCMProtocolMock(@protocol(VoiceSearchCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:voice_search_handler_
                     forProtocol:@protocol(VoiceSearchCommands)];

    page_side_swipe_handler_ =
        OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:page_side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];

    BrowserActionFactory* action_factory_ =
        [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                             scenario:kTestMenuScenario];

    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    GeminiBrowserAgent::CreateForBrowser(browser_.get());
    gemini_browser_agent_ = GeminiBrowserAgent::FromBrowser(browser_.get());

    buttons_mediator_ = [[ToolbarButtonsMediator alloc]
            initWithIncognito:NO
                 webStateList:browser_->GetWebStateList()
                actionFactory:action_factory_
                      profile:profile_.get()
                  prefService:profile_->GetPrefs()
        authenticationService:auth_service_
           geminiBrowserAgent:gemini_browser_agent_];
    buttons_mediator_.navigationBrowserAgent =
        WebNavigationBrowserAgent::FromBrowser(browser_.get());
    delegate_ = [[FakeToolbarMediatorDelegate alloc] init];
    delegate_.buttonsMediator = buttons_mediator_;

    mediator_ = [[ToolbarMediator alloc]
                initWithWebStateList:browser_->GetWebStateList()
                fullscreenController:TestFullscreenController::FromBrowser(
                                         browser_.get())
              fullscreenBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                         browser_.get())
                         topPosition:GetParam()
        defaultBrowserBannerAppAgent:GetParam() ? mock_app_agent_ : nil];
    mediator_.settingsHandler = settings_handler_;
    mediator_.delegate = delegate_;

    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
    OCMStub([consumer_ updateTabCount:0]).ignoringNonObjectArgs();
    OCMStub([consumer_ setInTabGroup:NO]).ignoringNonObjectArgs();
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

  void SetLocationEligible(bool eligible) {
    if (eligible) {
      scoped_variations_service_.Get()->OverrideStoredPermanentCountry("us");
      TestingApplicationContext::GetGlobal()
          ->GetApplicationLocaleStorage()
          ->SetTag(base::i18n::GetKnownLanguageTag("en-US"));
    } else {
      scoped_variations_service_.Get()->OverrideStoredPermanentCountry("fr");
      TestingApplicationContext::GetGlobal()
          ->GetApplicationLocaleStorage()
          ->SetTag(base::i18n::GetKnownLanguageTag("fr-FR"));
    }
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    [buttons_mediator_ disconnect];
    buttons_mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  IOSChromeScopedTestingVariationsService scoped_variations_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ToolbarButtonsMediator* buttons_mediator_;
  FakeToolbarMediatorDelegate* delegate_;
  ToolbarMediator* mediator_;
  id consumer_;
  id page_side_swipe_handler_;
  id scene_handler_;
  id browser_coordinator_handler_;
  id voice_search_handler_;
  id mock_app_agent_;
  id settings_handler_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<GeminiBrowserAgent> gemini_browser_agent_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
};

// Tests that changing BrowserLayoutState.toolbarPosition updates the consumer's
// hasOmnibox state accordingly.
TEST_P(ToolbarMediatorTest, TestBrowserLayoutStateUpdatesHasOmnibox) {
  BrowserLayoutState* layout_state = browser_->GetBrowserLayoutState();
  BOOL top_position = GetParam();

  OCMExpect([consumer_ setHasOmnibox:top_position]);
  mediator_.browserLayoutState = layout_state;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Transition toolbar to bottom.
  OCMExpect([consumer_ setHasOmnibox:!top_position]);
  [layout_state setToolbarPosition:ToolbarPosition::kBottom
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Transition toolbar back to top.
  OCMExpect([consumer_ setHasOmnibox:top_position]);
  [layout_state setToolbarPosition:ToolbarPosition::kTop
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the delegate is told when the toolbar gets or loses the omnibox,
// which (de)activates the toolbar in `ToolbarButtonsMediator`.
TEST_P(ToolbarMediatorTest, TestHasOmniboxUpdatesNotifyDelegate) {
  BrowserLayoutState* layout_state = browser_->GetBrowserLayoutState();
  BOOL top_position = GetParam();
  id delegate = OCMProtocolMock(@protocol(ToolbarMediatorDelegate));
  mediator_.delegate = delegate;

  OCMExpect([delegate toolbarMediator:mediator_
                  didUpdateHasOmnibox:top_position]);
  mediator_.browserLayoutState = layout_state;
  EXPECT_OCMOCK_VERIFY(delegate);

  OCMExpect([delegate toolbarMediator:mediator_
                  didUpdateHasOmnibox:!top_position]);
  [layout_state setToolbarPosition:ToolbarPosition::kBottom
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  EXPECT_OCMOCK_VERIFY(delegate);

  OCMExpect([delegate toolbarMediator:mediator_
                  didUpdateHasOmnibox:top_position]);
  [layout_state setToolbarPosition:ToolbarPosition::kTop
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that consumer updates are skipped while a toolbar does not have the
// omnibox, except for NTP visibility on the top toolbar.
TEST_P(ToolbarMediatorTest, TestInactiveToolbarSkipsConsumerUpdates) {
  BrowserLayoutState* layout_state = browser_->GetBrowserLayoutState();
  BOOL top_position = GetParam();
  ToolbarPosition inactive_position =
      top_position ? ToolbarPosition::kBottom : ToolbarPosition::kTop;

  [layout_state setToolbarPosition:inactive_position
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  OCMExpect([consumer_ setHasOmnibox:NO]);
  mediator_.browserLayoutState = layout_state;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Insert and activate a WebState while this toolbar is inactive. Button and
  // menu updates should be skipped, while the top toolbar still updates NTP
  // visibility.
  if (top_position) {
    OCMExpect([consumer_ setNTPVisible:NO
                        isStartSurface:NO
                             isLoading:NO
                       loadingProgress:0]);
  } else {
    OCMReject([consumer_ setNTPVisible:OCMOCK_ANY
                        isStartSurface:OCMOCK_ANY
                             isLoading:OCMOCK_ANY
                       loadingProgress:0]);
  }
  OCMReject([consumer_ setCanGoBack:OCMOCK_ANY]);
  OCMReject([consumer_ setShareEnabled:OCMOCK_ANY]);
  OCMReject([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that consumer state is synced immediately when the toolbar position
// switches to make the toolbar active.
TEST_P(ToolbarMediatorTest,
       TestSwitchingToolbarPositionSyncsNewlyActiveConsumer) {
  BrowserLayoutState* layout_state = browser_->GetBrowserLayoutState();
  BOOL top_position = GetParam();
  ToolbarPosition inactive_position =
      top_position ? ToolbarPosition::kBottom : ToolbarPosition::kTop;
  ToolbarPosition active_position =
      top_position ? ToolbarPosition::kTop : ToolbarPosition::kBottom;

  [layout_state setToolbarPosition:inactive_position
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  OCMExpect([consumer_ setHasOmnibox:NO]);
  mediator_.browserLayoutState = layout_state;
  EXPECT_OCMOCK_VERIFY(consumer_);

  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());

  // Switching the toolbar position to make this toolbar active should
  // immediately sync the consumer state.
  OCMExpect([consumer_ setHasOmnibox:YES]);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setShareEnabled:YES]);
  OCMExpect([consumer_ setMenu:[OCMArg any]
                 forButtonType:ToolbarButtonTypeTabGrid]);
  [layout_state setToolbarPosition:active_position
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
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

  ToolbarMediator* local_mediator = [[ToolbarMediator alloc]
              initWithWebStateList:browser_->GetWebStateList()
              fullscreenController:TestFullscreenController::FromBrowser(
                                       browser_.get())
            fullscreenBrowserAgent:nil
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

  ToolbarMediator* local_mediator = [[ToolbarMediator alloc]
              initWithWebStateList:browser_->GetWebStateList()
              fullscreenController:TestFullscreenController::FromBrowser(
                                       browser_.get())
            fullscreenBrowserAgent:nil
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

// Tests that keyboard notifications force fullscreen mode for the bottom
// toolbar when `kFullscreenRefactoring` is enabled.
TEST_P(ToolbarMediatorTest, TestKeyboardForcesFullscreenWhenRefactored) {
  if (GetParam() || !IsBottomOmniboxAvailable()) {
    // Only the bottom toolbar pins above the keyboard, and bottom omnibox is
    // not available on iPad.
    return;
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kFullscreenRefactoring);

  BrowserLayoutState* layout_state = browser_->GetBrowserLayoutState();
  [layout_state setToolbarPosition:ToolbarPosition::kBottom
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  mediator_.browserLayoutState = layout_state;

  id mock_fullscreen_commands = OCMProtocolMock(@protocol(FullscreenCommands));
  mediator_.fullscreenCommands = mock_fullscreen_commands;

  id mock_web_view_proxy = OCMProtocolMock(@protocol(CRWWebViewProxy));
  OCMStub([mock_web_view_proxy isKeyboardVisible]).andReturn(YES);

  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web_state->SetWebViewProxy(mock_web_view_proxy);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  OCMExpect([mock_fullscreen_commands
      forceFullscreen:YES
              feature:ForceFullscreenFeature::kKeyboard]);
  OCMExpect([consumer_ setLocationIndicatorVisible:YES
                                   forNotification:[OCMArg any]]);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil];
  EXPECT_OCMOCK_VERIFY(mock_fullscreen_commands);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([mock_fullscreen_commands
      forceFullscreen:NO
              feature:ForceFullscreenFeature::kKeyboard]);
  OCMExpect([consumer_ setLocationIndicatorVisible:NO
                                   forNotification:[OCMArg any]]);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillHideNotification
                    object:nil];
  EXPECT_OCMOCK_VERIFY(mock_fullscreen_commands);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Re-show the keyboard, then move the toolbar position to `kTop` while the
  // keyboard is still visible; forced fullscreen should be exited immediately.
  OCMExpect([mock_fullscreen_commands
      forceFullscreen:YES
              feature:ForceFullscreenFeature::kKeyboard]);
  OCMExpect([consumer_ setLocationIndicatorVisible:YES
                                   forNotification:[OCMArg any]]);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil];
  EXPECT_OCMOCK_VERIFY(mock_fullscreen_commands);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([mock_fullscreen_commands
      forceFullscreen:NO
              feature:ForceFullscreenFeature::kKeyboard]);
  OCMExpect([consumer_ setLocationIndicatorVisible:NO forNotification:nil]);
  OCMExpect([consumer_ setHasOmnibox:NO]);
  [layout_state setToolbarPosition:ToolbarPosition::kTop
                           passKey:layout_state::LayoutStateTestPassKeyFactory::
                                       CreateToolbarKey()];
  EXPECT_OCMOCK_VERIFY(mock_fullscreen_commands);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

INSTANTIATE_TEST_SUITE_P(ToolbarMediatorTest,
                         ToolbarMediatorTest,
                         testing::Bool());

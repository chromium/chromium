// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_mediator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/providers/voice_search/test_voice_search.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

@interface TestAdaptiveToolbarMediator
    : AdaptiveToolbarMediator <CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestAdaptiveToolbarMediator
@end

namespace {

MenuScenarioHistogram kTestMenuScenario = kMenuScenarioHistogramHistoryEntry;

static const int kNumberOfWebStates = 3;
static const char kTestUrl[] = "http://www.chromium.org";

class AdaptiveToolbarMediatorTest : public PlatformTest {
 public:
  AdaptiveToolbarMediatorTest() {
    ios::provider::test::SetVoiceSearchEnabled(false);

    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    test_browser_ = std::make_unique<TestBrowser>(profile_.get());
    WebNavigationBrowserAgent::CreateForBrowser(test_browser_.get());

    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    test_web_state_ = std::make_unique<web::FakeWebState>();
    test_web_state_->SetBrowserState(profile_.get());
    test_web_state_->SetNavigationManager(std::move(navigation_manager));
    test_web_state_->SetLoading(true);
    web_state_ = test_web_state_.get();
    mediator_ = [[TestAdaptiveToolbarMediator alloc] init];
    mediator_.navigationBrowserAgent =
        WebNavigationBrowserAgent::FromBrowser(test_browser_.get());
    mediator_.actionFactory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];
    mediator_.templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
    strict_consumer_ = OCMStrictProtocolMock(@protocol(ToolbarConsumer));
    SetUpWebStateList();

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_handler_
                     forProtocol:@protocol(SettingsCommands)];

    mock_browser_coordinator_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_commands_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    mock_qr_scanner_commands_handler_ =
        OCMStrictProtocolMock(@protocol(QRScannerCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_qr_scanner_commands_handler_
                     forProtocol:@protocol(QRScannerCommands)];

    mock_load_query_commands_handler_ =
        OCMStrictProtocolMock(@protocol(LoadQueryCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_load_query_commands_handler_
                     forProtocol:@protocol(LoadQueryCommands)];

    [[UIPasteboard generalPasteboard] setItems:@[]];

    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
  }

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~AdaptiveToolbarMediatorTest() override {
    ios::provider::test::SetVoiceSearchEnabled(false);

    ClipboardRecentContent::SetInstance(nullptr);

    [mediator_ disconnect];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  void SetUpWebStateList() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(std::move(test_web_state_),
                                    WebStateList::InsertionParams::AtIndex(0));
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    GURL url("http://test/" + base::NumberToString(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(
        std::move(web_state), WebStateList::InsertionParams::AtIndex(index));
  }

  void SetUpActiveWebState() { web_state_list_->ActivateWebStateAt(0); }

  TestAdaptiveToolbarMediator* mediator_;
  std::unique_ptr<TestBrowser> test_browser_;
  raw_ptr<web::FakeWebState> web_state_;
  raw_ptr<ToolbarTestNavigationManager> navigation_manager_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id consumer_;
  id strict_consumer_;
  id mock_application_commands_handler_;
  id mock_settings_commands_handler_;
  id mock_browser_coordinator_commands_handler_;
  id mock_qr_scanner_commands_handler_;
  id mock_load_query_commands_handler_;
  std::unique_ptr<TestProfileIOS> profile_;

 private:
  std::unique_ptr<web::FakeWebState> test_web_state_;
};

// Test no setup is being done on the Toolbar if there's no Webstate.
TEST_F(AdaptiveToolbarMediatorTest, TestToolbarSetupWithNoWebstate) {
  mediator_.consumer = consumer_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setLoadingState:YES];
}

// Test no setup is being done on the Toolbar if there's no active Webstate.
TEST_F(AdaptiveToolbarMediatorTest, TestToolbarSetupWithNoActiveWebstate) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setLoadingState:YES];
}

// Test no WebstateList related setup is being done on the Toolbar if there's no
// WebstateList.
TEST_F(AdaptiveToolbarMediatorTest, TestToolbarSetupWithNoWebstateList) {
  mediator_.consumer = consumer_;

  [[[consumer_ reject] ignoringNonObjectArgs] setTabCount:0
                                        addedInBackground:NO];
}

// Tests the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set.
TEST_F(AdaptiveToolbarMediatorTest, TestToolbarSetup) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setLoadingState:YES];
  [[consumer_ verify] setShareMenuEnabled:NO];
}

// Tests the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set in reverse order.
TEST_F(AdaptiveToolbarMediatorTest, TestToolbarSetupReverse) {
  mediator_.consumer = consumer_;
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setLoadingState:YES];
  [[consumer_ verify] setShareMenuEnabled:NO];
}

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set.
TEST_F(AdaptiveToolbarMediatorTest, TestWebstateListRelatedSetup) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  [[consumer_ verify] setTabCount:3 addedInBackground:NO];
}

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set in reverse order.
TEST_F(AdaptiveToolbarMediatorTest, TestWebstateListRelatedSetupReverse) {
  mediator_.consumer = consumer_;
  mediator_.webStateList = web_state_list_.get();

  [[consumer_ verify] setTabCount:3 addedInBackground:NO];
}

// Tests the Toolbar is updated when the Webstate observer method
// DidStartLoading is triggered by SetLoading.
TEST_F(AdaptiveToolbarMediatorTest, TestDidStartLoading) {
  // Change the default loading state to false to verify the Webstate
  // callback with true.
  web_state_->SetLoading(false);
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  web_state_->SetLoading(true);
  [[consumer_ verify] setLoadingState:YES];
}

// Tests the Toolbar is updated when the Webstate observer method DidStopLoading
// is triggered by SetLoading.
TEST_F(AdaptiveToolbarMediatorTest, TestDidStopLoading) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  web_state_->SetLoading(false);
  [[consumer_ verify] setLoadingState:NO];
}

// Tests the Toolbar is not updated when the Webstate observer method
// DidStartLoading is triggered by SetLoading on the NTP.
TEST_F(AdaptiveToolbarMediatorTest, TestDidStartLoadingNTP) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  web_state_->SetLoading(false);
  web_state_->SetVisibleURL(GURL(kChromeUINewTabURL));
  web_state_->SetLoading(true);
  [[consumer_ verify] setLoadingState:NO];
}

// Tests the Toolbar is updated when the Webstate observer method
// DidLoadPageWithSuccess is triggered by OnPageLoaded.
TEST_F(AdaptiveToolbarMediatorTest, TestDidLoadPageWithSucess) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Tests the Toolbar is updated when the Webstate observer method
// didFinishNavigation is called.
TEST_F(AdaptiveToolbarMediatorTest, TestDidFinishNavigation) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Tests the Toolbar is updated when the Webstate observer method
// didChangeVisibleSecurityState is called.
TEST_F(AdaptiveToolbarMediatorTest, TestDidChangeVisibleSecurityState) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web_state_->OnVisibleSecurityStateChanged();

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Tests the Toolbar is updated when the Webstate observer method
// didChangeLoadingProgress is called.
TEST_F(AdaptiveToolbarMediatorTest, TestLoadingProgress) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  [mediator_ webState:web_state_ didChangeLoadingProgress:0.42];
  [[consumer_ verify] setLoadingProgressFraction:0.42];
}

// Tests the Toolbar is updated when Webstate observer method
// didChangeBackForwardState is called.
TEST_F(AdaptiveToolbarMediatorTest, TestDidChangeBackForwardState) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->OnBackForwardStateChanged();

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
}

// Test that increasing the number of Webstates will update the consumer with
// the right value.
TEST_F(AdaptiveToolbarMediatorTest, TestIncreaseNumberOfWebstates) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  InsertNewWebState(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates + 1 addedInBackground:YES];
}

// Test that decreasing the number of Webstates will update the consumer with
// the right value.
TEST_F(AdaptiveToolbarMediatorTest, TestDecreaseNumberOfWebstates) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  web_state_list_->DetachWebStateAt(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates - 1 addedInBackground:NO];
}

// Test that consumer is informed that voice search is enabled.
TEST_F(AdaptiveToolbarMediatorTest, TestVoiceSearchProviderEnabled) {
  ios::provider::test::SetVoiceSearchEnabled(true);

  OCMExpect([consumer_ setVoiceSearchEnabled:YES]);
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that consumer is informed that voice search is not enabled.
TEST_F(AdaptiveToolbarMediatorTest, TestVoiceSearchProviderNotEnabled) {
  ios::provider::test::SetVoiceSearchEnabled(false);

  OCMExpect([consumer_ setVoiceSearchEnabled:NO]);
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that updating the consumer for a specific webState works.
TEST_F(AdaptiveToolbarMediatorTest, TestUpdateConsumerForWebState) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();
  navigation_manager->set_can_go_forward(true);
  navigation_manager->set_can_go_back(true);
  std::unique_ptr<web::FakeWebState> test_web_state =
      std::make_unique<web::FakeWebState>();
  test_web_state->SetNavigationManager(std::move(navigation_manager));
  test_web_state->SetCurrentURL(GURL(kTestUrl));
  test_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  OCMExpect([consumer_ setCanGoForward:YES]);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setShareMenuEnabled:YES]);

  [mediator_ updateConsumerForWebState:test_web_state.get()];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests the menu elements.
TEST_F(AdaptiveToolbarMediatorTest, MenuElements) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();

  UIMenu* new_tab_menu =
      [mediator_ menuForButtonOfType:AdaptiveToolbarButtonTypeNewTab];

  ASSERT_EQ(4U, new_tab_menu.children.count);
  for (UIMenuElement* element in new_tab_menu.children) {
    ASSERT_TRUE([element isKindOfClass:[UIAction class]]);
    UIAction* action = (UIAction*)element;
    EXPECT_EQ(0U, action.attributes);
  }

  UIMenu* tab_grid_menu =
      [mediator_ menuForButtonOfType:AdaptiveToolbarButtonTypeTabGrid];

  ASSERT_EQ(3U, tab_grid_menu.children.count);

  ASSERT_TRUE([tab_grid_menu.children[0] isKindOfClass:[UIAction class]]);
  UIAction* close_tab = (UIAction*)tab_grid_menu.children[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLOSE_TAB),
              close_tab.title);
  EXPECT_EQ(UIMenuElementAttributesDestructive, close_tab.attributes);

  ASSERT_TRUE([tab_grid_menu.children[1] isKindOfClass:[UIAction class]]);
  UIAction* action = (UIAction*)tab_grid_menu.children[1];
  EXPECT_EQ(0U, action.attributes);

  ASSERT_TRUE([tab_grid_menu.children[2] isKindOfClass:[UIAction class]]);
  action = (UIAction*)tab_grid_menu.children[2];
  EXPECT_EQ(0U, action.attributes);
}

// Tests the back/forward items for the menu.
TEST_F(AdaptiveToolbarMediatorTest, MenuElementsBackForward) {
  std::unique_ptr<web::FakeNavigationManager> navigation_manager =
      std::make_unique<web::FakeNavigationManager>();

  navigation_manager->AddItem(GURL("http://chromium.org/1"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);
  navigation_manager->AddItem(GURL("http://chromium.org/2"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);

  navigation_manager->AddItem(GURL("http://chromium.org/current"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);

  navigation_manager->AddItem(GURL("http://chromium.org/4"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);
  navigation_manager->AddItem(GURL("http://chromium.org/5"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);
  navigation_manager->AddItem(GURL("http://chromium.org/6"),
                              ui::PageTransition::PAGE_TRANSITION_LINK);
  navigation_manager->GoBack();
  navigation_manager->GoBack();
  navigation_manager->GoBack();

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetNavigationManager(std::move(navigation_manager));
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  UIMenu* back_menu =
      [mediator_ menuForButtonOfType:AdaptiveToolbarButtonTypeBack];

  ASSERT_EQ(2U, back_menu.children.count);
  EXPECT_NSEQ(@"chromium.org/2", back_menu.children[0].title);
  EXPECT_NSEQ(@"chromium.org/1", back_menu.children[1].title);

  UIMenu* forward_menu =
      [mediator_ menuForButtonOfType:AdaptiveToolbarButtonTypeForward];
  ASSERT_EQ(3U, forward_menu.children.count);
  EXPECT_NSEQ(@"chromium.org/4", forward_menu.children[0].title);
  EXPECT_NSEQ(@"chromium.org/5", forward_menu.children[1].title);
  EXPECT_NSEQ(@"chromium.org/6", forward_menu.children[2].title);
}

}  // namespace

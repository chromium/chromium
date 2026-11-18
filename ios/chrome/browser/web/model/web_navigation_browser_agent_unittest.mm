// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/web/model/web_navigation_ntp_delegate.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@interface FakeNTPDelegate : NSObject <WebNavigationNTPDelegate>

// Redeclare protocol property readwrite.
@property(nonatomic, readwrite, getter=isNTPActiveForCurrentWebState)
    BOOL NTPActiveForCurrentWebState;

// YES if reloadNTPForWebState was called for `webState`
- (BOOL)didReloadForWebState:(web::WebState*)webState;

@end

@implementation FakeNTPDelegate {
  raw_ptr<web::WebState> _lastReloadedWebState;
}

- (void)reloadNTPForWebState:(web::WebState*)webState {
  _lastReloadedWebState = webState;
}

- (BOOL)didReloadForWebState:(web::WebState*)webState {
  return webState == _lastReloadedWebState;
}

@end

namespace {

constexpr char kTestURL0[] = "https://example.com/0";
constexpr char kTestURL1[] = "https://example.com/1";
constexpr char kTestURL2[] = "https://example.com/2";
constexpr int kFirstIndex = 0;
constexpr int kMiddleIndex = 1;
constexpr int kLastIndex = 2;

class WebNavigationBrowserAgentTest : public PlatformTest {
 public:
  WebNavigationBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    page_side_swipe_handler_ =
        OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:page_side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];
    delegate_ = [[FakeNTPDelegate alloc] init];
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = WebNavigationBrowserAgent::FromBrowser(browser_.get());
    agent_->SetDelegate(delegate_);
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  ~WebNavigationBrowserAgentTest() override { agent_->SetDelegate(nil); }

 protected:
  // Inserts and activates a web state with three navigation items committed at
  // `kMiddleIndex`, and returns its navigation manager.
  web::FakeNavigationManager* InsertActiveWebStateWithHistory() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kTestURL0), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL(kTestURL1), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL(kTestURL2), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItemIndex(kMiddleIndex);
    web::FakeNavigationManager* raw_navigation_manager =
        navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return raw_navigation_manager;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id<PageSideSwipeCommands> page_side_swipe_handler_;
  FakeNTPDelegate* delegate_;
  raw_ptr<WebNavigationBrowserAgent> agent_;
  // Navigation manager for the web state at index 0 in `browser_`'s web state
  // list.
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
};

// Tests that reloading when there is no active NTP reloads the web state, and
// the NTP delegate doesn't reload.
TEST_F(WebNavigationBrowserAgentTest, TestReloadNoNTP) {
  agent_->Reload();
  EXPECT_FALSE([delegate_
      didReloadForWebState:browser_->GetWebStateList()->GetWebStateAt(0)]);
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());
}

// Tests that reloading when there is an active NTP causes the delegate to
// reload, and the web state doesn't reload.
TEST_F(WebNavigationBrowserAgentTest, TestReloadActiveNTP) {
  delegate_.NTPActiveForCurrentWebState = YES;
  agent_->Reload();
  EXPECT_TRUE([delegate_
      didReloadForWebState:browser_->GetWebStateList()->GetWebStateAt(0)]);
  EXPECT_FALSE(navigation_manager_->ReloadWasCalled());
}

// Tests that back and forward availability and navigation are disabled while
// the active web state is `kActorControlled`, and restored once `kInactive`.
TEST_F(WebNavigationBrowserAgentTest,
       TestNavigationBlockedAndDisabledWhenActorControlled) {
  web::FakeNavigationManager* navigation_manager =
      InsertActiveWebStateWithHistory();
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  ActorTabHelper::CreateForWebState(web_state);
  ActorTabHelper* actor_tab_helper = ActorTabHelper::FromWebState(web_state);
  ASSERT_NE(nullptr, actor_tab_helper);
  EXPECT_TRUE(agent_->CanGoBack());
  EXPECT_TRUE(agent_->CanGoForward());

  actor_tab_helper->SetControlState(actor::ActorControlState::kActorControlled);
  EXPECT_FALSE(agent_->CanGoBack());
  EXPECT_FALSE(agent_->CanGoForward());

  agent_->GoBack();
  EXPECT_EQ(kMiddleIndex, navigation_manager->GetLastCommittedItemIndex());

  agent_->GoForward();
  EXPECT_EQ(kMiddleIndex, navigation_manager->GetLastCommittedItemIndex());

  actor_tab_helper->SetControlState(actor::ActorControlState::kInactive);
  EXPECT_TRUE(agent_->CanGoBack());
  EXPECT_TRUE(agent_->CanGoForward());

  agent_->GoBack();
  EXPECT_EQ(kFirstIndex, navigation_manager->GetLastCommittedItemIndex());

  agent_->GoForward();
  EXPECT_EQ(kMiddleIndex, navigation_manager->GetLastCommittedItemIndex());

  agent_->GoForward();
  EXPECT_EQ(kLastIndex, navigation_manager->GetLastCommittedItemIndex());
}

// Tests that navigation is allowed when the active web state has no
// `ActorTabHelper` attached.
TEST_F(WebNavigationBrowserAgentTest, TestNavigationAllowedWithoutActorHelper) {
  web::FakeNavigationManager* navigation_manager =
      InsertActiveWebStateWithHistory();

  EXPECT_TRUE(agent_->CanGoBack());
  agent_->GoBack();
  EXPECT_EQ(kFirstIndex, navigation_manager->GetLastCommittedItemIndex());
}

}  // namespace

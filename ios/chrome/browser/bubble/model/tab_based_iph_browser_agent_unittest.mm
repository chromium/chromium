// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"

#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/bubble/public/in_product_help_type.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeHelpCommandsHandler : NSObject <HelpCommands>
@property(nonatomic, assign) BOOL presentSendTabToSelfOmniboxCalled;
@end

@implementation FakeHelpCommandsHandler

- (void)presentInProductHelpWithType:(InProductHelpType)type {
  if (type == InProductHelpType::kSendTabToSelfOmnibox) {
    _presentSendTabToSelfOmniboxCalled = YES;
  }
}

- (void)handleToolbarSwipeGesture {
}
- (void)handleTapOutsideOfVisibleGestureInProductHelp {
}
- (void)hideAllHelpBubbles {
}

@end

namespace {

class TabBasedIPHBrowserAgentTest : public PlatformTest {
 protected:
  TabBasedIPHBrowserAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));
    profile_ = std::move(builder).Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    help_handler_ = [[FakeHelpCommandsHandler alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:help_handler_
                     forProtocol:@protocol(HelpCommands)];

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    TabBasedIPHBrowserAgent::CreateForBrowser(browser_.get());
  }

  BrowserViewVisibilityNotifierBrowserAgent* visibility_notifier() {
    return BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(
        browser_.get());
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      send_tab_to_self::kSendTabToSelfExtraEntryPoints};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeHelpCommandsHandler* help_handler_;
};

// Test that Send Tab to Self omnibox IPH is presented when eligible.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedWhenEligible) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that Send Tab to Self omnibox IPH is presented even if the page is
// loading.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedWhenLoading) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web_state->SetLoading(true);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that Send Tab to Self omnibox IPH is not presented when not eligible.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedWhenNotEligible) {
  auto* service = static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  service->SetEntryPointDisplayReason(std::nullopt);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that Send Tab to Self omnibox IPH is not presented when there is no
// active web state.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedWhenNoActiveWebState) {
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that Send Tab to Self omnibox IPH is not presented when entry point
// feature flag is disabled.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHDisabledWhenFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      send_tab_to_self::kSendTabToSelfExtraEntryPoints);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that Send Tab to Self omnibox IPH is only triggered once upon startup.
TEST_F(TabBasedIPHBrowserAgentTest, SendTabToSelfOmniboxIPHOnlyTriggeredOnce) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // First visibility change on startup should trigger IPH.
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);

  // Subsequent visibility changes should not trigger IPH again.
  help_handler_.presentSendTabToSelfOmniboxCalled = NO;
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  // Subsequent page loads should not trigger IPH.
  raw_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Test that if ineligible upon startup, subsequent visibility changes do not
// re-trigger the IPH even if eligibility changes later.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotRetriggeredAfterIneligibleStartup) {
  auto* service = static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  service->SetEntryPointDisplayReason(std::nullopt);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Startup visibility change while ineligible.
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  // Becoming eligible later (e.g. navigation or sync update) followed by
  // another visibility change should not trigger the one-time startup IPH.
  service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature);
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kAppearing);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

}  // namespace

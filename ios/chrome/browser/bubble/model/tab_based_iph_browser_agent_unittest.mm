// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"

#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/bubble/public/in_product_help_type.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeHelpCommandsHandler : NSObject <HelpCommands>
@property(nonatomic, assign) BOOL presentSendTabToSelfOmniboxCalled;
// Whether `presentInProductHelpWithType:` was called with `kBottomOmnibox`.
@property(nonatomic, assign) BOOL presentBottomOmniboxCalled;
@end

@implementation FakeHelpCommandsHandler

- (void)presentInProductHelpWithType:(InProductHelpType)type {
  if (type == InProductHelpType::kSendTabToSelfOmnibox) {
    _presentSendTabToSelfOmniboxCalled = YES;
  } else if (type == InProductHelpType::kBottomOmnibox) {
    _presentBottomOmniboxCalled = YES;
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

  // Simulates the browser view becoming visible.
  void MakeBrowserViewVisible() {
    visibility_notifier()->GetNotificationCallback().Run(
        BrowserViewVisibilityState::kVisible,
        BrowserViewVisibilityState::kAppearing);
  }

  // Returns the `UrlLoadingNotifierBrowserAgent` for `browser_`.
  UrlLoadingNotifierBrowserAgent* url_loading_notifier() {
    return UrlLoadingNotifierBrowserAgent::FromBrowser(browser_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeHelpCommandsHandler* help_handler_;
};

// Tests that the Send Tab to Self omnibox IPH is not presented on the startup
// tab.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedOnStartup) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  MakeBrowserViewVisible();
  raw_web_state->WasShown();
  raw_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is presented after a tab switch.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedOnTabSwitch) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  web_state_list->InsertWebState(std::move(web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
  MakeBrowserViewVisible();

  web_state_list->ActivateWebStateAt(1);
  raw_web_state->WasShown();

  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that replacing the active `WebState` does not trigger the Send Tab to
// Self omnibox IPH.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedOnReplace) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  auto replacement_web_state = std::make_unique<web::FakeWebState>();
  replacement_web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_replacement_web_state = replacement_web_state.get();
  web_state_list->ReplaceWebStateAt(0, std::move(replacement_web_state));
  raw_replacement_web_state->WasShown();

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is presented after a user
// navigation.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedOnUserNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);

  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is not presented after an
// uncommitted navigation.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedOnUncommittedNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(false);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is only presented after a
// renderer-initiated navigation if it has a user gesture.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHOnRendererInitiatedNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasCommitted(true);
  context.SetIsRendererInitiated(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  context.SetUrl(GURL("https://www.example.org/second"));
  context.SetHasUserGesture(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is deferred while the BVC is
// covered.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHDeferredWhileCovered) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kCoveredByModal,
      BrowserViewVisibilityState::kVisible);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kCoveredByModal);
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is deferred after a tab switch
// while the BVC is covered.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHDeferredOnTabSwitchWhileCovered) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  web_state_list->InsertWebState(std::move(web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
  MakeBrowserViewVisible();
  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kCoveredByModal,
      BrowserViewVisibilityState::kVisible);

  web_state_list->ActivateWebStateAt(1);
  raw_web_state->WasShown();
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  visibility_notifier()->GetNotificationCallback().Run(
      BrowserViewVisibilityState::kVisible,
      BrowserViewVisibilityState::kCoveredByModal);
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is not presented again after a
// later user navigation or tab switch.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedAgainAfterAttempt) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  web_state_list->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  auto other_web_state = std::make_unique<web::FakeWebState>();
  other_web_state->SetCurrentURL(GURL("https://www.example.org"));
  web::FakeWebState* raw_other_web_state = other_web_state.get();
  web_state_list->InsertWebState(std::move(other_web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);

  help_handler_.presentSendTabToSelfOmniboxCalled = NO;
  context.SetUrl(GURL("https://www.example.org/second"));
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  web_state_list->ActivateWebStateAt(1);
  raw_other_web_state->WasShown();

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that an ineligible trigger does not use up the Send Tab to Self omnibox
// IPH attempt.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedAfterIneligibleTrigger) {
  auto* sync_service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);

  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferSignIn);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature);
  context.SetUrl(GURL("https://www.example.org/second"));
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->SetCurrentURL(context.GetUrl());
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that switching to an ineligible tab does not stop observing future tab
// switches.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHPresentedAfterIneligibleTabSwitch) {
  auto* sync_service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  WebStateList* web_state_list = browser_->GetWebStateList();
  auto first_web_state = std::make_unique<web::FakeWebState>();
  first_web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_first_web_state = first_web_state.get();
  web_state_list->InsertWebState(
      std::move(first_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  auto second_web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* raw_second_web_state = second_web_state.get();
  web_state_list->InsertWebState(std::move(second_web_state),
                                 WebStateList::InsertionParams::AtIndex(1));
  MakeBrowserViewVisible();

  sync_service->SetEntryPointDisplayReason(std::nullopt);
  web_state_list->ActivateWebStateAt(1);
  raw_second_web_state->WasShown();
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  sync_service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature);
  web_state_list->ActivateWebStateAt(0);
  raw_first_web_state->WasShown();
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is not presented when a
// navigation commits the same URL.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedOnSameURLNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  GURL url("https://www.example.com");
  web_state->SetCurrentURL(url);
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is not presented after a
// same-document navigation.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHNotPresentedOnSameDocumentNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.com/#frag"));
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the Send Tab to Self omnibox IPH is deferred until the browser
// view first becomes visible.
TEST_F(TabBasedIPHBrowserAgentTest,
       SendTabToSelfOmniboxIPHDeferredUntilBrowserViewFirstVisible) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL("https://www.example.com"));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.example.org/next"));
  context.SetHasUserGesture(true);
  context.SetHasCommitted(true);
  raw_web_state->OnNavigationStarted(&context);
  raw_web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(help_handler_.presentSendTabToSelfOmniboxCalled);

  MakeBrowserViewVisible();
  EXPECT_TRUE(help_handler_.presentSendTabToSelfOmniboxCalled);
}

// Tests that the bottom omnibox IPH is presented when navigating using the
// omnibox to a non-NTP URL.
TEST_F(TabBasedIPHBrowserAgentTest,
       BottomOmniboxIPHPresentedOnOmniboxNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetCurrentURL(GURL(kChromeUINewTabURL));
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  GURL destination_url("https://www.example.com");
  url_loading_notifier()->TabDidLoadUrl(
      destination_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      raw_web_state->GetWeakPtr());
  raw_web_state->SetCurrentURL(destination_url);
  raw_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_TRUE(help_handler_.presentBottomOmniboxCalled);
}

// Tests that the bottom omnibox IPH is not presented when navigating to the
// NTP.
TEST_F(TabBasedIPHBrowserAgentTest,
       BottomOmniboxIPHNotPresentedOnNTPNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  GURL ntp_url(kChromeUINewTabURL);
  web_state->SetVisibleURL(ntp_url);
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  url_loading_notifier()->TabDidLoadUrl(
      ntp_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      raw_web_state->GetWeakPtr());
  raw_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(help_handler_.presentBottomOmniboxCalled);
}

// Tests that the bottom omnibox IPH is not presented when navigating without
// using the omnibox.
TEST_F(TabBasedIPHBrowserAgentTest,
       BottomOmniboxIPHNotPresentedOnLinkNavigation) {
  auto web_state = std::make_unique<web::FakeWebState>();
  GURL destination_url("https://www.example.com");
  web_state->SetVisibleURL(destination_url);
  web::FakeWebState* raw_web_state = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  MakeBrowserViewVisible();

  url_loading_notifier()->TabDidLoadUrl(
      destination_url, ui::PAGE_TRANSITION_LINK, raw_web_state->GetWeakPtr());
  raw_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(help_handler_.presentBottomOmniboxCalled);
}

}  // namespace

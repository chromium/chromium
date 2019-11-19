// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"

#include "base/observer_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestDialogPresenterDelegate : NSObject<DialogPresenterDelegate> {
  std::vector<web::WebState*> _presentedWebStates;
}
// The web states for the dialogs that have been presented.
@property(nonatomic, readonly) std::vector<web::WebState*>& presentedWebStates;
// Whether the dialog should be allowed to present a dialog.
@property(nonatomic, assign) BOOL shouldAllowDialogPresentation;
@end

@implementation TestDialogPresenterDelegate

- (instancetype)init {
  if (self = [super init]) {
    _shouldAllowDialogPresentation = YES;
  }
  return self;
}

- (std::vector<web::WebState*>&)presentedWebStates {
  return _presentedWebStates;
}

- (void)dialogPresenter:(DialogPresenter*)presenter
    willShowDialogForWebState:(web::WebState*)webState {
  _presentedWebStates.push_back(webState);
}

- (BOOL)shouldDialogPresenterPresentDialog:(DialogPresenter*)presenter {
  return self.shouldAllowDialogPresentation;
}

@end

class DialogPresenterTest : public PlatformTest {
 protected:
  DialogPresenterTest()
      : delegate_([[TestDialogPresenterDelegate alloc] init]),
        viewController_([[UIViewController alloc] init]),
        presenter_([[DialogPresenter alloc] initWithDelegate:delegate_
                                    presentingViewController:viewController_]) {
    [presenter_ setActive:YES];
  }
  ~DialogPresenterTest() override {
    [[presenter_ presentedDialogCoordinator] stop];
  }

  TestDialogPresenterDelegate* delegate() { return delegate_; }
  UIViewController* viewController() { return viewController_; }
  DialogPresenter* presenter() { return presenter_; }

 private:
  TestDialogPresenterDelegate* delegate_;
  UIViewController* viewController_;
  DialogPresenter* presenter_;
};

// Tests that a dialog was successfully shown and that the delegate was notified
// with the correct context.
TEST_F(DialogPresenterTest, SimpleTest) {
  web::TestWebState webState;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState
                                completionHandler:nil];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState, delegate().presentedWebStates.front());
}

// Test that javascript dialogs are presented with a different title when they
// are presented from a URL with a different origin to the webstate origin.
TEST_F(DialogPresenterTest, IFrameTest) {
  web::TestWebState web_state;
  GURL foo_url = GURL("http://foo.com");
  GURL bar_url = GURL("http://bar.com");

  web_state.SetCurrentURL(foo_url);
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:foo_url
                                         webState:&web_state
                                completionHandler:nil];

  // Ensure alerts from the same domain have a correct title.
  NSString* same_origin_title =
      [presenter() presentedDialogCoordinator].alertController.title;
  NSString* hostname = base::SysUTF8ToNSString(foo_url.host());
  NSString* expected_title = l10n_util::GetNSStringF(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE, base::SysNSStringToUTF16(hostname));
  EXPECT_NSEQ(expected_title, same_origin_title);

  [presenter() cancelAllDialogs];

  // Ensure that alerts from an embedded iframe with a different domain have
  // a title and it's different to the same-origin title.
  web_state.SetCurrentURL(bar_url);
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:foo_url
                                         webState:&web_state
                                completionHandler:nil];
  NSString* different_origin_title =
      [presenter() presentedDialogCoordinator].alertController.title;
  expected_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(expected_title, different_origin_title);
}

// Tests that JavaScript dialogs have correct title when they are presented from
// about:blank page.
TEST_F(DialogPresenterTest, AboutBlankTest) {
  web::TestWebState web_state;
  web_state.SetCurrentURL(GURL(url::kAboutBlankURL));
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL(url::kAboutBlankURL)
                                         webState:&web_state
                                completionHandler:nil];

  NSString* expected_title = l10n_util::GetNSStringF(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE, base::UTF8ToUTF16(url::kAboutBlankURL));
  NSString* actual_title =
      [presenter() presentedDialogCoordinator].alertController.title;
  EXPECT_NSEQ(expected_title, actual_title);
}

// Tests that multiple JavaScript dialogs are queued
TEST_F(DialogPresenterTest, QueueTest) {
  // Tests that the dialog for |webState1| has been shown.
  web::TestWebState webState1;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState1
                                completionHandler:nil];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
  // Attempt to present another dialog for |webState2|, and verify that only
  // |webState2| has been shown.
  web::TestWebState webState2;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState2
                                completionHandler:nil];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
  // Dismiss the dialog for |webState1| and call the confirm handler to trigger
  // showing the dialog for |webState2|.
  [presenter().presentedDialogCoordinator stop];
  [presenter()
      dialogCoordinatorWasStopped:presenter().presentedDialogCoordinator];
  EXPECT_EQ(2U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
  EXPECT_EQ(&webState2, delegate().presentedWebStates.back());
}

// Tests that cancelling a queued JavaScript dialog will call its completion
// handler.
TEST_F(DialogPresenterTest, CancelTest) {
  // Show a dialog for |webState1| and enqueue a dialog for |webState2|.
  web::TestWebState webState1;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState1
                                completionHandler:nil];
  web::TestWebState webState2;
  __block BOOL completion_called = NO;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState2
                                completionHandler:^{
                                  completion_called = YES;
                                }];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
  // Cancel the dialog for |webState2| and verify that |completion_called| was
  // reset.
  [presenter() cancelDialogForWebState:&webState2];
  EXPECT_TRUE(completion_called);
}

// Tests that if the delegate is presenting, the alert is showed only when
// notified.
TEST_F(DialogPresenterTest, DelegatePresenting) {
  // Tests that the dialog is not shown if the delegate is presenting.
  web::TestWebState webState1;
  delegate().shouldAllowDialogPresentation = NO;
  [presenter() runJavaScriptAlertPanelWithMessage:@""
                                       requestURL:GURL()
                                         webState:&webState1
                                completionHandler:nil];
  EXPECT_EQ(0U, delegate().presentedWebStates.size());
  [presenter() tryToPresent];
  EXPECT_EQ(0U, delegate().presentedWebStates.size());

  // The delegate is not presenting anymore, the dialog is not shown yet.
  delegate().shouldAllowDialogPresentation = YES;
  EXPECT_EQ(0U, delegate().presentedWebStates.size());

  // Notify the presenter that it can present.
  [presenter() tryToPresent];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
}

// Tests that cancelling all queued JavaScript dialogs will call all completion
// handlers.
TEST_F(DialogPresenterTest, CancelAllTest) {
  // Show a dialog for |context1| and enqueue a dialog for |context2| and
  // |context3|.
  web::TestWebState webState1;
  __block BOOL completion1_called = NO;
  [presenter() runJavaScriptAlertPanelWithMessage:@"1"
                                       requestURL:GURL()
                                         webState:&webState1
                                completionHandler:^{
                                  completion1_called = YES;
                                }];
  web::TestWebState webState2;
  __block BOOL completion2_called = NO;
  [presenter() runJavaScriptAlertPanelWithMessage:@"2"
                                       requestURL:GURL()
                                         webState:&webState2
                                completionHandler:^{
                                  completion2_called = YES;
                                }];
  web::TestWebState webState3;
  __block BOOL completion3_called = NO;
  [presenter() runJavaScriptAlertPanelWithMessage:@"3"
                                       requestURL:GURL()
                                         webState:&webState3
                                completionHandler:^{
                                  completion3_called = YES;
                                }];
  EXPECT_EQ(1U, delegate().presentedWebStates.size());
  EXPECT_EQ(&webState1, delegate().presentedWebStates.front());
  // Cancel all dialogs and verify that all |completion_called| were called.
  [presenter() cancelAllDialogs];
  EXPECT_TRUE(completion1_called);
  EXPECT_TRUE(completion2_called);
  EXPECT_TRUE(completion3_called);
}

// Tests that dialogs are appropriately cancelled for
// WebStateObserver::DidStartNavigation().
TEST_F(DialogPresenterTest, CancelForNavigationStarted) {
  // Set up a WebState complete with NavigationManager with last commited item.
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL("https://chromium.org"));
  std::unique_ptr<web::TestNavigationManager> nav_manager =
      std::make_unique<web::TestNavigationManager>();
  nav_manager->SetLastCommittedItem(item.get());
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  web_state->SetNavigationManager(std::move(nav_manager));
  // Verify cancellation for DidStartNavigation().
  __block BOOL dialog_cancelled = NO;
  [presenter() runJavaScriptTextInputPanelWithPrompt:@""
                                         defaultText:@""
                                          requestURL:item->GetURL()
                                            webState:web_state.get()
                                   completionHandler:^(NSString* input) {
                                     dialog_cancelled = !input;
                                   }];
  web::FakeNavigationContext context;
  web_state->OnNavigationStarted(&context);
  EXPECT_TRUE(dialog_cancelled);
}

// Tests that dialogs are appropriately cancelled for
// WebStateObserver::DidFinishNavigation().
TEST_F(DialogPresenterTest, CancelForNavigationFinished) {
  // Set up a WebState complete with NavigationManager with last commited item.
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL("https://chromium.org"));
  std::unique_ptr<web::TestNavigationManager> nav_manager =
      std::make_unique<web::TestNavigationManager>();
  nav_manager->SetLastCommittedItem(item.get());
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  web_state->SetNavigationManager(std::move(nav_manager));
  // Verify cancellation for DidFinishNavigation().
  __block BOOL dialog_cancelled = NO;
  [presenter() runJavaScriptTextInputPanelWithPrompt:@""
                                         defaultText:@""
                                          requestURL:item->GetURL()
                                            webState:web_state.get()
                                   completionHandler:^(NSString* input) {
                                     dialog_cancelled = !input;
                                   }];
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state->OnNavigationFinished(&context);
  EXPECT_TRUE(dialog_cancelled);
}

// Tests that dialogs are appropriately cancelled for
// WebStateObserver::RenderProcessGone().
TEST_F(DialogPresenterTest, CancelForRenderProcessGone) {
  // Set up a WebState complete with NavigationManager with last commited item.
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL("https://chromium.org"));
  std::unique_ptr<web::TestNavigationManager> nav_manager =
      std::make_unique<web::TestNavigationManager>();
  nav_manager->SetLastCommittedItem(item.get());
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  web_state->SetNavigationManager(std::move(nav_manager));
  // Verify cancellation for RenderProcessGone().
  __block BOOL dialog_cancelled = NO;
  [presenter() runJavaScriptTextInputPanelWithPrompt:@""
                                         defaultText:@""
                                          requestURL:item->GetURL()
                                            webState:web_state.get()
                                   completionHandler:^(NSString* input) {
                                     dialog_cancelled = !input;
                                   }];
  web_state->OnRenderProcessGone();
  EXPECT_TRUE(dialog_cancelled);
}

// Tests that dialogs are appropriately cancelled for
// WebStateObserver::WebStateDestroyed().
TEST_F(DialogPresenterTest, CancelForWebStateDestroyed) {
  // Set up a WebState complete with NavigationManager with last commited item.
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL("https://chromium.org"));
  std::unique_ptr<web::TestNavigationManager> nav_manager =
      std::make_unique<web::TestNavigationManager>();
  nav_manager->SetLastCommittedItem(item.get());
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  web_state->SetNavigationManager(std::move(nav_manager));
  // Verify cancellation for WebStateDestroyed().
  __block BOOL dialog_cancelled = NO;
  [presenter() runJavaScriptTextInputPanelWithPrompt:@""
                                         defaultText:@""
                                          requestURL:item->GetURL()
                                            webState:web_state.get()
                                   completionHandler:^(NSString* input) {
                                     dialog_cancelled = !input;
                                   }];
  web_state = nullptr;
  EXPECT_TRUE(dialog_cancelled);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_web_modal_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

const char kTestURL[] = "https://gemini.google.com/quiz/123";
const char kTestURLHost[] = "gemini.google.com";
const char kLinkURL[] = "https://www.example.com/article";

}  // namespace

// Counts dismissal notifications from the coordinator.
@interface FakeGeminiWebModalCoordinatorDelegate
    : NSObject <GeminiWebModalCoordinatorDelegate>
@property(nonatomic, assign) int dismissalCount;
@end

@implementation FakeGeminiWebModalCoordinatorDelegate
- (void)geminiWebModalCoordinatorDidDismiss:
    (GeminiWebModalCoordinator*)coordinator {
  self.dismissalCount++;
}
@end

class GeminiWebModalCoordinatorTest : public PlatformTest {
 public:
  GeminiWebModalCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    mock_gemini_commands_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_gemini_commands_handler_
                     forProtocol:@protocol(GeminiCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    delegate_ = [[FakeGeminiWebModalCoordinatorDelegate alloc] init];
    coordinator_ = [[GeminiWebModalCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                               URL:GURL(kTestURL)];
    coordinator_.delegate = delegate_;
  }

  ~GeminiWebModalCoordinatorTest() override { [coordinator_ stop]; }

 protected:
  // Returns the modal's view controller, or nil if nothing is presented.
  GeminiModalContentViewController* PresentedModal() {
    UINavigationController* navigationController =
        base::apple::ObjCCast<UINavigationController>(
            base_view_controller_.presentedViewController);
    return base::apple::ObjCCast<GeminiModalContentViewController>(
        navigationController.topViewController);
  }

  // Returns whether the modal went away. Dismissal is animated, so it is not
  // observable synchronously.
  bool WaitForDismissal() {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^{
          return base_view_controller_.presentedViewController == nil;
        });
  }

  // Returns whether a new tab was opened. Opening is posted async.
  bool WaitForNewTab() {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, ^{
          return url_loader_->load_new_tab_call_count > 0;
        });
  }

  // Expects that `URL` was opened in a new tab, Gemini was minimized and the
  // delegate was notified of the dismissal.
  void ExpectOpenedInNewTab(const GURL& URL) {
    ASSERT_TRUE(WaitForNewTab());
    EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
    EXPECT_EQ(URL, url_loader_->last_params.web_params.url);
    EXPECT_EQ(1, delegate_.dismissalCount);
    EXPECT_EQ(1, user_action_tester_.GetActionCount(
                     "MobileGeminiWebModalNavigatedToNewTab"));
    EXPECT_OCMOCK_VERIFY(mock_gemini_commands_handler_);
  }

  web::WebTaskEnvironment task_environment_;
  base::UserActionTester user_action_tester_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_ = nullptr;
  id mock_gemini_commands_handler_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  GeminiWebModalCoordinator* coordinator_;
  FakeGeminiWebModalCoordinatorDelegate* delegate_;
};

// Test that the domain is always shown, and that the title is not replaced by
// the URL while the page has no title.
TEST_F(GeminiWebModalCoordinatorTest, NavigationItemShowsDomain) {
  [coordinator_ start];

  GeminiModalContentViewController* viewController = PresentedModal();
  ASSERT_TRUE(viewController);
  if (@available(iOS 26, *)) {
    EXPECT_EQ(0u, viewController.title.length);
    EXPECT_NSEQ(@(kTestURLHost), viewController.navigationItem.subtitle);
  } else {
    EXPECT_NSEQ(@(kTestURLHost), viewController.title);
  }
}

// Test that stopping the coordinator dismisses the modal.
TEST_F(GeminiWebModalCoordinatorTest, StopDismissesModal) {
  [coordinator_ start];
  ASSERT_TRUE(PresentedModal());

  [coordinator_ stop];

  EXPECT_TRUE(WaitForDismissal());
}

// Test that tapping close notifies the delegate.
TEST_F(GeminiWebModalCoordinatorTest, CloseButtonNotifiesDelegate) {
  [coordinator_ start];

  GeminiModalContentViewController* viewController = PresentedModal();
  ASSERT_TRUE(viewController);
  [viewController.delegate
      geminiModalContentViewControllerDidTapClose:viewController];

  EXPECT_EQ(1, delegate_.dismissalCount);
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "MobileGeminiWebModalNavigatedToNewTab"));
}

// Test that swiping the sheet down also notifies the delegate.
TEST_F(GeminiWebModalCoordinatorTest, InteractiveDismissalNotifiesDelegate) {
  [coordinator_ start];

  UIPresentationController* presentationController =
      base_view_controller_.presentedViewController.presentationController;
  ASSERT_TRUE(presentationController);
  ASSERT_EQ((id)coordinator_, presentationController.delegate);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_EQ(1, delegate_.dismissalCount);
}

// Test that stopping without ever starting is a no-op rather than a crash.
TEST_F(GeminiWebModalCoordinatorTest, StopWithoutStartIsSafe) {
  [coordinator_ stop];

  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

// Test that a link asking for a new window opens in a new tab.
TEST_F(GeminiWebModalCoordinatorTest, NewWindowRequestOpensNewTab) {
  [coordinator_ start];
  OCMExpect([mock_gemini_commands_handler_ minimizeGeminiIfInvoked]);

  // The coordinator conforms to `CRWWebStateDelegate` privately.
  id<CRWWebStateDelegate> webStateDelegate = (id)coordinator_;
  EXPECT_EQ(nullptr, [webStateDelegate webState:nullptr
                         createNewWebStateForURL:GURL(kLinkURL)
                                       openerURL:GURL(kTestURL)
                                 initiatedByUser:YES]);

  ExpectOpenedInNewTab(GURL(kLinkURL));
}

// Test that the initial load is allowed before any page has committed.
TEST_F(GeminiWebModalCoordinatorTest, InitialLoadIsAllowed) {
  [coordinator_ start];

  // The coordinator conforms to `CRWWebStatePolicyDecider` privately.
  id<CRWWebStatePolicyDecider> policyDecider = (id)coordinator_;
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@(kTestURL)]];
  web::WebStatePolicyDecider::RequestInfo requestInfo(
      ui::PAGE_TRANSITION_TYPED, /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false, /*is_user_initiated=*/false,
      /*user_tapped_recently=*/false);
  __block bool allowed = false;
  [policyDecider shouldAllowRequest:request
                        requestInfo:requestInfo
                    decisionHandler:^(
                        web::WebStatePolicyDecider::PolicyDecision decision) {
                      allowed = decision.ShouldAllowNavigation();
                    }];

  EXPECT_TRUE(allowed);
  EXPECT_EQ(0, url_loader_->load_new_tab_call_count);
}

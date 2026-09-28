// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_web_modal_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

const char kTestURL[] = "https://gemini.google.com/quiz/123";
const char kTestURLHost[] = "gemini.google.com";
const char kLinkURL[] = "https://www.example.com/article";

}  // namespace

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

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    coordinator_ = [[GeminiWebModalCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                               URL:GURL(kTestURL)
                  dismissalHandler:^{
                    dismissal_count_++;
                  }];
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

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  GeminiWebModalCoordinator* coordinator_;
  int dismissal_count_ = 0;
};

// Test that the title falls back to the host while the page has no title.
TEST_F(GeminiWebModalCoordinatorTest, TitleFallsBackToHost) {
  [coordinator_ start];

  GeminiModalContentViewController* viewController = PresentedModal();
  ASSERT_TRUE(viewController);
  EXPECT_NSEQ(@(kTestURLHost), viewController.title);
}

// Test that stopping the coordinator dismisses the modal.
TEST_F(GeminiWebModalCoordinatorTest, StopDismissesModal) {
  [coordinator_ start];
  ASSERT_TRUE(PresentedModal());

  [coordinator_ stop];

  EXPECT_TRUE(WaitForDismissal());
}

// Test that tapping close calls the dismissal handler.
TEST_F(GeminiWebModalCoordinatorTest, CloseButtonCallsDismissalHandler) {
  [coordinator_ start];

  GeminiModalContentViewController* viewController = PresentedModal();
  ASSERT_TRUE(viewController);
  [viewController.delegate
      geminiModalContentViewControllerDidTapClose:viewController];

  EXPECT_EQ(1, dismissal_count_);
}

// Test that swiping the sheet down also calls the dismissal handler.
TEST_F(GeminiWebModalCoordinatorTest,
       InteractiveDismissalCallsDismissalHandler) {
  [coordinator_ start];

  UIPresentationController* presentationController =
      base_view_controller_.presentedViewController.presentationController;
  ASSERT_TRUE(presentationController);
  ASSERT_EQ((id)coordinator_, presentationController.delegate);
  [presentationController.delegate
      presentationControllerDidDismiss:presentationController];

  EXPECT_EQ(1, dismissal_count_);
}

// Test that stopping without ever starting is a no-op rather than a crash.
TEST_F(GeminiWebModalCoordinatorTest, StopWithoutStartIsSafe) {
  [coordinator_ stop];

  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

// Test that a link asking for a new tab does not get one.
TEST_F(GeminiWebModalCoordinatorTest, NewWindowRequestStaysInModal) {
  [coordinator_ start];

  // The coordinator conforms to `CRWWebStateDelegate` privately.
  id<CRWWebStateDelegate> webStateDelegate = (id)coordinator_;
  EXPECT_EQ(nullptr, [webStateDelegate webState:nullptr
                         createNewWebStateForURL:GURL(kLinkURL)
                                       openerURL:GURL(kTestURL)
                                 initiatedByUser:YES]);
}

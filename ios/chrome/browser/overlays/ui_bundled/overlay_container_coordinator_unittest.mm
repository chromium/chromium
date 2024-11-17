// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_contained_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator+initialization.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_util.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/test_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test fixture for OverlayContainerCoordinator.
class OverlayContainerCoordinatorTest : public PlatformTest {
 public:
  OverlayContainerCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    context_ = std::make_unique<TestOverlayPresentationContext>(browser_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[OverlayContainerCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
               presentationContext:context_.get()];
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;
  }
  ~OverlayContainerCoordinatorTest() override {
    // The browser needs to be destroyed before `context_` so that observers
    // can be unhooked due to BrowserDestroyed().  This is not a problem for
    // non-test OverlayPresentationContextImpls since they're owned by the
    // Browser and get destroyed after BrowserDestroyed() is called.
    browser_.reset();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestOverlayPresentationContext> context_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  OverlayContainerCoordinator* coordinator_ = nil;
};

// Tests that the coordinator updates its OverlayPresentationContext's
// presentation capabilities when started and stopped.
TEST_F(OverlayContainerCoordinatorTest, UpdatePresentationCapabilities) {
  ASSERT_FALSE(OverlayPresentationContextSupportsContainedUI(context_.get()));

  // Start the coordinator and verify that the presentation context begins
  // supporting contained overlay UI.
  [coordinator_ start];
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  context_->PrepareToShowOverlayUI(request.get());
  EXPECT_TRUE(OverlayPresentationContextSupportsContainedUI(context_.get()));

  // Stop the coordinator and verify that the presentation context no longer
  // supports contained overlay UI.
  [coordinator_ stop];
  EXPECT_FALSE(OverlayPresentationContextSupportsContainedUI(context_.get()));
}

// Tests that the coordinator sets up the presentation context upon being added
// to the window.
TEST_F(OverlayContainerCoordinatorTest, PresentationContextSetup) {
  ASSERT_FALSE(OverlayPresentationContextSupportsPresentedUI(context_.get()));

  // Start the coordinator.  This will add it to the key window, triggering the
  // presentation of the UIViewController that will be used as the base for
  // overlay UI implemented using presentation.
  [coordinator_ start];
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  context_->PrepareToShowOverlayUI(request.get());
  UIViewController* presented_view_controller =
      coordinator_.viewController.presentedViewController;
  ASSERT_TRUE(presented_view_controller);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return presented_view_controller.presentingViewController &&
           !presented_view_controller.beingPresented;
  }));

  // Once `presented_view_controller` is finished being presented, it should
  // update the presentation capabilities to allow presented overlay UI.
  EXPECT_TRUE(OverlayPresentationContextSupportsPresentedUI(context_.get()));

  // Stop the container coordinator and wait for `presented_view_controller` to
  // finish being dismissed, verifying that the context no longer supports
  // presented overlay UI.
  [coordinator_ stop];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !presented_view_controller.presentingViewController;
  }));
  EXPECT_FALSE(OverlayPresentationContextSupportsPresentedUI(context_.get()));
}

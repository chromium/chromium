// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
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
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test fixture for OverlayContainerCoordinator.
class OverlayContainerCoordinatorTest : public PlatformTest {
 public:
  OverlayContainerCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    FullscreenController::CreateForBrowser(browser_.get());
    context_ = std::make_unique<TestOverlayPresentationContext>(browser_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[OverlayContainerCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
               presentationContext:context_.get()];
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;
  }
  ~OverlayContainerCoordinatorTest() override = default;

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

// Tests that stopping the coordinator while an active contained overlay is
// presented does not re-enter `UpdatePresentationCapabilities()` or leave
// contained UI capabilities enabled.
TEST_F(OverlayContainerCoordinatorTest, StopWithActiveContainedOverlay) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebViewProxyTabHelper::CreateForWebState(web_state.get());
  CRWWebViewScrollViewProxy* scroll_view_proxy =
      [[CRWWebViewScrollViewProxy alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [scroll_view_proxy setScrollView:scroll_view];
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
  web_state->SetWebViewProxy(web_view_proxy_mock);
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetActiveWebState(),
      OverlayModality::kTesting);
  queue->AddRequest(OverlayRequest::CreateWithConfig<TestContainedOverlay>());

  [coordinator_ start];
  EXPECT_TRUE(OverlayPresentationContextSupportsContainedUI(context_.get()));
  EXPECT_TRUE(context_->IsShowingOverlayUI());

  [coordinator_ stop];
  EXPECT_FALSE(OverlayPresentationContextSupportsContainedUI(context_.get()));
  EXPECT_FALSE(context_->IsShowingOverlayUI());
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

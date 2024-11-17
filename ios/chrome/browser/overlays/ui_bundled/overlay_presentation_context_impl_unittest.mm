// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_contained_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_util.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/test_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

@class FakeOverlayPresenationContextDelegate;

namespace {

// Mock observer for the presentation context.
class MockOverlayPresentationContextImplObserver
    : public OverlayPresentationContextObserver {
 public:
  MockOverlayPresentationContextImplObserver() {}
  ~MockOverlayPresentationContextImplObserver() override {}

  MOCK_METHOD2(OverlayPresentationContextWillChangePresentationCapabilities,
               void(OverlayPresentationContext*,
                    OverlayPresentationContext::UIPresentationCapabilities));
  MOCK_METHOD1(OverlayPresentationContextDidChangePresentationCapabilities,
               void(OverlayPresentationContext*));
  MOCK_METHOD2(OverlayPresentationContextDidMoveToWindow,
               void(OverlayPresentationContext*, UIWindow*));
};

// Returns the presentation capabilities for a context whose contained and
// presented overlay UI support is described by `supports_contained` and
// `supports_presented`.
OverlayPresentationContext::UIPresentationCapabilities GetCapabilities(
    bool supports_contained,
    bool supports_presented) {
  int capabilities =
      OverlayPresentationContext::UIPresentationCapabilities::kNone;
  if (supports_contained) {
    capabilities =
        capabilities |
        OverlayPresentationContext::UIPresentationCapabilities::kContained;
  }
  if (supports_presented) {
    capabilities =
        capabilities |
        OverlayPresentationContext::UIPresentationCapabilities::kPresented;
  }
  return static_cast<OverlayPresentationContext::UIPresentationCapabilities>(
      capabilities);
}

}  // namespace

class OverlayPresentationContextImplTest;

// Fake delegate to use for tests.
@interface FakeOverlayPresenationContextDelegate
    : NSObject <OverlayPresentationContextImplDelegate>
@property(nonatomic, assign) OverlayPresentationContextImplTest* test;
@end

// Test fixture for OverlayPresentationContextImpl.
class OverlayPresentationContextImplTest : public PlatformTest {
 public:
  OverlayPresentationContextImplTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    context_ = std::make_unique<TestOverlayPresentationContext>(browser_.get());
    delegate_ = [[FakeOverlayPresenationContextDelegate alloc] init];
    root_view_controller_ = [[UIViewController alloc] init];
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;
    delegate_.test = this;
    context_->SetDelegate(delegate_);
    context_->AddObserver(&observer_);
    EXPECT_CALL(observer_, OverlayPresentationContextDidMoveToWindow(
                               context_.get(), scoped_window_.Get()));
    context_->SetWindow(scoped_window_.Get());
  }
  ~OverlayPresentationContextImplTest() override {
    context_->RemoveObserver(&observer_);
    // The browser needs to be destroyed before `context_` so that observers
    // can be unhooked due to BrowserDestroyed().  This is not a problem for
    // non-test OverlayPresentationContextImpls since they're owned by the
    // Browser and get destroyed after BrowserDestroyed() is called.
    browser_.reset();
  }

  // Setter for whether the presentation context should support overlay UI
  // implemented using child UIViewControllers.
  void SetSupportsContainedOverlayUI() {
    if (supports_contained_ui_)
      return;

    // Updating the support for contained overlay UI will notifiy the observer
    // of this change.
    EXPECT_CALL(
        observer_,
        OverlayPresentationContextWillChangePresentationCapabilities(
            context_.get(), GetCapabilities(true, supports_presented_ui_)));
    EXPECT_CALL(observer_,
                OverlayPresentationContextDidChangePresentationCapabilities(
                    context_.get()));

    supports_contained_ui_ = true;

    context_->SetContainerViewController(root_view_controller_);

    // Check that the presentation capabilities have been updated.
    ASSERT_EQ(supports_contained_ui_,
              OverlayPresentationContextSupportsContainedUI(context_.get()));
  }

  // Setter for whether the presentation context should support overlay UI
  // implemented using presented UIViewControllers.
  void SetSupportsPresentedOverlayUI() {
    if (supports_presented_ui_)
      return;

    // Updating the support for presented overlay UI will notifiy the observer
    // of this change.
    EXPECT_CALL(
        observer_,
        OverlayPresentationContextWillChangePresentationCapabilities(
            context_.get(), GetCapabilities(supports_contained_ui_, true)));
    EXPECT_CALL(observer_,
                OverlayPresentationContextDidChangePresentationCapabilities(
                    context_.get()));

    supports_presented_ui_ = true;

    // Present a UIViewController over `root_view_controller_`'s context, then
    // supply the view controller to the presentation context.
    UIViewController* presentation_context_view_controller =
        [[UIViewController alloc] init];
    presentation_context_view_controller.definesPresentationContext = YES;
    presentation_context_view_controller.modalPresentationStyle =
        UIModalPresentationOverCurrentContext;
    __block bool presentation_finished = NO;
    [root_view_controller_
        presentViewController:presentation_context_view_controller
                     animated:NO
                   completion:^{
                     context_->SetPresentationContextViewController(
                         presentation_context_view_controller);
                     presentation_finished = YES;
                   }];

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return presentation_finished;
    }));

    // Check that the presentation capabilities have been updated.
    ASSERT_EQ(supports_presented_ui_,
              OverlayPresentationContextSupportsPresentedUI(context_.get()));
  }

  // Shows the overlay UI for `request` in the context.
  void ShowOverlayUI(OverlayRequest* request) {
    overlay_presentation_finished_ = false;
    overlay_dismissal_finished_ = false;
    overlay_dismissal_reason_ = OverlayDismissalReason::kUserInteraction;
    context_->ShowOverlayUI(request, base::BindOnce(^{
                              overlay_presentation_finished_ = true;
                            }),
                            base::BindOnce(^(OverlayDismissalReason reason) {
                              overlay_dismissal_finished_ = true;
                              overlay_dismissal_reason_ = reason;
                            }));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestOverlayPresentationContext> context_;
  MockOverlayPresentationContextImplObserver observer_;
  FakeOverlayPresenationContextDelegate* delegate_ = nil;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  bool overlay_presentation_finished_ = false;
  bool overlay_dismissal_finished_ = false;
  OverlayDismissalReason overlay_dismissal_reason_ =
      OverlayDismissalReason::kUserInteraction;

 private:
  // Support for presented or contained UI should only be updated using the
  // setters above.
  bool supports_contained_ui_ = false;
  bool supports_presented_ui_ = false;
};

// FakeOverlayPresenationContextDelegate implementation needs to be declared
// after the test fixture so that it can call its public API.
@implementation FakeOverlayPresenationContextDelegate

- (void)updatePresentationContext:(OverlayPresentationContextImpl*)context
      forPresentationCapabilities:
          (OverlayPresentationContext::UIPresentationCapabilities)capabilities {
  if (capabilities &
      OverlayPresentationContext::UIPresentationCapabilities::kContained) {
    self.test->SetSupportsContainedOverlayUI();
  }
  if (capabilities &
      OverlayPresentationContext::UIPresentationCapabilities::kPresented) {
    self.test->SetSupportsPresentedOverlayUI();
  }
}

@end

// Tests that neither contained nor presented overlay UI can be shown in the
// context if no view controllers have been provided.
TEST_F(OverlayPresentationContextImplTest, NoPresentationCapabilities) {
  ASSERT_EQ(context_->GetPresentationCapabilities(),
            OverlayPresentationContext::UIPresentationCapabilities::kNone);

  std::unique_ptr<OverlayRequest> contained_request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  EXPECT_FALSE(context_->CanShowUIForRequest(contained_request.get()));
  std::unique_ptr<OverlayRequest> presented_request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  EXPECT_FALSE(context_->CanShowUIForRequest(presented_request.get()));
}

// Tests that contained overlay UI can be shown if the container
// UIViewController is provided.
TEST_F(OverlayPresentationContextImplTest, ContainedPresentationCapability) {
  std::unique_ptr<OverlayRequest> contained_request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  context_->PrepareToShowOverlayUI(contained_request.get());
  EXPECT_TRUE(context_->CanShowUIForRequest(contained_request.get()));

  std::unique_ptr<OverlayRequest> presented_request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  EXPECT_FALSE(context_->CanShowUIForRequest(presented_request.get()));
}

// Tests that presented overlay UI can be shown if the presentation context
// UIViewController is provided.
TEST_F(OverlayPresentationContextImplTest, PresentedPresentationCapability) {
  std::unique_ptr<OverlayRequest> presented_request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  context_->PrepareToShowOverlayUI(presented_request.get());
  EXPECT_TRUE(context_->CanShowUIForRequest(presented_request.get()));

  std::unique_ptr<OverlayRequest> contained_request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  EXPECT_FALSE(context_->CanShowUIForRequest(contained_request.get()));
}

// Tests that CanShowRequest() returns the expected value when the presentation
// capabilities are pass in.
TEST_F(OverlayPresentationContextImplTest, CanShowRequest) {
  std::unique_ptr<OverlayRequest> contained_request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  EXPECT_TRUE(context_->CanShowUIForRequest(
      contained_request.get(),
      OverlayPresentationContext::UIPresentationCapabilities::kContained));
  EXPECT_FALSE(context_->CanShowUIForRequest(
      contained_request.get(),
      OverlayPresentationContext::UIPresentationCapabilities::kPresented));

  std::unique_ptr<OverlayRequest> presented_request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  EXPECT_FALSE(context_->CanShowUIForRequest(
      presented_request.get(),
      OverlayPresentationContext::UIPresentationCapabilities::kContained));
  EXPECT_TRUE(context_->CanShowUIForRequest(
      presented_request.get(),
      OverlayPresentationContext::UIPresentationCapabilities::kPresented));
}

// Tests the presentation flow for contained overlay UI.
TEST_F(OverlayPresentationContextImplTest, ContainedOverlayUI) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestContainedOverlay>();
  context_->PrepareToShowOverlayUI(request.get());
  ASSERT_EQ(0U, root_view_controller_.view.subviews.count);
  ASSERT_EQ(0U, root_view_controller_.childViewControllers.count);

  // Show the UI for `request` and verify that the overlay UI is added to the
  // `root_view_controller_`'s view.
  ShowOverlayUI(request.get());
  EXPECT_EQ(1U, root_view_controller_.view.subviews.count);
  EXPECT_EQ(1U, root_view_controller_.childViewControllers.count);
  EXPECT_TRUE(overlay_presentation_finished_);

  // Hide the overlay UI and verify that it was removed from
  // `root_view_controller_`'s view.
  context_->HideOverlayUI(request.get());
  EXPECT_EQ(0U, root_view_controller_.view.subviews.count);
  EXPECT_EQ(0U, root_view_controller_.childViewControllers.count);
  EXPECT_TRUE(overlay_dismissal_finished_);
  EXPECT_EQ(OverlayDismissalReason::kHiding, overlay_dismissal_reason_);

  // Show the UI again, then cancel it and verify that the view was removed.
  ShowOverlayUI(request.get());
  context_->CancelOverlayUI(request.get());
  EXPECT_EQ(0U, root_view_controller_.view.subviews.count);
  EXPECT_EQ(0U, root_view_controller_.childViewControllers.count);
  EXPECT_TRUE(overlay_dismissal_finished_);
  EXPECT_EQ(OverlayDismissalReason::kCancellation, overlay_dismissal_reason_);
}

// Tests the presentation flow for presented overlay UI.
TEST_F(OverlayPresentationContextImplTest, PresentedOverlayUI) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  context_->PrepareToShowOverlayUI(request.get());
  UIViewController* presentation_base_view_controller =
      root_view_controller_.presentedViewController;
  ASSERT_TRUE(presentation_base_view_controller);
  ASSERT_FALSE(presentation_base_view_controller.presentedViewController);

  // Blocks used to determine when presentation and dismissal is finished.
  bool (^presentation_completion_condition)(void) = ^bool {
    UIViewController* presented_view_controller =
        presentation_base_view_controller.presentedViewController;
    return presented_view_controller &&
           !presented_view_controller.beingPresented &&
           overlay_presentation_finished_;
  };
  bool (^dismissal_completion_condition)(void) = ^bool {
    return !presentation_base_view_controller.presentedViewController &&
           overlay_dismissal_finished_;
  };

  // Show the UI for `request` and verify that the overlay UI is presented over
  // `presentation_base_view_controller`.
  ShowOverlayUI(request.get());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout,
                                          presentation_completion_condition));

  // Hide the overlay UI and verify that it was dismissed.
  context_->HideOverlayUI(request.get());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout,
                                          dismissal_completion_condition));
  EXPECT_EQ(OverlayDismissalReason::kHiding, overlay_dismissal_reason_);

  // Show the UI again, then cancel it and verify that the view was removed.
  ShowOverlayUI(request.get());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout,
                                          presentation_completion_condition));
  context_->CancelOverlayUI(request.get());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout,
                                          dismissal_completion_condition));
  EXPECT_EQ(OverlayDismissalReason::kCancellation, overlay_dismissal_reason_);
}

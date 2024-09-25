// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/mock_overlay_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {
// Request config type used for testing.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ModalConfig);
}

// Mediator used by FakeInfobarModalOverlayCoordinators.
@interface FakeModalMediator : InfobarModalOverlayMediator
@end

@implementation FakeModalMediator
+ (const OverlayRequestSupport*)requestSupport {
  return ModalConfig::RequestSupport();
}
@end

// Modal coordinator for tests.
@interface FakeInfobarModalOverlayCoordinator : InfobarModalOverlayCoordinator {
  FakeModalMediator* _mediator;
  UIViewController* _viewController;
}
@end

@implementation FakeInfobarModalOverlayCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return ModalConfig::RequestSupport();
}

- (OverlayRequestMediator*)modalMediator {
  return _mediator;
}

- (UIViewController*)modalViewController {
  return _viewController;
}

- (void)configureModal {
  _mediator = [[FakeModalMediator alloc] initWithRequest:self.request];
  _viewController = [[UIViewController alloc] init];
}

- (void)resetModal {
  _mediator = nil;
  _viewController = nil;
}

@end

// Test fixture for InfobarModalOverlayCoordinator.
class InfobarModalOverlayCoordinatorTest : public PlatformTest {
 public:
  InfobarModalOverlayCoordinatorTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())),
        request_(OverlayRequest::CreateWithConfig<ModalConfig>()),
        root_view_controller_([[UIViewController alloc] init]),
        coordinator_([[FakeInfobarModalOverlayCoordinator alloc]
            initWithBaseViewController:root_view_controller_
                               browser:browser_.get()
                               request:request_.get()
                              delegate:&delegate_]) {
    scoped_window_.Get().rootViewController = root_view_controller_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  MockOverlayRequestCoordinatorDelegate delegate_;
  std::unique_ptr<OverlayRequest> request_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  FakeInfobarModalOverlayCoordinator* coordinator_ = nil;
};

// Tests the modal presentation flow for a FakeInfobarModalOverlayCoordinator.
TEST_F(InfobarModalOverlayCoordinatorTest, ModalPresentation) {
  // Start the coordinator, expecting OverlayUIDidFinishPresentation() to be
  // executed.
  EXPECT_CALL(delegate_, OverlayUIDidFinishPresentation(request_.get()));
  [coordinator_ startAnimated:NO];

  // Wait for presentation to finish.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    UIViewController* presented_view_controller =
        root_view_controller_.presentedViewController;
    return presented_view_controller &&
           !presented_view_controller.beingPresented;
  }));

  // Verify that the view hierarchy is set up as expected.  The coordinator
  // should have added the configured modal view as the root view controller for
  // a UINavigationController, then presented it on `root_view_controller_`.
  UIViewController* presented_view_controller =
      root_view_controller_.presentedViewController;
  EXPECT_TRUE(presented_view_controller);
  EXPECT_TRUE(
      [presented_view_controller isKindOfClass:[UINavigationController class]]);
  UINavigationController* navigation_controller =
      static_cast<UINavigationController*>(presented_view_controller);
  UIViewController* navigation_root_controller =
      [navigation_controller.viewControllers firstObject];
  EXPECT_TRUE(navigation_root_controller);
  EXPECT_EQ(coordinator_.modalViewController, navigation_root_controller);

  // Verify that the mediator has been set on the coordinator and that its
  // delegate was set to the coordinator.
  OverlayRequestMediator* mediator = coordinator_.mediator;
  EXPECT_TRUE(mediator);
  EXPECT_EQ(coordinator_.modalMediator, mediator);
  EXPECT_EQ((id<OverlayRequestMediatorDelegate>)coordinator_,
            mediator.delegate);

  // Stop the coordinator, expecting OverlayUIDidFinishDismissal() to be
  // executed.
  EXPECT_CALL(delegate_, OverlayUIDidFinishDismissal(request_.get()));
  [coordinator_ stopAnimated:NO];

  // Wait for dismissal to finish.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    return !root_view_controller_.presentedViewController;
  }));
}

// Tests the modal dismiss flow for a FakeInfobarModalOverlayCoordinator.
TEST_F(InfobarModalOverlayCoordinatorTest, ModalDismiss) {
  // Start the coordinator, expecting OverlayUIDidFinishPresentation() to be
  // executed.
  EXPECT_CALL(delegate_, OverlayUIDidFinishPresentation(request_.get()));
  [coordinator_ startAnimated:NO];

  // Wait for presentation to finish.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    UIViewController* presented_view_controller =
        root_view_controller_.presentedViewController;
    return presented_view_controller &&
           !presented_view_controller.beingPresented;
  }));

  // Stop the coordinator, expecting OverlayUIDidFinishDismissal() to be
  // executed once.
  EXPECT_CALL(delegate_, OverlayUIDidFinishDismissal(request_.get())).Times(1);
  [coordinator_ stopAnimated:NO];

  // Stop coordinator again. It should be a no-op since stop has been called
  // already (i.e. No OverlayUIDidFinishDismissal called).
  [coordinator_ stopAnimated:NO];

  // Wait for dismissal to finish.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^BOOL {
    return !root_view_controller_.presentedViewController;
  }));
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/infobars/test/test_infobar_password_delegate.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test ContainerCoordinatorPositioner.
@interface TestContainerCoordinatorPositioner : NSObject <InfobarPositioner>
@property(nonatomic, strong) UIView* baseView;
@end
@implementation TestContainerCoordinatorPositioner
- (UIView*)parentView {
  return self.baseView;
}
@end

@interface FakeBaseViewController : UIViewController
@property(nonatomic, weak) InfobarContainerCoordinator* containerCoordinator;
@end

@implementation FakeBaseViewController
- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.containerCoordinator baseViewDidAppear];
}
@end

// Test fixture for testing InfobarContainerCoordinatorTest.
// TODO(crbug.com/927064): Remove once Overlays is turned on by default.
class InfobarContainerCoordinatorTest : public PlatformTest {
 protected:
  InfobarContainerCoordinatorTest()
      : browser_(std::make_unique<TestBrowser>()),
        base_view_controller_([[FakeBaseViewController alloc] init]),
        positioner_([[TestContainerCoordinatorPositioner alloc] init]),
        web_view_([[WKWebView alloc]
            initWithFrame:scoped_window_.Get().bounds
            configuration:[[WKWebViewConfiguration alloc] init]]),
        content_view_([[CRWWebViewContentView alloc]
            initWithWebView:web_view_
                 scrollView:web_view_.scrollView]) {

    // Setup WebstateList, Webstate and NavigationManager (Needed for
    // InfobarManager).
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(browser_->GetBrowserState());
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_->GetBrowserState());
    web_state->SetView(content_view_);
    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    UIScrollView* scroll_view = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    web_state->SetWebViewProxy(web_view_proxy_mock);

    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_NO_FLAGS,
                                                WebStateOpener());
    browser_->GetWebStateList()->ActivateWebStateAt(0);

    // Setup InfobarBadgeTabHelper and InfoBarManager
    InfoBarManagerImpl::CreateForWebState(
        browser_->GetWebStateList()->GetActiveWebState());
    InfobarBadgeTabHelper::CreateForWebState(
        browser_->GetWebStateList()->GetActiveWebState());

    // Setup the InfobarContainerCoordinator.
    infobar_container_coordinator_ = [[InfobarContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    base_view_controller_.containerCoordinator = infobar_container_coordinator_;
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    positioner_.baseView = base_view_controller_.view;
    infobar_container_coordinator_.positioner = positioner_;
    [infobar_container_coordinator_ start];
  }

  ~InfobarContainerCoordinatorTest() override {
    // Make sure InfobarBanner has been dismissed.
    if (infobar_container_coordinator_.infobarBannerState ==
        InfobarBannerPresentationState::Presented) {
      [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                        completion:nil];
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return infobar_container_coordinator_.infobarBannerState ==
                   InfobarBannerPresentationState::NotPresented;
          }));
    }
    [infobar_container_coordinator_ stop];
  }

  // Adds an Infobar to the InfobarManager, triggering an InfobarBanner
  // presentation.
  void AddInfobar(bool high_priority_presentation) {
    // Setup the InfobarCoordinator and InfobarDelegate.
    TestInfobarPasswordDelegate* test_infobar_delegate =
        new TestInfobarPasswordDelegate(@"Title");
    coordinator_ = [[InfobarPasswordCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate
                           type:InfobarType::kInfobarTypePasswordSave];
    coordinator_.highPriorityPresentation = high_priority_presentation;
    infobar_delegate_ = std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate>(
        test_infobar_delegate);

    GetInfobarManager()->AddInfoBar(std::make_unique<InfoBarIOS>(
        coordinator_, std::move(infobar_delegate_)));
  }

  // Adds an Infobar to the InfobarManager, triggering an InfobarBanner
  // presentation.
  void AddSecondInfobar(bool high_priority_presentation) {
    // Setup the InfobarCoordinator and InfobarDelegate.
    TestInfobarPasswordDelegate* test_infobar_delegate =
        new TestInfobarPasswordDelegate(@"Title 2");
    second_coordinator_ = [[InfobarPasswordCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate
                           type:InfobarType::kInfobarTypeSaveCard];
    second_coordinator_.highPriorityPresentation = high_priority_presentation;
    std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> infobar_delegate =
        std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate>(
            test_infobar_delegate);

    GetInfobarManager()->AddInfoBar(std::make_unique<InfoBarIOS>(
        second_coordinator_, std::move(infobar_delegate)));
  }

  // Adds a Confirm Infobar to the InfobarManager, triggering an InfobarBanner
  // presentation.
  void AddConfirmInfobar(bool high_priority_presentation) {
    // Setup the InfobarCoordinator and InfobarDelegate.
    TestInfoBarDelegate* test_infobar_delegate =
        new TestInfoBarDelegate(@"Title 3");
    confirm_coordinator_ = [[InfobarConfirmCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate
                   badgeSupport:NO
                           type:InfobarType::kInfobarTypeConfirm];
    confirm_coordinator_.highPriorityPresentation = high_priority_presentation;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar_delegate =
        std::unique_ptr<ConfirmInfoBarDelegate>(test_infobar_delegate);

    GetInfobarManager()->AddInfoBar(std::make_unique<InfoBarIOS>(
        confirm_coordinator_, std::move(infobar_delegate)));
  }

  void AddSecondWebstate() {
    auto second_web_state = std::make_unique<web::FakeWebState>();
    second_web_state->SetView(content_view_);
    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    UIScrollView* scroll_view = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    second_web_state->SetWebViewProxy(web_view_proxy_mock);
    InfoBarManagerImpl::CreateForWebState(second_web_state.get());
    InfobarBadgeTabHelper::CreateForWebState(second_web_state.get());
    browser_->GetWebStateList()->InsertWebState(1, std::move(second_web_state),
                                                WebStateList::INSERT_NO_FLAGS,
                                                WebStateOpener());
  }

  // Returns InfoBarManager attached to web_state_.
  infobars::InfoBarManager* GetInfobarManager() {
    return InfoBarManagerImpl::FromWebState(
        browser_->GetWebStateList()->GetActiveWebState());
  }

  base::test::TaskEnvironment environment_;
  InfobarContainerCoordinator* infobar_container_coordinator_;
  std::unique_ptr<Browser> browser_;
  web::FakeNavigationManager* navigation_manager_;
  ScopedKeyWindow scoped_key_window_;
  FakeBaseViewController* base_view_controller_;
  TestContainerCoordinatorPositioner* positioner_;
  InfobarPasswordCoordinator* coordinator_;
  InfobarPasswordCoordinator* second_coordinator_;
  InfobarConfirmCoordinator* confirm_coordinator_;
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> infobar_delegate_;
  ScopedKeyWindow scoped_window_;
  WKWebView* web_view_ = nil;
  CRWWebViewContentView* content_view_ = nil;
};

// Tests infobarBannerState is InfobarBannerPresentationState::Presented once an
// InfobarBanner is presented.
TEST_F(InfobarContainerCoordinatorTest,
       InfobarBannerPresentationStatePresented) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  EXPECT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
  AddInfobar(/*high_priority_presentation=*/false);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is automatically dismissed after
// kInfobarBannerPresentationDurationInSeconds seconds.
TEST_F(InfobarContainerCoordinatorTest, TestAutomaticInfobarBannerDismissal) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  EXPECT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  AddInfobar(/*high_priority_presentation=*/false);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kInfobarBannerDefaultPresentationDurationInSeconds, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is correctly dismissed after calling
// dismissInfobarBannerAnimated.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarBannerDismissal) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  EXPECT_FALSE(infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented);

  AddInfobar(/*high_priority_presentation=*/false);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is dismissed when changing Webstates.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerDismissAtWebStateChange) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondWebstate();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  browser_->GetWebStateList()->ActivateWebStateAt(1);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
}

// Tests that the InfobarBanner is not presented again after returning from a
// different Webstate.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerNotPresentAfterWebStateChange) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondWebstate();

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  browser_->GetWebStateList()->ActivateWebStateAt(1);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !(infobar_container_coordinator_.infobarBannerState ==
                 InfobarBannerPresentationState::Presented);
      }));
  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  browser_->GetWebStateList()->ActivateWebStateAt(0);
  // Wait for any potential presentation. This value was initially 1 second but
  // started to cause Flake on iOS13, this seems to be fixed when we change it
  // to 2 seconds. If this happens again with a different iOS version or device,
  // etc. or the test keeps flaking, then it should probably be redesigned.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(2));

  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
}

// Tests infobarBannerState is NotPresented once an InfobarBanner has been
// dismissed directly by its base VC.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarBannerDismissalByBaseVC) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
}

// Tests that the Infobar is dismissed before its presentation is completed.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerDismissalMidPresentation) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  // Call dismiss without calling WaitUntilConditionOrTimeout before.
  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
}

// Tests that the Infobar is dismissed by closing the Webstate before its
// presentation is completed.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarBannerDismissedClosingWebstate) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  // Close the Webstate without calling WaitUntilConditionOrTimeout.
  browser_->GetWebStateList()->CloseWebStateAt(0, 0);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
}

// Tests that the Infobar is dismissed when both the VC and Webstate are closed.
TEST_F(InfobarContainerCoordinatorTest, TestDismissingAndClosingWebstate) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
  browser_->GetWebStateList()->CloseWebStateAt(0, 0);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
}

// Tests that the Infobar is dismissed when both the VC and Webstate are closed,
// and there's more than one webstate.
TEST_F(InfobarContainerCoordinatorTest,
       TestDismissingAndClosingWebstateSecondWebstate) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondWebstate();
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
  browser_->GetWebStateList()->CloseWebStateAt(0, 0);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
}

// Tests that the ChildCoordinators are deleted once the Webstate is closed.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarChildCoordinatorCountWebstate) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  AddSecondInfobar(/*high_priority_presentation=*/false);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  browser_->GetWebStateList()->CloseWebStateAt(0, 0);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));
  ASSERT_NE(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
  ASSERT_EQ(NSUInteger(0),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that the ChildCoordinators are deleted once they stop.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarChildCoordinatorCountStop) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  AddSecondInfobar(/*high_priority_presentation=*/false);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return infobar_container_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  // Stop the first Coordinator.
  [coordinator_ stop];
  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);

  // Stop the second Coordinator.
  [second_coordinator_ stop];
  ASSERT_EQ(NSUInteger(0),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that that a second Infobar (added right after the first one) is
// displayed after the first one has been dismissed.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarQueueAndDisplay) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondInfobar(/*high_priority_presentation=*/false);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that Infobars added while the baseVC is not in window will be displayed
// once the baseVC moves to it. Also tests that a non high-priority Infobar
// added after a high priority one will appear first.
TEST_F(InfobarContainerCoordinatorTest,
       TestInfobarQueueAndDisplayWhenAppeared) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  [scoped_key_window_.Get() setRootViewController:nil];
  AddInfobar(/*high_priority_presentation=*/true);
  AddSecondInfobar(/*high_priority_presentation=*/false);

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  [scoped_key_window_.Get() setRootViewController:base_view_controller_];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that that a second Infobar (added right after the first one) is
// not displayed if its destroyed before presentation.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarQueueStoppedNoDisplay) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondInfobar(/*high_priority_presentation=*/false);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [second_coordinator_ stop];
  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that a High Priority Presentation Infobar added after a non High
// Priority Presentation Infobar is presented first.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarQueuePriority) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  [scoped_key_window_.Get() setRootViewController:nil];
  AddInfobar(/*high_priority_presentation=*/false);
  AddSecondInfobar(/*high_priority_presentation=*/true);

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  [scoped_key_window_.Get() setRootViewController:base_view_controller_];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(second_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that a High Priority Presentation Infobar added after a High
// Priority Presentation Infobar is presented first.
TEST_F(InfobarContainerCoordinatorTest, TestInfobarQueueHighPriority) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  [scoped_key_window_.Get() setRootViewController:nil];
  AddInfobar(/*high_priority_presentation=*/true);
  AddSecondInfobar(/*high_priority_presentation=*/true);

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);

  [scoped_key_window_.Get() setRootViewController:base_view_controller_];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(second_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return second_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  ASSERT_EQ(NSUInteger(2),
            infobar_container_coordinator_.childCoordinators.count);
}

// Tests that a Confirm Infobar is stopped after it has been dismissed.
TEST_F(InfobarContainerCoordinatorTest, TestConfirmInfobarStoppedOnDismissal) {
  if (IsInfobarOverlayUIEnabled()) {
    return;
  }
  AddConfirmInfobar(/*high_priority_presentation=*/false);
  ASSERT_EQ(NSUInteger(1),
            infobar_container_coordinator_.childCoordinators.count);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return confirm_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::Presented;
      }));
  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::Presented);

  [infobar_container_coordinator_ dismissInfobarBannerAnimated:NO
                                                    completion:nil];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return confirm_coordinator_.infobarBannerState ==
               InfobarBannerPresentationState::NotPresented;
      }));

  ASSERT_EQ(infobar_container_coordinator_.infobarBannerState,
            InfobarBannerPresentationState::NotPresented);
  ASSERT_EQ(NSUInteger(0),
            infobar_container_coordinator_.childCoordinators.count);
}

// TODO(crbug.com/961343): Add tests that use a BadgedInfobar, in order to do
// this a new TestInfoBarDelegate needs to be created.

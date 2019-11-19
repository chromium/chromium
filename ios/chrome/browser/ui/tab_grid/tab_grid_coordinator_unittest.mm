// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"
#import "ios/chrome/test/block_cleanup_test.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestTabSwitcherDelegate : NSObject<TabSwitcherDelegate>
@property(nonatomic) BOOL didEndCalled;
@end

@implementation TestTabSwitcherDelegate
@synthesize didEndCalled = _didEndCalled;
- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    shouldFinishWithActiveModel:(TabModel*)tabModel
                   focusOmnibox:(BOOL)focusOmnibox {
  // No-op.
}

- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  self.didEndCalled = YES;
}
@end

namespace {

class TabGridCoordinatorTest : public BlockCleanupTest {
 public:
  TabGridCoordinatorTest() {
    UIWindow* window = [UIApplication sharedApplication].keyWindow;
    coordinator_ = [[TabGridCoordinator alloc]
                    initWithWindow:window
        applicationCommandEndpoint:OCMProtocolMock(
                                       @protocol(ApplicationCommands))];
    coordinator_.animationsDisabledForTesting = YES;
    // TabGirdCoordinator will make its view controller the root, so stash the
    // original root view controller before starting |coordinator_|.
    original_root_view_controller_ =
        [[[UIApplication sharedApplication] keyWindow] rootViewController];

    [coordinator_ start];

    delegate_ = [[TestTabSwitcherDelegate alloc] init];
    coordinator_.tabSwitcher.delegate = delegate_;

    normal_tab_view_controller_ = [[UIViewController alloc] init];
    normal_tab_view_controller_.view.frame = CGRectMake(20, 20, 10, 10);

    incognito_tab_view_controller_ = [[UIViewController alloc] init];
    incognito_tab_view_controller_.view.frame = CGRectMake(40, 40, 10, 10);
  }

  ~TabGridCoordinatorTest() override {}

  void TearDown() override {
    if (original_root_view_controller_) {
      [[UIApplication sharedApplication] keyWindow].rootViewController =
          original_root_view_controller_;
      original_root_view_controller_ = nil;
    }
  }

 protected:
  // The TabGridCoordinator that is under test.  The test fixture sets
  // this VC as the root VC for the window.
  TabGridCoordinator* coordinator_;

  // Delegate for the coordinator's TabSwitcher interface.
  TestTabSwitcherDelegate* delegate_;

  // The key window's original root view controller, which must be restored at
  // the end of the test.
  UIViewController* original_root_view_controller_;

  // The following view controllers are created by the test fixture and are
  // available for use in tests.
  UIViewController* normal_tab_view_controller_;
  UIViewController* incognito_tab_view_controller_;
};

// Tests that the tab grid view controller is the initial active view
// controller.
TEST_F(TabGridCoordinatorTest, InitialActiveViewController) {
  EXPECT_EQ(coordinator_.baseViewController, coordinator_.activeViewController);
}

// Tests that it is possible to set a TabViewController without first setting a
// TabSwitcher.
TEST_F(TabGridCoordinatorTest, TabViewControllerBeforeTabSwitcher) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  // Now setting a TabSwitcher will make the switcher active.
  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];
  EXPECT_EQ([coordinator_.tabSwitcher viewController],
            coordinator_.activeViewController);
}

// Tests that it is possible to set a TabViewController after setting a
// TabSwitcher.
TEST_F(TabGridCoordinatorTest, TabViewControllerAfterTabSwitcher) {
  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];
  EXPECT_EQ([coordinator_.tabSwitcher viewController],
            coordinator_.activeViewController);

  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  // Showing the TabSwitcher again will make it active.
  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];
  EXPECT_EQ([coordinator_.tabSwitcher viewController],
            coordinator_.activeViewController);
}

// Tests swapping between two TabViewControllers.
TEST_F(TabGridCoordinatorTest, SwapTabViewControllers) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showTabViewController:incognito_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(incognito_tab_view_controller_, coordinator_.activeViewController);
}

// Tests calling showTabSwitcher twice in a row with the same VC.
TEST_F(TabGridCoordinatorTest, ShowTabSwitcherTwice) {
  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];
  EXPECT_EQ([coordinator_.tabSwitcher viewController],
            coordinator_.activeViewController);

  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];
  EXPECT_EQ([coordinator_.tabSwitcher viewController],
            coordinator_.activeViewController);
}

// Tests calling showTabViewController twice in a row with the same VC.
TEST_F(TabGridCoordinatorTest, ShowTabViewControllerTwice) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);
}

// Tests that setting the active view controller work and that completion
// handlers are called properly after the new view controller is made active.
TEST_F(TabGridCoordinatorTest, CompletionHandlers) {
  // Setup: show the switcher.
  [coordinator_ showTabSwitcher:coordinator_.tabSwitcher];

  // Tests that the completion handler is called when showing a tab view
  // controller. Tests that the delegate 'didEnd' method is also called.
  delegate_.didEndCalled = NO;
  __block BOOL completion_handler_was_called = NO;
  [coordinator_ showTabViewController:normal_tab_view_controller_
                           completion:^{
                             completion_handler_was_called = YES;
                           }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_TRUE(delegate_.didEndCalled);

  // Tests that the completion handler is called when replacing an existing tab
  // view controller. Tests that the delegate 'didEnd' method is *not* called.
  delegate_.didEndCalled = NO;
  [coordinator_ showTabViewController:incognito_tab_view_controller_
                           completion:^{
                             completion_handler_was_called = YES;
                           }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_FALSE(delegate_.didEndCalled);
}

// Test that the tab grid coordinator sizes its view controller to the window.
TEST_F(TabGridCoordinatorTest, SizeTabGridCoordinatorViewController) {
  CGRect rect = [UIScreen mainScreen].bounds;
  [coordinator_ start];
  EXPECT_TRUE(
      CGRectEqualToRect(rect, coordinator_.baseViewController.view.frame));
}

}  // namespace

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/selector_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/elements/selector_picker_view_controller.h"
#import "ios/chrome/browser/ui/elements/selector_view_controller_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SelectorCoordinator ()<SelectorViewControllerDelegate>
// The view controller for the picker view the coordinator presents. Exposed for
// testing.
@property SelectorPickerViewController* selectorPickerViewController;
@end

using SelectorCoordinatorTest = PlatformTest;

// Tests that invoking start on the coordinator presents the selector view, and
// that invoking stop dismisses the view and invokes the delegate.
TEST_F(SelectorCoordinatorTest, StartAndStop) {
  base::test::TaskEnvironment task_environment_;
  ScopedKeyWindow scopedKeyWindow;
  UIWindow* keyWindow = scopedKeyWindow.Get();
  UIViewController* rootViewController = keyWindow.rootViewController;
  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state.get());
  SelectorCoordinator* coordinator =
      [[SelectorCoordinator alloc] initWithBaseViewController:rootViewController
                                                      browser:browser.get()];

  void (^testSteps)(void) = ^{
    [coordinator start];
    EXPECT_NSEQ(coordinator.selectorPickerViewController,
                rootViewController.presentedViewController);

    [coordinator stop];
    bool success = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^{
      return !rootViewController.presentedViewController;
    });
    EXPECT_TRUE(success);
  };
  // Ensure any other presented controllers are dismissed before starting the
  // coordinator.
  [rootViewController dismissViewControllerAnimated:NO completion:testSteps];
}

// Tests that calling the view controller delegate method invokes the
// SelectorCoordinatorDelegate method and stops the coordinator.
TEST_F(SelectorCoordinatorTest, Delegate) {
  base::test::TaskEnvironment task_environment_;

  ScopedKeyWindow scopedKeyWindow;
  UIWindow* keyWindow = scopedKeyWindow.Get();
  UIViewController* rootViewController = keyWindow.rootViewController;
  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state.get());
  SelectorCoordinator* coordinator =
      [[SelectorCoordinator alloc] initWithBaseViewController:rootViewController
                                                      browser:browser.get()];
  id delegate =
      [OCMockObject mockForProtocol:@protocol(SelectorCoordinatorDelegate)];
  coordinator.delegate = delegate;

  void (^testSteps)(void) = ^{
    [coordinator start];
    NSString* testOption = @"Test Option";
    [[delegate expect] selectorCoordinator:coordinator
                  didCompleteWithSelection:testOption];
    [coordinator selectorViewController:coordinator.selectorPickerViewController
                        didSelectOption:testOption];
    bool success = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^{
      return !rootViewController.presentedViewController;
    });
    EXPECT_TRUE(success);
  };
  // Ensure any other presented controllers are dismissed before starting the
  // coordinator.
  [rootViewController dismissViewControllerAnimated:NO completion:testSteps];
  EXPECT_OCMOCK_VERIFY(delegate);
}

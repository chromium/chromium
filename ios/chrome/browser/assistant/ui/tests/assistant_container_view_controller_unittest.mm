// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expose private methods needed for testing.
@interface AssistantContainerViewController (ExposedMethods)
- (void)updateHeightConstraint;
- (NSInteger)absoluteMaxHeight;
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture;
@end

// Expose accessors for private properties.
@interface AssistantContainerViewController (TestingHelpers)
@property(nonatomic, readonly) NSLayoutConstraint* heightConstraint;
@property(nonatomic, readonly) UIPanGestureRecognizer* headerPanGesture;
@end

// Implementation of property testing helpers using KVC.
@implementation AssistantContainerViewController (TestingHelpers)
- (NSLayoutConstraint*)heightConstraint {
  return [self valueForKey:@"_heightConstraint"];
}
- (UIPanGestureRecognizer*)headerPanGesture {
  return [self valueForKey:@"_headerPanGesture"];
}
@end

namespace {

class AssistantContainerViewControllerTest : public PlatformTest {
 protected:
  AssistantContainerViewControllerTest() {
    UIViewController* child = [[UIViewController alloc] init];
    view_controller_ =
        [[AssistantContainerViewController alloc] initWithViewController:child];

    // Setup view hierarchy with fixed bounds.
    window_ = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 580)];
    window_.backgroundColor = [UIColor whiteColor];
    [window_ makeKeyAndVisible];  // Ensure it behaves like a real window
    [window_ addSubview:view_controller_.view];

    // Constraint to bottom of window to ensure MaxY is valid for maxHeight
    // calculation.
    view_controller_.view.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [view_controller_.view.leadingAnchor
          constraintEqualToAnchor:window_.leadingAnchor],
      [view_controller_.view.trailingAnchor
          constraintEqualToAnchor:window_.trailingAnchor],
      [view_controller_.view.bottomAnchor
          constraintEqualToAnchor:window_.bottomAnchor],
    ]];

    // Trigger loadView and viewDidLoad.
    [window_ layoutIfNeeded];

    minimized_detent_ = AssistantContainerFixedDetent(
        100.0, kAssistantContainerMinimizedDetentIdentifier);
    large_detent_ = AssistantContainerFixedDetent(
        300.0, kAssistantContainerLargeDetentIdentifier);
  }

  AssistantContainerViewController* view_controller_;
  UIWindow* window_;
  AssistantContainerDetent* minimized_detent_;
  AssistantContainerDetent* large_detent_;
};

// Tests that the container snaps to the nearest detent.
TEST_F(AssistantContainerViewControllerTest, SnapsToNearestDetent) {
  [view_controller_ setDetents:@[ minimized_detent_, large_detent_ ]];

  // Simulate current height closer to small detent.
  view_controller_.heightConstraint.constant = 120.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant, 100.0);

  // Simulate current height closer to large detent.
  view_controller_.heightConstraint.constant = 280.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant, 300.0);
}

// Tests that the container snaps back to the only detent.
TEST_F(AssistantContainerViewControllerTest, SnapsToSingleDetent) {
  [view_controller_ setDetents:@[ minimized_detent_ ]];

  // Simulate current height being off target.
  view_controller_.heightConstraint.constant = 250.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant, 100.0);
}

// Tests that specific limits are respected.
TEST_F(AssistantContainerViewControllerTest, RespectsLimits) {
  AssistantContainerDetent* hugeDetent =
      AssistantContainerFixedDetent(1000.0, @"huge");

  [view_controller_ setDetents:@[ hugeDetent ]];
  [view_controller_ updateHeightConstraint];

  // Should clamp to max height.
  EXPECT_NEAR(view_controller_.heightConstraint.constant,
              [view_controller_ absoluteMaxHeight], 0.1);
}

// Tests that the container cannot be effectively resized if only one detent is
// provided. When the drag is released, the container should snap back to
// the single detent.
TEST_F(AssistantContainerViewControllerTest, OneDetentPreventsResizing) {
  // Set 1 detent.
  [view_controller_ setDetents:@[ minimized_detent_ ]];

  // Simulate a drag extending the height.
  view_controller_.heightConstraint.constant = 250.0;

  // Update constraints (simulating gesture end).
  [view_controller_ updateHeightConstraint];

  // Verify it snaps back to the single detent size.
  EXPECT_EQ(view_controller_.heightConstraint.constant, 100.0);
}

// Tests that the delegate can intercept the pan gesture.
TEST_F(AssistantContainerViewControllerTest, DelegateInterceptsPanGesture) {
  id delegate_mock =
      OCMStrictProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect([delegate_mock
                       assistantContainer:view_controller_
                shouldInterceptPanGesture:view_controller_.headerPanGesture])
      .andReturn(YES);

  [view_controller_ handlePanGesture:view_controller_.headerPanGesture];

  EXPECT_OCMOCK_VERIFY(delegate_mock);
}

// Tests that the container processes the pan gesture when the delegate allows
// it.
TEST_F(AssistantContainerViewControllerTest, DelegateAllowsPanGesture) {
  id delegate_mock =
      OCMStrictProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect([delegate_mock
                       assistantContainer:view_controller_
                shouldInterceptPanGesture:view_controller_.headerPanGesture])
      .andReturn(NO);

  [view_controller_ handlePanGesture:view_controller_.headerPanGesture];

  EXPECT_OCMOCK_VERIFY(delegate_mock);
}

}  // namespace

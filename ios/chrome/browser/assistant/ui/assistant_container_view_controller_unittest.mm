// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expose private methods needed for testing.
@interface AssistantContainerViewController (ExposedMethods)
- (void)updateHeightConstraint;
- (NSInteger)absoluteMaxHeight;
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture;
- (void)handleDimmingViewTap:(UITapGestureRecognizer*)gesture;
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
    view_controller_.minimizedDetentHeight =
        kAssistantContainerMinimizedDetentHeight;

    // Setup view hierarchy with fixed bounds.
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window_.frame = CGRectMake(0, 0, 320, 580);
    window_.backgroundColor = [UIColor whiteColor];

    UIViewController* rootVC = [[UIViewController alloc] init];
    rootVC.view.frame = window_.bounds;

    [rootVC addChildViewController:view_controller_];
    [rootVC.view addSubview:view_controller_.view];
    [view_controller_ didMoveToParentViewController:rootVC];

    view_controller_.view.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(view_controller_.view, rootVC.view);

    window_.rootViewController = rootVC;
    [window_ makeKeyAndVisible];

    // Trigger loadView and viewDidLoad.
    [window_ layoutIfNeeded];
  }

  AssistantContainerViewController* view_controller_;
  UIWindow* window_;
};

// Tests that the container snaps to the nearest detent.
TEST_F(AssistantContainerViewControllerTest, SnapsToNearestDetent) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  // Simulate current height closer to small detent.
  view_controller_.heightConstraint.constant =
      kAssistantContainerMinimizedDetentHeight + 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));

  // Simulate current height closer to large detent.
  view_controller_.heightConstraint.constant =
      [view_controller_ absoluteMaxHeight] - 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            [view_controller_ absoluteMaxHeight]);
}

// Tests that the container snaps back to the only detent.
TEST_F(AssistantContainerViewControllerTest, SnapsToSingleDetent) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized}];

  // Simulate current height being off target.
  view_controller_.heightConstraint.constant =
      [view_controller_ absoluteMaxHeight] / 2.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));
}

// Tests that updating the minimizedDetentHeight property dynamically updates
// the active constraints and heights.
TEST_F(AssistantContainerViewControllerTest, UpdatesMinimizedDetentHeight) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized}];

  // Verify the default minimal height remains valid.
  view_controller_.heightConstraint.constant =
      kAssistantContainerMinimizedDetentHeight - 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));

  // Update the minimal height assignment.
  CGFloat new_min_height = kAssistantContainerMinimizedDetentHeight + 50.0;
  view_controller_.minimizedDetentHeight = new_min_height;

  // The active constraint must now correctly lock to the new min height.
  view_controller_.heightConstraint.constant = new_min_height - 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant, new_min_height);
}

// Tests that specific limits are respected.
TEST_F(AssistantContainerViewControllerTest, RespectsLimits) {
  [view_controller_ setDetents:{AssistantContainerDetent::kLarge}];
  [view_controller_ updateHeightConstraint];

  // Should clamp to max height.
  EXPECT_NEAR(view_controller_.heightConstraint.constant,
              [view_controller_ absoluteMaxHeight], 0.1);
}

// Tests that setting detents in an unsorted order properly sorts them,
// ensuring limits successfully clamp back to valid boundaries.
TEST_F(AssistantContainerViewControllerTest, SortsDetentsCorrectly) {
  // Pass them in shuffled order: large, small, medium.
  [view_controller_ setDetents:{AssistantContainerDetent::kLarge,
                                AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kMedium}];

  // Implicitly verify sorting structure by clamping to extremes and seeing if
  // it correctly attaches to valid boundaries, avoiding out-of-order crashes.
  view_controller_.heightConstraint.constant =
      kAssistantContainerMinimizedDetentHeight - 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));

  view_controller_.heightConstraint.constant =
      [view_controller_ absoluteMaxHeight] + 10.0;
  [view_controller_ updateHeightConstraint];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            [view_controller_ absoluteMaxHeight]);
}

// Tests that the container cannot be effectively resized if only one detent is
// provided. When the drag is released, the container should snap back to
// the single detent.
TEST_F(AssistantContainerViewControllerTest, OneDetentPreventsResizing) {
  // Set 1 detent.
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized}];

  // Simulate a drag extending the height.
  view_controller_.heightConstraint.constant =
      [view_controller_ absoluteMaxHeight] / 2.0;

  // Update constraints (simulating gesture end).
  [view_controller_ updateHeightConstraint];

  // Verify it snaps back to the single detent size.
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));
}

// Tests that the delegate can intercept the pan gesture.
TEST_F(AssistantContainerViewControllerTest, DelegateInterceptsPanGesture) {
  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
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
  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect([delegate_mock
                       assistantContainer:view_controller_
                shouldInterceptPanGesture:view_controller_.headerPanGesture])
      .andReturn(NO);

  [view_controller_ handlePanGesture:view_controller_.headerPanGesture];

  EXPECT_OCMOCK_VERIFY(delegate_mock);
}

// Tests that passing an invalid detent to animateToDetent is safely ignored.
TEST_F(AssistantContainerViewControllerTest, AnimateToDetentInvalidIdentifier) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kMedium}];

  CGFloat initial_height = view_controller_.heightConstraint.constant;

  // Attempt to animate to a non-existent detent.
  [view_controller_ animateToDetent:AssistantContainerDetent::kLarge
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  // The height should remain unchanged.
  EXPECT_EQ(initial_height, view_controller_.heightConstraint.constant);
}

// Tests that passing a valid identifier updates the height constraint.
TEST_F(AssistantContainerViewControllerTest, AnimateToDetentValid) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect([delegate_mock assistantContainer:view_controller_
       animateAlongsideTransitionToPercentage:1.0]);

  // Expect the delegate to be notified of the detent change.
  OCMExpect([delegate_mock
      assistantContainer:view_controller_
         didChangeDetent:AssistantContainerDetent::kLarge]);

  [view_controller_ animateToDetent:AssistantContainerDetent::kLarge
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  // Height constraint should now be exactly the large detent value.
  EXPECT_EQ([view_controller_ absoluteMaxHeight],
            view_controller_.heightConstraint.constant);

  EXPECT_OCMOCK_VERIFY(delegate_mock);
}

// Tests that tapping the dimming view when in the large detent dismisses the
// container back to the minimized detent.
TEST_F(AssistantContainerViewControllerTest, HandleDimmingViewTap) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  // Force the container to the large detent to simulate an expanded state.
  [view_controller_ animateToDetent:AssistantContainerDetent::kLarge
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  // Verify it is at the large detent.
  EXPECT_EQ([view_controller_ absoluteMaxHeight],
            view_controller_.heightConstraint.constant);

  // Simulate a tap on the dimming view.
  UITapGestureRecognizer* dummy_gesture = [[UITapGestureRecognizer alloc] init];
  [view_controller_ performSelector:@selector(handleDimmingViewTap:)
                         withObject:dummy_gesture];

  // After the tap, it should have triggered a transition to minimized.
  [view_controller_.view layoutIfNeeded];
  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));
}

// Tests that LayoutState updates trigger layout updates.
TEST_F(AssistantContainerViewControllerTest, UpdatesLayoutOnLayoutStateChange) {
  LayoutState* layout_state = [[LayoutState alloc] init];
  view_controller_.layoutState = layout_state;

  // Initially unsupported, should be in sheet mode.
  EXPECT_EQ(view_controller_.presentationContext,
            AssistantPresentationContext::kSheet);

  // Update state to supported.
  layout_state.containedLayoutSupported = YES;

  // Should switch to panel mode.
  EXPECT_EQ(view_controller_.presentationContext,
            AssistantPresentationContext::kPanel);
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
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
- (void)handleGrabberButtonTapped:(UIButton*)sender;
- (BOOL)isGrabberHidden;
@end

// Expose accessors for private properties.
@interface AssistantContainerViewController (TestingHelpers)
@property(nonatomic, readonly) NSLayoutConstraint* heightConstraint;
@property(nonatomic, readonly) UIPanGestureRecognizer* headerPanGesture;
@property(nonatomic, readonly) NSLayoutConstraint* outerBottomConstraint;
@property(nonatomic, readonly) UIButton* grabberButton;
@property(nonatomic, assign) BOOL isAnimating;
@end

// Implementation of property testing helpers using KVC.
@implementation AssistantContainerViewController (TestingHelpers)
- (NSLayoutConstraint*)heightConstraint {
  return [self valueForKey:@"_heightConstraint"];
}
- (UIPanGestureRecognizer*)headerPanGesture {
  return [self valueForKey:@"_headerPanGesture"];
}
- (NSLayoutConstraint*)outerBottomConstraint {
  return [self valueForKey:@"_outerBottomConstraint"];
}
- (UIButton*)grabberButton {
  return [[self valueForKey:@"_assistantContainerView"]
      valueForKey:@"_grabberButton"];
}
@end

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

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

// Tests that the delegate is notified of detent height updates on layout and
// orientation changes.
TEST_F(AssistantContainerViewControllerTest,
       NotifiesDelegateOnDetentHeightUpdates) {
  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  // Initial layout should notify the delegate.
  OCMExpect([delegate_mock
      assistantContainerDidUpdateDetentHeights:view_controller_]);
  [view_controller_.view setNeedsLayout];
  [view_controller_.view layoutIfNeeded];
  EXPECT_OCMOCK_VERIFY(delegate_mock);

  // Orientation / size transition should notify the delegate again.
  id<UIViewControllerTransitionCoordinator> mock_coordinator =
      OCMProtocolMock(@protocol(UIViewControllerTransitionCoordinator));
  OCMStub([mock_coordinator animateAlongsideTransition:[OCMArg any]
                                            completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(id<UIViewControllerTransitionCoordinatorContext>) =
            nil;
        [invocation getArgument:&completion atIndex:3];
        if (completion) {
          completion(nil);
        }
      });

  OCMExpect([delegate_mock
      assistantContainerDidUpdateDetentHeights:view_controller_]);
  [view_controller_ viewWillTransitionToSize:CGSizeMake(580, 320)
                   withTransitionCoordinator:mock_coordinator];
  [view_controller_.view setNeedsLayout];
  [view_controller_.view layoutIfNeeded];

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

// Tests that tapping the grabber button cycles through detents.
TEST_F(AssistantContainerViewControllerTest, CyclesThroughDetentsOnTap) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kMedium,
                                AssistantContainerDetent::kLarge}];

  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  // Force the container to the minimized detent to start.
  [view_controller_ animateToDetent:AssistantContainerDetent::kMinimized
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  // Tap 1: Minimized -> Medium.
  OCMExpect([delegate_mock
      assistantContainer:view_controller_
         didChangeDetent:AssistantContainerDetent::kMedium]);
  [view_controller_ handleGrabberButtonTapped:nil];
  EXPECT_OCMOCK_VERIFY(delegate_mock);
  view_controller_.isAnimating = NO;
  // Tap 2: Medium -> Large.
  OCMExpect([delegate_mock
      assistantContainer:view_controller_
         didChangeDetent:AssistantContainerDetent::kLarge]);
  [view_controller_ handleGrabberButtonTapped:nil];
  EXPECT_OCMOCK_VERIFY(delegate_mock);
  view_controller_.isAnimating = NO;

  // Tap 3: Large -> Minimized.
  OCMExpect([delegate_mock
      assistantContainer:view_controller_
         didChangeDetent:AssistantContainerDetent::kMinimized]);
  [view_controller_ handleGrabberButtonTapped:nil];
  EXPECT_OCMOCK_VERIFY(delegate_mock);
  view_controller_.isAnimating = NO;
}

// Tests that SceneLayoutState updates trigger layout updates.
TEST_F(AssistantContainerViewControllerTest, UpdatesLayoutOnLayoutStateChange) {
  web::WebTaskEnvironment task_environment;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  FakeSceneState* scene_state =
      [[FakeSceneState alloc] initWithProfile:profile.get()];
  std::unique_ptr<TestBrowser> browser =
      std::make_unique<TestBrowser>(profile.get(), scene_state);
  BrowserLayoutState* layout_state = browser->GetBrowserLayoutState();

  view_controller_.browserLayoutState = layout_state;
  view_controller_.sceneLayoutState = scene_state.layoutState;

  // Initially unsupported, should be in sheet mode.
  EXPECT_EQ(view_controller_.presentationContext,
            AssistantPresentationContext::kSheet);

  // Update state to supported.
  [scene_state.layoutState
      setContainedLayoutSupported:YES
                          passKey:LayoutStateTestPassKeyFactory::
                                      CreateSceneKey()];

  // Should switch to panel mode.
  EXPECT_EQ(view_controller_.presentationContext,
            AssistantPresentationContext::kPanel);
  view_controller_.browserLayoutState = nil;
  [scene_state shutdown];
}

// Tests that performing accessibility escape when in the large detent moves
// the container to the minimized detent.
TEST_F(AssistantContainerViewControllerTest,
       AccessibilityPerformEscapeToMinimized) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  [view_controller_ animateToDetent:AssistantContainerDetent::kLarge
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  BOOL handled = [view_controller_ accessibilityPerformEscape];
  EXPECT_TRUE(handled);
  [window_ layoutIfNeeded];

  EXPECT_EQ(view_controller_.heightConstraint.constant,
            static_cast<CGFloat>(kAssistantContainerMinimizedDetentHeight));
}

// Tests that performing accessibility escape when in the minimized detent
// requests dismissal from the delegate.
TEST_F(AssistantContainerViewControllerTest,
       AccessibilityPerformEscapeRequestsDismissal) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  [view_controller_ animateToDetent:AssistantContainerDetent::kMinimized
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  id delegate_mock = OCMProtocolMock(@protocol(AssistantContainerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect(
      [delegate_mock assistantContainerDidRequestDismissal:view_controller_]);

  BOOL handled = [view_controller_ accessibilityPerformEscape];
  EXPECT_TRUE(handled);

  EXPECT_OCMOCK_VERIFY(delegate_mock);
}

// Tests that keyCommands contains the escape key command and
// canBecomeFirstResponder is YES.
TEST_F(AssistantContainerViewControllerTest, KeyCommandsAndFirstResponder) {
  EXPECT_TRUE(view_controller_.canBecomeFirstResponder);

  NSArray<UIKeyCommand*>* commands = view_controller_.keyCommands;
  ASSERT_EQ(1u, commands.count);
  UIKeyCommand* escapeCommand = commands.firstObject;
  EXPECT_TRUE(
      [escapeCommand.input isEqualToString:UIKeyCommand.cr_close.input]);
  EXPECT_EQ(escapeCommand.modifierFlags, UIKeyCommand.cr_close.modifierFlags);
  EXPECT_EQ(escapeCommand.action, @selector(keyCommand_close));
}

// Tests that performing accessibility escape when kMinimized is already active
// and no delegate is set returns NO.
TEST_F(AssistantContainerViewControllerTest,
       AccessibilityPerformEscapeNotHandledWithoutDelegate) {
  [view_controller_ setDetents:{AssistantContainerDetent::kMinimized,
                                AssistantContainerDetent::kLarge}];

  [view_controller_ animateToDetent:AssistantContainerDetent::kMinimized
                           duration:0.0
                              curve:UIViewAnimationCurveEaseInOut];

  view_controller_.delegate = nil;

  BOOL handled = [view_controller_ accessibilityPerformEscape];
  EXPECT_FALSE(handled);
}

// Tests that setting guideName and layoutGuideCenter dynamically updates
// the container's bottom constraint to anchor to the resolved guide.
TEST_F(AssistantContainerViewControllerTest, AnchorsToLayoutGuide) {
  // Create a mock LayoutGuideCenter.
  NSString* const kMockGuideName = @"MockGuide";
  id layout_guide_center_mock = OCMClassMock([LayoutGuideCenter class]);

  // Create an anchor view and add it to the same view hierarchy.
  UIView* anchor_view = [[UIView alloc] init];
  anchor_view.translatesAutoresizingMaskIntoConstraints = NO;
  [window_.rootViewController.view addSubview:anchor_view];

  // Stub referencedViewUnderName: to return our anchor view.
  OCMStub([layout_guide_center_mock referencedViewUnderName:kMockGuideName])
      .andReturn(anchor_view);

  // Assign guideName and layoutGuideCenter.
  view_controller_.layoutGuideCenter = layout_guide_center_mock;
  view_controller_.guideName = kMockGuideName;

  // Trigger layout pass.
  [window_ layoutIfNeeded];

  // Retrieve the private outerBottomConstraint using the type-safe accessor.
  NSLayoutConstraint* outer_bottom_constraint =
      view_controller_.outerBottomConstraint;

  // Verify that the constraint is active and resolved against the anchor
  // view's top anchor.
  ASSERT_NE(nil, outer_bottom_constraint);
  EXPECT_TRUE(outer_bottom_constraint.active);
  EXPECT_EQ(outer_bottom_constraint.secondAnchor, anchor_view.topAnchor);
}

// Tests that setting grabberHidden dynamically hides the grabber button and
// disables header pan resizing.
TEST_F(AssistantContainerViewControllerTest,
       GrabberHiddenDisablesInteractions) {
  EXPECT_FALSE([view_controller_ isGrabberHidden]);
  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.hidden);

  [view_controller_ setGrabberHidden:YES animated:NO];
  EXPECT_TRUE([view_controller_ isGrabberHidden]);
  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_TRUE(view_controller_.grabberButton.hidden);

  [view_controller_ setGrabberHidden:NO animated:NO];
  EXPECT_FALSE([view_controller_ isGrabberHidden]);
  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.hidden);
}

// Tests that setting grabberHidden dynamically with animation hides the grabber
// button and disables header pan resizing after the animation completes.
TEST_F(AssistantContainerViewControllerTest,
       GrabberHiddenDisablesInteractionsAnimated) {
  EXPECT_FALSE([view_controller_ isGrabberHidden]);
  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.hidden);

  [view_controller_ setGrabberHidden:YES animated:YES];
  EXPECT_TRUE([view_controller_ isGrabberHidden]);
  // During animation, interactions are disabled immediately.
  EXPECT_FALSE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.enabled);

  // Wait for the animation to complete.
  constexpr double kAnimationDelayDelta = 0.1;
  const base::TimeDelta delay = base::Seconds(
      kAssistantGrabberVisibilityAnimationDuration + kAnimationDelayDelta);
  base::test::ios::SpinRunLoopWithMinDelay(delay);

  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_TRUE(view_controller_.grabberButton.hidden);

  [view_controller_ setGrabberHidden:NO animated:YES];
  EXPECT_FALSE([view_controller_ isGrabberHidden]);
  // During animation, interactions are disabled immediately.
  EXPECT_FALSE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.enabled);

  // Wait for the animation to complete.
  base::test::ios::SpinRunLoopWithMinDelay(delay);

  EXPECT_TRUE(view_controller_.headerPanGesture.enabled);
  EXPECT_FALSE(view_controller_.grabberButton.hidden);
}

}  // namespace

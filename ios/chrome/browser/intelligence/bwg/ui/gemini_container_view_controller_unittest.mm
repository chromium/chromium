// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_mutator.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeGeminiContainerMutator : NSObject <GeminiContainerMutator>
@property(nonatomic, assign) BOOL keyboardDidShowCalled;
@property(nonatomic, assign) CGFloat actuationHeight;
@end

@implementation FakeGeminiContainerMutator
- (void)containerKeyboardDidShowWithDuration:(NSTimeInterval)duration
                                       curve:(UIViewAnimationCurve)curve {
  self.keyboardDidShowCalled = YES;
}

- (void)containerDidChangeActuationHeight:(CGFloat)height {
  self.actuationHeight = height;
}
@end

namespace {

// Fitting heights of `FakeWrappingInterventionViewController`, which wraps
// below `kInterventionWrappingWidth`.
constexpr CGFloat kInterventionWrappingWidth = 250.0;
constexpr CGFloat kWideInterventionHeight = 60.0;
constexpr CGFloat kNarrowInterventionHeight = 120.0;

}  // namespace

// Intervention whose fitting height depends on its width, like a wrapping card,
// without depending on font metrics.
@interface FakeWrappingInterventionViewController
    : ActuationInterventionViewController
// Width of the last measurement.
@property(nonatomic, readonly) CGFloat lastMeasuredWidth;
// Number of measurements.
@property(nonatomic, readonly) NSUInteger measurementCount;
@end

@implementation FakeWrappingInterventionViewController
- (CGFloat)fittingHeightForWidth:(CGFloat)width {
  _lastMeasuredWidth = width;
  _measurementCount++;
  return width < kInterventionWrappingWidth ? kNarrowInterventionHeight
                                            : kWideInterventionHeight;
}
@end

namespace {

// Returns whether `view` or one of its ancestors is hidden.
bool IsHiddenInHierarchy(UIView* view) {
  for (UIView* ancestor = view; ancestor; ancestor = ancestor.superview) {
    if (ancestor.hidden) {
      return true;
    }
  }
  return false;
}

}  // namespace

class GeminiContainerViewControllerTest : public PlatformTest {
 protected:
  GeminiContainerViewControllerTest() {
    child_view_controller_ = [[UIViewController alloc] init];
    internal_text_field_ =
        [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 30)];
    [child_view_controller_.view addSubview:internal_text_field_];

    container_view_controller_ = [[GeminiContainerViewController alloc]
        initWithWorklogViewController:nil
           interventionViewController:nil];
    [container_view_controller_ setGeminiViewController:child_view_controller_];
    mutator_ = [[FakeGeminiContainerMutator alloc] init];
    container_view_controller_.mutator = mutator_;

    [scoped_key_window_.Get() addSubview:container_view_controller_.view];

    external_text_field_ =
        [[UITextField alloc] initWithFrame:CGRectMake(0, 100, 100, 30)];
    [scoped_key_window_.Get() addSubview:external_text_field_];
  }

  ~GeminiContainerViewControllerTest() override {
    [internal_text_field_ resignFirstResponder];
    [external_text_field_ resignFirstResponder];
  }

  void PostKeyboardWillShowNotification() {
    NSDictionary* user_info = @{
      UIKeyboardAnimationDurationUserInfoKey : @(0),
      UIKeyboardAnimationCurveUserInfoKey : @(UIViewAnimationCurveLinear),
    };
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIKeyboardWillShowNotification
                      object:nil
                    userInfo:user_info];
  }

  ScopedKeyWindow scoped_key_window_;
  UIViewController* child_view_controller_;
  UITextField* internal_text_field_;
  UITextField* external_text_field_;
  GeminiContainerViewController* container_view_controller_;
  FakeGeminiContainerMutator* mutator_;
};

// Tests that when an external view is first responder, keyboard notifications
// are ignored by the GeminiContainerViewController.
TEST_F(GeminiContainerViewControllerTest,
       IgnoreKeyboardWhenExternalViewIsFirstResponder) {
  [external_text_field_ becomeFirstResponder];
  EXPECT_TRUE([external_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_FALSE(mutator_.keyboardDidShowCalled);
}

// Tests that when a view inside GeminiContainerViewController is first
// responder, keyboard notifications are forwarded to the delegate.
TEST_F(GeminiContainerViewControllerTest,
       HandleKeyboardWhenInternalViewIsFirstResponder) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_TRUE(mutator_.keyboardDidShowCalled);
}

// Tests that dismissKeyboard ends editing on the view.
TEST_F(GeminiContainerViewControllerTest, TestDismissKeyboard) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  [container_view_controller_ dismissKeyboard];

  EXPECT_FALSE([internal_text_field_ isFirstResponder]);
}

// Tests that updateZeroStateVisibility updates
// zeroStateViewController view hidden state and contentHeight reflects the
// visible content.
TEST_F(GeminiContainerViewControllerTest, TestUpdateZeroStateVisibility) {
  UIViewController* zero_state_view_controller =
      [[UIViewController alloc] init];
  [zero_state_view_controller.view.heightAnchor constraintEqualToConstant:280.0]
      .active = YES;
  [child_view_controller_.view.heightAnchor constraintEqualToConstant:80.0]
      .active = YES;

  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc] initWithWorklogViewController:nil
                                                interventionViewController:nil];
  container.zeroStateViewController = zero_state_view_controller;
  [scoped_key_window_.Get() addSubview:container.view];
  [container loadViewIfNeeded];
  [container setGeminiViewController:child_view_controller_];

  [container updateZeroStateVisibility:YES];
  EXPECT_FALSE(zero_state_view_controller.view.hidden);
  EXPECT_EQ([container contentHeight], 388.0);

  [container updateZeroStateVisibility:NO];
  EXPECT_TRUE(zero_state_view_controller.view.hidden);
  EXPECT_EQ([container contentHeight], 108.0);

  [container setHasPadding:NO];
  EXPECT_EQ([container contentHeight], 80.0);

  [container updateZeroStateVisibility:YES];
  EXPECT_EQ([container contentHeight], 360.0);

  [container.view removeFromSuperview];
}

// Tests that inside content has padding on all 4 sides and does not touch the
// container edges when not minimized, and has 0 padding when minimized.
TEST_F(GeminiContainerViewControllerTest, TestContentPadding) {
  constexpr CGFloat kContainerPadding = 14.0;
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc] initWithWorklogViewController:nil
                                                interventionViewController:nil];
  [scoped_key_window_.Get() addSubview:container.view];
  container.view.frame = CGRectMake(0, 0, 300, 400);
  [container.view setNeedsLayout];
  [container.view layoutIfNeeded];

  // The last subview added to `container.view` is the content view wrapper.
  UIView* content_view = container.view.subviews.lastObject;
  ASSERT_TRUE(content_view != nil);

  EXPECT_EQ(content_view.frame.origin.x, kContainerPadding);
  EXPECT_EQ(content_view.frame.origin.y, kContainerPadding);
  EXPECT_EQ(content_view.frame.size.width,
            container.view.bounds.size.width - 2 * kContainerPadding);
  EXPECT_EQ(content_view.frame.size.height,
            container.view.bounds.size.height - 2 * kContainerPadding);

  // When minimized, padding should be 0.
  [container setHasPadding:NO];
  [container.view setNeedsLayout];
  [container.view layoutIfNeeded];

  EXPECT_EQ(content_view.frame.origin.x, 0.0);
  EXPECT_EQ(content_view.frame.origin.y, 0.0);
  EXPECT_EQ(content_view.frame.size.width, container.view.bounds.size.width);
  EXPECT_EQ(content_view.frame.size.height, container.view.bounds.size.height);

  // When expanded again, padding is restored.
  [container setHasPadding:YES];
  [container.view setNeedsLayout];
  [container.view layoutIfNeeded];

  EXPECT_EQ(content_view.frame.origin.x, kContainerPadding);
  EXPECT_EQ(content_view.frame.origin.y, kContainerPadding);
  EXPECT_EQ(content_view.frame.size.width,
            container.view.bounds.size.width - 2 * kContainerPadding);
  EXPECT_EQ(content_view.frame.size.height,
            container.view.bounds.size.height - 2 * kContainerPadding);

  [container.view removeFromSuperview];
}

// Tests that during actuation, the worklog replaces the Gemini content, except
// at the expanded mode where the Gemini content is shown between the worklog
// header and the intervention. The intervention is hidden while minimized.
TEST_F(GeminiContainerViewControllerTest, TestActuationContentVisibility) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  ActuationInterventionViewController* intervention_view_controller =
      [[ActuationInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  [scoped_key_window_.Get() addSubview:container.view];
  [container loadViewIfNeeded];
  [container setGeminiViewController:child_view_controller_];
  UIView* gemini_view = child_view_controller_.view;
  UIView* worklog_view = worklog_view_controller.view;
  UIView* intervention_view = intervention_view_controller.view;

  [container setWorklogDisplayMode:ActuationWorklogDisplayModeExpanded];
  [container setActuationActive:YES];
  [worklog_view_controller setActuationActive:YES];
  EXPECT_FALSE(IsHiddenInHierarchy(gemini_view));
  EXPECT_FALSE(IsHiddenInHierarchy(worklog_view));
  EXPECT_FALSE(IsHiddenInHierarchy(intervention_view));

  [container setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  EXPECT_TRUE(IsHiddenInHierarchy(gemini_view));
  EXPECT_FALSE(IsHiddenInHierarchy(worklog_view));
  EXPECT_FALSE(IsHiddenInHierarchy(intervention_view));

  [container setWorklogDisplayMode:ActuationWorklogDisplayModeMinimized];
  EXPECT_TRUE(IsHiddenInHierarchy(gemini_view));
  EXPECT_FALSE(IsHiddenInHierarchy(worklog_view));
  EXPECT_TRUE(IsHiddenInHierarchy(intervention_view));

  [container setActuationActive:NO];
  EXPECT_FALSE(IsHiddenInHierarchy(gemini_view));
  EXPECT_TRUE(IsHiddenInHierarchy(worklog_view));
  EXPECT_TRUE(IsHiddenInHierarchy(intervention_view));

  [container.view removeFromSuperview];
}

// Tests that the actuation state set before the view loads is applied once it
// loads.
TEST_F(GeminiContainerViewControllerTest, TestActuationStateBeforeViewLoad) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  ActuationInterventionViewController* intervention_view_controller =
      [[ActuationInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  [container setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  [container setActuationActive:YES];

  [container loadViewIfNeeded];
  [worklog_view_controller setActuationActive:YES];
  EXPECT_FALSE(IsHiddenInHierarchy(worklog_view_controller.view));
  EXPECT_FALSE(IsHiddenInHierarchy(intervention_view_controller.view));
}

// Tests that the reported actuation height fits the compact worklog and the
// intervention below it, and that it is reported again when the intervention
// changes or actuation starts.
TEST_F(GeminiContainerViewControllerTest, TestActuationHeight) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  ActuationInterventionViewController* intervention_view_controller =
      [[ActuationInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  container.mutator = mutator_;
  [scoped_key_window_.Get() addSubview:container.view];
  container.view.frame = CGRectMake(0, 0, 300, 600);
  [container setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  // Laid out first so that the children are measured at the container width.
  [container.view layoutIfNeeded];
  [container setActuationActive:YES];
  CGFloat worklog_height = [worklog_view_controller
      heightForDisplayMode:ActuationWorklogDisplayModeCompact];
  CGFloat insets_height = 2 * intelligence::actor::kSpacingLarge;
  EXPECT_DOUBLE_EQ(mutator_.actuationHeight, worklog_height + insets_height);

  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:@"Continue"];
  [intervention_view_controller
      setIntervention:[ActuationInterventionData
                          rowItemWithActions:@[ action ]]];
  CGFloat intervention_height = [intervention_view_controller fittingHeight];
  EXPECT_GT(intervention_height, 0.0);
  CGFloat expected_height =
      worklog_height + intervention_height + insets_height;
  EXPECT_DOUBLE_EQ(mutator_.actuationHeight, expected_height);

  // In a container of the reported height, the intervention starts right below
  // the worklog's compact content. Frames are rounded to pixels.
  container.view.frame = CGRectMake(0, 0, 300, mutator_.actuationHeight);
  [container.view layoutIfNeeded];
  EXPECT_NEAR(
      CGRectGetMinY(worklog_view_controller.view.frame) + worklog_height,
      CGRectGetMinY(intervention_view_controller.view.frame), 1.0);

  mutator_.actuationHeight = 0;
  [container setActuationActive:NO];
  [container setActuationActive:YES];
  EXPECT_DOUBLE_EQ(mutator_.actuationHeight, expected_height);

  [intervention_view_controller setIntervention:nil];
  EXPECT_DOUBLE_EQ(mutator_.actuationHeight, worklog_height + insets_height);

  [container.view removeFromSuperview];
}

// Tests that at the expanded mode, the worklog and the intervention keep their
// measured heights, even when the Gemini content prefers another height, and
// that the Gemini content fills the space between them.
TEST_F(GeminiContainerViewControllerTest, TestExpandedActuationLayout) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  ActuationInterventionViewController* intervention_view_controller =
      [[ActuationInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  [scoped_key_window_.Get() addSubview:container.view];
  container.view.frame = CGRectMake(0, 0, 300, 600);
  [container setGeminiViewController:child_view_controller_];
  UIView* gemini_view = child_view_controller_.view;
  // Simulates the Gemini SDK view preferring a height of its own.
  NSLayoutConstraint* preferred_height_constraint =
      [gemini_view.heightAnchor constraintEqualToConstant:100];
  preferred_height_constraint.priority = UILayoutPriorityDefaultHigh;
  preferred_height_constraint.active = YES;
  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:@"Continue"];
  [intervention_view_controller
      setIntervention:[ActuationInterventionData
                          rowItemWithActions:@[ action ]]];

  [container setActuationActive:YES];
  [container setWorklogDisplayMode:ActuationWorklogDisplayModeExpanded];
  [container.view layoutIfNeeded];

  CGRect worklog_frame = worklog_view_controller.view.frame;
  CGRect intervention_frame = intervention_view_controller.view.frame;
  CGRect gemini_frame = [gemini_view convertRect:gemini_view.bounds
                                          toView:container.view];
  EXPECT_NEAR(CGRectGetHeight(worklog_frame),
              [worklog_view_controller
                  heightForDisplayMode:ActuationWorklogDisplayModeExpanded],
              1.0);
  EXPECT_NEAR(CGRectGetHeight(intervention_frame),
              [intervention_view_controller fittingHeight], 1.0);
  EXPECT_NEAR(CGRectGetMaxY(intervention_frame),
              600 - intelligence::actor::kSpacingLarge, 1.0);
  EXPECT_NEAR(CGRectGetMinY(gemini_frame), CGRectGetMaxY(worklog_frame), 1.0);
  EXPECT_NEAR(CGRectGetMaxY(gemini_frame), CGRectGetMinY(intervention_frame),
              1.0);

  [container.view removeFromSuperview];
}

// Tests that in a container shorter than the worklog and the intervention, the
// intervention keeps its height right below the worklog, to be clipped rather
// than squeezed.
TEST_F(GeminiContainerViewControllerTest, TestShortContainerClipsIntervention) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  ActuationInterventionViewController* intervention_view_controller =
      [[ActuationInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  [scoped_key_window_.Get() addSubview:container.view];
  container.view.frame = CGRectMake(0, 0, 300, 600);
  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:@"Continue"];
  [intervention_view_controller
      setIntervention:[ActuationInterventionData
                          rowItemWithActions:@[ action ]]];
  [container setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  [container setActuationActive:YES];
  [container.view layoutIfNeeded];
  CGFloat intervention_height = [intervention_view_controller fittingHeight];
  CGFloat fitting_container_height =
      [worklog_view_controller
          heightForDisplayMode:ActuationWorklogDisplayModeCompact] +
      intervention_height + 2 * intelligence::actor::kSpacingLarge;

  container.view.frame = CGRectMake(0, 0, 300, fitting_container_height - 20);
  [container.view layoutIfNeeded];

  CGRect intervention_frame = intervention_view_controller.view.frame;
  EXPECT_NEAR(CGRectGetHeight(intervention_frame), intervention_height, 1.0);
  EXPECT_NEAR(CGRectGetMinY(intervention_frame),
              CGRectGetMaxY(worklog_view_controller.view.frame), 1.0);

  [container.view removeFromSuperview];
}

// Tests that a width change, e.g. between detents, re-measures the intervention
// in the layout pass seeing the new width, and that a layout at an unchanged
// width doesn't measure it again.
TEST_F(GeminiContainerViewControllerTest,
       TestWidthChangeRemeasuresIntervention) {
  ActuationWorklogViewController* worklog_view_controller =
      [[ActuationWorklogViewController alloc] init];
  FakeWrappingInterventionViewController* intervention_view_controller =
      [[FakeWrappingInterventionViewController alloc] init];
  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:worklog_view_controller
             interventionViewController:intervention_view_controller];
  [scoped_key_window_.Get() addSubview:container.view];
  container.view.frame = CGRectMake(0, 0, 300, 600);
  [container setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  [container setActuationActive:YES];
  [container.view layoutIfNeeded];
  UIView* intervention_view = intervention_view_controller.view;
  EXPECT_EQ(intervention_view_controller.lastMeasuredWidth, 300.0);
  EXPECT_NEAR(CGRectGetHeight(intervention_view.frame), kWideInterventionHeight,
              1.0);

  container.view.frame = CGRectMake(0, 0, 200, 600);
  [container.view layoutIfNeeded];
  EXPECT_EQ(intervention_view_controller.lastMeasuredWidth, 200.0);
  EXPECT_NEAR(CGRectGetHeight(intervention_view.frame),
              kNarrowInterventionHeight, 1.0);

  const NSUInteger measurement_count =
      intervention_view_controller.measurementCount;
  [container.view setNeedsLayout];
  [container.view layoutIfNeeded];
  EXPECT_EQ(intervention_view_controller.measurementCount, measurement_count);

  [container.view removeFromSuperview];
}

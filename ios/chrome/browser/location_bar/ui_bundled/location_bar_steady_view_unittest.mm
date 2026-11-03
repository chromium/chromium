// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Spacing used between the custom leading view and the location label.
const CGFloat kCustomLeadingViewSpacing = 4.0;
// Target width of the custom leading view once visible.
const CGFloat kCustomLeadingViewTargetWidth = 20.0;

// Returns a connected `UIWindowScene` for attaching test windows.
UIWindowScene* GetWindowScene() {
  for (UIScene* connected_scene in UIApplication.sharedApplication
           .connectedScenes) {
    UIWindowScene* scene =
        base::apple::ObjCCast<UIWindowScene>(connected_scene);
    if (scene) {
      return scene;
    }
  }
  return nil;
}

// Returns the set of constraints currently installed on the container holding
// `locationLabel`. The container is private, but Auto Layout installs the
// constraints between the label and its container on the container itself,
// which makes `locationLabel.superview.constraints` the simplest observable of
// the constraints rebuilt by `-updateContainerConstraints`. Constraint objects
// are compared by identity, so an equal set means they were not recreated.
NSSet<NSLayoutConstraint*>* ContainerConstraints(LocationBarSteadyView* view) {
  return [NSSet setWithArray:view.locationLabel.superview.constraints];
}

// Returns whether `constraints` contains a constraint referencing `item`.
bool ConstraintsReferenceItem(NSSet<NSLayoutConstraint*>* constraints,
                              id item) {
  for (NSLayoutConstraint* constraint in constraints) {
    if (constraint.firstItem == item || constraint.secondItem == item) {
      return true;
    }
  }
  return false;
}

// Returns whether `constraints` contains a constraint referencing a
// `UIImageView`.
bool ConstraintsReferenceImageView(NSSet<NSLayoutConstraint*>* constraints) {
  for (NSLayoutConstraint* constraint in constraints) {
    if ([constraint.firstItem isKindOfClass:[UIImageView class]] ||
        [constraint.secondItem isKindOfClass:[UIImageView class]]) {
      return true;
    }
  }
  return false;
}

}  // namespace

// Test fixture for `LocationBarSteadyView`.
class LocationBarSteadyViewTest : public PlatformTest {
 protected:
  LocationBarSteadyViewTest()
      : view_([[LocationBarSteadyView alloc] initWithTextOnly:NO]) {}

  LocationBarSteadyView* view_;
};

// Tests that updating the location text without changing the container layout
// inputs reuses the existing container constraints instead of rebuilding them.
TEST_F(LocationBarSteadyViewTest,
       ContainerConstraintsReusedWhenInputsUnchanged) {
  [view_ setLocationLabelText:@"example.com" clipTail:NO];
  NSSet<NSLayoutConstraint*>* initial_constraints = ContainerConstraints(view_);
  ASSERT_GT(initial_constraints.count, 0u);
  ASSERT_TRUE(
      ConstraintsReferenceItem(initial_constraints, view_.locationLabel));

  [view_ setLocationLabelText:@"chromium.org" clipTail:YES];
  EXPECT_NSEQ(initial_constraints, ContainerConstraints(view_));

  [view_ setLocationLabelPlaceholderText:@"Search or type URL"];
  EXPECT_NSEQ(initial_constraints, ContainerConstraints(view_));
}

// Tests that setting a location image rebuilds the container constraints, and
// that subsequent text updates reuse the rebuilt constraints.
TEST_F(LocationBarSteadyViewTest,
       ContainerConstraintsRebuiltWhenLocationImageChanges) {
  [view_ setLocationLabelText:@"example.com" clipTail:NO];
  NSSet<NSLayoutConstraint*>* initial_constraints = ContainerConstraints(view_);
  ASSERT_GT(initial_constraints.count, 0u);
  ASSERT_FALSE(ConstraintsReferenceImageView(initial_constraints));

  [view_ setLocationImage:[[UIImage alloc] init]];
  NSSet<NSLayoutConstraint*>* image_constraints = ContainerConstraints(view_);
  EXPECT_NSNE(initial_constraints, image_constraints);
  EXPECT_TRUE(ConstraintsReferenceImageView(image_constraints));

  [view_ setLocationLabelText:@"chromium.org" clipTail:NO];
  EXPECT_NSEQ(image_constraints, ContainerConstraints(view_));

  [view_ setLocationImage:nil];
  NSSet<NSLayoutConstraint*>* no_image_constraints =
      ContainerConstraints(view_);
  EXPECT_NSNE(image_constraints, no_image_constraints);
  EXPECT_FALSE(ConstraintsReferenceImageView(no_image_constraints));
}

// Tests that adding a custom leading view and making it visible rebuilds the
// container constraints so that they reference the new leading view container.
TEST_F(LocationBarSteadyViewTest,
       ContainerConstraintsRebuiltWhenCustomLeadingViewSet) {
  [view_ setLocationLabelText:@"example.com" clipTail:NO];
  NSSet<NSLayoutConstraint*>* initial_constraints = ContainerConstraints(view_);
  ASSERT_GT(initial_constraints.count, 0u);

  UIView* custom_view = [[UIView alloc] init];
  // Avoid the localized default accessibility label.
  custom_view.accessibilityLabel = @"custom";
  [view_ addCustomLeadingView:custom_view
                  targetWidth:kCustomLeadingViewTargetWidth
                      spacing:kCustomLeadingViewSpacing];

  // The custom view is wrapped in a container which lives in the same container
  // as the location label.
  UIView* custom_view_container = custom_view.superview;
  ASSERT_TRUE(custom_view_container);
  EXPECT_EQ(view_.locationLabel.superview, custom_view_container.superview);

  // The leading view is hidden by default, so it is not yet constrained in the
  // location container; constraints were still rebuilt since the container is
  // a new instance.
  NSSet<NSLayoutConstraint*>* hidden_constraints = ContainerConstraints(view_);
  EXPECT_NSNE(initial_constraints, hidden_constraints);
  EXPECT_FALSE(
      ConstraintsReferenceItem(hidden_constraints, custom_view_container));

  [view_ updateCustomLeadingViewVisibility:YES animated:NO];
  NSSet<NSLayoutConstraint*>* visible_constraints = ContainerConstraints(view_);
  EXPECT_NSNE(hidden_constraints, visible_constraints);
  EXPECT_TRUE(
      ConstraintsReferenceItem(visible_constraints, custom_view_container));

  // Text updates keep reusing the constraints once the leading view is shown.
  [view_ setLocationLabelText:@"chromium.org" clipTail:NO];
  EXPECT_NSEQ(visible_constraints, ContainerConstraints(view_));
}

// Tests that `trailingButton` is a direct sibling of `locationButton` (not
// nested inside it) and that `locationButton.accessibilityFrame` excludes
// `trailingButton` when it is visible and enabled in LTR, while spanning the
// full `LocationBarSteadyView` bounds when `trailingButton` is hidden,
// disabled, or transparent.
TEST_F(LocationBarSteadyViewTest,
       TrailingButtonIsSiblingAndExcludesLocationButtonAccessibilityFrame) {
  UIWindow* window = [[UIWindow alloc] initWithWindowScene:GetWindowScene()];
  view_.translatesAutoresizingMaskIntoConstraints = YES;
  view_.frame = CGRectMake(20, 40, 300, 44);
  [window addSubview:view_];
  [view_ setTrailingButtonHidden:NO];
  [view_ enableTrailingButton:YES];
  [window layoutIfNeeded];

  EXPECT_EQ(view_.trailingButton.superview, view_);
  EXPECT_NE(view_.trailingButton.superview, view_.locationButton);
  ASSERT_GT(CGRectGetMidX(view_.trailingButton.frame),
            CGRectGetMidX(view_.bounds));

  CGRect trailing_button_frame = UIAccessibilityConvertFrameToScreenCoordinates(
      view_.trailingButton.frame, view_);
  EXPECT_FALSE(CGRectIntersectsRect(view_.locationButton.accessibilityFrame,
                                    trailing_button_frame))
      << "locationButton="
      << base::SysNSStringToUTF8(
             NSStringFromCGRect(view_.locationButton.accessibilityFrame))
      << " trailingButton="
      << base::SysNSStringToUTF8(NSStringFromCGRect(trailing_button_frame));
  CGRect expected_ltr_frame = UIAccessibilityConvertFrameToScreenCoordinates(
      CGRectMake(0, 0, CGRectGetMinX(view_.trailingButton.frame), 44), view_);
  EXPECT_NSEQ(NSStringFromCGRect(view_.locationButton.accessibilityFrame),
              NSStringFromCGRect(expected_ltr_frame));

  CGRect full_frame =
      UIAccessibilityConvertFrameToScreenCoordinates(view_.bounds, view_);

  [view_ setTrailingButtonHidden:YES];
  EXPECT_NSEQ(NSStringFromCGRect(view_.locationButton.accessibilityFrame),
              NSStringFromCGRect(full_frame));

  [view_ setTrailingButtonHidden:NO];
  [view_ enableTrailingButton:NO];
  EXPECT_NSEQ(NSStringFromCGRect(view_.locationButton.accessibilityFrame),
              NSStringFromCGRect(full_frame));

  [view_ enableTrailingButton:YES];
  view_.trailingButton.alpha = 0;
  EXPECT_NSEQ(NSStringFromCGRect(view_.locationButton.accessibilityFrame),
              NSStringFromCGRect(full_frame));
}

// Tests that `locationButton.accessibilityFrame` excludes `trailingButton` on
// the leading (left) edge when laid out in right-to-left (RTL) direction.
TEST_F(LocationBarSteadyViewTest,
       LocationButtonAccessibilityFrameExcludesTrailingButtonInRTL) {
  UIWindow* window = [[UIWindow alloc] initWithWindowScene:GetWindowScene()];
  view_.translatesAutoresizingMaskIntoConstraints = YES;
  view_.frame = CGRectMake(20, 40, 300, 44);
  view_.semanticContentAttribute = UISemanticContentAttributeForceRightToLeft;
  [window addSubview:view_];
  [view_ setTrailingButtonHidden:NO];
  [view_ enableTrailingButton:YES];
  [window layoutIfNeeded];

  ASSERT_LT(CGRectGetMidX(view_.trailingButton.frame),
            CGRectGetMidX(view_.bounds));
  CGRect trailing_button_frame = UIAccessibilityConvertFrameToScreenCoordinates(
      view_.trailingButton.frame, view_);
  EXPECT_FALSE(CGRectIntersectsRect(view_.locationButton.accessibilityFrame,
                                    trailing_button_frame))
      << "locationButton="
      << base::SysNSStringToUTF8(
             NSStringFromCGRect(view_.locationButton.accessibilityFrame))
      << " trailingButton="
      << base::SysNSStringToUTF8(NSStringFromCGRect(trailing_button_frame));
  CGFloat trailing_button_max_x = CGRectGetMaxX(view_.trailingButton.frame);
  CGRect expected_rtl_frame = UIAccessibilityConvertFrameToScreenCoordinates(
      CGRectMake(trailing_button_max_x, 0,
                 CGRectGetWidth(view_.bounds) - trailing_button_max_x, 44),
      view_);
  EXPECT_NSEQ(NSStringFromCGRect(view_.locationButton.accessibilityFrame),
              NSStringFromCGRect(expected_rtl_frame));
}

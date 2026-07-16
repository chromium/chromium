// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_page_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake step view controller for lightweight unit testing of the container.
@interface FakeStepViewController : UIViewController <GeminiFirstRunStep>
@property(nonatomic, assign) BOOL primaryTapped;
@property(nonatomic, assign) BOOL secondaryTapped;
@end

@implementation FakeStepViewController
@synthesize stepDelegate;

- (GeminiFirstRunStepIdentifier)stepIdentifier {
  return GeminiFirstRunStepIdentifier::kPromo;
}

- (ButtonStackConfiguration*)buttonStackConfiguration {
  return [[ButtonStackConfiguration alloc] init];
}

- (CGFloat)contentHeight {
  return 200.0;
}

- (void)stepDidBecomeActive {
}

- (void)stepWillResignActive {
}

- (void)didTapPrimaryButton {
  self.primaryTapped = YES;
}

- (void)didTapSecondaryButton {
  self.secondaryTapped = YES;
}
@end

// Test fixture for `GeminiFirstRunPageViewController`.
class GeminiFirstRunPageViewControllerTest : public PlatformTest {
 public:
  GeminiFirstRunPageViewController* CreateController(size_t num_steps,
                                                     bool show_header = false) {
    steps_ = [[NSMutableArray alloc] init];
    for (size_t i = 0; i < num_steps; ++i) {
      [steps_ addObject:[[FakeStepViewController alloc] init]];
    }

    GeminiFirstRunPageViewController* view_controller =
        [[GeminiFirstRunPageViewController alloc] initWithSteps:steps_
                                             showBrandingHeader:show_header];

    // Force view initialization since this view controller is never added into
    // the hierarchy in this unit test.
    [view_controller view];
    return view_controller;
  }

  void PrimaryAction(GeminiFirstRunPageViewController* view_controller) {
    id<ButtonStackActionDelegate> action_delegate =
        (id<ButtonStackActionDelegate>)view_controller;
    [action_delegate didTapPrimaryActionButton];
  }

  void SecondaryAction(GeminiFirstRunPageViewController* view_controller) {
    id<ButtonStackActionDelegate> action_delegate =
        (id<ButtonStackActionDelegate>)view_controller;
    [action_delegate didTapSecondaryActionButton];
  }

 protected:
  NSMutableArray<FakeStepViewController*>* steps_ = nil;
};

// Tests initialization with two steps (e.g., promo shown followed by consent).
TEST_F(GeminiFirstRunPageViewControllerTest, TwoStepInitialization) {
  CreateController(2);

  EXPECT_FALSE(steps_[0].view.accessibilityElementsHidden);
  EXPECT_TRUE(steps_[1].view.accessibilityElementsHidden);
}

// Tests initialization with a single step (e.g., post-promo nonconsent flow).
TEST_F(GeminiFirstRunPageViewControllerTest, SingleStepInitialization) {
  CreateController(1);

  EXPECT_FALSE(steps_[0].view.accessibilityElementsHidden);
}

// Tests transitioning between steps when continuing after the first step.
TEST_F(GeminiFirstRunPageViewControllerTest, StepTransitionFlow) {
  GeminiFirstRunPageViewController* view_controller = CreateController(2);

  EXPECT_FALSE(steps_[0].view.accessibilityElementsHidden);
  EXPECT_TRUE(steps_[1].view.accessibilityElementsHidden);

  PrimaryAction(view_controller);

  EXPECT_TRUE(steps_[0].primaryTapped);
  EXPECT_TRUE(steps_[0].view.accessibilityElementsHidden);
  EXPECT_FALSE(steps_[1].view.accessibilityElementsHidden);
}

// Tests that tapping the primary action on the last step delegates to the step
// without attempting to transition past the last object.
TEST_F(GeminiFirstRunPageViewControllerTest, PrimaryActionOnLastStep) {
  GeminiFirstRunPageViewController* view_controller = CreateController(1);

  EXPECT_EQ(view_controller.currentStep, steps_[0]);
  PrimaryAction(view_controller);
  EXPECT_TRUE(steps_[0].primaryTapped);
  EXPECT_EQ(view_controller.currentStep, steps_[0]);
}

// Tests that tapping the secondary action delegates directly to the active
// step.
TEST_F(GeminiFirstRunPageViewControllerTest, SecondaryActionDelegatesToStep) {
  GeminiFirstRunPageViewController* view_controller = CreateController(2);

  EXPECT_EQ(view_controller.currentStep, steps_[0]);
  SecondaryAction(view_controller);
  EXPECT_TRUE(steps_[0].secondaryTapped);
  EXPECT_EQ(view_controller.currentStep, steps_[0]);
}

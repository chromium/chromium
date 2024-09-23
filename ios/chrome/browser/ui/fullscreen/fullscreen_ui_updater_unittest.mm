// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "testing/platform_test.h"

#pragma mark - TestFullscreenUIElement

@interface TestFullscreenUIElement : NSObject<FullscreenUIElement>
// The values that are passed to the UI element through the UI updater.
@property(nonatomic, readonly) CGFloat progress;
@property(nonatomic, readonly) UIEdgeInsets minViewportInsets;
@property(nonatomic, readonly) UIEdgeInsets maxViewportInsets;
@property(nonatomic, readonly, getter=isEnabled) BOOL enabled;
@property(nonatomic, readonly) FullscreenAnimator* animator;
// Whether the UI element should implement optional selectors.  Defaults to YES.
@property(nonatomic, assign) BOOL implementsOptionalSelectors;
@end

@implementation TestFullscreenUIElement

- (instancetype)init {
  if ((self = [super init])) {
    _implementsOptionalSelectors = YES;
  }
  return self;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  if (!self.implementsOptionalSelectors &&
      (aSelector == @selector(updateForFullscreenMinViewportInsets:
                                                 maxViewportInsets:) ||
       aSelector == @selector(updateForFullscreenEnabled:) ||
       aSelector == @selector(animateFullscreenWithAnimator:))) {
    return NO;
  }
  return [super respondsToSelector:aSelector];
}

- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
  _minViewportInsets = minViewportInsets;
  _maxViewportInsets = maxViewportInsets;
}

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _progress = progress;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  _enabled = enabled;
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  _animator = animator;
}

@end

#pragma mark - FullscreenUIUpdaterTest

// Test fixture for FullscreenBroadcastForwarder.
class FullscreenUIUpdaterTest : public PlatformTest {
 public:
  FullscreenUIUpdaterTest()
      : controller_(&model_),
        element_([[TestFullscreenUIElement alloc] init]),
        updater_(&controller_, element_) {}

  TestFullscreenController* controller() { return &controller_; }
  TestFullscreenUIElement* element() { return element_; }

 private:
  FullscreenModel model_;
  TestFullscreenController controller_;
  __strong TestFullscreenUIElement* element_;
  FullscreenUIUpdater updater_;
};

// Tests that the updater correctly changes the UI element's progress value.
TEST_F(FullscreenUIUpdaterTest, Progress) {
  ASSERT_TRUE(AreCGFloatsEqual(element().progress, 0.0));
  const CGFloat kProgress = 0.5;
  controller()->OnFullscreenProgressUpdated(kProgress);
  EXPECT_TRUE(AreCGFloatsEqual(element().progress, kProgress));
}

// Tests that the updater correctly changes the UI elements viewport insets.
TEST_F(FullscreenUIUpdaterTest, Insets) {
  const UIEdgeInsets kMinInsets = UIEdgeInsetsMake(10, 10, 10, 10);
  const UIEdgeInsets kMaxInsets = UIEdgeInsetsMake(20, 20, 20, 20);
  controller()->OnFullscreenViewportInsetRangeChanged(kMinInsets, kMaxInsets);
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(element().minViewportInsets, kMinInsets));
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(element().maxViewportInsets, kMaxInsets));
}

// Tests that the updater correctly changes the UI element's enabled state.
TEST_F(FullscreenUIUpdaterTest, EnabledDisabled) {
  ASSERT_FALSE(element().enabled);
  controller()->OnFullscreenEnabledStateChanged(true);
  EXPECT_TRUE(element().enabled);
  controller()->OnFullscreenEnabledStateChanged(false);
  EXPECT_FALSE(element().enabled);
}

// Tests that the updater sends the animator to the UI element.
TEST_F(FullscreenUIUpdaterTest, ScrollEnd) {
  ASSERT_FALSE(element().animator);
  // Create a test animator.  The start progress of 0.0 is a dummy value, as the
  // animator's progress properties are unused in this test.
  FullscreenAnimator* const kAnimator = [[FullscreenAnimator alloc]
      initWithStartProgress:0.0
                      style:FullscreenAnimatorStyle::ENTER_FULLSCREEN];
  controller()->OnFullscreenWillAnimate(kAnimator);
  EXPECT_EQ(element().animator, kAnimator);
}

// Tests the default behavior of FullscreenUIUpdater when optional
// FullscreenUIElement selectors aren not implemented.
TEST_F(FullscreenUIUpdaterTest, OptionalSelectors) {
  element().implementsOptionalSelectors = NO;
  // Verify that the fullscreen progress gets reset to 1.0 when the enabled
  // state selector is not implemented.
  ASSERT_TRUE(AreCGFloatsEqual(element().progress, 0.0));
  controller()->OnFullscreenEnabledStateChanged(false);
  EXPECT_TRUE(AreCGFloatsEqual(element().progress, 1.0));
  // Verify that the fullscreen progress gets reset to 0.0 for an
  // ENTER_FULLSCREEN animator when the animation selector is not implemented.
  FullscreenAnimator* animator = [[FullscreenAnimator alloc]
      initWithStartProgress:0.0
                      style:FullscreenAnimatorStyle::ENTER_FULLSCREEN];
  controller()->OnFullscreenWillAnimate(animator);
  [animator startAnimation];
  EXPECT_TRUE(AreCGFloatsEqual(element().progress, 0.0));
  [animator stopAnimation:YES];
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - TestFullscreenUIElement

@interface TestFullscreenUIElement : NSObject<FullscreenUIElement>
// The values that are passed to the UI element through the UI updater.
@property(nonatomic, readonly) CGFloat progress;
@property(nonatomic, readonly, getter=isEnabled) BOOL enabled;
@property(nonatomic, readonly) FullscreenAnimator* animator;
@end

@implementation TestFullscreenUIElement
@synthesize progress = _progress;
@synthesize enabled = _enabled;
@synthesize animator = _animator;

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
      : PlatformTest(),
        element_([[TestFullscreenUIElement alloc] init]),
        updater_(element_) {}

  TestFullscreenUIElement* element() { return element_; }
  FullscreenControllerObserver* observer() { return &updater_; }

 private:
  __strong TestFullscreenUIElement* element_;
  FullscreenUIUpdater updater_;
};

// Tests that the updater correctly changes the UI element's progress value.
TEST_F(FullscreenUIUpdaterTest, Progress) {
  ASSERT_TRUE(AreCGFloatsEqual(element().progress, 0.0));
  const CGFloat kProgress = 0.5;
  observer()->FullscreenProgressUpdated(nullptr, kProgress);
  EXPECT_TRUE(AreCGFloatsEqual(element().progress, kProgress));
}

// Tests that the updater correctly changes the UI element's enabled state.
TEST_F(FullscreenUIUpdaterTest, EnabledDisabled) {
  ASSERT_FALSE(element().enabled);
  observer()->FullscreenEnabledStateChanged(nullptr, true);
  EXPECT_TRUE(element().enabled);
  observer()->FullscreenEnabledStateChanged(nullptr, false);
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
  observer()->FullscreenWillAnimate(nullptr, kAnimator);
  EXPECT_EQ(element().animator, kAnimator);
}

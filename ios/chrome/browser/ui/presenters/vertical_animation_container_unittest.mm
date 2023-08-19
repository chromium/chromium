// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"

#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test delegate helper; the delegate callback sets the `presented` and
// `dismissed` property.
@interface TestContainedPresenterDelegate : NSObject<ContainedPresenterDelegate>
@property(nonatomic) BOOL presented;
@property(nonatomic) BOOL dismissed;
@end

@implementation TestContainedPresenterDelegate
@synthesize presented = _presented;
@synthesize dismissed = _dismissed;

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  self.presented = YES;
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  self.dismissed = YES;
}

@end

namespace {

class VerticalAnimationContainerTest : public PlatformTest {
 public:
  VerticalAnimationContainerTest()
      : delegate_([[TestContainedPresenterDelegate alloc] init]),
        base_([[UIViewController alloc] init]),
        presented_([[UIViewController alloc] init]),
        presenter_([[VerticalAnimationContainer alloc] init]) {
    presenter_.baseViewController = base_;
    presenter_.presentedViewController = presented_;
    presenter_.delegate = delegate_;
  }

 protected:
  __strong TestContainedPresenterDelegate* delegate_;
  __strong UIViewController* base_;
  __strong UIViewController* presented_;
  __strong id<ContainedPresenter> presenter_;
};

TEST_F(VerticalAnimationContainerTest, TestPreparation) {
  // Presenter does not set width constrains, so set them manually.
  const CGFloat base_view_width = base_.view.frame.size.width;
  [presented_.view.widthAnchor constraintEqualToConstant:base_view_width]
      .active = YES;

  [presenter_ prepareForPresentation];

  // General expectations for presentation prep.
  EXPECT_TRUE([base_.childViewControllers containsObject:presented_]);
  EXPECT_TRUE(presented_.view.superview == base_.view);

  // For vertical animation, the presented view should start below the
  // base view controller's view, and be the same width.
  EXPECT_EQ(base_view_width, CGRectGetWidth(presented_.view.bounds));
  EXPECT_EQ(presented_.view.frame.origin.x, 0);
  EXPECT_GE(presented_.view.frame.origin.y, base_.view.bounds.size.height);

  // The presentation did not finish yet.
  EXPECT_FALSE(delegate_.presented);
}

TEST_F(VerticalAnimationContainerTest, TestPresentation) {
  [presenter_ prepareForPresentation];
  [presenter_ presentAnimated:NO];

  // For vertical animation, the presented view should be entirely contained
  // by the base view controller's view, and be aligned with its bottom.
  EXPECT_TRUE(CGRectContainsRect(base_.view.bounds, presented_.view.frame));
  EXPECT_EQ(CGRectGetMaxY(base_.view.bounds),
            CGRectGetMaxY(presented_.view.frame));
  EXPECT_TRUE(delegate_.presented);
  // The delegate method should not be called here.
  EXPECT_FALSE(delegate_.dismissed);
}

TEST_F(VerticalAnimationContainerTest, TestDismissal) {
  [presenter_ prepareForPresentation];
  [presenter_ presentAnimated:NO];
  [presenter_ dismissAnimated:NO];

  EXPECT_FALSE([base_.childViewControllers containsObject:presented_]);
  EXPECT_FALSE(presented_.view.superview == base_.view);
  EXPECT_TRUE(delegate_.dismissed);
}

}  // namespace

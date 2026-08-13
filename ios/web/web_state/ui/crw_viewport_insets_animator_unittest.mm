// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/web/web_state/crw_web_view.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface CRWViewportInsetsAnimator (Testing)
- (void)handleDisplayLink:(CADisplayLink*)displayLink;
@end

namespace {

using CRWViewportInsetsAnimatorTest = PlatformTest;

// Tests that animating with a positive duration updates insets and fires
// completion upon finish.
TEST_F(CRWViewportInsetsAnimatorTest, AnimateWithDurationRunsToCompletion) {
  if (@available(iOS 26, *)) {
    CRWWebView* webView = [[CRWWebView alloc] initWithFrame:CGRectZero];
    UIScrollView* scrollView = [[UIScrollView alloc] init];

    const NSTimeInterval duration = 0.4;
    UIEdgeInsets startInsets = UIEdgeInsetsMake(0, 0, 0, 0);
    UIEdgeInsets targetInsets = UIEdgeInsetsMake(100, 0, 80, 0);
    __block BOOL completionCalled = NO;

    CRWViewportInsetsAnimator* animator =
        [[CRWViewportInsetsAnimator alloc] initWithWebView:webView
                                                scrollView:scrollView
                                               startInsets:startInsets
                                              targetInsets:targetInsets
                                                  duration:duration
                                                completion:^{
                                                  completionCalled = YES;
                                                }];

    [animator start];

    id mockDisplayLink = OCMClassMock([CADisplayLink class]);
    __block CFTimeInterval mockTimestamp = 100.0;
    [[[mockDisplayLink stub] andDo:^(NSInvocation* invocation) {
      [invocation setReturnValue:&mockTimestamp];
    }] timestamp];

    // Tick 1: start (t = 0.0)
    mockTimestamp = 100.0;
    [animator handleDisplayLink:mockDisplayLink];
    EXPECT_FALSE(completionCalled);

    // Tick 2: end (t = 0.4s)
    mockTimestamp = 100.4;
    [animator handleDisplayLink:mockDisplayLink];

    EXPECT_TRUE(completionCalled);
    EXPECT_TRUE(
        UIEdgeInsetsEqualToEdgeInsets(targetInsets, animator.currentInsets));
    EXPECT_TRUE(
        UIEdgeInsetsEqualToEdgeInsets(targetInsets, scrollView.contentInset));
  }
}

// Tests that stop cancels active animation without invoking completion.
TEST_F(CRWViewportInsetsAnimatorTest, StopAnimationCancelsDisplayLink) {
  if (@available(iOS 26, *)) {
    CRWWebView* webView = [[CRWWebView alloc] initWithFrame:CGRectZero];
    UIScrollView* scrollView = [[UIScrollView alloc] init];

    __block BOOL completionCalled = NO;
    CRWViewportInsetsAnimator* animator = [[CRWViewportInsetsAnimator alloc]
        initWithWebView:webView
             scrollView:scrollView
            startInsets:UIEdgeInsetsZero
           targetInsets:UIEdgeInsetsMake(50, 0, 50, 0)
               duration:0.3
             completion:^{
               completionCalled = YES;
             }];

    [animator start];
    [animator stop];

    EXPECT_FALSE(completionCalled);
  }
}

}  // namespace

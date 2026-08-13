// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

#import <QuartzCore/QuartzCore.h>

#import <cmath>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/web/common/crw_viewport_controller.h"

@implementation CRWViewportInsetsAnimator {
  __weak UIView<CRWViewportController>* _webView;
  __weak UIScrollView* _scrollView;

  NSTimeInterval _duration;
  UIEdgeInsets _startInsets;
  ProceduralBlock _completion;

  CADisplayLink* _insetsDisplayLink;
  CFTimeInterval _animationStartTime;
}

- (instancetype)initWithWebView:(UIView<CRWViewportController>*)webView
                     scrollView:(UIScrollView*)scrollView
                    startInsets:(UIEdgeInsets)startInsets
                   targetInsets:(UIEdgeInsets)targetInsets
                       duration:(NSTimeInterval)duration
                     completion:(ProceduralBlock)completion {
  self = [super init];
  if (self) {
    CHECK(webView);
    CHECK(scrollView);
    _webView = webView;
    _scrollView = scrollView;
    _startInsets = startInsets;
    _targetInsets = targetInsets;
    _currentInsets = startInsets;
    _duration = duration;
    _completion = [completion copy];
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start {
  if (_insetsDisplayLink) {
    return;
  }
  _animationStartTime = 0;
  _insetsDisplayLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(handleDisplayLink:)];
  [_insetsDisplayLink addToRunLoop:[NSRunLoop mainRunLoop]
                           forMode:NSRunLoopCommonModes];
}

- (void)stop {
  [_insetsDisplayLink invalidate];
  _insetsDisplayLink = nil;
  _animationStartTime = 0;
}

#pragma mark - Private

// Applies the specified insets to the scroll view and underlying web view.
- (void)applyInsets:(UIEdgeInsets)obscuredInsets {
  UIScrollView* scrollView = _scrollView;
  scrollView.contentInset = obscuredInsets;

  UIView<CRWViewportController>* webView = _webView;
  if (webView) {
    if (@available(iOS 26, *)) {
      [webView setObscuredContentInsets:obscuredInsets];
    } else {
      NOTREACHED();
    }
  }
}

// Handles each display link frame tick, calculates easing interpolation
// progress, applies intermediate insets, and invokes completion on finish.
- (void)handleDisplayLink:(CADisplayLink*)displayLink {
  if (_animationStartTime <= 0) {
    _animationStartTime = displayLink.timestamp;
  }
  CFTimeInterval elapsed = displayLink.timestamp - _animationStartTime;
  double progress = _duration > 0 ? (elapsed / _duration) : 1.0;
  if (progress >= 1.0) {
    UIEdgeInsets targetInsets = _targetInsets;
    ProceduralBlock completion = _completion;
    _completion = nil;
    [self stop];
    _currentInsets = targetInsets;
    [self applyInsets:targetInsets];
    if (completion) {
      completion();
    }
    return;
  }

  // Ease-in-out quadratic curve.
  double easedProgress = progress < 0.5
                             ? 2.0 * progress * progress
                             : 1.0 - std::pow(-2.0 * progress + 2.0, 2) / 2.0;

  UIEdgeInsets currentInsets = UIEdgeInsetsMake(
      _startInsets.top + easedProgress * (_targetInsets.top - _startInsets.top),
      _startInsets.left +
          easedProgress * (_targetInsets.left - _startInsets.left),
      _startInsets.bottom +
          easedProgress * (_targetInsets.bottom - _startInsets.bottom),
      _startInsets.right +
          easedProgress * (_targetInsets.right - _startInsets.right));

  _currentInsets = currentInsets;
  [self applyInsets:currentInsets];
}

@end

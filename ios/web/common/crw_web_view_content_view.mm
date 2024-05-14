// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/crw_web_view_content_view.h"

#import <WebKit/WebKit.h>
#import <cmath>
#import <limits>

#import "base/check.h"
#import "base/notreached.h"

namespace {

// Background color RGB values for the content view which is displayed when the
// `_webView` is offset from the screen due to user interaction. Displaying this
// background color is handled by UIWebView but not WKWebView, so it needs to be
// set in CRWWebViewContentView to support both. The color value matches that
// used by UIWebView.
const CGFloat kBackgroundRGBComponents[] = {0.75f, 0.74f, 0.76f};

}  // namespace

@implementation CRWWebViewContentView
@synthesize contentOffset = _contentOffset;
@synthesize contentInset = _contentInset;
@synthesize scrollView = _scrollView;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;
@synthesize viewportEdgesAffectedBySafeArea = _viewportEdgesAffectedBySafeArea;
@synthesize viewportInsets = _viewportInsets;
@synthesize webView = _webView;
@synthesize fullscreenState = _fullscreenState;

- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView
                fullscreenState:(CrFullscreenState)fullscreenState {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    DCHECK([scrollView isDescendantOfView:webView]);
    _webView = webView;
    _scrollView = scrollView;
    _fullscreenState = fullscreenState;
  }
  return self;
}

- (instancetype)initForTesting {
  return [super initWithFrame:CGRectZero];
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (self.superview) {
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:_webView];
    self.backgroundColor = [UIColor colorWithRed:kBackgroundRGBComponents[0]
                                           green:kBackgroundRGBComponents[1]
                                            blue:kBackgroundRGBComponents[2]
                                           alpha:1.0];
  }
}

- (BOOL)becomeFirstResponder {
  return [_webView becomeFirstResponder];
}

- (void)updateFullscreenState:(CrFullscreenState)fullscreenState {
  _fullscreenState = fullscreenState;
}

#pragma mark Layout

- (void)setContentOffset:(CGPoint)contentOffset {
  if (CGPointEqualToPoint(_contentOffset, contentOffset))
    return;
  _contentOffset = contentOffset;
  [self setNeedsLayout];
}

- (UIEdgeInsets)contentInset {
  return self.shouldUseViewContentInset ? [_scrollView contentInset]
                                        : _contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  UIEdgeInsets oldInsets = self.contentInset;
  CGFloat delta = std::fabs(oldInsets.top - contentInset.top) +
                  std::fabs(oldInsets.left - contentInset.left) +
                  std::fabs(oldInsets.bottom - contentInset.bottom) +
                  std::fabs(oldInsets.right - contentInset.right);
  if (delta <= std::numeric_limits<CGFloat>::epsilon())
    return;
  _contentInset = contentInset;
  if (self.shouldUseViewContentInset) {
    [_scrollView setContentInset:contentInset];
  }
}

- (void)setShouldUseViewContentInset:(BOOL)shouldUseViewContentInset {
  if (_shouldUseViewContentInset != shouldUseViewContentInset) {
    UIEdgeInsets oldContentInset = self.contentInset;
    self.contentInset = UIEdgeInsetsZero;
    _shouldUseViewContentInset = shouldUseViewContentInset;
    self.contentInset = oldContentInset;
  }
}

#pragma mark - CRWViewportAdjusting

// TODO(crbug.com/40123534): Implement.
- (void)updateMinViewportInsets:(UIEdgeInsets)minInsets
              maxViewportInsets:(UIEdgeInsets)maxInsets {
}

@end

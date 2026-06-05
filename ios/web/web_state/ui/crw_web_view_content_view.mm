// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_content_view.h"

#import <cmath>
#import <limits>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/web/common/crw_viewport_controller.h"
#import "ios/web/common/crw_web_view_resizing_type.h"
#import "ios/web/public/web_client.h"

namespace {

// Background color RGB values for the content view which is displayed when the
// `_webView` is offset from the screen due to user interaction. Displaying this
// background color is handled by UIWebView but not WKWebView, so it needs to be
// set in CRWWebViewContentView to support both. The color value matches that
// used by UIWebView.
const CGFloat kBackgroundRGBComponents[] = {0.75f, 0.74f, 0.76f};

}  // namespace

@interface CRWWebViewContentView () {
  UIEdgeInsets _pendingMinInset;
  UIEdgeInsets _pendingMaxInset;
  BOOL _hasPendingViewportInsets;
}
@end

@implementation CRWWebViewContentView
@synthesize contentOffset = _contentOffset;
@synthesize contentInset = _contentInset;
@synthesize obscuredInsets = _obscuredInsets;
@synthesize scrollView = _scrollView;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;
@synthesize viewportEdgesAffectedBySafeArea = _viewportEdgesAffectedBySafeArea;
@synthesize viewportInsets = _viewportInsets;
@synthesize webView = _webView;
@synthesize fullscreenState = _fullscreenState;
@synthesize webViewResizingType = _webViewResizingType;

- (instancetype)initWithWebView:(UIView<CRWViewportController>*)webView
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
    // Default resizing value.
    if (@available(iOS 26, *)) {
      _webViewResizingType = WebViewResizingType::kContentInset;
    } else {
      _webViewResizingType = WebViewResizingType::kFrame;
    }
  }
  return self;
}

- (instancetype)initForTesting {
  return [super initWithFrame:CGRectZero];
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
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

- (void)layoutSubviews {
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset:
      if (_hasPendingViewportInsets) {
        [self setMinimumViewportInset:_pendingMinInset
                 maximumViewportInset:_pendingMaxInset];
      }
      break;
    case WebViewResizingType::kFrame:
      _webView.frame = UIEdgeInsetsInsetRect(self.frame, _obscuredInsets);
      break;
  }
  [super layoutSubviews];
}

#pragma mark Layout

- (void)setContentOffset:(CGPoint)contentOffset {
  if (CGPointEqualToPoint(_contentOffset, contentOffset)) {
    return;
  }
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
  if (delta <= std::numeric_limits<CGFloat>::epsilon()) {
    return;
  }
  _contentInset = contentInset;
  if (self.shouldUseViewContentInset) {
    [_scrollView setContentInset:contentInset];
  }
}

- (UIEdgeInsets)obscuredInsets {
  return _obscuredInsets;
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets {
  if (UIEdgeInsetsEqualToEdgeInsets(_obscuredInsets, obscuredInsets)) {
    return;
  }
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset:
      _scrollView.contentInsetAdjustmentBehavior =
          UIScrollViewContentInsetAdjustmentNever;
      _scrollView.contentInset = obscuredInsets;
      if (@available(iOS 26, *)) {
        [_webView setObscuredContentInsets:obscuredInsets];
      } else {
        NOTREACHED();
      }
      break;
    case WebViewResizingType::kFrame:
      // Update the scroll offset to account for the changing frame.
      CGPoint offset = _scrollView.contentOffset;
      if (offset.y > 0) {
        CGFloat topDelta = obscuredInsets.top - _obscuredInsets.top;
        offset.y = std::max<CGFloat>(0, offset.y + topDelta);
        _scrollView.contentOffset = offset;
      }
      // Update the frame.
      _webView.frame = UIEdgeInsetsInsetRect(self.frame, obscuredInsets);
      break;
  }
  _obscuredInsets = obscuredInsets;
}

- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset {
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset:
      if (_webView.window) {
        [_webView setMinimumViewportInset:minInset
                     maximumViewportInset:maxInset];
        [_webView setNeedsLayout];
        _hasPendingViewportInsets = NO;
      } else {
        _pendingMinInset = minInset;
        _pendingMaxInset = maxInset;
        _hasPendingViewportInsets = YES;
      }
      break;
    case WebViewResizingType::kFrame:
      // Do not set the min/max viewport insets if we are resizing frame. Since
      // these insets are relative to the frame and we cannot report negative
      // insets, there is no way to properly report the minimum insets. If we
      // could report negative insets, we would have to update this every time
      // the obscured insets changed. See http://crbug.com/40944174#comment17.
      break;
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

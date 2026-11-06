// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_content_view.h"

#import <cmath>
#import <limits>
#import <optional>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/crw_viewport_controller.h"
#import "ios/web/common/crw_web_view_resizing_type.h"
#import "ios/web/public/content_type_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

namespace {

// Feature flag to enable the strict bounds check for WKWebView viewport insets.
BASE_FEATURE(kCRWWebViewContentViewLayoutFix, base::FEATURE_ENABLED_BY_DEFAULT);

// Helper function to replicate WebKit's validation logic for a single inset.
// WebKit casts `frame.size` and insets to 32-bit floats and considers the
// viewport valid if the unobscured size is not empty (> 0), or if the inset
// itself is empty.
BOOL IsInsetValidForFrame(UIEdgeInsets inset, CGSize frameSize) {
  float insetWidth = static_cast<float>(inset.left + inset.right);
  float insetHeight = static_cast<float>(inset.top + inset.bottom);
  BOOL insetEmpty = insetWidth <= 0 || insetHeight <= 0;
  if (insetEmpty) {
    return YES;
  }

  float frameWidth = static_cast<float>(frameSize.width);
  float frameHeight = static_cast<float>(frameSize.height);

  BOOL unobscuredEmpty =
      (frameWidth - insetWidth) <= 0 || (frameHeight - insetHeight) <= 0;
  return !unobscuredEmpty;
}

// Helper function to check if the frame is large enough for both min/max
// insets.
BOOL IsFrameLargeEnoughToApplyViewportInsets(CGSize frameSize,
                                             UIEdgeInsets minInset,
                                             UIEdgeInsets maxInset) {
  if (!IsInsetValidForFrame(maxInset, frameSize)) {
    return NO;
  }
  return IsInsetValidForFrame(minInset, frameSize);
}

// Returns `insets` with horizontal edges expanded to at least `safeAreaInsets`.
UIEdgeInsets AdjustInsetsForSafeArea(UIEdgeInsets insets,
                                     UIEdgeInsets safeAreaInsets) {
  UIEdgeInsets adjusted = insets;
  adjusted.left = std::max(adjusted.left, safeAreaInsets.left);
  adjusted.right = std::max(adjusted.right, safeAreaInsets.right);
  return adjusted;
}

// Returns `contentInsets` reduced by `maxInsets` (excluding bottom) to simulate
// toolbar collapse visually for PDFs without changing the web view frame.
UIEdgeInsets AdjustedPdfContentInsets(UIEdgeInsets contentInsets,
                                      UIEdgeInsets maxInsets) {
  UIEdgeInsets adjusted = contentInsets;
  adjusted.top -= maxInsets.top;
  adjusted.left -= maxInsets.left;
  adjusted.right -= maxInsets.right;
  return adjusted;
}

// Background color RGB values for the content view which is displayed when the
// `_webView` is offset from the screen due to user interaction. Displaying this
// background color is handled by UIWebView but not WKWebView, so it needs to be
// set in CRWWebViewContentView to support both. The color value matches that
// used by UIWebView.
const CGFloat kBackgroundRGBComponents[] = {0.75f, 0.74f, 0.76f};

}  // namespace

@implementation CRWWebViewContentView {
  std::optional<UIEdgeInsets> _minViewportInset;
  std::optional<UIEdgeInsets> _maxViewportInset;
  BOOL _hasPendingViewportInsets;
  // Bounds size at the last viewport insets notification. Rotation can change
  // the size without changing the insets (e.g. on iPad).
  std::optional<CGSize> _lastViewportInsetsBoundsSize;
  BOOL _usesObscuredInsets;
  std::string _mimeTypeString;
  CRWViewportInsetsAnimator* _insetsAnimator;
}
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
@synthesize mimeType = _mimeType;

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

- (void)setMimeType:(NSString*)mimeType {
  if (_mimeType != mimeType && ![_mimeType isEqualToString:mimeType]) {
    _mimeType = mimeType;
    _mimeTypeString = base::SysNSStringToUTF8(mimeType);
    [self setNeedsLayout];
    if (_usesObscuredInsets) {
      // Force a re-evaluation of obscuredInsets now that the MIME type is
      // known.
      UIEdgeInsets currentInsets = _obscuredInsets;
      _obscuredInsets = UIEdgeInsetsZero;
      [self setObscuredInsets:currentInsets];
    }
  }
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

- (void)dealloc {
  [_insetsAnimator stop];
  _insetsAnimator = nil;
}

- (void)willMoveToWindow:(UIWindow*)newWindow {
  [super willMoveToWindow:newWindow];
  if (!newWindow) {
    [_insetsAnimator stop];
    _insetsAnimator = nil;
  }
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

- (void)safeAreaInsetsDidChange {
  [super safeAreaInsetsDidChange];
  if (_minViewportInset && _maxViewportInset) {
    [self setMinimumViewportInset:*_minViewportInset
             maximumViewportInset:*_maxViewportInset];
  }
  if (_usesObscuredInsets) {
    if (_insetsAnimator) {
      [self applyContentInsetModeObscuredInsets:_insetsAnimator.currentInsets];
    } else {
      UIEdgeInsets obscuredInsets = _obscuredInsets;
      [UIView performWithoutAnimation:^{
        [self setObscuredInsets:obscuredInsets];
      }];
    }
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset: {
      if (!_minViewportInset || !_maxViewportInset) {
        break;
      }
      BOOL boundsSizeChanged =
          _lastViewportInsetsBoundsSize &&
          !CGSizeEqualToSize(*_lastViewportInsetsBoundsSize, self.bounds.size);
      if (_hasPendingViewportInsets || boundsSizeChanged) {
        [self setMinimumViewportInset:*_minViewportInset
                 maximumViewportInset:*_maxViewportInset];
      }
      break;
    }
    case WebViewResizingType::kFrame:
      if (!_usesObscuredInsets) {
        break;
      }
      if (web::IsContentTypePdf(_mimeTypeString)) {
        if (_maxViewportInset) {
          UIEdgeInsets maxInsets = *_maxViewportInset;
          maxInsets.bottom = 0;
          _webView.frame = UIEdgeInsetsInsetRect(self.bounds, maxInsets);
        }
      } else {
        UIEdgeInsets contentInsets =
            AdjustInsetsForSafeArea(_obscuredInsets, self.safeAreaInsets);
        _webView.frame = UIEdgeInsetsInsetRect(self.bounds, contentInsets);
      }
      break;
  }
}

- (void)viewportInsetsDidChangeWithMinInset:(UIEdgeInsets)minInset
                                   maxInset:(UIEdgeInsets)maxInset {
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
  if (_insetsAnimator) {
    return _insetsAnimator.currentInsets;
  }
  return _obscuredInsets;
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets {
  [self setObscuredInsets:obscuredInsets initialVelocity:0.0];
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets
          initialVelocity:(CGFloat)initialVelocity {
  if (!UIEdgeInsetsEqualToEdgeInsets(obscuredInsets, UIEdgeInsetsZero)) {
    _usesObscuredInsets = YES;
  }
  BOOL insetsEqual =
      UIEdgeInsetsEqualToEdgeInsets(self.obscuredInsets, obscuredInsets);

  UIEdgeInsets contentInsets =
      AdjustInsetsForSafeArea(obscuredInsets, self.safeAreaInsets);

  BOOL appliedInsetsEqual = YES;
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset: {
      // Preserve any larger bottom content inset set by external callers (such
      // as for the virtual keyboard) so toolbar inset updates do not clobber
      // it. Note that this must check against `_obscuredInsets.bottom` (the
      // previous toolbar inset) rather than `obscuredInsets.bottom`, otherwise
      // the inset would never shrink when collapsing toolbars on scroll.
      if (_scrollView.contentInset.bottom > _obscuredInsets.bottom) {
        contentInsets.bottom =
            std::max(contentInsets.bottom, _scrollView.contentInset.bottom);
      }
      appliedInsetsEqual = UIEdgeInsetsEqualToEdgeInsets(
          _scrollView.contentInset, contentInsets);
      break;
    }
    case WebViewResizingType::kFrame:
      if (!_usesObscuredInsets) {
        break;
      }
      if (web::IsContentTypePdf(_mimeTypeString)) {
        if (_maxViewportInset) {
          appliedInsetsEqual = UIEdgeInsetsEqualToEdgeInsets(
              _scrollView.contentInset,
              AdjustedPdfContentInsets(contentInsets, *_maxViewportInset));
        }
      } else {
        appliedInsetsEqual = CGRectEqualToRect(
            _webView.frame, UIEdgeInsetsInsetRect(self.bounds, contentInsets));
      }
      break;
  }
  if (insetsEqual && appliedInsetsEqual) {
    return;
  }
  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset: {
      _scrollView.contentInsetAdjustmentBehavior =
          UIScrollViewContentInsetAdjustmentNever;
      NSTimeInterval duration = [UIView inheritedAnimationDuration];
      if (duration > 0) {
        if (_insetsAnimator) {
          if (UIEdgeInsetsEqualToEdgeInsets(_insetsAnimator.targetInsets,
                                            obscuredInsets)) {
            return;
          }
          _obscuredInsets = _insetsAnimator.currentInsets;
          [_insetsAnimator stop];
          _insetsAnimator = nil;
        }
        __weak __typeof(self) weakSelf = self;
        _insetsAnimator = [[CRWViewportInsetsAnimator alloc]
            initWithStartInsets:_obscuredInsets
            targetInsets:obscuredInsets
            duration:duration
            initialVelocity:initialVelocity
            updateHandler:^(UIEdgeInsets insets) {
              [weakSelf applyContentInsetModeObscuredInsets:insets];
            }
            completion:^{
              [weakSelf insetsAnimationDidComplete];
            }];
        [_insetsAnimator start];
      } else {
        if (_insetsAnimator) {
          _obscuredInsets = _insetsAnimator.currentInsets;
          [_insetsAnimator stop];
          _insetsAnimator = nil;
        }
        [self applyContentInsetModeObscuredInsets:obscuredInsets];
      }
      break;
    }
    case WebViewResizingType::kFrame: {
      if (_insetsAnimator) {
        [_insetsAnimator stop];
        _insetsAnimator = nil;
      }
      if (!_usesObscuredInsets) {
        break;
      }
      if (web::IsContentTypePdf(_mimeTypeString)) {
        _obscuredInsets = obscuredInsets;
        if (_maxViewportInset) {
          _scrollView.contentInsetAdjustmentBehavior =
              UIScrollViewContentInsetAdjustmentNever;

          // Keep the WKWebView frame constant during scroll. Resizing the frame
          // dynamically breaks scroll momentum in PDFs. We do not change the
          // frame here, but rather rely on layoutSubviews and
          // setMinimumViewportInset.
          _scrollView.contentInset =
              AdjustedPdfContentInsets(contentInsets, *_maxViewportInset);
        }
        break;
      }

      // Update the scroll offset to account for the changing frame.
      CGPoint offset = _scrollView.contentOffset;
      if (offset.y > 0) {
        CGFloat topDelta = obscuredInsets.top - _obscuredInsets.top;
        offset.y = std::max<CGFloat>(0, offset.y + topDelta);
        _scrollView.contentOffset = offset;
      }
      // Update the frame.
      _webView.frame = UIEdgeInsetsInsetRect(self.bounds, contentInsets);
      _obscuredInsets = obscuredInsets;
      break;
    }
  }
}

- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset {
  _minViewportInset = minInset;
  _maxViewportInset = maxInset;

  UIEdgeInsets effectiveMinInset =
      AdjustInsetsForSafeArea(minInset, self.safeAreaInsets);
  UIEdgeInsets effectiveMaxInset =
      AdjustInsetsForSafeArea(maxInset, self.safeAreaInsets);

  switch (self.webViewResizingType) {
    case WebViewResizingType::kContentInset: {
      BOOL isFrameLargeEnough;
      if (base::FeatureList::IsEnabled(kCRWWebViewContentViewLayoutFix)) {
        isFrameLargeEnough = IsFrameLargeEnoughToApplyViewportInsets(
            _webView.frame.size, effectiveMinInset, effectiveMaxInset);
      } else {
        isFrameLargeEnough = !CGRectIsEmpty(
            UIEdgeInsetsInsetRect(_webView.bounds, effectiveMaxInset));
      }

      // Only apply the viewport insets if the web view's frame is large enough
      // to accommodate them.
      if (_webView.window && isFrameLargeEnough) {
        _hasPendingViewportInsets = NO;
        BOOL insetsUnchanged =
            UIEdgeInsetsEqualToEdgeInsets(_webView.minimumViewportInset,
                                          effectiveMinInset) &&
            UIEdgeInsetsEqualToEdgeInsets(_webView.maximumViewportInset,
                                          effectiveMaxInset);
        CGSize boundsSize = self.bounds.size;
        BOOL boundsSizeUnchanged =
            _lastViewportInsetsBoundsSize &&
            CGSizeEqualToSize(*_lastViewportInsetsBoundsSize, boundsSize);
        if (insetsUnchanged && boundsSizeUnchanged) {
          break;
        }
        if (!insetsUnchanged) {
          [_webView setMinimumViewportInset:effectiveMinInset
                       maximumViewportInset:effectiveMaxInset];
        }
        _lastViewportInsetsBoundsSize = boundsSize;
        [self viewportInsetsDidChangeWithMinInset:effectiveMinInset
                                         maxInset:effectiveMaxInset];
        [_webView setNeedsLayout];
      } else {
        _hasPendingViewportInsets = YES;
      }
      break;
    }
    case WebViewResizingType::kFrame: {
      if (web::IsContentTypePdf(_mimeTypeString) && _usesObscuredInsets) {
        // Inset the frame by maxInsets to prevent covering the page indicator
        // badge underneath the top toolbar.
        UIEdgeInsets maxInsetsForFrame = maxInset;
        maxInsetsForFrame.bottom = 0;
        _webView.frame = UIEdgeInsetsInsetRect(self.bounds, maxInsetsForFrame);

        _scrollView.contentInsetAdjustmentBehavior =
            UIScrollViewContentInsetAdjustmentNever;
        UIEdgeInsets contentInsets =
            AdjustInsetsForSafeArea(_obscuredInsets, self.safeAreaInsets);
        _scrollView.contentInset =
            AdjustedPdfContentInsets(contentInsets, maxInset);
      }

      // Do not pass the min/max viewport insets to the underlying web view if
      // we are resizing its frame. Since these insets are relative to the frame
      // and we cannot report negative insets, there is no way to properly
      // report the minimum insets. See http://crbug.com/40944174#comment17. We
      // do, however, cache the maxInset so it can be used to lock the frame
      // size for the iOS 18 PDF scroll momentum workaround.
      break;
    }
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

#pragma mark - Private

// Applies obscured insets to the scroll view and web view when using
// WebViewResizingType::kContentInset.
- (void)applyContentInsetModeObscuredInsets:(UIEdgeInsets)obscuredInsets {
  UIEdgeInsets contentInsets =
      AdjustInsetsForSafeArea(obscuredInsets, self.safeAreaInsets);

  UIEdgeInsets oldInsets = _scrollView.contentInset;
  // Preserve any larger bottom content inset set by external callers (such as
  // for the virtual keyboard) so toolbar inset updates do not clobber it. Note
  // that this must check against `_obscuredInsets.bottom` (the previous toolbar
  // inset) rather than `obscuredInsets.bottom`, otherwise the inset would never
  // shrink when collapsing toolbars on scroll.
  if (oldInsets.bottom > _obscuredInsets.bottom) {
    contentInsets.bottom = std::max(contentInsets.bottom, oldInsets.bottom);
  }
  _scrollView.contentInset = contentInsets;
  CGFloat topDelta = contentInsets.top - oldInsets.top;
  if (!web::IsContentTypePdf(_mimeTypeString) && topDelta != 0 &&
      std::fabs(_scrollView.contentOffset.y - (-oldInsets.top)) < 0.1) {
    CGPoint offset = _scrollView.contentOffset;
    offset.y = -contentInsets.top;
    _scrollView.contentOffset = offset;
  }
  if (@available(iOS 26, *)) {
    [_webView setObscuredContentInsets:obscuredInsets];
  }
  _obscuredInsets = obscuredInsets;
}

// Called when an insets animation completes, clearing the animator instance.
- (void)insetsAnimationDidComplete {
  _insetsAnimator = nil;
}

@end

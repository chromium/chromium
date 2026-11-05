// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/crc_web_view_proxy_impl.h"

#import <cmath>
#import <limits>

#import "base/check.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

// TODO(crbug.com/40257932): These methods are defined in
// crw_web_view_proxy_impl.h. Move them out of the category and into
// the main class.
@interface CRWWebViewScrollViewProxy (ForwardDeclares)
- (void)cr_addInsets:(UIEdgeInsets)insets;
- (void)cr_removeInsets:(UIEdgeInsets)insets;
@end

@implementation CRCWebViewProxyImpl {
  NSMutableDictionary* _registeredInsets;
  // The WebViewScrollViewProxy is a wrapper around the UIScrollView
  // to give components access in a limited and controlled manner.
  CRWWebViewScrollViewProxy* _contentViewScrollViewProxy;
  BOOL _ignoreObscuredInsets;
  BOOL _shouldSuppressInputViews;
}
@synthesize contentView = _contentView;
@synthesize contentInset = _contentInset;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;
@synthesize allowsLinkPreview = _allowsLinkPreview;
@synthesize ignoreObscuredInsets = _ignoreObscuredInsets;
@synthesize obscuredContentInsets = _obscuredContentInsets;
@synthesize shouldSuppressInputViews = _shouldSuppressInputViews;
@dynamic keyboardVisible;

- (instancetype)init {
  self = [super init];
  if (self) {
    _registeredInsets = [[NSMutableDictionary alloc] init];
    _contentViewScrollViewProxy = [[CRWWebViewScrollViewProxy alloc] init];
  }
  return self;
}

- (CRWWebViewScrollViewProxy*)scrollViewProxy {
  return _contentViewScrollViewProxy;
}

- (BOOL)allowsBackForwardNavigationGestures {
  return NO;
}

- (void)setAllowsBackForwardNavigationGestures:
    (BOOL)allowsBackForwardNavigationGestures {
}

- (CGRect)bounds {
  return [_contentView bounds];
}

- (CGRect)frame {
  return [_contentView frame];
}

- (CGPoint)contentOffset {
  return _contentView.contentOffset;
}

- (void)setContentOffset:(CGPoint)contentOffset {
  _contentView.contentOffset = contentOffset;
}

- (UIEdgeInsets)contentInset {
  return self.shouldUseViewContentInset ? _contentView.contentInset
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
    _contentView.contentInset = contentInset;
  }
}

- (NSArray*)gestureRecognizers {
  return [_contentView gestureRecognizers];
}

- (void)setShouldUseViewContentInset:(BOOL)shouldUseViewContentInset {
  if (_shouldUseViewContentInset != shouldUseViewContentInset) {
    UIEdgeInsets oldContentInset = self.contentInset;
    self.contentInset = UIEdgeInsetsZero;
    _shouldUseViewContentInset = shouldUseViewContentInset;
    self.contentInset = oldContentInset;
  }
}

- (void)registerInsets:(UIEdgeInsets)insets forCaller:(id)caller {
  NSValue* callerValue = [NSValue valueWithNonretainedObject:caller];
  if ([_registeredInsets objectForKey:callerValue]) {
    [self unregisterInsetsForCaller:caller];
  }
  [self.scrollViewProxy cr_addInsets:insets];
  [_registeredInsets setObject:[NSValue valueWithUIEdgeInsets:insets]
                        forKey:callerValue];
}

- (void)unregisterInsetsForCaller:(id)caller {
  NSValue* callerValue = [NSValue valueWithNonretainedObject:caller];
  NSValue* insetsValue = [_registeredInsets objectForKey:callerValue];
  [self.scrollViewProxy cr_removeInsets:[insetsValue UIEdgeInsetsValue]];
  [_registeredInsets removeObjectForKey:callerValue];
}

- (void)setContentView:(UIScrollView*)contentView {
  DCHECK(contentView);
  _contentView = contentView;
  [_contentViewScrollViewProxy setScrollView:contentView];
}

- (void)addSubview:(UIView*)view {
  return [_contentView addSubview:view];
}

- (BOOL)isKeyboardVisible {
  return NO;
}

- (BOOL)becomeFirstResponder {
  return [_contentView becomeFirstResponder];
}

- (BOOL)isWebPageInFullscreenMode {
  return NO;
}

- (UIEdgeInsets)obscuredInsets {
  return self.contentInset;
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets {
  [self setObscuredInsets:obscuredInsets initialVelocity:0.0];
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets
          initialVelocity:(CGFloat)initialVelocity {
  [self setContentInset:obscuredInsets];
}

- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset {
}
@end

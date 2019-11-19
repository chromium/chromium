// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#import "ios/web/common/crw_content_view.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the first responder in the subviews of |view|, or nil if no view in
// the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder])
    return view;

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder)
      return firstResponder;
  }

  return nil;
}

}  // namespace

@interface CRWWebViewScrollViewProxy (ContentInsetsAlgebra)
// Adds the given insets to the current content insets and scroll indicator
// insets of the receiver.
- (void)cr_addInsets:(UIEdgeInsets)insets;
// Removes the given insets to the current content insets and scroll indicator
// insets of the receiver.
- (void)cr_removeInsets:(UIEdgeInsets)insets;
@end

@implementation CRWWebViewScrollViewProxy (ContentInsetsAlgebra)

- (void)cr_addInsets:(UIEdgeInsets)insets {
  if (UIEdgeInsetsEqualToEdgeInsets(insets, UIEdgeInsetsZero))
    return;

  UIEdgeInsets currentInsets = [self contentInset];
  currentInsets.top += insets.top;
  currentInsets.left += insets.left;
  currentInsets.bottom += insets.bottom;
  currentInsets.right += insets.right;
  [self setContentInset:currentInsets];
  [self setScrollIndicatorInsets:currentInsets];
}

- (void)cr_removeInsets:(UIEdgeInsets)insets {
  UIEdgeInsets negativeInsets = UIEdgeInsetsZero;
  negativeInsets.top = -insets.top;
  negativeInsets.left = -insets.left;
  negativeInsets.bottom = -insets.bottom;
  negativeInsets.right = -insets.right;
  [self cr_addInsets:negativeInsets];
}

@end

@implementation CRWWebViewProxyImpl {
  __weak CRWWebController* _webController;
  NSMutableDictionary* _registeredInsets;
  // The WebViewScrollViewProxy is a wrapper around the web view's
  // UIScrollView to give components access in a limited and controlled manner.
  CRWWebViewScrollViewProxy* _contentViewScrollViewProxy;
}
@synthesize contentView = _contentView;

- (instancetype)initWithWebController:(CRWWebController*)webController {
  self = [super init];
  if (self) {
    DCHECK(webController);
    _registeredInsets = [[NSMutableDictionary alloc] init];
    _webController = webController;
    _contentViewScrollViewProxy = [[CRWWebViewScrollViewProxy alloc] init];
  }
  return self;
}

- (CRWWebViewScrollViewProxy*)scrollViewProxy {
  return _contentViewScrollViewProxy;
}

- (BOOL)allowsBackForwardNavigationGestures {
  return _webController.allowsBackForwardNavigationGestures;
}

- (void)setAllowsBackForwardNavigationGestures:
    (BOOL)allowsBackForwardNavigationGestures {
  _webController.allowsBackForwardNavigationGestures =
      allowsBackForwardNavigationGestures;
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
  return _contentView.contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  _contentView.contentInset = contentInset;
}

- (NSArray*)gestureRecognizers {
  return [_contentView gestureRecognizers];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_contentView addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_contentView removeGestureRecognizer:gestureRecognizer];
}

- (BOOL)shouldUseViewContentInset {
  SEL shouldUseInsetSelector = @selector(shouldUseViewContentInset);
  return [_contentView respondsToSelector:shouldUseInsetSelector] &&
         [_contentView shouldUseViewContentInset];
}

- (void)setShouldUseViewContentInset:(BOOL)shouldUseViewContentInset {
  if ([_contentView
          respondsToSelector:@selector(setShouldUseViewContentInset:)]) {
    [_contentView setShouldUseViewContentInset:shouldUseViewContentInset];
  }
}

- (void)registerInsets:(UIEdgeInsets)insets forCaller:(id)caller {
  NSValue* callerValue = [NSValue valueWithNonretainedObject:caller];
  if ([_registeredInsets objectForKey:callerValue])
    [self unregisterInsetsForCaller:caller];
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

- (void)setContentView:(CRWContentView*)contentView {
  _contentView = contentView;
  [_contentViewScrollViewProxy setScrollView:contentView.scrollView];
}

- (void)addSubview:(UIView*)view {
  return [_contentView addSubview:view];
}

- (BOOL)hasSearchableTextContent {
  return _contentView != nil && [_webController contentIsHTML];
}

- (UIView*)keyboardAccessory {
  if (!_contentView)
    return nil;
  UIView* firstResponder = GetFirstResponderSubview(_contentView);
  return firstResponder.inputAccessoryView;
}

- (BOOL)becomeFirstResponder {
  return [_contentView becomeFirstResponder];
}

@end

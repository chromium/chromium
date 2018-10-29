// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller_container_view.h"

#include "base/logging.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebControllerContainerView ()

// Redefine properties as readwrite.
@property(nonatomic, strong, readwrite)
    CRWWebViewContentView* webViewContentView;
@property(nonatomic, strong, readwrite) id<CRWNativeContent> nativeController;
@property(nonatomic, strong, readwrite) CRWContentView* transientContentView;

// Convenience getter for the proxy object.
@property(nonatomic, weak, readonly) CRWWebViewProxyImpl* contentViewProxy;

// Returns |self.bounds| after being inset at the top and bottom by the header
// and footer heights returned by the delegate.  This is only used to lay out
// native controllers, as the header height is already accounted for in the
// scroll view content insets for other CRWContentViews.
@property(nonatomic, readonly) CGRect nativeContentVisibleFrame;

@end

@implementation CRWWebControllerContainerView
@synthesize webViewContentView = _webViewContentView;
@synthesize nativeController = _nativeController;
@synthesize transientContentView = _transientContentView;
@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:
        (id<CRWWebControllerContainerViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  self.contentViewProxy.contentView = nil;
}

#pragma mark Accessors

- (void)setWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  if (![_webViewContentView isEqual:webViewContentView]) {
    [_webViewContentView removeFromSuperview];
    _webViewContentView = webViewContentView;
    [_webViewContentView setFrame:self.bounds];
    [self addSubview:_webViewContentView];
  }
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  if (![_nativeController isEqual:nativeController]) {
    __weak id oldController = _nativeController;
    if ([oldController respondsToSelector:@selector(willBeDismissed)]) {
      [oldController willBeDismissed];
    }
    [[oldController view] removeFromSuperview];
    _nativeController = nativeController;
    // TODO(crbug.com/503297): Re-enable this DCHECK once native controller
    // leaks are fixed.
    //    DCHECK(!oldController);
  }
}

- (void)setTransientContentView:(CRWContentView*)transientContentView {
  if (![_transientContentView isEqual:transientContentView]) {
    [_transientContentView removeFromSuperview];
    _transientContentView = transientContentView;
  }
}

- (CRWWebViewProxyImpl*)contentViewProxy {
  return [_delegate contentViewProxyForContainerView:self];
}

- (CGRect)nativeContentVisibleFrame {
  CGFloat headerHeight =
      [_delegate nativeContentHeaderHeightForContainerView:self];
  return UIEdgeInsetsInsetRect(self.bounds,
                               UIEdgeInsetsMake(headerHeight, 0, 0, 0));
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  // webViewContentView layout.  |-setNeedsLayout| is called in case any webview
  // layout updates need to occur despite the bounds size staying constant.
  self.webViewContentView.frame = self.bounds;
  [self.webViewContentView setNeedsLayout];

  // TODO(crbug.com/570114): Move adding of the following subviews to another
  // place.

  // nativeController layout.
  if (self.nativeController) {
    UIView* nativeView = [self.nativeController view];
    if (!nativeView.superview) {
      [self addSubview:nativeView];
      [nativeView setNeedsUpdateConstraints];
    }
    nativeView.frame = self.nativeContentVisibleFrame;
  }

  // transientContentView layout.
  if (self.transientContentView) {
    if (!self.transientContentView.superview)
      [self addSubview:self.transientContentView];
    self.transientContentView.frame = self.bounds;
  }
}

- (BOOL)isViewAlive {
  return self.webViewContentView || self.transientContentView ||
         [self.nativeController isViewAlive];
}

#pragma mark Content Setters

- (void)resetContent {
  self.webViewContentView = nil;
  self.nativeController = nil;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = nil;
}

- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  DCHECK(webViewContentView);
  self.webViewContentView = webViewContentView;
  self.nativeController = nil;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
  [self setNeedsLayout];
}

- (void)displayNativeContent:(id<CRWNativeContent>)nativeController {
  DCHECK(nativeController);
  self.webViewContentView = nil;
  self.nativeController = nativeController;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = nil;
  [self setNeedsLayout];
}

- (void)displayTransientContent:(CRWContentView*)transientContentView {
  DCHECK(transientContentView);
  self.transientContentView = transientContentView;
  self.contentViewProxy.contentView = self.transientContentView;
  [self setNeedsLayout];
}

- (void)clearTransientContentView {
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
}

@end

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_proxy_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullscreenWebViewProxyObserver ()<CRWWebViewScrollViewProxyObserver>
// The model and mediator passed on initialization.
@property(nonatomic, readonly) FullscreenModel* model;
@property(nonatomic, readonly) FullscreenMediator* mediator;
@end

@implementation FullscreenWebViewProxyObserver
@synthesize proxy = _proxy;
@synthesize model = _model;
@synthesize mediator = _mediator;

- (instancetype)initWithModel:(FullscreenModel*)model
                     mediator:(FullscreenMediator*)mediator {
  if (self = [super init]) {
    _model = model;
    DCHECK(_model);
    _mediator = mediator;
    DCHECK(_mediator);
  }
  return self;
}

#pragma mark - Accessors

- (void)setProxy:(id<CRWWebViewProxy>)proxy {
  if (_proxy == proxy)
    return;
  [_proxy.scrollViewProxy removeObserver:self];
  _proxy = proxy;
  [_proxy.scrollViewProxy addObserver:self];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewProxyDidSetScrollView:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  MoveContentBelowHeader(self.proxy, self.model);
}

- (BOOL)webViewScrollViewShouldScrollToTop:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Exit fullscreen when the status bar is tapped, but don't allow the scroll-
  // to-top animation to occur if the toolbars are fully collapsed.
  BOOL scrollToTop = !AreCGFloatsEqual(self.model->progress(), 0.0);
  self.mediator->ExitFullscreen();
  return scrollToTop;
}

@end

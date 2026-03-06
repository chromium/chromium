// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_web_view_proxy_observer.h"

#import "base/check.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

@interface FullscreenWebViewProxyObserver () <CRWWebViewScrollViewProxyObserver>
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
  if ((self = [super init])) {
    _model = model;
    DCHECK(_model);
    _mediator = mediator;
    DCHECK(_mediator);
  }
  return self;
}

#pragma mark - Accessors

- (void)setProxy:(id<CRWWebViewProxy>)proxy {
  if (_proxy == proxy) {
    return;
  }
  [_proxy.scrollViewProxy removeObserver:self];
  _proxy = proxy;
  [_proxy.scrollViewProxy addObserver:self];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (BOOL)webViewScrollViewShouldScrollToTop:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Exit fullscreen when the status bar is tapped, but don't allow the scroll-
  // to-top animation to occur if the toolbars are fully collapsed.
  BOOL scrollToTop = !AreCGFloatsEqual(self.model->progress(), 0.0);
  self.mediator->ExitFullscreen(FullscreenExitReason::kUserTapped);
  return scrollToTop;
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    // When the broadcaster is disabled, the model's top inset must be manually
    // updated to ensure that it correctly identifies the top of the page when
    // calculating overscroll and scroll boundaries.
    self.model->SetTopContentInset(webViewScrollViewProxy.contentInset.top);
    self.model->SetContentHeight(webViewScrollViewProxy.contentSize.height);
    self.model->SetScrollViewHeight(webViewScrollViewProxy.frame.size.height);
    self.model->SetYContentOffset(webViewScrollViewProxy.contentOffset.y);
  }
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    // Manually relay dimensions on drag start to ensure the model has
    // up-to-date state before processing the scroll.
    self.model->SetTopContentInset(webViewScrollViewProxy.contentInset.top);
    self.model->SetContentHeight(webViewScrollViewProxy.contentSize.height);
    self.model->SetScrollViewHeight(webViewScrollViewProxy.frame.size.height);
    self.model->SetYContentOffset(webViewScrollViewProxy.contentOffset.y);
    self.model->SetScrollViewIsScrolling(true);
    self.model->SetScrollViewIsDragging(true);
  }
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    self.model->SetScrollViewIsDragging(false);
  }
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    self.model->SetScrollViewIsDragging(false);
    if (!decelerate) {
      self.model->SetScrollViewIsScrolling(false);
    }
  }
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    self.model->SetScrollViewIsScrolling(false);
  }
}

- (void)webViewScrollViewWillBeginZooming:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    self.model->SetScrollViewIsZooming(true);
  }
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  if (!web::features::ShouldUseBroadcasterForSmoothScrolling()) {
    self.model->SetScrollViewIsZooming(false);
  }
}

@end

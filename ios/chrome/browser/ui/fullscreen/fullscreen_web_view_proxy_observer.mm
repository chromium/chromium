// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_proxy_observer.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

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

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetYContentOffset(webViewScrollViewProxy.contentOffset.y);
  }
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetScrollViewIsScrolling(true);
    self.model->SetScrollViewIsDragging(true);
  }
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetScrollViewIsScrolling(false);
    self.model->SetScrollViewIsDragging(false);
  }
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetScrollViewIsScrolling(false);
    self.model->SetScrollViewIsDragging(false);
  }
}

- (void)webViewScrollViewWillBeginZooming:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetScrollViewIsZooming(true);
  }
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    self.model->SetScrollViewIsZooming(false);
  }
}

@end

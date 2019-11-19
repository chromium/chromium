// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"

#include "base/ios/ios_util.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullscreenWebViewResizer ()
// The fullscreen model, used to get the information about the state of
// fullscreen.
@property(nonatomic, assign) FullscreenModel* model;
@end

@implementation FullscreenWebViewResizer

@synthesize model = _model;
@synthesize webState = _webState;

- (instancetype)initWithModel:(FullscreenModel*)model {
  self = [super init];
  if (self) {
    _model = model;
    _compensateFrameChangeByOffset = YES;
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webState->GetView())
    [_webState->GetView() removeObserver:self forKeyPath:@"frame"];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;

  if (_webState && _webState->GetView())
    [_webState->GetView() removeObserver:self forKeyPath:@"frame"];

  _webState = webState;

  if (webState) {
    [self observeWebStateViewFrame:webState];
    self.compensateFrameChangeByOffset = NO;
    [self updateForCurrentState];
    self.compensateFrameChangeByOffset = YES;
  }
}

#pragma mark - Public

- (void)updateForCurrentState {
  if (!self.webState)
    return;

  [self updateForFullscreenProgress:self.model->progress()];
}

- (void)forceToUpdateToProgress:(CGFloat)progress {
  if (!self.webState)
    return;

  [self.webState->GetView() removeObserver:self forKeyPath:@"frame"];
  [self updateForFullscreenProgress:progress];
  [self observeWebStateViewFrame:self.webState];
}

#pragma mark - Private

// Updates the WebView of the current webState to adjust it to the current
// fullscreen |progress|. |progress| should be between 0 and 1, 0 meaning that
// the application is in fullscreen, 1 that it is out of fullscreen.
- (void)updateForFullscreenProgress:(CGFloat)progress {
  if (!self.webState || !self.webState->GetView().superview)
    return;

  [self updateForInsets:self.model->GetToolbarInsetsAtProgress(progress)];
  self.model->SetWebViewSafeAreaInsets(self.webState->GetView().safeAreaInsets);
}

// Updates the WebState view, resizing it such as |insets| is the insets between
// the WebState view and its superview.
- (void)updateForInsets:(UIEdgeInsets)insets {
  UIView* webView = self.webState->GetView();

  id<CRWWebViewProxy> webViewProxy = self.webState->GetWebViewProxy();
  CRWWebViewScrollViewProxy* scrollViewProxy = webViewProxy.scrollViewProxy;

  if (self.webState->GetContentsMimeType() == "application/pdf") {
    scrollViewProxy.contentInset = insets;
    if (!CGRectEqualToRect(webView.frame, webView.superview.bounds)) {
      webView.frame = webView.superview.bounds;
    }
    return;
  }

  CGRect newFrame = UIEdgeInsetsInsetRect(webView.superview.bounds, insets);

  // Make sure the frame has changed to avoid a loop as the frame property is
  // actually monitored by this object.
  if (std::fabs(newFrame.origin.x - webView.frame.origin.x) < 0.01 &&
      std::fabs(newFrame.origin.y - webView.frame.origin.y) < 0.01 &&
      std::fabs(newFrame.size.width - webView.frame.size.width) < 0.01 &&
      std::fabs(newFrame.size.height - webView.frame.size.height) < 0.01)
    return;

  // Update the content offset of the scroll view to match the padding
  // that will be included in the frame.
  CGFloat currentTopInset = webView.frame.origin.y;
  CGPoint newContentOffset = scrollViewProxy.contentOffset;
  newContentOffset.y += insets.top - currentTopInset;
  if (self.compensateFrameChangeByOffset) {
    scrollViewProxy.contentOffset = newContentOffset;
  }

  webView.frame = newFrame;

  // Setting WKWebView frame can mistakenly reset contentOffset. Change it
  // back to the initial value if necessary.
  // TODO(crbug.com/645857): Remove this workaround once WebKit bug is
  // fixed.
  if (self.compensateFrameChangeByOffset &&
      [scrollViewProxy contentOffset].y != newContentOffset.y) {
    [scrollViewProxy setContentOffset:newContentOffset];
  }
}

// Observes the frame property of the view of the |webState| using KVO.
- (void)observeWebStateViewFrame:(web::WebState*)webState {
  if (!webState->GetView())
    return;

  [webState->GetView() addObserver:self
                        forKeyPath:@"frame"
                           options:0
                           context:nil];
}

// Callback for the KVO.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (![keyPath isEqualToString:@"frame"] || object != _webState->GetView())
    return;

  [self updateForCurrentState];
}

@end

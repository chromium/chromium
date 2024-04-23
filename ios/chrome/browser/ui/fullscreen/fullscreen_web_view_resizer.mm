// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

@interface FullscreenWebViewResizer ()
// The fullscreen model, used to get the information about the state of
// fullscreen.
@property(nonatomic, assign) FullscreenModel* model;
@end

@implementation FullscreenWebViewResizer {
  BOOL _installedObserver;
}

@synthesize model = _model;
@synthesize webState = _webState;

- (instancetype)initWithModel:(FullscreenModel*)model {
  self = [super init];
  if (self) {
    _installedObserver = NO;
    _model = model;
    _compensateFrameChangeByOffset = YES;
  }
  return self;
}

- (void)dealloc {
  [self stopObservingWebStateViewFrame];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;

  [self stopObservingWebStateViewFrame];
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

  [self stopObservingWebStateViewFrame];
  [self updateForFullscreenProgress:progress];
  [self observeWebStateViewFrame:self.webState];
}

#pragma mark - Private

// Updates the WebView of the current webState to adjust it to the current
// fullscreen `progress`. `progress` should be between 0 and 1, 0 meaning that
// the application is in fullscreen, 1 that it is out of fullscreen.
- (void)updateForFullscreenProgress:(CGFloat)progress {
  if (!self.webState || !self.webState->GetView().superview ||
      self.model->IsForceFullscreenMode()) {
    return;
  }

  [self updateForInsets:self.model->GetToolbarInsetsAtProgress(progress)];
  self.model->SetWebViewSafeAreaInsets(self.webState->GetView().safeAreaInsets);
}

// Updates the WebState view, resizing it such as `insets` is the insets between
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

  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    // Make sure the frame has changed to avoid a loop as the frame property is
    // actually monitored by this object.
    if (std::fabs(newFrame.origin.x - webView.frame.origin.x) < 0.01 &&
        std::fabs(newFrame.origin.y - webView.frame.origin.y) < 0.01 &&
        std::fabs(newFrame.size.width - webView.frame.size.width) < 0.01 &&
        std::fabs(newFrame.size.height - webView.frame.size.height) < 0.01) {
      return;
    }
  }

  CGFloat currentTopInset = webView.frame.origin.y;
  CGPoint newContentOffset = scrollViewProxy.contentOffset;
  newContentOffset.y += insets.top - currentTopInset;
  if (ios::provider::IsFullscreenSmoothScrollingSupported()) {
    // Update the content offset of the scroll view to match the padding
    // that will be included in the frame.
    if (self.compensateFrameChangeByOffset) {
      scrollViewProxy.contentOffset = newContentOffset;
    }
  }

  webView.frame = newFrame;

  if (ios::provider::IsFullscreenSmoothScrollingSupported()) {
    // Setting WKWebView frame can mistakenly reset contentOffset. Change it
    // back to the initial value if necessary.
    // TODO(crbug.com/40484457): Remove this workaround once WebKit bug is
    // fixed.
    if (self.compensateFrameChangeByOffset &&
        [scrollViewProxy contentOffset].y != newContentOffset.y) {
      [scrollViewProxy setContentOffset:newContentOffset];
    }
  }
}

// Observes the frame property of the view of the `webState` using KVO.
- (void)observeWebStateViewFrame:(web::WebState*)webState {
  if (!ios::provider::IsFullscreenSmoothScrollingSupported() ||
      _installedObserver || !webState->GetView()) {
    return;
  }

  NSKeyValueObservingOptions options = 0;
  [webState->GetView() addObserver:self
                        forKeyPath:@"frame"
                           options:options
                           context:nil];
  _installedObserver = YES;
}

// Callback for the KVO.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (!ios::provider::IsFullscreenSmoothScrollingSupported() ||
      ![keyPath isEqualToString:@"frame"] || object != _webState->GetView()) {
    return;
  }

  [self updateForCurrentState];
}

- (void)stopObservingWebStateViewFrame {
  if (!_installedObserver) {
    return;
  }

  DCHECK(_webState);
  DCHECK(_webState->GetView());
  [_webState->GetView() removeObserver:self forKeyPath:@"frame"];
  _installedObserver = NO;
}

@end

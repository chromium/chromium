// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_controller.h"

#import "base/values.h"
#import "ios/web/common/crw_viewport_adjustment.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/context_menu/context_menu_params_utils.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/web_state/ui/crw_context_menu_element_fetcher.h"
#import "ui/gfx/geometry/rect_f.h"
#import "ui/gfx/image/image.h"

namespace {

const CGFloat kJavaScriptTimeout = 1;

// Wrapper around CFRunLoop() to help crash server put all crashes happening
// while the loop is executed in the same bucket. Marked as `noinline` to
// prevent clang from optimising the function out in official builds.
void __attribute__((noinline)) ContextMenuNestedCFRunLoop() {
  CFRunLoopRun();
}

}  // namespace

@interface CRWContextMenuController () <UIContextMenuInteractionDelegate>

// The context menu responsible for the interaction.
@property(nonatomic, strong) UIContextMenuInteraction* contextMenu;

// View used to do the highlight/dismiss animation.
@property(nonatomic, strong) UIImageView* screenshotView;

@property(nonatomic, strong) WKWebView* webView;

@property(nonatomic, assign) web::WebState* webState;

@property(nonatomic, strong) CRWContextMenuElementFetcher* elementFetcher;

@end

@implementation CRWContextMenuController

@synthesize screenshotView = _screenshotView;

- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebState*)webState
                  containerView:(UIView*)containerView {
  self = [super init];
  if (self) {
    _contextMenu = [[UIContextMenuInteraction alloc] initWithDelegate:self];

    _webView = webView;

    // Do not add the interaction to the WKWebView itself as this may interfer
    // with the JS touch event. see crbug/351696381.
    [containerView addInteraction:_contextMenu];

    _webState = webState;

    _elementFetcher =
        [[CRWContextMenuElementFetcher alloc] initWithWebView:webView
                                                     webState:webState];
  }
  return self;
}

#pragma mark - Property

- (UIImageView*)screenshotView {
  if (!_screenshotView) {
    // If the views have a CGRectZero size, it is not taken into account.
    CGRect rectSizedOne = CGRectMake(0, 0, 1, 1);
    _screenshotView = [[UIImageView alloc] initWithFrame:rectSizedOne];
    _screenshotView.backgroundColor = UIColor.clearColor;
  }
  return _screenshotView;
}

- (void)setScreenshotView:(UIImageView*)screenshotView {
  if (_screenshotView.superview) {
    [_screenshotView removeFromSuperview];
  }
  _screenshotView = screenshotView;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  CGPoint locationInWebView =
      [self.webView.scrollView convertPoint:location fromView:interaction.view];

  locationInWebView.x /= self.webView.scrollView.zoomScale;
  locationInWebView.y /= self.webView.scrollView.zoomScale;

  std::optional<web::ContextMenuParams> optionalParams =
      [self fetchContextMenuParamsAtLocation:locationInWebView];

  if (!optionalParams.has_value()) {
    return nil;
  }
  web::ContextMenuParams params = optionalParams.value();

  self.screenshotView.center = location;

  // Adding the screenshotView here so they can be used in the
  // delegate's methods. Will be removed if no menu is presented.
  [interaction.view addSubview:self.screenshotView];

  params.location = [self.webView convertPoint:location
                                      fromView:interaction.view];

  __block UIContextMenuConfiguration* configuration = nil;
  if (self.webState && self.webState->GetDelegate()) {
    self.webState->GetDelegate()->ContextMenuConfiguration(
        self.webState, params, ^(UIContextMenuConfiguration* conf) {
          configuration = conf;
        });
  }

  if (configuration) {
    // User long pressed on a link or an image. Cancelling all touches will
    // intentionally suppress system context menu UI. See crbug.com/1250352.
    [self cancelAllTouches];
  } else {
    [self.screenshotView removeFromSuperview];
  }

  return configuration;
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForHighlightingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  UIPreviewParameters* previewParameters = [[UIPreviewParameters alloc] init];
  previewParameters.backgroundColor = UIColor.clearColor;

  // If the preview view is not attached to the view hierarchy, fallback to nil
  // to prevent app crashing. See crbug.com/1351669.
  UITargetedPreview* targetPreview =
      self.screenshotView.window
          ? [[UITargetedPreview alloc] initWithView:self.screenshotView
                                         parameters:previewParameters]
          : nil;
  return targetPreview;
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForDismissingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  UIPreviewParameters* previewParameters = [[UIPreviewParameters alloc] init];
  previewParameters.backgroundColor = UIColor.clearColor;

  // If the dismiss view is not attached to the view hierarchy, fallback to nil
  // to prevent app crashing. See crbug.com/1231888.
  UITargetedPreview* targetPreview =
      self.screenshotView.window
          ? [[UITargetedPreview alloc] initWithView:self.screenshotView
                                         parameters:previewParameters]
          : nil;
  self.screenshotView = nil;
  return targetPreview;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
    willPerformPreviewActionForMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration
                                            animator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  if (self.webState && self.webState->GetDelegate()) {
    self.webState->GetDelegate()->ContextMenuWillCommitWithAnimator(
        self.webState, animator);
  }
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  __weak UIView* weakScreenshotView = self.screenshotView;
  [animator addCompletion:^{
    // Check if `self.screenshotView` has already been replaced and removed.
    if (self.screenshotView && self.screenshotView == weakScreenshotView) {
      [self.screenshotView removeFromSuperview];
    }
  }];
}

#pragma mark - Private

// Prevents the web view gesture recognizer to get the touch events.
- (void)cancelAllTouches {
  // All user gestures are handled by a subview of web view scroll view
  // (WKContentView).
  for (UIView* subview in self.webView.scrollView.subviews) {
    for (UIGestureRecognizer* recognizer in subview.gestureRecognizers) {
      if (recognizer.enabled) {
        recognizer.enabled = NO;
        recognizer.enabled = YES;
      }
    }
  }
}

// Fetches the context menu params for the element at `locationInWebView`. The
// returned params can be empty.
- (std::optional<web::ContextMenuParams>)fetchContextMenuParamsAtLocation:
    (CGPoint)locationInWebView {
  // While traditionally using dispatch_async would be used here, we have to
  // instead use CFRunLoop because dispatch_async blocks the thread. As this
  // function is called by iOS when it detects the user's force touch, it is on
  // the main thread and we cannot block that. CFRunLoop instead just loops on
  // the main thread until the completion block is fired.
  __block BOOL isRunLoopNested = NO;
  __block BOOL javascriptEvaluationComplete = NO;
  __block BOOL isRunLoopComplete = NO;

  __block std::optional<web::ContextMenuParams> resultParams;

  __weak __typeof(self) weakSelf = self;
  [self.elementFetcher
      fetchDOMElementAtPoint:locationInWebView
           completionHandler:^(const web::ContextMenuParams& params) {
             javascriptEvaluationComplete = YES;
             resultParams = params;
             if (isRunLoopNested) {
               CFRunLoopStop(CFRunLoopGetCurrent());
             }
           }];

  // Make sure to timeout in case the JavaScript doesn't return in a timely
  // manner. While this is executing, the scrolling on the page is frozen.
  // Interacting with the page will force this method to return even before any
  // of this code is called.
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               (int64_t)(kJavaScriptTimeout * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   if (!isRunLoopComplete) {
                     // JavaScript didn't complete. Cancel the JavaScript and
                     // return.
                     CFRunLoopStop(CFRunLoopGetCurrent());
                     __typeof(self) strongSelf = weakSelf;
                     [strongSelf.elementFetcher cancelFetches];
                   }
                 });

  // CFRunLoopRun isn't necessary if javascript evaluation is completed by the
  // time we reach this line.
  if (!javascriptEvaluationComplete) {
    isRunLoopNested = YES;
    ContextMenuNestedCFRunLoop();
    isRunLoopNested = NO;
  }

  isRunLoopComplete = YES;

  return resultParams;
}

@end

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_controller.h"

#import "base/values.h"
#import "ios/web/js_features/context_menu/context_menu_params_utils.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/web_state/ui/crw_context_menu_element_fetcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Views used to do the highlight/dismiss animation. Those view are empty and
// are used to override the default animation which is to focus the whole
// WebView (as the interaction is used on the whole WebView).
@property(nonatomic, strong, readonly) UIView* highlightView;
@property(nonatomic, strong, readonly) UIView* dismissView;

@property(nonatomic, strong) WKWebView* webView;

@property(nonatomic, assign) web::WebState* webState;

@property(nonatomic, strong) CRWContextMenuElementFetcher* elementFetcher;

@end

@implementation CRWContextMenuController

@synthesize highlightView = _highlightView;
@synthesize dismissView = _dismissView;

- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _contextMenu = [[UIContextMenuInteraction alloc] initWithDelegate:self];

    _webView = webView;
    [webView addInteraction:_contextMenu];

    _webState = webState;

    _elementFetcher =
        [[CRWContextMenuElementFetcher alloc] initWithWebView:webView
                                                     webState:webState];
  }
  return self;
}

#pragma mark - Property

- (UIView*)highlightView {
  if (!_highlightView) {
    // If the views have a CGRectZero size, it is not taken into account.
    CGRect rectSizedOne = CGRectMake(0, 0, 1, 1);
    _highlightView = [[UIView alloc] initWithFrame:rectSizedOne];
    _highlightView.backgroundColor = UIColor.clearColor;
  }
  return _highlightView;
}

- (UIView*)dismissView {
  if (!_dismissView) {
    // If the views have a CGRectZero size, it is not taken into account.
    CGRect rectSizedOne = CGRectMake(0, 0, 1, 1);
    _dismissView = [[UIView alloc] initWithFrame:rectSizedOne];
    _dismissView.backgroundColor = UIColor.clearColor;
  }
  return _dismissView;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  CGPoint locationInWebView =
      [self.webView.scrollView convertPoint:location fromView:interaction.view];

  absl::optional<web::ContextMenuParams> params =
      [self fetchContextMenuParamsAtLocation:locationInWebView];

  if (!params.has_value() ||
      !web::CanShowContextMenuForParams(params.value())) {
    return nil;
  }

  // User long pressed on a link or an image. Cancelling all touches will
  // intentionally suppress system context menu UI. See crbug.com/1250352.
  [self cancelAllTouches];

  // Adding the highlight/dismiss view here so they can be used in the
  // delegate's methods.
  [interaction.view addSubview:self.highlightView];
  [interaction.view addSubview:self.dismissView];
  self.highlightView.center = location;
  self.dismissView.center = location;

  params.value().location = [self.webView convertPoint:location
                                              fromView:interaction.view];

  __block UIContextMenuConfiguration* configuration;
  self.webState->GetDelegate()->ContextMenuConfiguration(
      self.webState, params.value(), ^(UIContextMenuConfiguration* conf) {
        configuration = conf;
      });

  return configuration;
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForHighlightingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  return [[UITargetedPreview alloc] initWithView:self.highlightView];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForDismissingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  // If the dismiss view is not attached to the view hierarchy, fallback to nil
  // to prevent app crashing. See crbug.com/1231888.
  return self.dismissView.window
             ? [[UITargetedPreview alloc] initWithView:self.dismissView]
             : nil;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
    willPerformPreviewActionForMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration
                                            animator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  self.webState->GetDelegate()->ContextMenuWillCommitWithAnimator(self.webState,
                                                                  animator);
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

// Fetches the context menu params for the element at |locationInWebView|. The
// returned params can be empty.
- (absl::optional<web::ContextMenuParams>)fetchContextMenuParamsAtLocation:
    (CGPoint)locationInWebView {
  // While traditionally using dispatch_async would be used here, we have to
  // instead use CFRunLoop because dispatch_async blocks the thread. As this
  // function is called by iOS when it detects the user's force touch, it is on
  // the main thread and we cannot block that. CFRunLoop instead just loops on
  // the main thread until the completion block is fired.
  __block BOOL isRunLoopNested = NO;
  __block BOOL javascriptEvaluationComplete = NO;
  __block BOOL isRunLoopComplete = NO;

  __block absl::optional<web::ContextMenuParams> resultParams;

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

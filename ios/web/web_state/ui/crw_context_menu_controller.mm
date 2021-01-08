// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_controller.h"

#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/web_state/ui/crw_context_menu_element_fetcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kJavaScriptTimeout = 1;
}  // namespace

@interface CRWContextMenuController () <UIContextMenuInteractionDelegate>

@property(nonatomic, assign) web::ContextMenuParams params;

// The context menu responsible for the interaction.
@property(nonatomic, strong) UIContextMenuInteraction* contextMenu;

@property(nonatomic, strong) WKWebView* webView;

@property(nonatomic, assign) web::WebState* webState;

@property(nonatomic, strong) CRWContextMenuElementFetcher* elementFetcher;

@end

@implementation CRWContextMenuController

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

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  CGPoint locationInWebView =
      [self.webView.scrollView convertPoint:location fromView:interaction.view];

  // While traditionally using dispatch_async would be used here, we have to
  // instead use CFRunLoop because dispatch_async blocks the thread. As this
  // function is called by iOS when it detects the user's force touch, it is on
  // the main thread and we cannot block that. CFRunLoop instead just loops on
  // the main thread until the completion block is fired.
  __block BOOL isRunLoopNested = NO;
  __block BOOL javascriptEvaluationComplete = NO;
  __block BOOL isRunLoopComplete = NO;

  __weak __typeof(self) weakSelf = self;
  [self.elementFetcher
      fetchDOMElementAtPoint:locationInWebView
           completionHandler:^(const web::ContextMenuParams& params) {
             __typeof(self) strongSelf = weakSelf;
             javascriptEvaluationComplete = YES;
             strongSelf.params = params;
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
    CFRunLoopRun();
    isRunLoopNested = NO;
  }

  isRunLoopComplete = YES;

  self.params.location = [self.webView convertPoint:location
                                           fromView:interaction.view];

  // TODO(crbug.com/1140387): Present the context menu with the params.

  return nil;
}

@end

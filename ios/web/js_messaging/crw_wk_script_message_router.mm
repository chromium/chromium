// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_wk_script_message_router.h"

#include "base/logging.h"
#include "ios/web/navigation/wk_navigation_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWKScriptMessageRouter () <WKScriptMessageHandler>

// Removes a specific message handler. Does nothing if handler does not exist.
- (void)tryRemoveScriptMessageHandlerForName:(NSString*)messageName
                                     webView:(WKWebView*)webView;

@end

@implementation CRWWKScriptMessageRouter {
  // Two level map of registed message handlers:
  //   {MessageName => {WKWebView => MessageCallbacks}}.
  NSMutableDictionary<NSString*,
                      NSMapTable<WKWebView*, void (^)(WKScriptMessage*)>*>*
      _handlers;
  // Wrapped WKUserContentController.
  WKUserContentController* _userContentController;
}

#pragma mark -
#pragma mark Interface

- (WKUserContentController*)userContentController {
  return _userContentController;
}

- (instancetype)initWithUserContentController:
    (WKUserContentController*)userContentController {
  DCHECK(userContentController);
  if ((self = [super init])) {
    _handlers = [[NSMutableDictionary alloc] init];
    _userContentController = userContentController;
  }
  return self;
}

- (void)setScriptMessageHandler:(void (^)(WKScriptMessage*))handler
                           name:(NSString*)messageName
                        webView:(WKWebView*)webView {
  DCHECK(handler);
  DCHECK(messageName);
  DCHECK(webView);

  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:messageName];
  if (!webViewToHandlerMap) {
    webViewToHandlerMap =
        [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory
                              valueOptions:NSPointerFunctionsCopyIn];
    [_handlers setObject:webViewToHandlerMap forKey:messageName];
    [_userContentController addScriptMessageHandler:self name:messageName];
  }
  DCHECK(![webViewToHandlerMap objectForKey:webView]);
  [webViewToHandlerMap setObject:handler forKey:webView];
}

- (void)removeScriptMessageHandlerForName:(NSString*)messageName
                                  webView:(WKWebView*)webView {
  DCHECK(messageName);
  DCHECK(webView);
  DCHECK([[_handlers objectForKey:messageName] objectForKey:webView]);
  [self tryRemoveScriptMessageHandlerForName:messageName webView:webView];
}

- (void)removeAllScriptMessageHandlersForWebView:(WKWebView*)webView {
  DCHECK(webView);
  for (NSString* messageName in [_handlers allKeys]) {
    [self tryRemoveScriptMessageHandlerForName:messageName webView:webView];
  }
}

#pragma mark -
#pragma mark WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  // Ignore frame registration messages from internal placeholder pages.
  GURL url = net::GURLWithNSURL(message.frameInfo.request.URL);
  if (web::wk_navigation_util::IsPlaceholderUrl(url)) {
    return;
  }

  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:message.name];
  DCHECK(webViewToHandlerMap);
  id handler = [webViewToHandlerMap objectForKey:message.webView];
  if (handler) {
    // Web process can send messages even if web view was reset and
    // script message handler has been removed from the router.
    ((void (^)(WKScriptMessage*))handler)(message);
  }
}

#pragma mark -
#pragma mark Implementation

- (void)tryRemoveScriptMessageHandlerForName:(NSString*)messageName
                                     webView:(WKWebView*)webView {
  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:messageName];
  NS_VALID_UNTIL_END_OF_SCOPE id handler =
      [webViewToHandlerMap objectForKey:webView];
  if (!handler)
    return;
  if (webViewToHandlerMap.count == 1) {
    [_handlers removeObjectForKey:messageName];
    [_userContentController removeScriptMessageHandlerForName:messageName];
  } else {
    [webViewToHandlerMap removeObjectForKey:webView];
  }
}

@end

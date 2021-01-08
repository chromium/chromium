// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_element_fetcher.h"

#include "base/strings/sys_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "ios/web/web_state/context_menu_params_utils.h"
#import "ios/web/web_state/ui/crw_html_element_fetch_request.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Javascript function name to obtain element details at a point.
const char kFindElementAtPointFunctionName[] = "findElementAtPoint";

// JavaScript message handler name installed in WKWebView for found element
// response.
NSString* const kFindElementResultHandlerName = @"FindElementResultHandler";
}  // namespace

@interface CRWContextMenuElementFetcher () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

@property(nonatomic, readonly, weak) WKWebView* webView;

@property(nonatomic, assign) web::WebState* webState;

// Details for currently in progress element fetches. The objects are
// instances of CRWHTMLElementFetchRequest and are keyed by a unique requestId
// string.
@property(nonatomic, strong) NSMutableDictionary* pendingElementFetchRequests;

@end

@implementation CRWContextMenuElementFetcher

- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _pendingElementFetchRequests = [[NSMutableDictionary alloc] init];

    _webView = webView;

    _webState = webState;
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    webState->AddObserver(_observer.get());

    // Listen for fetched element response.
    web::WKWebViewConfigurationProvider& configurationProvider =
        web::WKWebViewConfigurationProvider::FromBrowserState(
            webState->GetBrowserState());
    CRWWKScriptMessageRouter* messageRouter =
        configurationProvider.GetScriptMessageRouter();
    __weak __typeof(self) weakSelf = self;
    [messageRouter
        setScriptMessageHandler:^(WKScriptMessage* message) {
          [weakSelf didReceiveScriptMessage:message];
        }
                           name:kFindElementResultHandlerName
                        webView:webView];
  }
  return self;
}

- (void)dealloc {
  if (self.webState)
    self.webState->RemoveObserver(_observer.get());
}

- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:
                 (void (^)(const web::ContextMenuParams&))handler {
  if (!self.webState) {
    return;
  }
  web::WebFrame* frame = GetMainFrame(self.webState);
  if (!frame) {
    // A WebFrame may not exist for certain types of content, like PDFs.
    return;
  }
  DCHECK(handler);

  std::string requestID = base::UnguessableToken::Create().ToString();
  CRWHTMLElementFetchRequest* fetchRequest =
      [[CRWHTMLElementFetchRequest alloc] initWithFoundElementHandler:handler];
  _pendingElementFetchRequests[base::SysUTF8ToNSString(requestID)] =
      fetchRequest;

  CGSize webViewContentSize = self.webView.scrollView.contentSize;

  std::vector<base::Value> args;
  args.push_back(base::Value(requestID));
  args.push_back(base::Value(point.x));
  args.push_back(base::Value(point.y));
  args.push_back(base::Value(webViewContentSize.width));
  args.push_back(base::Value(webViewContentSize.height));
  frame->CallJavaScriptFunction(std::string(kFindElementAtPointFunctionName),
                                args);
}

- (void)cancelFetches {
  for (CRWHTMLElementFetchRequest* fetchRequest in _pendingElementFetchRequests
           .allValues) {
    [fetchRequest invalidate];
  }
}

#pragma mark - Private

// Called when web controller receives a new message from the web page.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  NSMutableDictionary* response =
      [[NSMutableDictionary alloc] initWithDictionary:message.body];

  NSString* requestID = response[web::kContextMenuElementRequestId];
  CRWHTMLElementFetchRequest* fetchRequest =
      _pendingElementFetchRequests[requestID];
  if (!fetchRequest) {
    // Do not process the message if a fetch request with a matching |requestID|
    // was not found. This ensures that the response matches a request made by
    // this instance.
    return;
  }

  web::ContextMenuParams params =
      web::ContextMenuParamsFromElementDictionary(response);
  params.is_main_frame = message.frameInfo.mainFrame;
  params.view = self.webView;

  [_pendingElementFetchRequests removeObjectForKey:requestID];
  [fetchRequest runHandlerWithResponse:params];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  if (self.webState)
    self.webState->RemoveObserver(_observer.get());
  self.webState = nullptr;
}

@end

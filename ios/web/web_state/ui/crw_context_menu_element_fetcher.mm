// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_element_fetcher.h"

#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/web_state/ui/crw_html_element_fetch_request.h"

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

  web::ContextMenuJavaScriptFeature* context_menu_feature =
      web::ContextMenuJavaScriptFeature::FromBrowserState(
          self.webState->GetBrowserState());
  if (!context_menu_feature->GetWebFramesManager(self.webState)
           ->GetMainWebFrame()) {
    // A WebFrame may not exist for certain types of content, like PDFs.
    return;
  }
  DCHECK(handler);

  std::string requestID = base::UnguessableToken::Create().ToString();
  CRWHTMLElementFetchRequest* fetchRequest =
      [[CRWHTMLElementFetchRequest alloc] initWithFoundElementHandler:handler];
  _pendingElementFetchRequests[base::SysUTF8ToNSString(requestID)] =
      fetchRequest;

  __weak __typeof(self) weakSelf = self;
  context_menu_feature->GetElementAtPoint(
      self.webState, requestID, point,
      base::BindOnce(^(const std::string& innerRequestID,
                       const web::ContextMenuParams& params) {
        web::ContextMenuParams context_menu_params(params);
        [weakSelf
            elementDetailsReceived:context_menu_params
                      forRequestID:base::SysUTF8ToNSString(innerRequestID)];
      }));
}

- (void)cancelFetches {
  for (CRWHTMLElementFetchRequest* fetchRequest in _pendingElementFetchRequests
           .allValues) {
    [fetchRequest invalidate];
  }
}

#pragma mark - Private

- (void)elementDetailsReceived:(web::ContextMenuParams&)params
                  forRequestID:(NSString*)requestID {
  CRWHTMLElementFetchRequest* fetchRequest =
      _pendingElementFetchRequests[requestID];
  if (!fetchRequest) {
    // Do not process the message if a fetch request with a matching `requestID`
    // was not found. This ensures that the response matches a request made by
    // this instance.
    return;
  }

  [_pendingElementFetchRequests removeObjectForKey:requestID];

  params.view = self.webView;
  [fetchRequest runHandlerWithResponse:params];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  if (self.webState)
    self.webState->RemoveObserver(_observer.get());
  self.webState = nullptr;
}

@end

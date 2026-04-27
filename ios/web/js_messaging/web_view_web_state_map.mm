// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_web_state_map.h"

#import <objc/runtime.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/web_state.h"

namespace {
const char kWebViewWebStateMapKeyName[] = "web_view_web_state_map";
}  // namespace

// Holds a pointer to a WebState.
@interface WebStateHandleForWebView : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, readonly) web::WebState* webState;

@end

@implementation WebStateHandleForWebView {
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  CHECK(webState);
  if ((self = [super init])) {
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (web::WebState*)webState {
  return _webState.get();
}

@end

namespace web {

void SetAssociatedWebViewForWebState(WKWebView* web_view, WebState* web_state) {
  CHECK(web_view);
  CHECK(web_state);

  objc_setAssociatedObject(
      web_view, kWebViewWebStateMapKeyName,
      [[WebStateHandleForWebView alloc] initWithWebState:web_state],
      OBJC_ASSOCIATION_RETAIN);
}

void ClearAssociatedWebViewForWebState(WKWebView* web_view,
                                       WebState* web_state) {
  CHECK(web_view);
  CHECK(web_state);
  CHECK_EQ(GetWebStateForWebView(web_view), web_state);

  objc_setAssociatedObject(web_view, kWebViewWebStateMapKeyName, nil,
                           OBJC_ASSOCIATION_RETAIN);
}

WebState* GetWebStateForWebView(WKWebView* web_view) {
  if (!web_view) {
    return nil;
  }

  if (WebStateHandleForWebView* handle =
          base::apple::ObjCCast<WebStateHandleForWebView>(
              objc_getAssociatedObject(web_view, kWebViewWebStateMapKeyName))) {
    return handle.webState;
  }

  return nil;
}

}  // namespace web

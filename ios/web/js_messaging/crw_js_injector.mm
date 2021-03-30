// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_js_injector.h"

#import <WebKit/WebKit.h>

#include "base/check.h"
#import "ios/web/js_messaging/crw_js_window_id_manager.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWJSInjector () {
  __weak id<CRWJSInjectorDelegate> _delegate;

  // Script manager for setting the windowID.
  CRWJSWindowIDManager* _windowIDJSManager;
}

@end

@implementation CRWJSInjector

- (instancetype)initWithDelegate:(id<CRWJSInjectorDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;

    _JSInjectionReceiver =
        [[CRWJSInjectionReceiver alloc] initWithEvaluator:self];
  }
  return self;
}

- (void)setWebView:(WKWebView*)webView {
  _webView = webView;
  if (webView) {
    _windowIDJSManager = [[CRWJSWindowIDManager alloc] initWithWebView:webView];
  } else {
    _windowIDJSManager = nil;
  }
}

- (void)injectWindowID {
  [_windowIDJSManager inject];
}

#pragma mark - CRWJSInjectionEvaluator

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  NSString* safeScript = [self scriptByAddingWindowIDCheckForScript:script];
  web::ExecuteJavaScript(self.webView, safeScript, completionHandler);
}

- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(void (^)(id, NSError*))completionHandler {
  // For security reasons, executing JavaScript on pages with app-specific URLs
  // is not allowed, because those pages may have elevated privileges.
  GURL lastCommittedURL = [_delegate lastCommittedURLForJSInjector:self];
  if (web::GetWebClient()->IsAppSpecificURL(lastCommittedURL)) {
    if (completionHandler) {
      dispatch_async(dispatch_get_main_queue(), ^{
        NSError* error = [[NSError alloc]
            initWithDomain:web::kJSEvaluationErrorDomain
                      code:web::JS_EVALUATION_ERROR_CODE_REJECTED
                  userInfo:nil];
        completionHandler(nil, error);
      });
    }
    return;
  }

  [_delegate willExecuteUserScriptForJSInjector:self];
  [self executeJavaScript:script completionHandler:completionHandler];
}

#pragma mark - JavaScript Helpers (Private)

// Returns a new script which wraps |script| with windowID check so |script| is
// not evaluated on windowID mismatch.
- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script {
  NSString* kTemplate = @"if (__gCrWeb['windowId'] === '%@') { %@; }";
  return [NSString
      stringWithFormat:kTemplate, [_windowIDJSManager windowID], script];
}

@end

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_js_injector.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#import "ios/web/js_messaging/crw_js_window_id_manager.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/deprecated/crw_js_injection_manager.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWJSInjector () {
  __weak id<CRWJSInjectorDelegate> _delegate;

  // Script manager for setting the windowID.
  CRWJSWindowIDManager* _windowIDJSManager;

  // A set of script managers whose scripts have been injected into the current
  // page.
  NSMutableSet* _injectedScriptManagers;
}

@end

@implementation CRWJSInjector

- (instancetype)initWithDelegate:(id<CRWJSInjectorDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;

    _JSInjectionReceiver =
        [[CRWJSInjectionReceiver alloc] initWithEvaluator:self];

    _injectedScriptManagers = [[NSMutableSet alloc] init];
  }
  return self;
}

- (void)resetInjectedScriptSet {
  [_injectedScriptManagers removeAllObjects];
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

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_injectedScriptManagers containsObject:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  DCHECK(script.length);
  // Script execution is an asynchronous operation which may pass sensitive
  // data to the page. executeJavaScript:completionHandler makes sure that
  // receiver page did not change by checking its window id.
  // |[self.webView executeJavaScript:completionHandler:]| is not used here
  // because it does not check that page is the same.
  [self executeJavaScript:script completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
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

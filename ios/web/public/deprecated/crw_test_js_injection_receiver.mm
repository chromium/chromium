// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_test_js_injection_receiver.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWTestWKWebViewEvaluator : NSObject <CRWJSInjectionEvaluator> {
  // Web view for JavaScript evaluation.
  WKWebView* _webView;
  // Set to track injected script managers.
  NSMutableSet* _injectedScriptManagers;
}
@end

@implementation CRWTestWKWebViewEvaluator

- (instancetype)init {
  if (self = [super init]) {
    _webView = [[WKWebView alloc] init];
    _injectedScriptManagers = [[NSMutableSet alloc] init];
  }
  return self;
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  web::ExecuteJavaScript(_webView, script, completionHandler);
}

- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(void (^)(id, NSError*))completionHandler {
  web::ExecuteJavaScript(_webView, script, completionHandler);
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_injectedScriptManagers containsObject:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  // Web layer guarantees that __gCrWeb object is always injected first.
  NSString* supplementedScript =
      [@"window.__gCrWeb = {};" stringByAppendingString:script];
  [_webView evaluateJavaScript:supplementedScript completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

@end

@interface CRWTestJSInjectionReceiver () {
  CRWTestWKWebViewEvaluator* evaluator_;
}
@end

@implementation CRWTestJSInjectionReceiver

- (id)init {
  CRWTestWKWebViewEvaluator* evaluator =
      [[CRWTestWKWebViewEvaluator alloc] init];
  if (self = [super initWithEvaluator:evaluator])
    evaluator_ = evaluator;
  return self;
}

@end

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
}
@end

@implementation CRWTestWKWebViewEvaluator

- (instancetype)init {
  if (self = [super init]) {
    _webView = [[WKWebView alloc] init];
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

@end

@interface CRWTestJSInjectionReceiver () {
  CRWTestWKWebViewEvaluator* _evaluator;
}
@end

@implementation CRWTestJSInjectionReceiver

- (id)init {
  CRWTestWKWebViewEvaluator* evaluator =
      [[CRWTestWKWebViewEvaluator alloc] init];
  if (self = [super initWithEvaluator:evaluator])
    _evaluator = evaluator;
  return self;
}

@end

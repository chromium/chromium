// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

#import "base/check.h"
#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWJSInjectionReceiver {
  // Used to evaluate JavaScript.
  __weak id<CRWJSInjectionEvaluator> _evaluator;
}

- (id)initWithEvaluator:(id<CRWJSInjectionEvaluator>)evaluator {
  DCHECK(evaluator);
  self = [super init];
  if (self) {
    _evaluator = evaluator;
  }
  return self;
}

#pragma mark -
#pragma mark CRWJSInjectionEvaluatorMethods

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  [_evaluator executeJavaScript:script completionHandler:completionHandler];
}

- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(void (^)(id, NSError*))completionHandler {
  [_evaluator executeUserJavaScript:script completionHandler:completionHandler];
}

@end

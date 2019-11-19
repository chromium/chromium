// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

#include "base/logging.h"
#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"
#import "ios/web/public/deprecated/crw_js_injection_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWJSInjectionReceiver {
  // Used to evaluate JavaScript.
  __weak id<CRWJSInjectionEvaluator> _evaluator;

  // Map from a CRWJSInjectionManager class to its instance created for this
  // receiver.
  NSMutableDictionary* _managers;
}

- (id)initWithEvaluator:(id<CRWJSInjectionEvaluator>)evaluator {
  DCHECK(evaluator);
  self = [super init];
  if (self) {
    _evaluator = evaluator;
    _managers = [[NSMutableDictionary alloc] init];
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

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_evaluator scriptHasBeenInjectedForClass:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)jsInjectionManagerClass {
  [_evaluator injectScript:script forClass:jsInjectionManagerClass];
}

- (CRWJSInjectionManager*)instanceOfClass:(Class)jsInjectionManagerClass {
  DCHECK(_managers);
  CRWJSInjectionManager* manager =
      [_managers objectForKey:jsInjectionManagerClass];
  if (!manager) {
    CRWJSInjectionManager* newManager =
        [[jsInjectionManagerClass alloc] initWithReceiver:self];
    [_managers setObject:newManager forKey:jsInjectionManagerClass];
    manager = newManager;
  }
  DCHECK(manager);
  return manager;
}

@end

@implementation CRWJSInjectionReceiver (Testing)
- (NSDictionary*)managers {
  return _managers;
}
@end

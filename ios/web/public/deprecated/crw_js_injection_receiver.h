// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"

@class CRWJSInjectionManager;

// CRWJSInjectionReceiver injects JavaScript into a web view.
@interface CRWJSInjectionReceiver : NSObject <CRWJSInjectionEvaluator>

// Init with JavaScript evaluator.
- (id)initWithEvaluator:(id<CRWJSInjectionEvaluator>)evaluator;

// Returns an instance of |jsInjectionManagerClass|. Instances of the classes
// it depends on are created if needed.
- (CRWJSInjectionManager*)instanceOfClass:(Class)jsInjectionManagerClass;

@end

@interface CRWJSInjectionReceiver (Testing)
// Returns a dictionary of instantiated managers keyed by class.
- (NSDictionary*)managers;
@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_

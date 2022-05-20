// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"

// CRWJSInjectionReceiver injects JavaScript into a web view.
@interface CRWJSInjectionReceiver : NSObject <CRWJSInjectionEvaluator>

// Init with JavaScript evaluator.
- (id)initWithEvaluator:(id<CRWJSInjectionEvaluator>)evaluator;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_RECEIVER_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_EVALUATOR_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_EVALUATOR_H_

#import <Foundation/Foundation.h>

@protocol CRWJSInjectionEvaluator

// Executes the supplied JavaScript in the WebView. Calls |completionHandler|
// with results of the execution (which may be nil if the implementing object
// has no way to run the execution or the execution returns a nil value)
// or an NSError if there is an error. The |completionHandler| can be nil.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler;

// Asynchronously executes |javaScript| in the main frame's context,
// registering user interaction. For security reasons, some implementations may
// reject the request if the page has some elevated privileges.
- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(void (^)(id, NSError*))completionHandler;

// Checks to see if the script for a class has been injected into the
// current page already.
- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass;

// Injects the given script into the current page on behalf of
// |injectionManagerClass|. This should only be used for injecting
// the manager's script, and not for evaluating arbitrary JavaScript.
- (void)injectScript:(NSString*)script forClass:(Class)injectionManagerClass;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_JS_INJECTION_EVALUATOR_H_

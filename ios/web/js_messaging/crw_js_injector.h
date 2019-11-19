// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_CRW_JS_INJECTOR_H_
#define IOS_WEB_JS_MESSAGING_CRW_JS_INJECTOR_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/deprecated/crw_js_injection_evaluator.h"
#include "url/gurl.h"

@class CRWJSInjectionReceiver;
@class WKWebView;
@class CRWJSInjector;

@protocol CRWJSInjectorDelegate <NSObject>

// Tells delegate that user script is about to be executed.
- (void)willExecuteUserScriptForJSInjector:(CRWJSInjector*)injector;

// Returns the last committed URL by the delegate.
- (GURL)lastCommittedURLForJSInjector:(CRWJSInjector*)injector;

@end

// TODO(crbug.com/954137): This class is responsible for both "injection" and
// "execution" and probably should be split into separate classes (f.e.
// CRWJSExecutor and CRWLegacyInjector) when we get to the next phase of
// refactoring. CRWLegacyInjector would be removed at some point, while
// CRWJSExecutor would always stay around to support omnibox.
@interface CRWJSInjector : NSObject <CRWJSInjectionEvaluator>

@property(strong, nonatomic, readonly)
    CRWJSInjectionReceiver* JSInjectionReceiver;

// Contains a web view, if one is associated.
@property(weak, nonatomic) WKWebView* webView;

// Designated initializer. Initializes with |delegate|.
- (instancetype)initWithDelegate:(id<CRWJSInjectorDelegate>)delegate;

// Resets list of all scripts injected with |injectScript|. Affects only results
// returned by |scriptHasBeenInjectedForClass|.
- (void)resetInjectedScriptSet;

// Injects windowId in the web page.
- (void)injectWindowID;

@end

#endif  // IOS_WEB_JS_MESSAGING_CRW_JS_INJECTOR_H_

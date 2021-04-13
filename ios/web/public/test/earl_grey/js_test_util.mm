// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/js_test_util.h"

#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#include "base/timer/elapsed_timer.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/web_state.h"
#import "ios/web/security/web_interstitial_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Executes |javascript| on the given |web_state|, and waits until execution is
// completed. If |out_error| is not nil, it is set to the error resulting from
// the execution, if one occurs. The return value is the result of the
// JavaScript execution, or nil if script execution timed out.
id ExecuteJavaScript(WebState* web_state,
                     NSString* javascript,
                     NSError* __autoreleasing* out_error) {
  __block bool did_complete = false;
  __block id result = nil;
  CRWJSInjectionReceiver* receiver = web_state->GetJSInjectionReceiver();
  [receiver executeJavaScript:javascript
            completionHandler:^(id value, NSError* error) {
              did_complete = true;
              result = [value copy];
              if (out_error)
                *out_error = [error copy];
            }];

  // Wait for completion.
  BOOL succeeded = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  return succeeded ? result : nil;
}

// Evaluates the given |script| on |interstitial|.
void ExecuteScriptForTesting(web::WebInterstitialImpl* interstitial,
                             NSString* script,
                             void (^handler)(id result, NSError*)) {
  DCHECK(interstitial);
  interstitial->ExecuteJavaScript(script, handler);
}

bool WaitUntilWindowIdInjected(WebState* web_state) {
  bool is_window_id_injected = false;
  bool is_timeout = false;
  bool is_unrecoverable_error = false;

  base::ElapsedTimer timer;
  base::TimeDelta timeout =
      base::TimeDelta::FromSeconds(kWaitForJSCompletionTimeout);

  // Keep polling until either the JavaScript execution returns with expected
  // value (indicating that Window ID is set), the timeout occurs, or an
  // unrecoverable error occurs.
  while (!is_window_id_injected && !is_timeout && !is_unrecoverable_error) {
    NSError* error = nil;
    id result = ExecuteJavaScript(web_state, @"0", &error);
    if (error) {
      is_unrecoverable_error = ![error.domain isEqual:WKErrorDomain] ||
                               error.code != WKErrorJavaScriptExceptionOccurred;
    } else {
      is_window_id_injected = [result isEqual:@0];
    }
    is_timeout = timeout < timer.Elapsed();
  }
  return !is_timeout && !is_unrecoverable_error;
}

id ExecuteScriptOnInterstitial(WebState* web_state, NSString* script) {
  web::WebInterstitialImpl* interstitial =
      static_cast<web::WebInterstitialImpl*>(web_state->GetWebInterstitial());

  __block id script_result = nil;
  __block bool did_finish = false;
  web::ExecuteScriptForTesting(interstitial, script, ^(id result, NSError*) {
    script_result = [result copy];
    did_finish = true;
  });
  BOOL succeeded = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });

  return succeeded ? script_result : nil;
}

}  // namespace web

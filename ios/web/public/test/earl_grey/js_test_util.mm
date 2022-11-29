// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/js_test_util.h"

#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

bool WaitUntilWindowIdInjected(WebState* web_state) {
  // Keep polling until either the JavaScript execution returns with expected
  // value (indicating that Window ID is set), the timeout occurs, or an
  // unrecoverable error occurs.
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    std::unique_ptr<base::Value> result =
        test::ExecuteJavaScript(web_state, "0");
    return result && result->is_double() && result->GetDouble() == 0.0;
  });
}

}  // namespace web

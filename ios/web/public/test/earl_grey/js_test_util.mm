// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/js_test_util.h"

#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Executes `javascript` on the given `web_state`, and waits until execution is
// completed. If `out_error` is not nil, it is set to the error resulting from
// the execution, if one occurs. The return value is the result of the
// JavaScript execution, or nil if script execution timed out.
absl::optional<base::Value> ExecuteJavaScript(
    WebState* web_state,
    const std::u16string& javascript) {
  __block bool did_complete = false;
  __block absl::optional<base::Value> result;

  web_state->ExecuteJavaScript(
      javascript, base::BindOnce(^(const base::Value* completion_result) {
        result = completion_result->Clone();
        did_complete = true;
      }));

  // Wait for completion.
  BOOL succeeded = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  return succeeded ? std::move(result) : absl::nullopt;
}

bool WaitUntilWindowIdInjected(WebState* web_state) {
  // Keep polling until either the JavaScript execution returns with expected
  // value (indicating that Window ID is set), the timeout occurs, or an
  // unrecoverable error occurs.
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    absl::optional<base::Value> result = ExecuteJavaScript(web_state, u"0");
    return result && result->is_double() && result->GetDouble() == 0.0;
  });
}

}  // namespace web

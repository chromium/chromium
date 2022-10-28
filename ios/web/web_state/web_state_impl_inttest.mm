// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#import "base/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

typedef WebTestWithWebState WebStateImplTest;

// Tests script execution with and without callback.
TEST_F(WebStateImplTest, ScriptExecution) {
  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state());

  ASSERT_TRUE(LoadHtml("<html></html>"));

  // Execute script without callback.
  web_state_impl->ExecuteJavaScript(u"window.foo = 'bar'");

  // Execute script with callback.
  __block std::unique_ptr<base::Value> execution_result;
  __block bool execution_complete = false;
  web_state_impl->ExecuteJavaScript(
      u"window.foo", base::BindOnce(^(const base::Value* value) {
        execution_result = std::make_unique<base::Value>(value->Clone());
        execution_complete = true;
      }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return execution_complete;
  }));

  ASSERT_TRUE(execution_result);
  ASSERT_TRUE(execution_result->is_string());
  EXPECT_EQ("bar", execution_result->GetString());
}

}  // namespace web

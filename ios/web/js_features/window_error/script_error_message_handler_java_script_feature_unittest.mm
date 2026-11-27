// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"

#import <limits>
#import <map>
#import <memory>
#import <optional>
#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/window_error/error_event_listener_java_script_feature.h"
#import "ios/web/js_features/window_error/script_error_details.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

class ScriptErrorMessageHandlerJavaScriptFeatureTest
    : public WebTestWithWebState {
 protected:
  ScriptErrorMessageHandlerJavaScriptFeatureTest()
      : WebTestWithWebState(),
        feature_(base::BindRepeating(^(ScriptErrorDetails error_details) {
          error_details_ = std::move(error_details);
          ++error_count_;
        })) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    OverrideJavaScriptFeatures(
        {ErrorEventListenerJavaScriptFeature::GetInstance(),
         web::java_script_features::GetBaseJavaScriptFeature(), &feature_});
  }

 protected:
  ScriptErrorMessageHandlerJavaScriptFeature feature_;
  std::optional<ScriptErrorDetails> error_details_;
  int error_count_ = 0;
};

// Tests that error details are received for a script error occurring in the
// head of the main frame.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromMainFramePageHead) {
  ASSERT_FALSE(error_details_);

  NSString* html = @"<html><head>"
                    "<script>nonexistentFunction();</script>"
                    "</head><body></body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details_;
  }));

  EXPECT_EQ(1, error_details_->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              base::SysUTF8ToNSString(error_details_->message));
  EXPECT_EQ("https://chromium.test/", error_details_->url.spec());
  EXPECT_TRUE(error_details_->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// body of the main frame.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromMainFramePageBody) {
  ASSERT_FALSE(error_details_);

  NSString* html = @"<html><body>"
                    "<script>nonexistentFunction();</script>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details_;
  }));

  EXPECT_EQ(1, error_details_->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              base::SysUTF8ToNSString(error_details_->message));
  EXPECT_EQ("https://chromium.test/", error_details_->url.spec());
  EXPECT_TRUE(error_details_->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// head of an iframe.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromIframePageHead) {
  ASSERT_FALSE(error_details_);

  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><head><script>nonexistentFunction();</"
                    "script></head></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details_;
  }));

  EXPECT_EQ(0, error_details_->line_number);
  EXPECT_NSEQ(@"Script error.",
              base::SysUTF8ToNSString(error_details_->message));
  EXPECT_EQ("about:srcdoc", error_details_->url.spec());
  EXPECT_FALSE(error_details_->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// body of an iframe.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromIframePageBody) {
  ASSERT_FALSE(error_details_);
  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><body><script>nonexistentFunction();</"
                    "script></body></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details_;
  }));

  EXPECT_EQ(0, error_details_->line_number);
  EXPECT_NSEQ(@"Script error.",
              base::SysUTF8ToNSString(error_details_->message));
  EXPECT_EQ("about:srcdoc", error_details_->url.spec());
  EXPECT_FALSE(error_details_->is_main_frame);
}

// Ensures that error details are still retreived after a document is recreated.
// (Since event listeners are removed and need to be reinjected after a set of
// calls to document.open/write/close.)
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorAfterDocumentRecreated) {
  ASSERT_FALSE(error_details_);
  LoadHtml(@"<html></html>");

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"nonexistentFunction();");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details_;
  }));

  EXPECT_EQ(1, error_details_->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              base::SysUTF8ToNSString(error_details_->message));
  EXPECT_EQ("https://chromium.test/", error_details_->url.spec());
  EXPECT_TRUE(error_details_->is_main_frame);
}

// Tests that non-dict message bodies posted to WindowErrorResultHandler are
// ignored without crashing.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       IgnoreNonDictMessageBody) {
  LoadHtml(@"<html></html>");

  ExecuteJavaScript(
      @"const h = window.webkit.messageHandlers.WindowErrorResultHandler;"
       "for (const payload of ['s', 123, 3.14, true, [1, 2]]) {"
       "  h.postMessage(payload);"
       "}"
       "h.postMessage({message: 'sentinel'});"
       "true;");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_.has_value();
  }));
  EXPECT_EQ(1, error_count_);
  EXPECT_EQ("sentinel", error_details_->message);
}

// Tests that WindowErrorResultHandler safely parses valid and malformed/bounded
// dictionary payloads across crashKeys and line_number edge cases.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ParseAndSanitizeDictMessages) {
  // Prevent the error reporter from attempting an upload under field trial
  // testing configs when a case provides a non-empty `api` field.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kLogCrWebJavaScriptErrors);

  LoadHtml(@"<html></html>");

  using CrashKeyMap = std::map<std::string, std::string>;
  struct TestCase {
    const char* label;
    NSString* js_payload;
    std::string expected_api;
    std::string expected_stack;
    int expected_line_number;
    CrashKeyMap expected_crash_keys;
  };
  const TestCase kCases[] = {
      {"valid payload",
       @"{api: 'testApi', line_number: 42, message: 'ok', stack: 'trace', "
        "crashKeys: {key1: 'val1', key2: 'val2'}}",
       "testApi",
       "trace",
       42,
       {{"key1", "val1"}, {"key2", "val2"}}},
      {"non-dict crashKeys",
       @"{message: 'ok', crashKeys: 'not-a-dict'}",
       "",
       "",
       0,
       {}},
      {"mixed valid/invalid crashKeys",
       @"{message: 'ok', crashKeys: {num: 1, boolKey: true, nullKey: null, "
        "nested: {a: 'b'}, arr: ['x'], valid: 'ok'}}",
       "",
       "",
       0,
       {{"valid", "ok"}}},
      {"crashKey length bounds",
       @"(() => { const k = {};"
        "k['k'.repeat(256)] = 'v'.repeat(1024);"
        "k['k'.repeat(257)] = 'too_long_key';"
        "k['too_long_val'] = 'v'.repeat(1025);"
        "return {message: 'ok', crashKeys: k}; })()",
       "",
       "",
       0,
       {{std::string(256, 'k'), std::string(1024, 'v')}}},
      {"line_number overflow max",
       @"{message: 'ok', line_number: 1e30}",
       "",
       "",
       std::numeric_limits<int>::max(),
       {}},
      {"line_number overflow min",
       @"{message: 'ok', line_number: -1e30}",
       "",
       "",
       std::numeric_limits<int>::min(),
       {}},
      {"non-numeric line_number",
       @"{message: 'ok', line_number: 'not-a-number'}",
       "",
       "",
       0,
       {}},
  };

  int expected_count = 0;
  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.label);
    ++expected_count;
    ExecuteJavaScript([NSString
        stringWithFormat:@"window.webkit.messageHandlers."
                         @"WindowErrorResultHandler.postMessage(%@); true;",
                         tc.js_payload]);
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return error_count_ == expected_count;
    }));
    EXPECT_EQ("ok", error_details_->message);
    EXPECT_EQ(tc.expected_api, error_details_->api);
    EXPECT_EQ(tc.expected_stack, error_details_->stack);
    EXPECT_EQ(tc.expected_line_number, error_details_->line_number);
    EXPECT_EQ(tc.expected_crash_keys, error_details_->crash_keys);
  }

  // Verify the crash key count cap (64) without letting skipped non-string
  // entries consume quota.
  ExecuteJavaScript(
      @"const many = {};"
       "for (let i = 0; i < 10; ++i) { many['bad' + i] = i; }"
       "for (let i = 0; i < 100; ++i) { many['k' + i] = 'v' + i; }"
       "window.webkit.messageHandlers.WindowErrorResultHandler.postMessage("
       "{message: 'count', crashKeys: many}); true;");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_count_ == expected_count + 1;
  }));
  EXPECT_EQ("count", error_details_->message);
  EXPECT_EQ(64u, error_details_->crash_keys.size());
}

}  // namespace web

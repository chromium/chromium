// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/js_features/clipboard/clipboard_constants.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/test/fakes/crw_fake_script_message_handler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {
// Url used for tests. Must be https as the Clipboard api is not available in
// insecure contexts.
NSString* const kHttpsTestUrl = @"https://chromium.test/";
}  // namespace

// Test fixture for clipboard.ts tests.
class ClipboardJsTest : public web::JavascriptTest {
 public:
  ClipboardJsTest()
      : clipboard_handler_([[CRWFakeScriptMessageHandler alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandler:clipboard_handler_
                           name:base::SysUTF8ToNSString(
                                    kScriptMessageHandlerName)];
  }

  void SetUp() override {
    web::JavascriptTest::SetUp();
    AddGCrWebScript();
    AddCommonScript();
    AddUserScript(@"clipboard");
    AddUserScript(@"paste_handler");

    ASSERT_TRUE(web::test::LoadHtml(web_view(), @"<html></html>",
                                    [NSURL URLWithString:kHttpsTestUrl]));
    web::test::ExecuteJavaScriptInWebView(web_view(),
                                          @"window.testState = {};");
    ASSERT_TRUE(web::test::WaitForInjectedScripts(web_view()));
  }

 protected:
  // Simulates a call to navigator.clipboard.writeText from the webpage.
  // The result of the promise is stored in `window.testState[result_key]`.
  void WriteText(const std::string& text,
                 const std::string& result_key = "lastResult") {
    NSString* script = [NSString
        stringWithFormat:@"(() => {"
                          "navigator.clipboard.writeText('%s')"
                          ".then(() => {"
                          "  window.testState.%s = 'resolved';"
                          "}).catch((err) => {"
                          "  window.testState.%s = err.message;"
                          "});"
                          "})();",
                         text.c_str(), result_key.c_str(), result_key.c_str()];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Simulates a call to navigator.clipboard.write from the webpage.
  // The result of the promise is stored in `window.testState[result_key]`.
  void Write(const std::string& result_key = "lastResult") {
    NSString* script =
        [NSString stringWithFormat:@"(() => {"
                                   @"const item = new ClipboardItem({"
                                   @"  'text/plain': Promise.resolve('test')"
                                   @"});"
                                    "navigator.clipboard.write([item])"
                                    ".then(() => {"
                                    "  window.testState.%s = 'resolved';"
                                    "}).catch((err) => {"
                                    "  window.testState.%s = err.message;"
                                    "});"
                                    "})();",
                                   result_key.c_str(), result_key.c_str()];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Simulates a call to navigator.clipboard.readText from the webpage.
  // The result of the promise is stored in `window.testState[result_key]`.
  void ReadText(const std::string& result_key = "lastResult") {
    NSString* script =
        [NSString stringWithFormat:@"(() => {"
                                    "navigator.clipboard.readText()"
                                    ".then((text) => {"
                                    "  window.testState.%s = text;"
                                    "}).catch((err) => {"
                                    "  window.testState.%s = err.message;"
                                    "});"
                                    "})();",
                                   result_key.c_str(), result_key.c_str()];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Simulates a call to navigator.clipboard.read from the webpage.
  // The result of the promise is stored in `window.testState[result_key]`.
  void Read(const std::string& result_key = "lastResult") {
    NSString* script = [NSString
        stringWithFormat:@"(() => {"
                         @"navigator.clipboard.read()"
                         @".then(async (items) => {"
                         @"  if (items.length === 0) {"
                         @"    window.testState.%s = 'empty';"
                         @"    return;"
                         @"  }"
                         @"  const blob = await items[0].getType('text/plain');"
                         @"  const text = await blob.text();"
                         @"  window.testState.%s = text;"
                         @"}).catch((err) => {"
                         @"  window.testState.%s = err.message;"
                         @"});"
                         @"})();",
                         result_key.c_str(), result_key.c_str(),
                         result_key.c_str()];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Simulates the native code's response to a clipboard request.
  void ResolveRequest(int request_id, bool is_allowed) {
    NSString* script = [NSString
        stringWithFormat:@"__gCrWeb.getRegisteredApi('clipboard').getFunction("
                         @"'resolveRequest')(%d, %s);",
                         request_id, is_allowed ? "true" : "false"];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Retrieves a result from the global test state object.
  std::string GetTestStateResult(const std::string& key) {
    NSString* script =
        [NSString stringWithFormat:@"window.testState.%s;", key.c_str()];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    if ([result isKindOfClass:[NSString class]]) {
      return base::SysNSStringToUTF8(result);
    }
    return "";
  }

  CRWFakeScriptMessageHandler* clipboard_handler_;
};

// Tests that a single writeText request can be successfully fulfilled.
TEST_F(ClipboardJsTest, TestWriteTextAndAllow) {
  WriteText(/*text_to_write=*/"test");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));

  // Verify the message sent to the native code.
  NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
      clipboard_handler_.lastReceivedScriptMessage.body);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kWriteCommand),
              message[base::SysUTF8ToNSString(kCommandKey)]);
  EXPECT_NSEQ(@0, message[base::SysUTF8ToNSString(kRequestIdKey)]);

  // Simulate the browser allowing the request.
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/true);

  // Verify that the promise was resolved.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("lastResult").empty();
  }));
  EXPECT_EQ("resolved", GetTestStateResult("lastResult"));
}

// Tests that a single write request can be successfully fulfilled.
TEST_F(ClipboardJsTest, TestWriteAndAllow) {
  Write();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));

  // Verify the message sent to the native code.
  NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
      clipboard_handler_.lastReceivedScriptMessage.body);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kWriteCommand),
              message[base::SysUTF8ToNSString(kCommandKey)]);
  EXPECT_NSEQ(@0, message[base::SysUTF8ToNSString(kRequestIdKey)]);

  // Simulate the browser allowing the request.
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/true);

  // Verify that the promise was resolved.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("lastResult").empty();
  }));
  EXPECT_EQ("resolved", GetTestStateResult("lastResult"));
}

// Tests that a single readText request can be successfully fulfilled.
TEST_F(ClipboardJsTest, TestReadTextAndAllow) {
  // First, write text to the clipboard to ensure there's something to read.
  WriteText("test");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return GetTestStateResult("lastResult") == "resolved";
  }));

  // Now, read the text back.
  ReadText("readResult");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [message[base::SysUTF8ToNSString(kRequestIdKey)] intValue] == 1;
  }));

  // Verify the message sent to the native code.
  NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
      clipboard_handler_.lastReceivedScriptMessage.body);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kReadCommand),
              message[base::SysUTF8ToNSString(kCommandKey)]);
  EXPECT_NSEQ(@1, message[base::SysUTF8ToNSString(kRequestIdKey)]);

  // Simulate the browser allowing the request.
  ResolveRequest(/*request_id=*/1, /*is_allowed=*/true);

  // Verify that the promise was resolved with the correct text.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("readResult").empty();
  }));
  EXPECT_EQ("test", GetTestStateResult("readResult"));
}

// Tests that a single read request can be successfully fulfilled.
TEST_F(ClipboardJsTest, TestReadAndAllow) {
  // First, write text to the clipboard to ensure there's something to read.
  WriteText("test");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return GetTestStateResult("lastResult") == "resolved";
  }));

  // Now, read the text back.
  Read("readResult");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [message[base::SysUTF8ToNSString(kRequestIdKey)] intValue] == 1;
  }));

  // Verify the message sent to the native code.
  NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
      clipboard_handler_.lastReceivedScriptMessage.body);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kReadCommand),
              message[base::SysUTF8ToNSString(kCommandKey)]);
  EXPECT_NSEQ(@1, message[base::SysUTF8ToNSString(kRequestIdKey)]);

  // Simulate the browser allowing the request.
  ResolveRequest(/*request_id=*/1, /*is_allowed=*/true);

  // Verify that the promise was resolved with the correct text.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("readResult").empty();
  }));
  EXPECT_EQ("test", GetTestStateResult("readResult"));
}

// Tests that a single writeText request can be denied.
TEST_F(ClipboardJsTest, TestWriteTextAndDeny) {
  WriteText("test");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));

  // Simulate the browser denying the request.
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/false);

  // Verify that the promise was rejected with the correct error.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("lastResult").empty();
  }));
  EXPECT_EQ("Clipboard access denied.", GetTestStateResult("lastResult"));
}

// Tests that a single readText request can be denied.
TEST_F(ClipboardJsTest, TestReadTextAndDeny) {
  ReadText();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));

  // Simulate the browser denying the request.
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/false);

  // Verify that the promise was rejected with the correct error.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("lastResult").empty();
  }));
  EXPECT_EQ("Clipboard access denied.", GetTestStateResult("lastResult"));
}

// Tests that fulfilling a newer request correctly rejects an older one.
TEST_F(ClipboardJsTest, TestSupersededRequest) {
  WriteText(/*text_to_write=*/"A", /*result_key=*/"result0");
  WriteText(/*text_to_write=*/"B", /*result_key=*/"result1");

  // Wait for the second message to arrive to ensure both have been processed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [message[@"requestId"] intValue] == 1;
  }));

  // Fulfill the NEWER request (ID 1).
  ResolveRequest(/*request_id=*/1, /*is_allowed=*/true);

  // Verify that the older request (ID 0) was rejected as superseded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result0").empty();
  }));
  EXPECT_EQ("Clipboard request was superseded by a newer one.",
            GetTestStateResult("result0"));

  // Verify that the newer request (ID 1) was resolved successfully.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result1").empty();
  }));
  EXPECT_EQ("resolved", GetTestStateResult("result1"));
}

// Tests that a superseded request is still rejected even if the newer request
// is denied.
TEST_F(ClipboardJsTest, TestSupersededRequestWhenNewerIsDenied) {
  WriteText(/*text_to_write=*/"A", /*result_key=*/"result0");
  WriteText(/*text_to_write=*/"B", /*result_key=*/"result1");

  // Wait for the second message to arrive to ensure both have been processed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [message[@"requestId"] intValue] == 1;
  }));

  // Deny the NEWER request (ID 1).
  ResolveRequest(/*request_id=*/1, /*is_allowed=*/false);

  // Verify that the older request (ID 0) was still rejected as superseded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result0").empty();
  }));
  EXPECT_EQ("Clipboard request was superseded by a newer one.",
            GetTestStateResult("result0"));

  // Verify that the newer request (ID 1) was rejected with the correct error.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result1").empty();
  }));
  EXPECT_EQ("Clipboard access denied.", GetTestStateResult("result1"));
}

// Tests that the oldest request is evicted when the queue is full.
TEST_F(ClipboardJsTest, TestEvictionLogic) {
  // Create 101 pending requests.
  WriteText(/*text_to_write=*/"A", /*result_key=*/"result0");
  for (int i = 1; i < 100; ++i) {
    WriteText("B");
  }
  WriteText(/*text_to_write=*/"C", /*result_key=*/"result100");

  // Wait for the 101st message to arrive to ensure all have been processed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [message[base::SysUTF8ToNSString(kRequestIdKey)] intValue] == 100;
  }));

  // Verify that the oldest request (ID 0) was rejected with the correct error.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result0").empty();
  }));
  EXPECT_EQ("Too many pending clipboard requests.",
            GetTestStateResult("result0"));

  // Fulfill the NEWER request (ID 100).
  ResolveRequest(/*request_id=*/100, /*is_allowed=*/true);

  // Verify that the newest request (ID 100) was resolved successfully.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !GetTestStateResult("result100").empty();
  }));
  EXPECT_EQ("resolved", GetTestStateResult("result100"));
}

// Tests that a paste event sends a message to the browser.
TEST_F(ClipboardJsTest, TestPasteEvent) {
  // Simulate a paste event.
  web::test::ExecuteJavaScriptInWebView(
      web_view(), @"document.dispatchEvent(new Event('paste'));");

  // Verify that a message was sent to the native code.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!clipboard_handler_.lastReceivedScriptMessage;
  }));
  NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
      clipboard_handler_.lastReceivedScriptMessage.body);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kDidFinishClipboardReadCommand),
              message[base::SysUTF8ToNSString(kCommandKey)]);
}

// Tests that a successful readText() sends a `didFinishClipboardRead`
// notification.
TEST_F(ClipboardJsTest, TestReadFulfillmentSendsNotification) {
  // Set up the clipboard with text to read, then read it. This is necessary to
  // test that the `didFinishClipboardRead` message is sent after a successful
  // read operation.
  WriteText("test");
  ResolveRequest(/*request_id=*/0, /*is_allowed=*/true);
  ReadText("readResult");
  ResolveRequest(/*request_id=*/1, /*is_allowed=*/true);

  // Verify that the `didFinishClipboardRead` message was sent.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    if (!clipboard_handler_.lastReceivedScriptMessage) {
      return false;
    }
    NSDictionary* message = base::apple::ObjCCastStrict<NSDictionary>(
        clipboard_handler_.lastReceivedScriptMessage.body);
    return [base::SysUTF8ToNSString(kDidFinishClipboardReadCommand)
        isEqualToString:message[base::SysUTF8ToNSString(kCommandKey)]];
  }));
}

}  // namespace web

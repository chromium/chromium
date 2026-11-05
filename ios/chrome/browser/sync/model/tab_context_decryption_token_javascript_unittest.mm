// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <array>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

NSString* const kScriptName = @"tab_context_decryption_token";
NSString* const kMessageHandlerName = @"TabContextDecryptionTokenMessage";
NSString* const kTestHtml = @"<html><body></body></html>";
NSString* const kObfuscatedGaiaIdKey = @"obfuscatedGaiaId";
NSString* const kContainerIdKey = @"containerId";
NSString* const kTestGaiaId = @"gaia_id_123";
NSString* const kValidContainerUuid = @"12345678-1234-1234-1234-123456789abc";

}  // namespace

// A fake `WKScriptMessageHandlerWithReply` that records the body of the last
// received `WKScriptMessage` and replies with a configurable value or error.
@interface TabContextFakeScriptMessageHandler
    : NSObject <WKScriptMessageHandlerWithReply>

@property(nonatomic, copy) id lastReceivedMessageBody;
@property(nonatomic, strong) id replyToNextCall;
@property(nonatomic, copy) NSString* errorToNextCall;

@end

@implementation TabContextFakeScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message
                 replyHandler:(void (^)(id, NSString*))replyHandler {
  self.lastReceivedMessageBody = message.body;
  replyHandler(self.replyToNextCall, self.errorToNextCall);
}

@end

// Test fixture for `tab_context_decryption_token.ts`.
class TabContextDecryptionTokenJavaScriptTest : public web::JavascriptTest {
 protected:
  TabContextDecryptionTokenJavaScriptTest()
      : handler_([[TabContextFakeScriptMessageHandler alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandlerWithReply:handler_
                            contentWorld:[WKContentWorld pageWorld]
                                    name:kMessageHandlerName];
  }

  ~TabContextDecryptionTokenJavaScriptTest() override = default;

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddUserScript(kScriptName);
    ASSERT_TRUE(LoadHtml(kTestHtml));
  }

  void TearDown() override {
    [web_view().configuration.userContentController
            removeAllScriptMessageHandlers];
    web::JavascriptTest::TearDown();
  }

  // Invokes `chrome.getContainerDecryptionToken` and waits for its callback to
  // complete. Returns the decoded UTF-8 token string, or `nil` if the callback
  // received `null`.
  NSString* CallGetContainerDecryptionToken(NSString* gaia_id,
                                            NSString* container_id) {
    NSString* script =
        [NSString stringWithFormat:
                      @"window.tokenDone = false;"
                       "window.tokenResult = null;"
                       "chrome.getContainerDecryptionToken(function(buffer) {"
                       "  if (buffer instanceof ArrayBuffer) {"
                       "    window.tokenResult = new TextDecoder().decode("
                       "        new Uint8Array(buffer));"
                       "  }"
                       "  window.tokenDone = true;"
                       "}, '%@', '%@');",
                      gaia_id, container_id];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          id done =
              web::test::ExecuteJavaScript(web_view(), @"window.tokenDone;");
          return [done isEqual:@YES];
        }));

    id result =
        web::test::ExecuteJavaScript(web_view(), @"window.tokenResult;");
    if (!result || [result isKindOfClass:[NSNull class]]) {
      return nil;
    }
    return base::apple::ObjCCastStrict<NSString>(result);
  }

  TabContextFakeScriptMessageHandler* handler_;
};

// Tests that `chrome.getContainerDecryptionToken` is exposed on `window`.
TEST_F(TabContextDecryptionTokenJavaScriptTest,
       GetContainerDecryptionTokenExposedOnWindow) {
  id typeof_chrome =
      web::test::ExecuteJavaScript(web_view(), @"typeof window.chrome");
  EXPECT_NSEQ(@"object", typeof_chrome);

  id typeof_api = web::test::ExecuteJavaScript(
      web_view(), @"typeof window.chrome.getContainerDecryptionToken");
  EXPECT_NSEQ(@"function", typeof_api);
}

// Tests that `chrome.getContainerDecryptionToken` throws a `TypeError`
// synchronously and does not post a WebKit message when given invalid
// arguments.
TEST_F(TabContextDecryptionTokenJavaScriptTest,
       ThrowsTypeErrorOnInvalidArguments) {
  const auto kInvalidCalls = std::to_array<NSString*>({
      @"chrome.getContainerDecryptionToken('not_a_fn', 'gaia', "
      @"'12345678-1234-1234-1234-123456789abc')",
      @"chrome.getContainerDecryptionToken(function() {}, 123, "
      @"'12345678-1234-1234-1234-123456789abc')",
      @"chrome.getContainerDecryptionToken(function() {}, 'gaia', 123)",
  });

  for (NSString* invalid_call : kInvalidCalls) {
    NSString* script = [NSString
        stringWithFormat:@"(function() {"
                          "  try {"
                          "    %@;"
                          "    return 'no_error';"
                          "  } catch (e) {"
                          "    return e instanceof TypeError ? 'TypeError' : "
                          "e.name;"
                          "  }"
                          "})();",
                         invalid_call];
    id result = web::test::ExecuteJavaScript(web_view(), script);
    EXPECT_NSEQ(@"TypeError", result)
        << " for call: " << base::SysNSStringToUTF8(invalid_call);
    EXPECT_FALSE(handler_.lastReceivedMessageBody);
  }
}

// Tests that `chrome.getContainerDecryptionToken` sends the expected parameters
// to the WebKit message handler and converts the byte array reply into an
// `ArrayBuffer`.
TEST_F(TabContextDecryptionTokenJavaScriptTest,
       ForwardsMessageAndResolvesTokenArrayBuffer) {
  // ASCII bytes for "secret".
  handler_.replyToNextCall = @[ @115, @101, @99, @114, @101, @116 ];

  NSString* token =
      CallGetContainerDecryptionToken(kTestGaiaId, kValidContainerUuid);

  ASSERT_TRUE(handler_.lastReceivedMessageBody);
  NSDictionary* expected_body = @{
    kObfuscatedGaiaIdKey : kTestGaiaId,
    kContainerIdKey : kValidContainerUuid,
  };
  EXPECT_NSEQ(expected_body, handler_.lastReceivedMessageBody);
  EXPECT_NSEQ(@"secret", token);
}

// Tests that `chrome.getContainerDecryptionToken` invokes the callback with
// `null` when the native handler replies with `null`.
TEST_F(TabContextDecryptionTokenJavaScriptTest, ResolvesNullWhenReplyIsNull) {
  handler_.replyToNextCall = [NSNull null];

  NSString* token =
      CallGetContainerDecryptionToken(kTestGaiaId, kValidContainerUuid);

  ASSERT_TRUE(handler_.lastReceivedMessageBody);
  EXPECT_FALSE(token);
}

// Tests that `chrome.getContainerDecryptionToken` invokes the callback with
// `null` when the native handler rejects the promise with an error.
TEST_F(TabContextDecryptionTokenJavaScriptTest,
       ResolvesNullWhenReplyFailsWithError) {
  handler_.errorToNextCall = @"Rejected by native handler";

  NSString* token =
      CallGetContainerDecryptionToken(kTestGaiaId, kValidContainerUuid);

  ASSERT_TRUE(handler_.lastReceivedMessageBody);
  EXPECT_FALSE(token);
}

// Tests that `chrome.getContainerDecryptionToken` does not invoke `callback` a
// second time with `null` if `callback` itself throws an exception.
TEST_F(TabContextDecryptionTokenJavaScriptTest,
       DoesNotInvokeCallbackTwiceWhenCallbackThrows) {
  handler_.replyToNextCall = @[ @115, @101, @99, @114, @101, @116 ];

  NSString* script = [NSString
      stringWithFormat:@"window.callbackCount = 0;"
                        "chrome.getContainerDecryptionToken(function(buffer) {"
                        "  window.callbackCount++;"
                        "  throw new Error('Callback error');"
                        "}, '%@', '%@');",
                       kTestGaiaId, kValidContainerUuid];
  web::test::ExecuteJavaScriptInWebView(web_view(), script);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        id count =
            web::test::ExecuteJavaScript(web_view(), @"window.callbackCount;");
        return [count intValue] >= 1;
      }));

  id final_count =
      web::test::ExecuteJavaScript(web_view(), @"window.callbackCount;");
  EXPECT_NSEQ(@1, final_count);
}

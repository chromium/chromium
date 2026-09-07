// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;

NSString* const kMessageHandlerName = @"DeviceTrustMessageHandler";

@interface DeviceTrustReplyMessageHandler
    : NSObject <WKScriptMessageHandlerWithReply>

@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@property(nonatomic, strong) NSMutableArray* pendingReplyHandlers;
@property(nonatomic, assign) NSUInteger receivedMessageCount;

- (void)replyToNextMessage:(id)reply;
- (void)rejectNextMessage:(NSString*)error;

@end

@implementation DeviceTrustReplyMessageHandler

- (instancetype)init {
  self = [super init];
  if (self) {
    _pendingReplyHandlers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message
                 replyHandler:(void (^)(id, NSString*))replyHandler {
  self.lastReceivedMessage = message;
  self.receivedMessageCount++;
  [self.pendingReplyHandlers addObject:[replyHandler copy]];
}

- (void)replyToNextMessage:(id)reply {
  ASSERT_GT(self.pendingReplyHandlers.count, 0u);
  void (^replyHandler)(id, NSString*) = self.pendingReplyHandlers.firstObject;
  [self.pendingReplyHandlers removeObjectAtIndex:0];
  replyHandler(reply, nil);
}

- (void)rejectNextMessage:(NSString*)error {
  ASSERT_GT(self.pendingReplyHandlers.count, 0u);
  void (^replyHandler)(id, NSString*) = self.pendingReplyHandlers.firstObject;
  [self.pendingReplyHandlers removeObjectAtIndex:0];
  replyHandler(nil, error);
}

@end

// Base fixture that loads the Device Trust script but does not install the
// public API onto `window`.
class DeviceTrustJsTest : public web::JavascriptTest {
 public:
  DeviceTrustJsTest()
      : handler_([[DeviceTrustReplyMessageHandler alloc] init]) {
    [web_view().configuration.userContentController
        addScriptMessageHandlerWithReply:handler_
                            contentWorld:[WKContentWorld pageWorld]
                                    name:kMessageHandlerName];
  }

 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddUserScript(@"device_trust");
    ASSERT_TRUE(LoadHtml(@"<html><body><script>"
                         @"window.testResult = null;"
                         @"window.testError = null;"
                         @"window.testErrorMessage = null;"
                         @"</script></body></html>"));
  }

  // Invokes the gCrWeb internal setup method to install the public API.
  void SetupDeviceTrustAPI() {
    ASSERT_TRUE([web::test::ExecuteJavaScript(
        web_view(), @"__gCrWeb.callFunctionInGcrWeb('deviceTrust', "
                    @"'setupDeviceTrustAPI', []); true;") boolValue]);
  }

  // Waits for the JS-to-Native bridge to receive a message.
  [[nodiscard]] bool WaitForMessageCount(NSUInteger count) {
    return base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^{
          return handler_.receivedMessageCount >= count;
        });
  }

  // Extracts the last received message as an NSDictionary.
  NSDictionary* GetLastMessage() {
    return base::apple::ObjCCastStrict<NSDictionary>(
        handler_.lastReceivedMessage.body);
  }

  // Evaluates a JS variable and returns the result.
  id GetJsVar(NSString* var_name) {
    return web::test::ExecuteJavaScript(web_view(), var_name);
  }

  // Waits for a JS variable (like a promise result) to populate as a string.
  [[nodiscard]] bool WaitForJsString(NSString* var_name) {
    return base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, ^{
          return [GetJsVar(var_name) isKindOfClass:[NSString class]];
        });
  }

  DeviceTrustReplyMessageHandler* handler_;
};

// Verifies that window.chrome.enterprise.deviceTrust is not exposed before
// SetupDeviceTrustAPI() is called.
TEST_F(DeviceTrustJsTest, APINotInstalledBeforeSetup) {
  EXPECT_NSEQ(@NO,
              GetJsVar(@"Boolean(window.chrome?.enterprise?.deviceTrust)"));
}

// Test fixture where the Device Trust API is explicitly installed.
class DeviceTrustJsAPIEnabledTest : public DeviceTrustJsTest {
 protected:
  void SetUp() override {
    DeviceTrustJsTest::SetUp();
    SetupDeviceTrustAPI();
  }
};

// Verifies that the API object is frozen and cannot be mutated.
TEST_F(DeviceTrustJsAPIEnabledTest, APIPresenceAndImmutability) {
  EXPECT_NSEQ(
      @YES, GetJsVar(@"Object.isFrozen(window.chrome.enterprise.deviceTrust)"));
}

// Verifies that non-string challenge arguments reject with
// INVALID_CHALLENGE_REQUEST.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsNonStringChallenge) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"window.chrome.enterprise.deviceTrust.getAttestation(123)."
                  @"catch(e => window.testError = e.code);"
                  @"true;") boolValue]);

  EXPECT_NSEQ(@"INVALID_CHALLENGE_REQUEST", GetJsVar(@"window.testError"));
  EXPECT_EQ(handler_.receivedMessageCount, 0u);
}

// Verifies that whitespace-only challenge strings reject with
// INVALID_CHALLENGE_REQUEST.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsEmptyChallenge) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"window.chrome.enterprise.deviceTrust.getAttestation('   ')."
                  @"catch(e => window.testError = e.code);"
                  @"true;") boolValue]);

  EXPECT_NSEQ(@"INVALID_CHALLENGE_REQUEST", GetJsVar(@"window.testError"));
  EXPECT_EQ(handler_.receivedMessageCount, 0u);
}

// Verifies that empty challenge strings reject with a descriptive message.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsEmptyStringChallenge) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"window.chrome.enterprise.deviceTrust.getAttestation('')"
                  @".catch(e => {"
                  @"  window.testError = e.code;"
                  @"  window.testErrorMessage = e.message;"
                  @"});"
                  @"true;") boolValue]);

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"INVALID_CHALLENGE_REQUEST", GetJsVar(@"window.testError"));
  EXPECT_NSEQ(@"challengeRequest must be non-empty.",
              GetJsVar(@"window.testErrorMessage"));
}

// Verifies that challenges exceeding the maximum allowed length (1024 chars)
// reject immediately.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsTooLargeChallenge) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"const hugeChallenge = 'a'.repeat(1025);"
      @"window.chrome.enterprise.deviceTrust.getAttestation(hugeChallenge)."
      @"catch(e => window.testError = e.code);"
      @"true;") boolValue]);

  EXPECT_NSEQ(@"INVALID_CHALLENGE_REQUEST", GetJsVar(@"window.testError"));
  EXPECT_EQ(handler_.receivedMessageCount, 0u);
}

// Verifies that concurrency limits (max 3 concurrent requests) are strictly
// enforced.
TEST_F(DeviceTrustJsAPIEnabledTest, EnforcesConcurrencyLimits) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_1');"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_2');"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_3');"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_4')."
      @"catch(e => "
      @"window.testError = e.code);"
      @"true;") boolValue]);

  EXPECT_TRUE(WaitForMessageCount(3));
  EXPECT_NSEQ(@"TOO_MANY_REQUESTS", GetJsVar(@"window.testError"));
  EXPECT_EQ(handler_.receivedMessageCount, 3u);
}

// Verifies that a valid challenge resolves with the signed payload returned
// from native code.
TEST_F(DeviceTrustJsAPIEnabledTest, ResolvesSuccessfully) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"then(res => window.testResult = res);"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  [handler_ replyToNextMessage:@{@"signedPayload" : @"signed_payload_123"}];

  EXPECT_TRUE(WaitForJsString(@"window.testResult"));
  EXPECT_NSEQ(@"signed_payload_123", GetJsVar(@"window.testResult"));
}

// Verifies that native error replies reject the promise with structured error
// code and message.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsFromNative) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.testErrorName = null;"
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"catch(e => { "
      @"  window.testError = e.code; "
      @"  window.testErrorMessage = e.message; "
      @"  window.testErrorName = e.name; "
      @"});"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  [handler_ replyToNextMessage:@{
    @"errorCode" : @"CUSTOM_ERR",
    @"errorMessage" : @"Native failed",
  }];

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"CUSTOM_ERR", GetJsVar(@"window.testError"));
  EXPECT_NSEQ(@"Native failed", GetJsVar(@"window.testErrorMessage"));
  EXPECT_NSEQ(@"DeviceTrustError", GetJsVar(@"window.testErrorName"));
}

// Verifies that default error codes and messages are assigned if native reply
// is empty.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsFromNativeWithDefaultErrors) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"catch(e => { window.testError = e.code; "
      @"window.testErrorMessage = e.message; });"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  [handler_ replyToNextMessage:@{}];

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"INTERNAL_ERROR", GetJsVar(@"window.testError"));
  EXPECT_NSEQ(@"Unknown internal error occurred.",
              GetJsVar(@"window.testErrorMessage"));
}

// Verifies that invalid non-dictionary native responses reject with
// INTERNAL_ERROR.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsInvalidNativeResponse) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"catch(e => window.testError = e.code);"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  [handler_ replyToNextMessage:@"invalid"];

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"INTERNAL_ERROR", GetJsVar(@"window.testError"));
}

// Verifies that WebKit message handler rejections reject with INTERNAL_ERROR.
TEST_F(DeviceTrustJsAPIEnabledTest, RejectsNativeBridgeError) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"catch(e => { "
      @"  window.testError = e.code; "
      @"});"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  [handler_ rejectNextMessage:@"Native bridge failed."];

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"INTERNAL_ERROR", GetJsVar(@"window.testError"));
}

// Verifies that requests time out after the configured timeout period.
TEST_F(DeviceTrustJsAPIEnabledTest, TimeoutBehavior) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"const originalSetTimeout = window.setTimeout;"
      @"window.setTimeout = (fn, delay) => originalSetTimeout(fn, 0);"
      @"window.chrome.enterprise.deviceTrust.getAttestation('valid_challenge')."
      @"catch(e => window.testError = e.code);"
      @"window.setTimeout = originalSetTimeout;"
      @"true;") boolValue]);

  EXPECT_TRUE(WaitForJsString(@"window.testError"));
  EXPECT_NSEQ(@"ATTESTATION_TIMEOUT", GetJsVar(@"window.testError"));
}

// Verifies that valid requests package and send the challenge dictionary to
// native code.
TEST_F(DeviceTrustJsAPIEnabledTest, SendWellFormattedMessage) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"window.chrome.enterprise.deviceTrust.getAttestation('valid_"
                  @"challenge_1');"
                  @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  NSDictionary* message = GetLastMessage();

  EXPECT_NSEQ(@"valid_challenge_1", message[@"challengeRequest"]);
  EXPECT_EQ(message.count, 1u);
}

// Verifies that a challenge of the maximum supported size (1024 chars) is
// accepted.
TEST_F(DeviceTrustJsAPIEnabledTest, AcceptMaxLengthChallenge) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"const maxChallenge = 'a'.repeat(1024);"
      @"window.chrome.enterprise.deviceTrust.getAttestation(maxChallenge)"
      @".catch(e => window.testError = e.code);"
      @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  NSString* challenge = base::apple::ObjCCastStrict<NSString>(
      GetLastMessage()[@"challengeRequest"]);
  EXPECT_EQ(1024u, challenge.length);
  EXPECT_NSEQ([NSNull null], GetJsVar(@"window.testError"));
}

// Verifies that completing a pending request frees up a concurrency slot for
// subsequent calls.
TEST_F(DeviceTrustJsAPIEnabledTest, ReleaseConcurrencySlotOnReply) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.firstResolved = false;"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_1')"
      @".then(() => { window.firstResolved = true; });"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_2')"
      @".catch(() => {});"
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_3')"
      @".catch(() => {});"
      @"true;") boolValue]);

  EXPECT_TRUE(WaitForMessageCount(3));
  [handler_ replyToNextMessage:@{@"signedPayload" : @"signed_Payload"}];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForJSCompletionTimeout, ^{
        return [GetJsVar(@"window.firstResolved") boolValue];
      }));

  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.chrome.enterprise.deviceTrust.getAttestation('challenge_4')"
      @".catch(e => window.testError = e.code);"
      @"true;") boolValue]);

  EXPECT_TRUE(WaitForMessageCount(4));
  EXPECT_NSEQ(@"challenge_4", GetLastMessage()[@"challengeRequest"]);
  EXPECT_NSEQ([NSNull null], GetJsVar(@"window.testError"));
}

// Verifies that redefining or overriding the API properties on window is
// blocked.
TEST_F(DeviceTrustJsAPIEnabledTest, PreventsTamperingAndOverriding) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"try { window.chrome.enterprise.deviceTrust = "
                  @"'test_object'; } catch(e) {}"
                  @"try { window.chrome.enterprise.deviceTrust.getAttestation "
                  @"= function() { return "
                  @"'test_func'; }; } catch(e) {}"
                  @"true;") boolValue]);

  EXPECT_NSEQ(@"object",
              GetJsVar(@"typeof window.chrome.enterprise.deviceTrust"));

  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(), @"window.chrome.enterprise.deviceTrust.getAttestation('"
                  @"secure_challenge');"
                  @"true;") boolValue]);

  ASSERT_TRUE(WaitForMessageCount(1));
  EXPECT_NSEQ(@"secure_challenge", GetLastMessage()[@"challengeRequest"]);
}

// Verifies that calling SetupDeviceTrustAPI multiple times is idempotent
// and preserves the exact same API object reference.
TEST_F(DeviceTrustJsAPIEnabledTest, SetupIsIdempotent) {
  ASSERT_TRUE([web::test::ExecuteJavaScript(
      web_view(),
      @"window.originalDeviceTrustAPI = window.chrome.enterprise.deviceTrust;"
      @"true;") boolValue]);

  SetupDeviceTrustAPI();

  EXPECT_NSEQ(@YES, GetJsVar(@"window.originalDeviceTrustAPI === "
                             @"window.chrome.enterprise.deviceTrust"));
}

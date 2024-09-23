// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest_mac.h"

namespace {
const char kEmptyPageUrl[] = "/blank";
const char kGetUserMediaAudioPageUrl[] = "/getUserMediaAudio";
const char kGetUserMediaAudioVideoPageUrl[] = "/getUserMediaAudioVideo";
const char kGetUserMediaVideoPageUrl[] = "/getUserMediaVideo";

const char kGetUserMediaAudioPageHtml[] =
    "<html><body><script>"
    "navigator.mediaDevices.getUserMedia({ audio : true });"
    "</script></body></html>";
const char kGetUserMediaAudioVideoPageHtml[] =
    "<html><body><script>"
    "navigator.mediaDevices.getUserMedia({ audio : true, video : true });"
    "</script></body></html>";
const char kGetUserMediaVideoPageHtml[] =
    "<html><body><script>"
    "navigator.mediaDevices.getUserMedia({ video : true });"
    "</script></body></html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kEmptyPageUrl) {
    http_response->set_content("<html><body></body></html>");
  } else if (request.relative_url == kGetUserMediaAudioPageUrl) {
    http_response->set_content(kGetUserMediaAudioPageHtml);
  } else if (request.relative_url == kGetUserMediaAudioVideoPageUrl) {
    http_response->set_content(kGetUserMediaAudioVideoPageHtml);
  } else if (request.relative_url == kGetUserMediaVideoPageUrl) {
    http_response->set_content(kGetUserMediaVideoPageHtml);
  } else {
    return nullptr;
  }
  return std::move(http_response);
}
}  // namespace

static NSString* kMessageHandlerResponseAudioKey = @"audio";
static NSString* kMessageHandlerResponseVideoKey = @"video";

@interface MediaScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@end

@implementation MediaScriptMessageHandler

- (void)configureForWebView:(WKWebView*)webView {
  [webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"MediaAPIAccessedHandler"];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  self.lastReceivedMessage = message;
}

@end

// Test fixture for media_overrides.ts.
class MediaOverridesJavaScriptTest : public web::JavascriptTest {
 protected:
  MediaOverridesJavaScriptTest()
      : server_(net::EmbeddedTestServer::TYPE_HTTP),
        message_handler_([[MediaScriptMessageHandler alloc] init]) {}
  ~MediaOverridesJavaScriptTest() override {}

  void SetUp() override {
    JavascriptTest::SetUp();

    AddUserScript(@"media_overrides");

    server_.RegisterRequestHandler(base::BindRepeating(&StandardResponse));
    ASSERT_TRUE(server_.Start());

    [message_handler_ configureForWebView:web_view()];
  }

  const net::EmbeddedTestServer& server() { return server_; }

  MediaScriptMessageHandler* message_handler() { return message_handler_; }

 private:
  net::EmbeddedTestServer server_;
  MediaScriptMessageHandler* message_handler_;
};

// Tests that `navigator.mediaDevices` API is not added for insecure pages where
// the API does not already exist. (Pages loaded with LoadHtml are not
// considered secure within WebKit.)
TEST_F(MediaOverridesJavaScriptTest, MediaAPINotAddedForInsecureContexts) {
  ASSERT_TRUE(LoadHtml(@"<html><head></head><body></body></html>"));

  id result = web::test::ExecuteJavaScript(web_view(),
                                           @"typeof navigator.mediaDevices");
  EXPECT_NSEQ(@"undefined", result);
}

// Tests that the original `getMediaDevices` method is still called by checking
// that it returns a promise.
TEST_F(MediaOverridesJavaScriptTest, MediaOverrideCallsOriginal) {
  ASSERT_TRUE(LoadUrl(server().GetURL(kEmptyPageUrl)));

  id result = web::test::ExecuteJavaScript(
      web_view(), @"const promise = navigator.mediaDevices.getUserMedia({ "
                  @"audio : true }); typeof promise.then;");
  EXPECT_NSEQ(@"function", result);
}

// Tests that the correct message is received for an audio media request.
TEST_F(MediaOverridesJavaScriptTest, WebKitAudioMessageReceived) {
  GURL URL = server().GetURL(kGetUserMediaAudioPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(2ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseAudioKey]);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseVideoKey]);

  EXPECT_TRUE([body[kMessageHandlerResponseAudioKey] boolValue]);
  EXPECT_FALSE([body[kMessageHandlerResponseVideoKey] boolValue]);
}

// Tests that the correct message is received for an audio and video media
// request.
TEST_F(MediaOverridesJavaScriptTest, WebKitAudioVideoMessageReceived) {
  GURL URL = server().GetURL(kGetUserMediaAudioVideoPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(2ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseAudioKey]);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseVideoKey]);

  EXPECT_TRUE([body[kMessageHandlerResponseAudioKey] boolValue]);
  EXPECT_TRUE([body[kMessageHandlerResponseVideoKey] boolValue]);
}

// Tests that the correct message is received for a video media request.
TEST_F(MediaOverridesJavaScriptTest, WebKitVideoMessageReceived) {
  GURL URL = server().GetURL(kGetUserMediaVideoPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(2ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseAudioKey]);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseVideoKey]);

  EXPECT_FALSE([body[kMessageHandlerResponseAudioKey] boolValue]);
  EXPECT_TRUE([body[kMessageHandlerResponseVideoKey] boolValue]);
}

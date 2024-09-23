// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest_mac.h"

namespace {
const char kClearWatchPageUrl[] = "/clearWatch";
const char kGetCurrentPositionPageUrl[] = "/getCurrentPosition";
const char kWatchPositionPageUrl[] = "/watchPosition";

const char kClearWatchPageHtml[] = "<html><body><script>"
                                   "navigator.geolocation.clearWatch("
                                   ");"
                                   "</script></body></html>";
const char kGetCurrentPositionPageHtml[] =
    "<html><body><script>"
    "navigator.geolocation.getCurrentPosition((position) => {});"
    "</script></body></html>";
const char kWatchPositionPageHtml[] =
    "<html><body><script>"
    "function doNothing(arg) {}"
    "watchId = navigator.geolocation.watchPosition(doNothing, doNothing, {});"
    "</script></body></html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kClearWatchPageUrl) {
    http_response->set_content(kClearWatchPageHtml);
  } else if (request.relative_url == kGetCurrentPositionPageUrl) {
    http_response->set_content(kGetCurrentPositionPageHtml);
  } else if (request.relative_url == kWatchPositionPageUrl) {
    http_response->set_content(kWatchPositionPageHtml);
  } else {
    return nullptr;
  }
  return std::move(http_response);
}
}  // namespace

static NSString* kMessageHandlerResponseApiKey = @"api";
static NSString* kMessageHandlerResponseApiValueClearWatch = @"clearWatch";
static NSString* kMessageHandlerResponseApiValueGetPosition =
    @"getCurrentPosition";
static NSString* kMessageHandlerResponseApiValueWatch = @"watchPosition";

@interface GeolocationScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@end

@implementation GeolocationScriptMessageHandler

- (void)configureForWebView:(WKWebView*)webView {
  [webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"GeolocationAPIAccessedHandler"];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  self.lastReceivedMessage = message;
}

@end

// Test fixture for geolocation_overrides.ts.
class GeolocationOverridesJavaScriptTest : public web::JavascriptTest {
 protected:
  GeolocationOverridesJavaScriptTest()
      : server_(net::EmbeddedTestServer::TYPE_HTTP),
        message_handler_([[GeolocationScriptMessageHandler alloc] init]) {}
  ~GeolocationOverridesJavaScriptTest() override {}

  void SetUp() override {
    JavascriptTest::SetUp();

    AddUserScript(@"geolocation_overrides");

    server_.RegisterRequestHandler(base::BindRepeating(&StandardResponse));
    ASSERT_TRUE(server_.Start());

    [message_handler_ configureForWebView:web_view()];
  }

  const net::EmbeddedTestServer& server() { return server_; }

  GeolocationScriptMessageHandler* message_handler() {
    return message_handler_;
  }

 private:
  net::EmbeddedTestServer server_;
  GeolocationScriptMessageHandler* message_handler_;
};

// Tests that the correct message is received when the clearWatch API is called.
TEST_F(GeolocationOverridesJavaScriptTest,
       GeolocationClearWatchMessageReceived) {
  GURL URL = server().GetURL(kClearWatchPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(1ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseApiKey]);
  EXPECT_NSEQ(kMessageHandlerResponseApiValueClearWatch,
              body[kMessageHandlerResponseApiKey]);
}

// Tests that the correct message is received when the getCurrentPosition API is
// called.
TEST_F(GeolocationOverridesJavaScriptTest,
       GeolocationGetCurrentPositionMessageReceived) {
  GURL URL = server().GetURL(kGetCurrentPositionPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(1ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseApiKey]);
  EXPECT_NSEQ(kMessageHandlerResponseApiValueGetPosition,
              body[kMessageHandlerResponseApiKey]);
}

// Tests that the correct message is received when the watchPosition API is
// called.
TEST_F(GeolocationOverridesJavaScriptTest,
       GeolocationWatchPositionMessageReceived) {
  GURL URL = server().GetURL(kWatchPositionPageUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(1ul, allKeys.count);
  EXPECT_TRUE([allKeys containsObject:kMessageHandlerResponseApiKey]);
  EXPECT_NSEQ(kMessageHandlerResponseApiValueWatch,
              body[kMessageHandlerResponseApiKey]);
}

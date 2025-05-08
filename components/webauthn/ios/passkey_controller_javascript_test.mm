// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

namespace {

const char kNavigatorCredentialsCreateUrl[] = "/credentialsCreate";
const char kNavigatorCredentialsGetUrl[] = "/credentialsGet";

const char kNavigatorCredentialsCreatePageHtml[] =
    "<html><body><script>"
    "navigator.credentials.create({ publicKey: {} });"
    "</script></body></html>";
const char kNavigatorCredentialsGetPageHtml[] =
    "<html><body><script>"
    "navigator.credentials.get({ publicKey: {} });"
    "</script></body></html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kNavigatorCredentialsCreateUrl) {
    http_response->set_content(kNavigatorCredentialsCreatePageHtml);
  } else if (request.relative_url == kNavigatorCredentialsGetUrl) {
    http_response->set_content(kNavigatorCredentialsGetPageHtml);
  } else {
    return nullptr;
  }
  return std::move(http_response);
}

}  // namespace

@interface PasskeyScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@end

@implementation PasskeyScriptMessageHandler

- (void)configureForWebView:(WKWebView*)webView {
  [webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"PasskeyInteractionHandler"];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  self.lastReceivedMessage = message;
}

@end

// Test fixture for passkey_controller.ts.
// TODO(crbug.com/369629469): Explore adding EG tests that verify original JS
// APIs still working with shim injected. It is infeasible in JS tests since
// navigator.credentials APIs for public key credentials require some user
// interaction with system UI.
// TODO(crbug.com/396929469): Similarly to previous TODO, if feasible, add tests
// for the get events logged on resolved promises.
class PasskeyControllerJavaScriptTest : public web::JavascriptTest {
 protected:
  PasskeyControllerJavaScriptTest()
      : server_(net::EmbeddedTestServer::TYPE_HTTP),
        message_handler_([[PasskeyScriptMessageHandler alloc] init]) {}
  ~PasskeyControllerJavaScriptTest() override {}

  void SetUp() override {
    JavascriptTest::SetUp();

    AddUserScript(@"passkey_controller");

    server_.RegisterRequestHandler(base::BindRepeating(&StandardResponse));
    ASSERT_TRUE(server_.Start());

    [message_handler_ configureForWebView:web_view()];
  }

  const net::EmbeddedTestServer& server() { return server_; }
  PasskeyScriptMessageHandler* message_handler() { return message_handler_; }

 private:
  net::EmbeddedTestServer server_;
  PasskeyScriptMessageHandler* message_handler_;
};

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsCreateMessageReceived) {
  GURL URL = server().GetURL(kNavigatorCredentialsCreateUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(allKeys.count, 1ul);
  EXPECT_TRUE([allKeys containsObject:@"event"]);

  EXPECT_NSEQ(@"createRequested", body[@"event"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsGetMessageReceived) {
  GURL URL = server().GetURL(kNavigatorCredentialsGetUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(allKeys.count, 1ul);
  EXPECT_TRUE([allKeys containsObject:@"event"]);

  EXPECT_NSEQ(@"getRequested", body[@"event"]);
}

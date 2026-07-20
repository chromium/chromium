// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"

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

namespace webauthn {

namespace {

const char kNavigatorCredentialsCreateUrl[] = "/credentialsCreate";
const char kNavigatorCredentialsGetUrl[] = "/credentialsGet";
const char kNavigatorCredentialsConditionalGetUrl[] =
    "/credentialsConditionalGet";
const char kNavigatorCredentialsCreateMissingRpIdUrl[] =
    "/credentialsCreateMissingRpId";
const char kAnotherPageUrl[] = "/anotherPage";

const char kNavigatorCredentialsCreatePageHtml[] =
    "<html><body><script>"
    "navigator.credentials.create({ publicKey: { "
    "challenge: new ArrayBuffer(0), "
    "rp: { id: new ArrayBuffer(0), name: '' },"
    "user: { id: new ArrayBuffer(0), name: '', displayName: '' } } });"
    "</script></body></html>";
const char kNavigatorCredentialsGetPageHtml[] =
    "<html><body><script>"
    "navigator.credentials.get({ publicKey: { "
    "challenge: new ArrayBuffer(0) } });"
    "</script></body></html>";
const char kNavigatorCredentialsConditionalGetPageHtml[] =
    "<html><body><script>"
    "navigator.credentials.get({ "
    "mediation: 'conditional', "
    "publicKey: { challenge: new ArrayBuffer(0) } "
    "});"
    "</script></body></html>";
const char kNavigatorCredentialsCreateMissingRpIdPageHtml[] =
    "<html><body><script>"
    "navigator.credentials.create({ publicKey: { "
    "challenge: new ArrayBuffer(0), "
    "rp: { name: 'My Website' },"
    "user: { id: new ArrayBuffer(0), name: '', displayName: '' } } });"
    "</script></body></html>";
const char kAnotherPageHtml[] = "<html><body>Another Page</body></html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kNavigatorCredentialsCreateUrl) {
    http_response->set_content(kNavigatorCredentialsCreatePageHtml);
  } else if (request.relative_url ==
             kNavigatorCredentialsCreateMissingRpIdUrl) {
    http_response->set_content(kNavigatorCredentialsCreateMissingRpIdPageHtml);
  } else if (request.relative_url == kNavigatorCredentialsGetUrl) {
    http_response->set_content(kNavigatorCredentialsGetPageHtml);
  } else if (request.relative_url == kNavigatorCredentialsConditionalGetUrl) {
    http_response->set_content(kNavigatorCredentialsConditionalGetPageHtml);
  } else if (request.relative_url == kAnotherPageUrl) {
    http_response->set_content(kAnotherPageHtml);
  } else {
    return nullptr;
  }
  return std::move(http_response);
}

}  // namespace

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

    // Get current test name to distinguish which feature flags to enable.
    const std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (test_name.find("Modal") != std::string::npos) {
      enabled_features.push_back(kIOSPasskeyModalLoginWithShim);
    } else {
      disabled_features.push_back(kIOSPasskeyModalLoginWithShim);
    }

    if (test_name.find("Conditional") != std::string::npos) {
      enabled_features.push_back(kIOSPasskeyConditionalLoginWithShim);
    } else {
      disabled_features.push_back(kIOSPasskeyConditionalLoginWithShim);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    // Get the script string directly from PasskeyJavaScriptFeature so that
    // it contains the feature based placeholder replacements.
    auto scripts = PasskeyJavaScriptFeature::GetInstance()->GetScripts();
    ASSERT_TRUE(scripts.size() == 1);
    NSString* script_string = scripts[0].GetScriptString();
    WKUserScript* script = [[WKUserScript alloc]
          initWithSource:script_string
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:NO];
    [web_view().configuration.userContentController addUserScript:script];

    server_.RegisterRequestHandler(base::BindRepeating(&StandardResponse));
    ASSERT_TRUE(server_.Start());

    [message_handler_ configureForWebView:web_view()];
  }

  const net::EmbeddedTestServer& server() { return server_; }
  PasskeyScriptMessageHandler* message_handler() { return message_handler_; }

 private:
  net::EmbeddedTestServer server_;
  PasskeyScriptMessageHandler* message_handler_;
  base::test::ScopedFeatureList feature_list_;
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

  EXPECT_NSEQ(@"logCreateRequest", body[@"event"]);
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

  EXPECT_NSEQ(@"logGetRequest", body[@"event"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateMessageReceived) {
  GURL URL = server().GetURL(kNavigatorCredentialsCreateUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(allKeys.count, 8ul);
  EXPECT_TRUE([allKeys containsObject:@"event"]);
  EXPECT_TRUE([allKeys containsObject:@"frameId"]);
  EXPECT_TRUE([allKeys containsObject:@"requestId"]);
  EXPECT_TRUE([allKeys containsObject:@"request"]);
  EXPECT_TRUE([allKeys containsObject:@"rpEntity"]);
  EXPECT_TRUE([allKeys containsObject:@"userEntity"]);
  EXPECT_TRUE([allKeys containsObject:@"excludeCredentials"]);
  EXPECT_TRUE([allKeys containsObject:@"extensions"]);

  EXPECT_NSEQ(@"handleCreateRequest", body[@"event"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateMissingRpIdMessageReceived) {
  GURL URL = server().GetURL(kNavigatorCredentialsCreateMissingRpIdUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(allKeys.count, 8ul);
  EXPECT_TRUE([allKeys containsObject:@"event"]);
  EXPECT_TRUE([allKeys containsObject:@"frameId"]);
  EXPECT_TRUE([allKeys containsObject:@"requestId"]);
  EXPECT_TRUE([allKeys containsObject:@"request"]);
  EXPECT_TRUE([allKeys containsObject:@"rpEntity"]);
  EXPECT_TRUE([allKeys containsObject:@"userEntity"]);
  EXPECT_TRUE([allKeys containsObject:@"excludeCredentials"]);
  EXPECT_TRUE([allKeys containsObject:@"extensions"]);

  EXPECT_NSEQ(@"handleCreateRequest", body[@"event"]);

  NSDictionary* rpEntity = body[@"rpEntity"];
  EXPECT_TRUE(rpEntity != nil);
  EXPECT_NSEQ(@"127.0.0.1", rpEntity[@"id"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalGetMessageReceived) {
  GURL URL = server().GetURL(kNavigatorCredentialsGetUrl);
  ASSERT_TRUE(LoadUrl(URL));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  NSArray* allKeys = body.allKeys;
  EXPECT_EQ(allKeys.count, 8ul);
  EXPECT_TRUE([allKeys containsObject:@"event"]);
  EXPECT_TRUE([allKeys containsObject:@"frameId"]);
  EXPECT_TRUE([allKeys containsObject:@"remoteFrameId"]);
  EXPECT_TRUE([allKeys containsObject:@"requestId"]);
  EXPECT_TRUE([allKeys containsObject:@"request"]);
  EXPECT_TRUE([allKeys containsObject:@"rpEntity"]);
  EXPECT_TRUE([allKeys containsObject:@"allowCredentials"]);
  EXPECT_TRUE([allKeys containsObject:@"extensions"]);

  EXPECT_NSEQ(@"handleGetRequest", body[@"event"]);

  NSDictionary* rpEntity = body[@"rpEntity"];
  EXPECT_TRUE(rpEntity != nil);
  EXPECT_NSEQ(@"127.0.0.1", rpEntity[@"id"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsCreateNoReloadOnFinishedPassthrough) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"logCreateRequest", body[@"event"]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it is a passthrough request, it finishes immediately (rejects) in the
  // test environment, so it shouldn't trigger reload on back navigation.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_TRUE(message_handler().lastReceivedMessage == nil);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateReloadsOnBackNavigation) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleCreateRequest", body[@"event"]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleCreateRequest", body[@"event"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsGetNoReloadOnFinishedPassthrough) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"logGetRequest", body[@"event"]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it is a passthrough request, it finishes immediately (rejects) in the
  // test environment, so it shouldn't trigger reload on back navigation.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_TRUE(message_handler().lastReceivedMessage == nil);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalGetReloadsOnBackNavigation) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleGetRequest", body[@"event"]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleGetRequest", body[@"event"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateNoReloadOnFinished) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleCreateRequest", body[@"event"]);

  NSString* requestId = body[@"requestId"];
  ASSERT_TRUE(requestId != nil);

  // Reject the request to simulate finishing it.
  NSString* rejectJs = [NSString
      stringWithFormat:@"__gCrWeb.getRegisteredApi('passkey')."
                       @"getFunction('rejectPasskeyRequest')('%@', '%s', '%s')",
                       requestId, kNotAllowedErrorName,
                       kNotAllowedErrorMessage];
  web::test::ExecuteJavaScriptInWebView(web_view(), rejectJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it finished, it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_TRUE(message_handler().lastReceivedMessage == nil);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsConditionalGetNoReloadOnSuccess) {
  GURL get_url = server().GetURL(kNavigatorCredentialsConditionalGetUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleGetRequest", body[@"event"]);

  NSString* requestId = body[@"requestId"];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request to simulate a successful request.
  // Note: we pass "AQ" as the id64 so that it has rawId.byteLength > 0 and
  // counts as a valid credential.
  NSString* resolveJs = [NSString
      stringWithFormat:
          @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
          @"resolveAssertionRequest')('%@', 'AQ', '', '', '', '', {})",
          requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it succeeded, it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_TRUE(message_handler().lastReceivedMessage == nil);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsConditionalGetNoReloadOnFailure) {
  GURL get_url = server().GetURL(kNavigatorCredentialsConditionalGetUrl);
  GURL another_url = server().GetURL(kAnotherPageUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"handleGetRequest", body[@"event"]);

  NSString* requestId = body[@"requestId"];
  ASSERT_TRUE(requestId != nil);

  NSString* rejectJs = [NSString
      stringWithFormat:@"__gCrWeb.getRegisteredApi('passkey')."
                       @"getFunction('rejectPasskeyRequest')('%@', '%s', '%s')",
                       requestId, kNotAllowedErrorName,
                       kNotAllowedErrorMessage];
  web::test::ExecuteJavaScriptInWebView(web_view(), rejectJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it finished (rejected), it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_TRUE(message_handler().lastReceivedMessage == nil);
}

}  // namespace webauthn

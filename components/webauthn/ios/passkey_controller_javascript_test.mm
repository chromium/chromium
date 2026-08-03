// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_request_params.h"
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
const char kNavigatorCredentialsGetWithUserHandleUrl[] =
    "/credentialsGetWithUserHandle";
const char kNavigatorCredentialsCreateWithResultUrl[] =
    "/credentialsCreateWithResult";
const char kNavigatorCredentialsGetWithResultUrl[] =
    "/credentialsGetWithResult";
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
const char kNavigatorCredentialsGetWithUserHandlePageHtml[] =
    "<html><body><script>"
    "window.onload = () => {"
    "  window.getPromise = navigator.credentials.get({ "
    "    mediation: 'conditional', "
    "    publicKey: { challenge: new ArrayBuffer(0) } "
    "  }).then(cred => { "
    "    window.credentialResult = cred; "
    "  }).catch(err => { "
    "    window.credentialError = err; "
    "  });"
    "};"
    "</script></body></html>";
const char kNavigatorCredentialsCreateWithResultPageHtml[] =
    "<html><body><script>"
    "window.onload = () => {"
    "  navigator.credentials.create({ publicKey: { "
    "    challenge: new ArrayBuffer(0), "
    "    rp: { id: 'example.com', name: 'Example' },"
    "    user: { id: new ArrayBuffer(0), name: '', displayName: '' } } "
    "  }).then(cred => { "
    "    window.credentialResult = cred; "
    "  }).catch(err => { "
    "    window.credentialError = err; "
    "  });"
    "};"
    "</script></body></html>";
const char kNavigatorCredentialsGetWithResultPageHtml[] =
    "<html><body><script>"
    "window.onload = () => {"
    "  navigator.credentials.get({ "
    "    publicKey: { challenge: new ArrayBuffer(0) } "
    "  }).then(cred => { "
    "    window.credentialResult = cred; "
    "  }).catch(err => { "
    "    window.credentialError = err; "
    "  });"
    "};"
    "</script></body></html>";
const char kAnotherPageHtml[] = "<html><body>Another Page</body></html>";

NSString* const kDisableNativeCapabilitiesJs =
    @"if (typeof PublicKeyCredential !== 'undefined') {"
    @"  Object.defineProperty(PublicKeyCredential, "
    @"'isConditionalMediationAvailable', {"
    @"    value: async () => false,"
    @"    configurable: true,"
    @"    writable: true"
    @"  });"
    @"  if (PublicKeyCredential.getClientCapabilities) {"
    @"    Object.defineProperty(PublicKeyCredential, "
    @"'getClientCapabilities', {"
    @"      value: async () => ({"
    @"        conditionalGet: false,"
    @"        conditionalCreate: false,"
    @"        userVerifyingPlatformAuthenticator: true"
    @"      }),"
    @"      configurable: true,"
    @"      writable: true"
    @"    });"
    @"  }"
    @"}";

NSString* const kRejectPasskeyRequestFormat =
    @"__gCrWeb.getRegisteredApi('passkey')."
    @"getFunction('rejectPasskeyRequest')('%@', '%s', '%s')";

NSString* const kResolveAssertionRequestWithEmptyUserHandleFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAssertionRequest')('%@', 'AQ', '', '', '', '', {})";

// Note: 'AQ==' is base64 of 0x01.
NSString* const kResolveAssertionRequestWithUserHandleFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAssertionRequest')('%@', 'AQ', '', '', 'AQ==', '', {})";

NSString* const kTriggerConditionalGetJs =
    @"window.resultReceived = false;"
    @"window.promiseResult = undefined;"
    @"window.promiseError = undefined;"
    @"navigator.credentials.get({"
    @"  mediation: 'conditional',"
    @"  publicKey: { challenge: new ArrayBuffer(0) }"
    @"}).then("
    @"  (res) => {"
    @"    window.promiseResult = res;"
    @"    window.resultReceived = true;"
    @"  },"
    @"  (err) => {"
    @"    window.promiseError = err;"
    @"    window.resultReceived = true;"
    @"  }"
    @");";

NSString* const kTriggerConditionalCreateJs =
    @"window.resultReceived = false;"
    @"window.promiseResult = undefined;"
    @"window.promiseError = undefined;"
    @"navigator.credentials.create({"
    @"  mediation: 'conditional',"
    @"  publicKey: {"
    @"    challenge: new ArrayBuffer(0),"
    @"    rp: { id: 'example.com', name: 'Example' },"
    @"    user: { id: new ArrayBuffer(0), name: 'user', "
    @"displayName: 'User' },"
    @"    pubKeyCredParams: []"
    @"  }"
    @"}).then("
    @"  (res) => {"
    @"    window.promiseResult = res;"
    @"    window.resultReceived = true;"
    @"  },"
    @"  (err) => {"
    @"    window.promiseError = err;"
    @"    window.resultReceived = true;"
    @"  }"
    @");";

NSString* const kDeferToRendererFormat =
    @"__gCrWeb.getRegisteredApi('passkey')."
    @"getFunction('deferToRenderer')('%@', %d)";

NSString* const kCheckCredentialResultDefinedJs =
    @"window.credentialResult !== undefined";

NSString* const kCheckUserHandleIsNullJs =
    @"window.credentialResult.response.userHandle === null";

NSString* const kCheckUserHandleByteLengthJs =
    @"window.credentialResult.response.userHandle.byteLength";

NSString* const kGetFirstByteOfUserHandleJs =
    @"new Uint8Array(window.credentialResult.response.userHandle)[0]";

NSString* const kCheckCredentialResultInstanceOfPublicKeyCredentialJs =
    @"window.credentialResult instanceof PublicKeyCredential";

NSString* const kCheckResponseInstanceOfAuthenticatorAttestationResponseJs =
    @"window.credentialResult.response instanceof "
    @"AuthenticatorAttestationResponse";

NSString* const kCheckResponseInstanceOfAuthenticatorAssertionResponseJs =
    @"window.credentialResult.response instanceof "
    @"AuthenticatorAssertionResponse";

NSString* const kCheckPublicKeyIsNullJs =
    @"window.credentialResult.response.getPublicKey() === null";

NSString* const kCheckPublicKeyByteLengthJs =
    @"window.credentialResult.response.getPublicKey().byteLength";

NSString* const kGetFirstByteOfPublicKeyJs =
    @"new Uint8Array(window.credentialResult.response.getPublicKey())[0]";

NSString* const kResolveAttestationRequestWithEmptyPublicKeyFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAttestationRequest')('%@', 'AQ', '', '', '', '', {})";

NSString* const kResolveAttestationRequestWithPublicKeyFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAttestationRequest')('%@', 'AQ', '', '', 'AQ==', '', {})";

NSString* const kResolveAttestationRequestFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAttestationRequest')('%@', 'AQ', 'AQ==', 'AQ==', 'AQ==', '{}', "
    @"{})";

NSString* const kResolveAssertionRequestFormat =
    @"__gCrWeb.getRegisteredApi('passkey').getFunction('"
    @"resolveAssertionRequest')('%@', 'AQ', 'AQ==', 'AQ==', 'AQ==', '{}', {})";

NSString* const kCheckResultReceivedJs = @"window.resultReceived";
NSString* const kGetPromiseResultJs = @"window.promiseResult";
NSString* const kGetPromiseErrorJs = @"window.promiseError";

const char kModalTestNameSubstring[] = "Modal";
const char kConditionalTestNameSubstring[] = "Conditional";
const char kUnsupportedResolvesWithNullTestNameSubstring[] =
    "UnsupportedResolvesWithNull";
const char kPassthroughNullAttachmentTestNameSubstring[] =
    "PassthroughNullAttachment";

NSString* const kMockCredentialWithNullAttachmentJs =
    @"if (typeof PublicKeyCredential === 'undefined') {"
    @"  window.PublicKeyCredential = class PublicKeyCredential {};"
    @"}"
    @"if (typeof AuthenticatorAttestationResponse === 'undefined') {"
    @"  window.AuthenticatorAttestationResponse = "
    @"      class AuthenticatorAttestationResponse {};"
    @"}"
    @"const mockCredential = {"
    @"  id: 'credential_id',"
    @"  type: 'public-key',"
    @"  authenticatorAttachment: null,"
    @"  rawId: new ArrayBuffer(8),"
    @"  response: {"
    @"    clientDataJSON: new ArrayBuffer(8),"
    @"    attestationObject: new ArrayBuffer(8),"
    @"    getAuthenticatorData: () => new ArrayBuffer(128),"
    @"    getPublicKey: () => new ArrayBuffer(64),"
    @"    getPublicKeyAlgorithm: () => -7,"
    @"    getTransports: () => [],"
    @"  },"
    @"  getClientExtensionResults: () => ({}),"
    @"  toJSON: () => ({}),"
    @"};"
    @"Object.setPrototypeOf(mockCredential, PublicKeyCredential.prototype);"
    @"Object.setPrototypeOf(mockCredential.response, "
    @"AuthenticatorAttestationResponse.prototype);"
    @"if (!navigator.credentials) {"
    @"  Object.defineProperty(navigator, 'credentials', {"
    @"    value: {}, writable: true, configurable: true"
    @"  });"
    @"}"
    @"navigator.credentials.create = async (options) => mockCredential;"
    @"navigator.credentials.get = async (options) => mockCredential;";

NSString* const kEventKey = @"event";
NSString* const kFrameIdKey = @"frameId";
NSString* const kRequestIdKey = @"requestId";
NSString* const kRequestKey = @"request";
NSString* const kRpEntityKey = @"rpEntity";
NSString* const kUserEntityKey = @"userEntity";
NSString* const kExcludeCredentialsKey = @"excludeCredentials";
NSString* const kExtensionsKey = @"extensions";
NSString* const kRemoteFrameIdKey = @"remoteFrameId";
NSString* const kAllowCredentialsKey = @"allowCredentials";
NSString* const kIdKey = @"id";

NSString* const kLogCreateRequestEvent = @"logCreateRequest";
NSString* const kLogGetRequestEvent = @"logGetRequest";
NSString* const kHandleCreateRequestEvent = @"handleCreateRequest";
NSString* const kHandleGetRequestEvent = @"handleGetRequest";

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
  } else if (request.relative_url ==
             kNavigatorCredentialsGetWithUserHandleUrl) {
    http_response->set_content(kNavigatorCredentialsGetWithUserHandlePageHtml);
  } else if (request.relative_url == kNavigatorCredentialsCreateWithResultUrl) {
    http_response->set_content(kNavigatorCredentialsCreateWithResultPageHtml);
  } else if (request.relative_url == kNavigatorCredentialsGetWithResultUrl) {
    http_response->set_content(kNavigatorCredentialsGetWithResultPageHtml);
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

    if (test_name.find(kModalTestNameSubstring) != std::string::npos) {
      enabled_features.push_back(kIOSPasskeyModalLoginWithShim);
    } else {
      disabled_features.push_back(kIOSPasskeyModalLoginWithShim);
    }

    if (test_name.find(kConditionalTestNameSubstring) != std::string::npos) {
      enabled_features.push_back(kIOSPasskeyConditionalLoginWithShim);
    } else {
      disabled_features.push_back(kIOSPasskeyConditionalLoginWithShim);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    if (test_name.find(kUnsupportedResolvesWithNullTestNameSubstring) !=
        std::string::npos) {
      WKUserScript* disableScript = [[WKUserScript alloc]
            initWithSource:kDisableNativeCapabilitiesJs
             injectionTime:WKUserScriptInjectionTimeAtDocumentStart
          forMainFrameOnly:NO];
      [web_view().configuration.userContentController
          addUserScript:disableScript];
    }

    if (test_name.find(kPassthroughNullAttachmentTestNameSubstring) !=
        std::string::npos) {
      WKUserScript* mockScript = [[WKUserScript alloc]
            initWithSource:kMockCredentialWithNullAttachmentJs
             injectionTime:WKUserScriptInjectionTimeAtDocumentStart
          forMainFrameOnly:NO];
      [web_view().configuration.userContentController addUserScript:mockScript];
    }

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
  EXPECT_TRUE([allKeys containsObject:kEventKey]);

  EXPECT_NSEQ(kLogCreateRequestEvent, body[kEventKey]);
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
  EXPECT_TRUE([allKeys containsObject:kEventKey]);

  EXPECT_NSEQ(kLogGetRequestEvent, body[kEventKey]);
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
  EXPECT_TRUE([allKeys containsObject:kEventKey]);
  EXPECT_TRUE([allKeys containsObject:kFrameIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestKey]);
  EXPECT_TRUE([allKeys containsObject:kRpEntityKey]);
  EXPECT_TRUE([allKeys containsObject:kUserEntityKey]);
  EXPECT_TRUE([allKeys containsObject:kExcludeCredentialsKey]);
  EXPECT_TRUE([allKeys containsObject:kExtensionsKey]);

  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);
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
  EXPECT_TRUE([allKeys containsObject:kEventKey]);
  EXPECT_TRUE([allKeys containsObject:kFrameIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestKey]);
  EXPECT_TRUE([allKeys containsObject:kRpEntityKey]);
  EXPECT_TRUE([allKeys containsObject:kUserEntityKey]);
  EXPECT_TRUE([allKeys containsObject:kExcludeCredentialsKey]);
  EXPECT_TRUE([allKeys containsObject:kExtensionsKey]);

  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSDictionary* rpEntity = body[kRpEntityKey];
  EXPECT_TRUE(rpEntity != nil);
  EXPECT_NSEQ(@"127.0.0.1", rpEntity[kIdKey]);
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
  EXPECT_TRUE([allKeys containsObject:kEventKey]);
  EXPECT_TRUE([allKeys containsObject:kFrameIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRemoteFrameIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestIdKey]);
  EXPECT_TRUE([allKeys containsObject:kRequestKey]);
  EXPECT_TRUE([allKeys containsObject:kRpEntityKey]);
  EXPECT_TRUE([allKeys containsObject:kAllowCredentialsKey]);
  EXPECT_TRUE([allKeys containsObject:kExtensionsKey]);

  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSDictionary* rpEntity = body[kRpEntityKey];
  EXPECT_TRUE(rpEntity != nil);
  EXPECT_NSEQ(@"127.0.0.1", rpEntity[kIdKey]);
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
  EXPECT_NSEQ(kLogCreateRequestEvent, body[kEventKey]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it is a passthrough request, it finishes immediately (rejects) in the
  // test environment, so it shouldn't trigger reload on back navigation.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_NSEQ(nil, message_handler().lastReceivedMessage);
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
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);
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
  EXPECT_NSEQ(kLogGetRequestEvent, body[kEventKey]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it is a passthrough request, it finishes immediately (rejects) in the
  // test environment, so it shouldn't trigger reload on back navigation.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_NSEQ(nil, message_handler().lastReceivedMessage);
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
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));

  body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);
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
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Reject the request to simulate finishing it.
  NSString* rejectJs =
      [NSString stringWithFormat:kRejectPasskeyRequestFormat, requestId,
                                 kNotAllowedErrorName, kNotAllowedErrorMessage];
  web::test::ExecuteJavaScriptInWebView(web_view(), rejectJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it finished, it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_NSEQ(nil, message_handler().lastReceivedMessage);
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
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request to simulate a successful request.
  // Note: we pass "AQ" as the id64 so that it has rawId.byteLength > 0 and
  // counts as a valid credential.
  NSString* resolveJs = [NSString
      stringWithFormat:kResolveAssertionRequestWithEmptyUserHandleFormat,
                       requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it succeeded, it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_NSEQ(nil, message_handler().lastReceivedMessage);
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
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  NSString* rejectJs =
      [NSString stringWithFormat:kRejectPasskeyRequestFormat, requestId,
                                 kNotAllowedErrorName, kNotAllowedErrorMessage];
  web::test::ExecuteJavaScriptInWebView(web_view(), rejectJs);

  message_handler().lastReceivedMessage = nil;

  ASSERT_TRUE(LoadUrl(another_url));

  [web_view() goBack];

  // Since it finished (rejected), it should NOT reload.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_NSEQ(nil, message_handler().lastReceivedMessage);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsConditionalGetWithEmptyUserHandleReturnsNull) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetWithUserHandleUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request with userHandle64 as empty string "".
  NSString* resolveJs = [NSString
      stringWithFormat:kResolveAssertionRequestWithEmptyUserHandleFormat,
                       requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult.response.userHandle is null.
  id isNull =
      web::test::ExecuteJavaScript(web_view(), kCheckUserHandleIsNullJs);
  EXPECT_NSEQ(@YES, isNull);
}

TEST_F(
    PasskeyControllerJavaScriptTest,
    NavigatorCredentialsConditionalGetWithNonEmptyUserHandleReturnsArrayBuffer) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetWithUserHandleUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request.
  NSString* resolveJs = [NSString
      stringWithFormat:kResolveAssertionRequestWithUserHandleFormat, requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult.response.userHandle is not null and has
  // length 1.
  id isNull =
      web::test::ExecuteJavaScript(web_view(), kCheckUserHandleIsNullJs);
  EXPECT_NSEQ(@NO, isNull);

  id byteLength =
      web::test::ExecuteJavaScript(web_view(), kCheckUserHandleByteLengthJs);
  EXPECT_NSEQ(@1, byteLength);

  id firstByte =
      web::test::ExecuteJavaScript(web_view(), kGetFirstByteOfUserHandleJs);
  EXPECT_NSEQ(@1, firstByte);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsConditionalGetUnsupportedResolvesWithNull) {
  GURL another_url = server().GetURL(kAnotherPageUrl);
  ASSERT_TRUE(LoadUrl(another_url));

  // Run script to trigger conditional get and record results.
  web::test::ExecuteJavaScriptInWebView(web_view(), kTriggerConditionalGetJs);

  // Wait for handleGetRequest to be received.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Call deferToRenderer, simulating unsupported conditional get capability.
  NSString* deferJs = [NSString
      stringWithFormat:kDeferToRendererFormat, requestId,
                       static_cast<int>(
                           PasskeyRequestParams::RequestType::kConditionalGet)];
  web::test::ExecuteJavaScriptInWebView(web_view(), deferJs);

  // Wait for promise resolution.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result =
            web::test::ExecuteJavaScript(web_view(), kCheckResultReceivedJs);
        return [result isKindOfClass:[NSNumber class]] && [result boolValue];
      }));

  // Assert that promiseResult is null (and not undefined or error).
  id promiseResult =
      web::test::ExecuteJavaScript(web_view(), kGetPromiseResultJs);
  EXPECT_NSEQ([NSNull null], promiseResult);

  id promiseError =
      web::test::ExecuteJavaScript(web_view(), kGetPromiseErrorJs);
  EXPECT_NSEQ(nil, promiseError);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsConditionalCreateUnsupportedResolvesWithNull) {
  GURL another_url = server().GetURL(kAnotherPageUrl);
  ASSERT_TRUE(LoadUrl(another_url));

  // Run script to trigger conditional create and record results.
  web::test::ExecuteJavaScriptInWebView(web_view(),
                                        kTriggerConditionalCreateJs);

  // Wait for handleCreateRequest to be received.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Call deferToRenderer, simulating unsupported conditional create capability.
  NSString* deferJs = [NSString
      stringWithFormat:kDeferToRendererFormat, requestId,
                       static_cast<int>(PasskeyRequestParams::RequestType::
                                            kConditionalCreate)];
  web::test::ExecuteJavaScriptInWebView(web_view(), deferJs);

  // Wait for promise resolution.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result =
            web::test::ExecuteJavaScript(web_view(), kCheckResultReceivedJs);
        return [result isKindOfClass:[NSNumber class]] && [result boolValue];
      }));

  // Assert that promiseResult is null (and not undefined or error).
  id promiseResult =
      web::test::ExecuteJavaScript(web_view(), kGetPromiseResultJs);
  EXPECT_NSEQ([NSNull null], promiseResult);

  id promiseError =
      web::test::ExecuteJavaScript(web_view(), kGetPromiseErrorJs);
  EXPECT_NSEQ(nil, promiseError);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateReturnsCorrectPrototype) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateWithResultUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request with valid dummy attestation parameters.
  NSString* resolveJs =
      [NSString stringWithFormat:kResolveAttestationRequestFormat, requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult is an instance of PublicKeyCredential.
  id isPublicKeyCredential = web::test::ExecuteJavaScript(
      web_view(), kCheckCredentialResultInstanceOfPublicKeyCredentialJs);
  EXPECT_NSEQ(@YES, isPublicKeyCredential);

  // Verify that window.credentialResult.response is an instance of
  // AuthenticatorAttestationResponse.
  id isAttestationResponse = web::test::ExecuteJavaScript(
      web_view(), kCheckResponseInstanceOfAuthenticatorAttestationResponseJs);
  EXPECT_NSEQ(@YES, isAttestationResponse);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalGetReturnsCorrectPrototype) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetWithResultUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleGetRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request with valid dummy assertion parameters.
  NSString* resolveJs =
      [NSString stringWithFormat:kResolveAssertionRequestFormat, requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult is an instance of PublicKeyCredential.
  id isPublicKeyCredential = web::test::ExecuteJavaScript(
      web_view(), kCheckCredentialResultInstanceOfPublicKeyCredentialJs);
  EXPECT_NSEQ(@YES, isPublicKeyCredential);

  // Verify that window.credentialResult.response is an instance of
  // AuthenticatorAssertionResponse.
  id isAssertionResponse = web::test::ExecuteJavaScript(
      web_view(), kCheckResponseInstanceOfAuthenticatorAssertionResponseJs);
  EXPECT_NSEQ(@YES, isAssertionResponse);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateWithEmptyPublicKeyReturnsNull) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateWithResultUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request with publicKeySpkiDer64 as empty string "".
  NSString* resolveJs = [NSString
      stringWithFormat:kResolveAttestationRequestWithEmptyPublicKeyFormat,
                       requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult.response.getPublicKey() is null.
  id isNull = web::test::ExecuteJavaScript(web_view(), kCheckPublicKeyIsNullJs);
  EXPECT_NSEQ(@YES, isNull);
}

TEST_F(PasskeyControllerJavaScriptTest,
       NavigatorCredentialsModalCreateWithNonEmptyPublicKeyReturnsArrayBuffer) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateWithResultUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return message_handler().lastReceivedMessage != nil;
      }));
  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(kHandleCreateRequestEvent, body[kEventKey]);

  NSString* requestId = body[kRequestIdKey];
  ASSERT_TRUE(requestId != nil);

  // Resolve the request.
  NSString* resolveJs =
      [NSString stringWithFormat:kResolveAttestationRequestWithPublicKeyFormat,
                                 requestId];
  web::test::ExecuteJavaScriptInWebView(web_view(), resolveJs);

  // Wait until window.credentialResult is set.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        id result = web::test::ExecuteJavaScript(
            web_view(), kCheckCredentialResultDefinedJs);
        return [result boolValue];
      }));

  // Verify that window.credentialResult.response.getPublicKey() is not null and
  // has length 1.
  id isNull = web::test::ExecuteJavaScript(web_view(), kCheckPublicKeyIsNullJs);
  EXPECT_NSEQ(@NO, isNull);

  id byteLength =
      web::test::ExecuteJavaScript(web_view(), kCheckPublicKeyByteLengthJs);
  EXPECT_NSEQ(@1, byteLength);

  id firstByte =
      web::test::ExecuteJavaScript(web_view(), kGetFirstByteOfPublicKeyJs);
  EXPECT_NSEQ(@1, firstByte);
}

TEST_F(PasskeyControllerJavaScriptTest,
       PassthroughNullAttachment_RegistrationSucceeds) {
  GURL create_url = server().GetURL(kNavigatorCredentialsCreateWithResultUrl);

  ASSERT_TRUE(LoadUrl(create_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        if (message_handler().lastReceivedMessage == nil) {
          return NO;
        }
        NSDictionary* body = message_handler().lastReceivedMessage.body;
        return [body[kEventKey] isEqualToString:@"logCreateResolved"];
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"logCreateResolved", body[kEventKey]);
  EXPECT_NSEQ(@NO, body[@"isGpm"]);
}

TEST_F(PasskeyControllerJavaScriptTest,
       PassthroughNullAttachment_AssertionSucceeds) {
  GURL get_url = server().GetURL(kNavigatorCredentialsGetWithResultUrl);

  ASSERT_TRUE(LoadUrl(get_url));

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        if (message_handler().lastReceivedMessage == nil) {
          return NO;
        }
        NSDictionary* body = message_handler().lastReceivedMessage.body;
        return [body[kEventKey] isEqualToString:@"logGetResolved"];
      }));

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"logGetResolved", body[kEventKey]);
  EXPECT_NSEQ(@"credential_id", body[@"credentialId"]);
}

}  // namespace webauthn

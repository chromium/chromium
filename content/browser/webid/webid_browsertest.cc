// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/in_memory_federated_permission_context.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/test/mock_digital_identity_provider.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/cors/cors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::EmbeddedTestServer;
using net::HttpStatusCode;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using DigitalCredential = content::DigitalIdentityProvider::DigitalCredential;
using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace content {

namespace {

constexpr char kRpHostName[] = "rp.example";

// Use localhost for IDP so that the well-known file can be fetched from the
// test server's custom port. IdpNetworkRequestManager::ComputeWellKnownUrl()
// does not enforce a specific port if the IDP is localhost.
constexpr char kIdpOrigin[] = "https://127.0.0.1";

constexpr char kOtherIdpHostname[] = "idp2.example";

constexpr char kExpectedConfigPath[] = "/fedcm.json";
constexpr char kExpectedWellKnownPath[] = "/.well-known/web-identity";
constexpr char kTestContentType[] = "application/json";
constexpr char kIdpForbiddenHeader[] = "Sec-FedCM-CSRF";

static constexpr char kSetLoginHeader[] = "Set-Login";
static constexpr char kLoggedInHeaderValue[] = "logged-in";
static constexpr char kLoggedOutHeaderValue[] = "logged-out";

// Token value in //content/test/data/id_assertion_endpoint.json
constexpr char kToken[] = "[not a real token]";

constexpr char kJsErrorPrefix[] = "a JavaScript error:";

// Extracts error from `result` removing `kJsErrorPrefix` and removing leading
// and trailing whitespace and quotes.
std::string ExtractJsError(const EvalJsResult& result) {
  if (result.is_ok()) {
    return "";
  }
  if (!base::StartsWith(result.ExtractError(), kJsErrorPrefix)) {
    return result.ExtractError();
  }

  std::string error_message =
      result.ExtractError().substr(strlen(kJsErrorPrefix));
  base::TrimString(error_message, "\n \"", &error_message);
  return error_message;
}

bool IsGetRequestWithPath(const HttpRequest& request,
                          const std::string& expected_path) {
  return request.method == HttpMethod::METHOD_GET &&
         request.relative_url == expected_path;
}

// This class implements the IdP logic, and responds to requests sent to the
// test HTTP server.
class IdpTestServer {
 public:
  struct ConfigDetails {
    HttpStatusCode status_code;
    std::string content_type;
    std::string accounts_endpoint_url;
    std::string client_metadata_endpoint_url;
    std::string id_assertion_endpoint_url;
    std::string vc_issuance_endpoint_url;
    std::string metrics_endpoint_url;
    std::string login_url;
    std::vector<std::string> types;
    std::map<std::string,
             base::RepeatingCallback<std::unique_ptr<HttpResponse>(
                 const HttpRequest&)>>
        servlets;
  };

  IdpTestServer() = default;
  ~IdpTestServer() = default;

  IdpTestServer(const IdpTestServer&) = delete;
  IdpTestServer& operator=(const IdpTestServer&) = delete;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // RP files are fetched from the /test base directory. Assume anything
    // to other paths is directed to the IdP.
    if (request.relative_url.rfind("/test", 0) == 0) {
      return nullptr;
    }

    if (request.relative_url.rfind("/header/", 0) == 0) {
      return BuildIdpHeaderResponse(request);
    }

    if (request.all_headers.find(kIdpForbiddenHeader) != std::string::npos) {
      EXPECT_EQ(request.headers.at(kIdpForbiddenHeader), "?1");
    }

    auto response = std::make_unique<BasicHttpResponse>();
    if (IsGetRequestWithPath(request, kExpectedConfigPath)) {
      BuildConfigResponseFromDetails(*response.get(), config_details_);
      return response;
    }

    if (IsGetRequestWithPath(request, kExpectedWellKnownPath)) {
      BuildWellKnownResponse(*response.get());
      return response;
    }

    if (config_details_.servlets[request.relative_url]) {
      return config_details_.servlets[request.relative_url].Run(request);
    }

    return nullptr;
  }

  std::unique_ptr<HttpResponse> BuildIdpHeaderResponse(
      const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url.find("/header/signin") != std::string::npos) {
      response->AddCustomHeader(kSetLoginHeader, kLoggedInHeaderValue);
    } else if (request.relative_url.find("/header/signout") !=
               std::string::npos) {
      response->AddCustomHeader(kSetLoginHeader, kLoggedOutHeaderValue);
    } else {
      return nullptr;
    }
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/plain");
    response->set_content("Header sent.");
    return response;
  }

  void SetConfigResponseDetails(ConfigDetails details) {
    config_details_ = details;
  }

 private:
  void BuildConfigResponseFromDetails(BasicHttpResponse& response,
                                      const ConfigDetails& details) {
    std::map<std::string, std::string> map = {
        {"accounts_endpoint", "\"" + details.accounts_endpoint_url + "\""},
        {"client_metadata_endpoint",
         "\"" + details.client_metadata_endpoint_url + "\""},
        {"id_assertion_endpoint",
         "\"" + details.id_assertion_endpoint_url + "\""},
        {"vc_issuance_endpoint",
         "\"" + details.vc_issuance_endpoint_url + "\""},
        {"login_url", "\"" + details.login_url + "\""},
        {"metrics_endpoint", "\"" + details.metrics_endpoint_url + "\""},
        {"formats", "[\"vc+sd-jwt\"]"},
    };
    std::string content = ConvertToJsonDictionary(map, details.types);
    response.set_code(details.status_code);
    response.set_content(content);
    response.set_content_type(details.content_type);
  }

  void BuildWellKnownResponse(BasicHttpResponse& response) {
    std::string content = base::StringPrintf("{\"provider_urls\": [\"%s\"]}",
                                             kExpectedConfigPath);
    response.set_code(net::HTTP_OK);
    response.set_content(content);
    response.set_content_type("application/json");
  }

  std::string ConvertToJsonDictionary(
      const std::map<std::string, std::string>& data,
      const std::vector<std::string>& types) {
    std::string out = "{";
    for (auto it : data) {
      out += "\"" + it.first + "\":" + it.second + ",";
    }
    if (!types.empty()) {
      out += "\"types\":[";
      for (const auto& type : types) {
        out += "\"" + type + "\",";
      }
      out[out.length() - 1] = ']';
      // Adding comma which will be replaced when setting '}'.
      out += ",";
    }
    out[out.length() - 1] = '}';
    return out;
  }

  ConfigDetails config_details_;
};

class TestFederatedIdentityModalDialogViewDelegate
    : public NiceMock<MockModalDialogViewDelegate> {
 public:
  base::OnceClosure closure_;
  bool closed_{false};

  void SetClosure(base::OnceClosure closure) { closure_ = std::move(closure); }

  void OnClose() override {
    DCHECK(closure_);
    std::move(closure_).Run();
    closed_ = true;
  }

  base::WeakPtr<TestFederatedIdentityModalDialogViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestFederatedIdentityModalDialogViewDelegate>
      weak_ptr_factory_{this};
};

}  // namespace

class WebIdBrowserTest : public ContentBrowserTest {
 public:
  WebIdBrowserTest() = default;
  ~WebIdBrowserTest() override = default;

  WebIdBrowserTest(const WebIdBrowserTest&) = delete;
  WebIdBrowserTest& operator=(const WebIdBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    idp_server_ = std::make_unique<IdpTestServer>();
    https_server().SetCertHostnames({kRpHostName, kOtherIdpHostname});
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server().RegisterRequestHandler(base::BindRepeating(
        &IdpTestServer::HandleRequest, base::Unretained(idp_server_.get())));
    ASSERT_TRUE(https_server().Start());

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server().GetURL(kRpHostName, "/title1.html")));

    test_browser_client_ = std::make_unique<WebIdTestContentBrowserClient>();
    SetTestIdentityRequestDialogController("not_real_account");
    SetTestModalDialogViewDelegate();
  }

  void TearDown() override { ContentBrowserTest::TearDown(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;

    // kSplitCacheByNetworkIsolationKey feature is needed to verify
    // that the network shard for fetching the config file is different
    // from that used for other IdP transactions, to prevent data leakage.
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kFedCm);
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  std::string IdpRootUrl() {
    return std::string(kIdpOrigin) + ":" +
           base::NumberToString(https_server().port());
  }

  std::string BaseIdpUrl() { return IdpRootUrl() + "/fedcm.json"; }

  std::string BaseRpUrl() {
    return https_server().GetOrigin(kRpHostName).Serialize();
  }

  std::string GetBasicRequestString() {
    return R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
           BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }]
            }
          }));
          return x.token;
        }) ()
    )";
  }

  IdpTestServer::ConfigDetails BuildValidConfigDetails() {
    std::string accounts_endpoint_url = "/fedcm/accounts_endpoint.json";
    std::string client_metadata_endpoint_url =
        "/fedcm/client_metadata_endpoint.json";
    std::string id_assertion_endpoint_url = "/fedcm/id_assertion_endpoint.json";
    std::string login_url = "/fedcm/login.html";
    std::map<std::string, base::RepeatingCallback<std::unique_ptr<HttpResponse>(
                              const HttpRequest&)>>
        servlets;
    servlets[id_assertion_endpoint_url] = base::BindRepeating(
        [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
          EXPECT_EQ(request.method, HttpMethod::METHOD_POST);
          EXPECT_EQ(request.has_content, true);
          auto response = std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/json");
          CHECK(request.headers.contains("Origin"));
          response->AddCustomHeader(
              network::cors::header_names::kAccessControlAllowOrigin,
              request.headers.at("Origin"));
          response->AddCustomHeader(
              network::cors::header_names::kAccessControlAllowCredentials,
              "true");
          // Standard scopes were used, so no extra permission needed.
          // Return a token immediately.
          response->set_content(R"({"token": ")" + std::string(kToken) +
                                R"("})");
          return response;
        });
    return {net::HTTP_OK,
            kTestContentType,
            accounts_endpoint_url,
            client_metadata_endpoint_url,
            id_assertion_endpoint_url,
            /*vc_issuance_endpoint_url=*/std::string(),
            /*metrics_endpoint_url=*/std::string(),
            login_url,
            /*types=*/{},
            servlets};
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  void SetTestIdentityRequestDialogController(
      std::optional<std::string> dialog_selected_account) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        std::move(dialog_selected_account), /*web_contents=*/nullptr);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

  void SetTestDigitalIdentityProvider() {
    auto provider = std::make_unique<MockDigitalIdentityProvider>();
    test_browser_client_->SetDigitalIdentityProvider(std::move(provider));
  }

  void SetTestModalDialogViewDelegate() {
    test_modal_dialog_view_delegate_ =
        std::make_unique<TestFederatedIdentityModalDialogViewDelegate>();
    test_browser_client_->SetIdentityRegistry(
        shell()->web_contents(), test_modal_dialog_view_delegate_->GetWeakPtr(),
        GURL(BaseIdpUrl()));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WebIdTestContentBrowserClient> test_browser_client_;
  std::unique_ptr<TestFederatedIdentityModalDialogViewDelegate>
      test_modal_dialog_view_delegate_;

 private:
  EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<IdpTestServer> idp_server_;
};

class WebIdIdpSigninStatusBrowserTest : public WebIdBrowserTest {
 public:
  InMemoryFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<InMemoryFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

class WebIdLightweightFedcmBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(features::kFedCmLightweightMode);
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  InMemoryFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<InMemoryFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

class WebIdIdpSigninStatusForFetchKeepAliveBrowserTest
    : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kKeepAliveInBrowserMigration}, {});
  }

  InMemoryFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<InMemoryFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

class WebIdIdPRegistryBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kFedCm);
    features.push_back(features::kFedCmIdPRegistration);
    features.push_back(features::kFedCmLightweightMode);

    scoped_feature_list_.InitWithFeatures(features, {});
  }

  InMemoryFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<InMemoryFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }

  void NavigateToIdpToRegisterAndSetLoginStatus(const GURL& configURL) {
    EXPECT_TRUE(NavigateToURL(shell(), configURL));

    std::string script = R"(
          (async () => {
            await IdentityProvider.register(')" +
                         configURL.spec() + R"(');
            // The permission was accepted if the promise resolves.
            return true;
          }) ()
      )";

    EXPECT_EQ(true, EvalJs(shell(), script));

    // Assert that the IdP was added to the Registry.
    const std::vector<GURL>& registeredIdPs =
        sharing_context()->GetRegisteredIdPs();
    EXPECT_NE(
        std::find(registeredIdPs.begin(), registeredIdPs.end(), configURL),
        registeredIdPs.end());

    // Set the login status and push accounts since we only support pushed
    // accounts for registered IDPs.
    static constexpr char set_status_script[] = R"(
      (async () => {
        await navigator.login.setStatus("logged-in", {accounts: [
          {id: "not_real_account", name: "Test Name", email: "test@idp.example"}
        ]});
        return true;
      })()
    )";
    EXPECT_EQ(true, EvalJs(shell(), set_status_script));
  }

  base::HistogramTester histogram_tester_;
};

using WebIdAuthzBrowserTest = WebIdBrowserTest;

// Verify a standard login flow with IdP sign-in page.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FullLoginFlow) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  EXPECT_EQ(std::string(kToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify full login flow where the IdP uses absolute rather than relative
// URLs.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, AbsoluteURLs) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();
  config_details.accounts_endpoint_url = "/fedcm/accounts_endpoint.json";
  config_details.client_metadata_endpoint_url =
      "/fedcm/client_metadata_endpoint.json";
  config_details.id_assertion_endpoint_url =
      "/fedcm/id_assertion_endpoint.json";

  idp_server()->SetConfigResponseDetails(config_details);

  EXPECT_EQ(std::string(kToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify an attempt to invoke FedCM with an insecure IDP path fails.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FailsOnHTTP) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The IdP is not potentially trustworthy \\(are you using HTTP\\?\\)");
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: 'http://idp.example:)" +
                       base::NumberToString(https_server().port()) +
                       R"(/fedcm.json',
                clientId: 'client_id_1',
                nonce: '12345',
              }]
            }
          }));
          return x.token;
        }) ()
    )";

  std::string expected_error = "NetworkError: Error retrieving a token.";
  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), script)));
  ASSERT_TRUE(console_observer.Wait());
}

// Verify that passing a non-string token in the ID assertion response results
// in an error when the flexible token formats feature is disabled.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, NonStringTokenRejected) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  config_details.id_assertion_endpoint_url = "/non_string_token_endpoint.php";

  // Add a servlet to serve a response with a non-string token.
  config_details.servlets["/non_string_token_endpoint.php"] =
      base::BindRepeating(
          [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type(kTestContentType);
            response->AddCustomHeader("Access-Control-Allow-Origin", "*");
            response->AddCustomHeader("Access-Control-Allow-Credentials",
                                      "true");

            // Return a JSON response with a non-string token (object instead of
            // string)
            response->set_content(R"({
                "token": {
                  "type": "jwt",
                  "value": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9..."
                }
              })");
            return std::move(response);
          });

  idp_server()->SetConfigResponseDetails(config_details);

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
              }]
            }
          }));
          return x.token;
        }) ()
    )";

  // Expect an error when non-string token is returned without flexible formats
  // enabled
  std::string expected_error =
      "IdentityCredentialError: Error retrieving a token.";
  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), script)));
}

// Verify that an IdP can register itself.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RegisterIdP) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  // Expects the account chooser to be opened. Selects the first account.
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.register(')" +
                       configURL.spec() + R"(');
          // The permission was accepted if the promise resolves.
          return true;
        }) ()
    )";

  EXPECT_EQ(true, EvalJs(shell(), script));

  EXPECT_EQ(std::vector<GURL>{configURL},
            sharing_context()->GetRegisteredIdPs());

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that the RP cannot register the IdP across origins.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RpCantRegisterIdP) {
  std::string script = R"(
        (async () => {
          return await IdentityProvider.register(')" +
                       BaseIdpUrl() + R"(');
        }) ()
    )";

  std::string expected_error =
      "NotAllowedError: Attempting to register a cross-origin config.";

  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), script)));
}

// Verify that an IdP can unregister itself.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, UnregisterIdP) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  // Expects the account chooser to be opened. Selects the first account.
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.register(')" +
                       configURL.spec() + R"(');
          await IdentityProvider.unregister(')" +
                       configURL.spec() + R"(');
          return true;
        }) ()
    )";

  EXPECT_EQ(true, EvalJs(shell(), script));

  EXPECT_TRUE(sharing_context()->GetRegisteredIdPs().empty());

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that an RP can request from registered IdPs.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, UseRegistry) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  // Expects the account chooser to be opened. Selects the first account.
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  NavigateToIdpToRegisterAndSetLoginStatus(configURL);

  // Navigate to the RP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  std::string get_script = R"(
        (async () => {
          var {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                nonce: "1234",
                configURL: "any",
                clientId: "https://rp.example",
              }]
            }
          });
          return token;
        }) ()
    )";

  SetTestIdentityRequestDialogController("not_real_account");

  EXPECT_EQ(std::string(kToken), EvalJs(shell(), get_script));

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that when type is requested, an IDP not matching it will not show
// up.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RegistryWithTypeNoMatch) {
  GURL configURL = GURL(BaseIdpUrl());
  auto details = BuildValidConfigDetails();
  details.types = {"idp_type"};
  idp_server()->SetConfigResponseDetails(details);

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.register(')" +
                       configURL.spec() + R"(');
          // The permission was accepted if the promise resolves.
          return true;
        }) ()
    )";

  EXPECT_EQ(true, EvalJs(shell(), script));

  // Navigate to the RP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  std::string get_script = R"(
        (async () => {
          var {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                nonce: "1234",
                configURL: "any",
                clientId: "https://rp.example",
                type: "no_match"
              }]
            }
          });
          return token;
        }) ()
    )";

  SetTestIdentityRequestDialogController("not_real_account");

  std::string expected_error = "NetworkError: Error retrieving a token.";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The requested IdP type did not match the registered IdP.");

  // If the IdP does not have type set, it should not show up.
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), get_script)));
  ASSERT_TRUE(console_observer.Wait());

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that when the type of the registered IdP matches the requested one,
// the FedCM flow is successful.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RegistryWithTypeMatch) {
  GURL configURL = GURL(BaseIdpUrl());
  auto details = BuildValidConfigDetails();
  details.types = {"type_no_match", "idp_type"};
  idp_server()->SetConfigResponseDetails(details);

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  NavigateToIdpToRegisterAndSetLoginStatus(configURL);

  // Navigate to the RP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  std::string get_script = R"(
        (async () => {
          var {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                nonce: "1234",
                configURL: "any",
                clientId: "https://rp.example",
                type: "idp_type"
              }]
            }
          });
          return token;
        }) ()
    )";

  SetTestIdentityRequestDialogController("not_real_account");
  EXPECT_EQ(std::string(kToken), EvalJs(shell(), get_script));

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Test that multiple IdPs can be registered and that the FedCM flow is
// successful when the two registered IdPs are shown.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, MultipleRegisteredIdps) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillRepeatedly(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  // Register the first IdP and push accounts.
  NavigateToIdpToRegisterAndSetLoginStatus(configURL);

  // Register the second IdP.
  mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  controller = static_cast<MockIdentityRequestDialogController*>(
      test_browser_client_->GetIdentityRequestDialogControllerForTests());
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  GURL otherConfigURL = https_server().GetURL(kOtherIdpHostname, "/fedcm.json");
  NavigateToIdpToRegisterAndSetLoginStatus(otherConfigURL);

  // Navigate to the RP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  std::string get_script = R"(
        (async () => {
          var {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                nonce: "1234",
                configURL: "any",
                clientId: "https://rp.example",
              }]
            }
          });
          return token;
        }) ()
    )";

  SetTestIdentityRequestDialogController("not_real_account");

  EXPECT_EQ(std::string(kToken), EvalJs(shell(), get_script));

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that if an IDP is registered but has no pushed accounts, the FedCM
// call fails.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RegistryNoPushedAccounts) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  // Expects the account chooser to be opened. Selects the first account.
  EXPECT_CALL(*controller, RequestIdPRegistrationPermision)
      .WillOnce(::testing::WithArg<1>(
          [](base::OnceCallback<void(bool accepted)> callback) {
            std::move(callback).Run(true);
          }));

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.register(')" +
                       configURL.spec() + R"(');
          // The permission was accepted if the promise resolves.
          return true;
        }) ()
    )";

  EXPECT_EQ(true, EvalJs(shell(), script));

  // Assert that the IdP was added to the Registry.
  const std::vector<GURL>& registeredIdPs =
      sharing_context()->GetRegisteredIdPs();
  EXPECT_NE(std::find(registeredIdPs.begin(), registeredIdPs.end(), configURL),
            registeredIdPs.end());

  // Do not push accounts.

  // Navigate to the RP.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  std::string get_script = R"(
        (async () => {
          var {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                nonce: "1234",
                configURL: "any",
                clientId: "https://rp.example",
              }]
            }
          });
          return token;
        }) ()
    )";

  SetTestIdentityRequestDialogController("not_real_account");

  std::string expected_error = "NetworkError: Error retrieving a token.";
  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), get_script)));

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest,
                       RegistrationFailsWithInvalidLoginUrl) {
  GURL configURL = GURL(BaseIdpUrl());
  auto details = BuildValidConfigDetails();
  // Set this as empty so that the login URL is invalid.
  details.login_url = "";
  idp_server()->SetConfigResponseDetails(details);

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.register(')" +
                       configURL.spec() + R"(');
          // The permission was accepted if the promise resolves.
          return true;
        }) ()
    )";

  EXPECT_EQ("NotAllowedError: Invalid identity provider registration config.",
            ExtractJsError(EvalJs(shell(), script)));

  EXPECT_TRUE(sharing_context()->GetRegisteredIdPs().empty());

  histogram_tester_.ExpectTotalCount("Blink.FedCm.AccountsRequestSent", 0);
}

// Verify that IDP sign-in headers work.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest, IdpSigninToplevel) {
  GURL url = https_server().GetURL(kRpHostName, "/header/signin");
  EXPECT_FALSE(sharing_context()
                   ->GetIdpSigninStatus(url::Origin::Create(url))
                   .has_value());
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));
  auto value = sharing_context()->GetIdpSigninStatus(url::Origin::Create(url));
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);
}

// Verify that IDP sign-out headers work.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest, IdpSignoutToplevel) {
  GURL url = https_server().GetURL(kRpHostName, "/header/signout");
  EXPECT_FALSE(sharing_context()
                   ->GetIdpSigninStatus(url::Origin::Create(url))
                   .has_value());
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));
  auto value = sharing_context()->GetIdpSigninStatus(url::Origin::Create(url));
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}
// Verify that IDP sign-in/out headers work in subresources.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest,
                       IdpSigninAndOutSubresource) {
  static constexpr char script[] = R"(
    (async () => {
      var resp = await fetch('/header/sign%s');
      return resp.status;
    }) ();
  )";

  GURL url_for_origin = https_server().GetURL(kRpHostName, "/header/");
  url::Origin origin = url::Origin::Create(url_for_origin);
  EXPECT_FALSE(sharing_context()->GetIdpSigninStatus(origin).has_value());
  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "in")));
    run_loop.Run();
  }
  auto value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);

  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "out")));
    run_loop.Run();
  }
  value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}

// Verify that IDP sign-in/out headers work in fetch keepalive subresources when
// proxied via browser.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusForFetchKeepAliveBrowserTest,
                       IdpSigninAndOutSubresourceFetchKeepAliveInBrowser) {
  static constexpr char script[] = R"(
    (async () => {
      var resp = await fetch('/header/sign%s', {keepalive: true});
      return resp.status;
    }) ();
  )";

  GURL url_for_origin = https_server().GetURL(kRpHostName, "/header/");
  url::Origin origin = url::Origin::Create(url_for_origin);
  EXPECT_FALSE(sharing_context()->GetIdpSigninStatus(origin).has_value());
  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "in")));
    run_loop.Run();
  }
  auto value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);

  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "out")));
    run_loop.Run();
  }
  value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}

// Verify that IDP sign-in/out headers work in sync XHR.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest,
                       IdpSigninAndOutSyncXhr) {
  static constexpr char script[] = R"(
    (async () => {
      const request = new XMLHttpRequest();
      request.open('GET', '/header/sign%s', false);
      request.send(null);
      return request.status;
    }) ();
  )";

  GURL url_for_origin = https_server().GetURL(kRpHostName, "/header/");
  url::Origin origin = url::Origin::Create(url_for_origin);
  EXPECT_FALSE(sharing_context()->GetIdpSigninStatus(origin).has_value());
  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "in")));
    run_loop.Run();
  }
  auto value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);

  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "out")));
    run_loop.Run();
  }
  value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}

// Verify that IDP sign-in/out headers work in fetch from worker.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest,
                       IdpSigninAndOutFetchFromWorker) {
  static constexpr char script[] = R"(
    (async () => {
      const script =
        '(async () => { return (await fetch("/header/sign%s")).status; })()'
      return new Promise(resolve => {
        const channel = new MessageChannel();
        channel.port1.addEventListener('message', (e) => {
          resolve(e.data);
        });
        channel.port1.start();
        const worker = new Worker('/fedcm/eval_worker.js');
        worker.postMessage(
          {
            nested: false,
            script: script,
          },
          [channel.port2]
        );
      });
    }) ();
  )";

  GURL url_for_origin = https_server().GetURL(kRpHostName, "/header/");
  url::Origin origin = url::Origin::Create(url_for_origin);
  EXPECT_FALSE(sharing_context()->GetIdpSigninStatus(origin).has_value());
  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "in")));
    run_loop.Run();
  }
  auto value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);

  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "out")));
    run_loop.Run();
  }
  value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}

// Verify that IDP sign-in/out headers work in fetch from nested worker.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest,
                       IdpSigninAndOutFetchFromNestedWorker) {
  static constexpr char script[] = R"(
    (async () => {
      const script =
        '(async () => { return (await fetch("/header/sign%s")).status; })()'
      return new Promise(resolve => {
        const channel = new MessageChannel();
        channel.port1.addEventListener('message', (e) => {
          resolve(e.data);
        });
        channel.port1.start();
        const worker = new Worker('/fedcm/eval_worker.js');
        worker.postMessage(
          {
            nested: true,
            script: script,
          },
          [channel.port2]
        );
      });
    }) ();
  )";

  GURL url_for_origin = https_server().GetURL(kRpHostName, "/header/");
  url::Origin origin = url::Origin::Create(url_for_origin);
  EXPECT_FALSE(sharing_context()->GetIdpSigninStatus(origin).has_value());
  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "in")));
    run_loop.Run();
  }
  auto value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);

  {
    base::RunLoop run_loop;
    sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());
    EXPECT_EQ(200, EvalJs(shell(), base::StringPrintf(script, "out")));
    run_loop.Run();
  }
  value = sharing_context()->GetIdpSigninStatus(origin);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(*value);
}

// Verify that an IdP can call close to close modal dialog views.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest, IdPClose) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  // We navigate to the IdP's configURL so that we can run
  // the script below with the IdP's origin as the top level
  // first party context.
  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  std::string script = R"(
        (async () => {
          await IdentityProvider.close();
          return true;
        }) ()
    )";

  // IdentityProvider.close() should invoke NotifyClose() on the delegate set
  // on the identity registry. Check that modal dialog is not closed.
  EXPECT_FALSE(test_modal_dialog_view_delegate_->closed_);

  // Run the script.
  {
    base::RunLoop run_loop;
    test_modal_dialog_view_delegate_->SetClosure(run_loop.QuitClosure());
    EXPECT_EQ(true, EvalJs(shell(), script));
    run_loop.Run();
  }

  // Check that modal dialog is closed.
  EXPECT_TRUE(test_modal_dialog_view_delegate_->closed_);
}

class WebIdDigitalCredentialsBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kWebIdentityDigitalCredentials);
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  InMemoryFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<InMemoryFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }

  void SetUpOnMainThread() override {
    WebIdBrowserTest::SetUpOnMainThread();

    SetTestDigitalIdentityProvider();

    MockDigitalIdentityProvider* digital_identity_provider =
        static_cast<MockDigitalIdentityProvider*>(
            test_browser_client_->GetDigitalIdentityProviderForTests());
    ON_CALL(*digital_identity_provider,
            ShowDigitalIdentityInterstitial(_, _, _, _))
        .WillByDefault(WithArg<3>(
            [](DigitalIdentityProvider::DigitalIdentityInterstitialCallback
                   callback) {
              std::move(callback).Run(
                  DigitalIdentityProvider::RequestStatusForMetrics::kSuccess);
              return base::OnceClosure();
            }));
  }
};

std::string BuildDigitalIdentityValidJsRequestDictionary() {
  return R"({
    digital: {
      requests: [{
        protocol: "openid4vp",
        data: {
          // Based on https://github.com/openid/OpenID4VP/issues/125
          client_id: "client.example.org",
          client_id_scheme: "web-origin",
          nonce: "n-0S6_WzA2Mj",
          presentation_definition: {
            // Presentation Exchange request, omitted for brevity
          }
        }
      }],
    },
  })";
}

EvalJsResult EvalJsAndReturnToken(const ToRenderFrameHost& execution_target,
                                  std::string_view script_setting_token) {
  std::string script = base::StringPrintf(R"(
      (async () => {
          %s
          return token;
      }) ()
      )",
                                          script_setting_token.data());
  return EvalJs(execution_target, script);
}

EvalJsResult RunDigitalIdentityValidRequest(
    const ToRenderFrameHost& execution_target) {
  std::string script = base::StringPrintf(
      "const {data} = await navigator.credentials.get(%s);return data;",
      BuildDigitalIdentityValidJsRequestDictionary().c_str());
  return EvalJsAndReturnToken(execution_target, script);
}

// Leniently parses the input string as JSON and compares it to already-parsed
// JSON.
MATCHER_P(JsonMatches, ref, "") {
  int json_parsing_options =
      base::JSONParserOptions::JSON_PARSE_CHROMIUM_EXTENSIONS |
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS;
  auto ref_json =
      base::JSONReader::ReadAndReturnValueWithError(ref, json_parsing_options);
  return ref_json.has_value() && (ref_json.value() == arg.ToValue());
}

// Test that a Verifiable Credential can be requested via the
// navigator.credentials JS API.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       NavigatorCredentialsApi) {
  base::Value kIdentityProviderResponse =
      base::JSONReader::Read(
          R"({"vp_token": "token data" , "presentation_submission":"bar"})",
          base::JSON_PARSE_CHROMIUM_EXTENSIONS)
          .value();

  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalIdentityProvider* digital_identity_provider =
      static_cast<MockDigitalIdentityProvider*>(
          test_browser_client_->GetDigitalIdentityProviderForTests());

  std::string_view request = R"(
  {
   "requests": [ {
      "protocol": "openid4vp",
      "data": {
        "client_id": "client.example.org",
        "client_id_scheme": "web-origin",
        "nonce": "n-0S6_WzA2Mj",
        "presentation_definition": {
        }
      },
   } ]
  }
  )";

  std::string json;
  // Invalid whitespace and newlines are added to the request string to make it
  // easier to read in this test, so we remove them before actually making the
  // JSON comparison in IsJson below.
  base::RemoveChars(request, "\n ", &json);

  EXPECT_CALL(*digital_identity_provider, Get(_, _, JsonMatches(json), _))
      .WillOnce(WithArg<3>(
          [&kIdentityProviderResponse](
              DigitalIdentityProvider::DigitalIdentityCallback callback) {
            std::move(callback).Run(DigitalCredential(
                "openid4vp", kIdentityProviderResponse.Clone()));
          }));

  EXPECT_EQ(kIdentityProviderResponse, RunDigitalIdentityValidRequest(shell()));
}

// Test that when there's a pending mdoc request, a second `get` call should be
// rejected.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       OnlyOneInFlightDigitalCredentialRequestIsAllowed) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalIdentityProvider* digital_identity_provider =
      static_cast<MockDigitalIdentityProvider*>(
          test_browser_client_->GetDigitalIdentityProviderForTests());

  base::Value kResponse =
      base::JSONReader::Read(R"({"token":"test-mdoc"})",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS)
          .value();

  EXPECT_CALL(*digital_identity_provider, Get)
      .WillOnce(WithArg<3>(
          [&](DigitalIdentityProvider::DigitalIdentityCallback callback) {
            EXPECT_EQ(
                "NotAllowedError: Only one navigator.credentials.get/create "
                "request may be outstanding at one time.",
                ExtractJsError(RunDigitalIdentityValidRequest(shell())));
            std::move(callback).Run(
                DigitalCredential("openid4vp", kResponse.Clone()));
          }));

  EXPECT_EQ(kResponse, RunDigitalIdentityValidRequest(shell()));
}

// Test that when the user declines a digital identity request, the error
// message returned to JavaScript does not indicate that the user declined the
// request.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       UserDeclinesRequest) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalIdentityProvider* digital_identity_provider =
      static_cast<MockDigitalIdentityProvider*>(
          test_browser_client_->GetDigitalIdentityProviderForTests());

  EXPECT_CALL(*digital_identity_provider, Get)
      .WillOnce(WithArg<3>(
          [&](DigitalIdentityProvider::DigitalIdentityCallback callback) {
            std::move(callback).Run(base::unexpected(
                DigitalIdentityProvider::RequestStatusForMetrics::
                    kErrorUserDeclined));
          }));

  EXPECT_EQ("NetworkError: Error retrieving a token.",
            ExtractJsError(RunDigitalIdentityValidRequest(shell())));
}

// Test that Blink.DigitalIdentityRequest.Status UMA metric is recorded when
// digital identity request completes.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       RecordRequestStatusHistogramAfterRequestCompletes) {
  base::HistogramTester histogram_tester;

  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalIdentityProvider* digital_identity_provider =
      static_cast<MockDigitalIdentityProvider*>(
          test_browser_client_->GetDigitalIdentityProviderForTests());

  EXPECT_CALL(*digital_identity_provider, Get)
      .WillOnce(WithArg<3>(
          [](DigitalIdentityProvider::DigitalIdentityCallback callback) {
            std::move(callback).Run(DigitalCredential(
                "openid4vp",
                base::JSONReader::Read(R"({"token":"test-mdoc"})",
                                       base::JSON_PARSE_CHROMIUM_EXTENSIONS)
                    .value()));
          }));

  RunDigitalIdentityValidRequest(shell());

  histogram_tester.ExpectUniqueSample(
      "Blink.DigitalIdentityRequest.Status",
      DigitalIdentityProvider::RequestStatusForMetrics::kSuccess, 1);
}

// Verify that the Authz parameters are passed to the id assertion endpoint.
IN_PROC_BROWSER_TEST_F(WebIdAuthzBrowserTest, Authz_noPopUpWindow) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  // Points the id assertion endpoint to a servlet.
  config_details.id_assertion_endpoint_url = "/authz/id_assertion_endpoint.php";

  // Add a servlet to serve a response for the id assertoin endpoint.
  config_details.servlets["/authz/id_assertion_endpoint.php"] =
      base::BindRepeating(
          [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
            EXPECT_EQ(request.method, HttpMethod::METHOD_POST);
            EXPECT_EQ(request.has_content, true);

            std::string content;
            content += "client_id=client_id_1&";
            content += "nonce=12345&";
            content += "account_id=not_real_account&";
            content += "disclosure_text_shown=false&";
            content += "is_auto_selected=false&";
            content += "mode=passive&";
            // Asserts that the fields and params parameters
            // were passed correctly to the id assertion endpoint.
            content += "fields=name,email,picture&";
            content +=
                "params=%7B%22foo%22:%22bar%22,%22hello%22"
                ":%22world%22,%22%3F+gets+://%22:%22%26+escaped+!%22%7D";

            EXPECT_EQ(request.content, content);

            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/json");
            DCHECK(request.headers.contains("Origin"));
            response->AddCustomHeader(
                network::cors::header_names::kAccessControlAllowOrigin,
                request.headers.at("Origin"));
            response->AddCustomHeader(
                network::cors::header_names::kAccessControlAllowCredentials,
                "true");
            // Standard scopes were used, so no extra permission needed.
            // Return a token immediately.
            response->set_content(R"({"token": "[request lgtm!]"})");
            return response;
          });

  idp_server()->SetConfigResponseDetails(config_details);

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
                fields: [
                  'name',
                  'email',
                  'picture',
                ],
                params: {
                  'foo': 'bar',
                  'hello': 'world',
                  '? gets ://': '& escaped !',
                }
              }]
            }
          }));
          return x.token;
        }) ()
    )";

  EXPECT_EQ(std::string("[request lgtm!]"), EvalJs(shell(), script));
}

// Verify that subsets of the default fields work.
IN_PROC_BROWSER_TEST_F(WebIdAuthzBrowserTest, Authz_invalidFields) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  // Points the id assertion endpoint to a servlet.
  config_details.id_assertion_endpoint_url = "/authz/id_assertion_endpoint.php";
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  std::string script = R"(
        (async () => {
          var result = await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
                fields: [
                  'name'
                ],
              }]
            }
         });
         return result.token;
      }) ()
    )";

  EXPECT_EQ(std::string("[not a real token]"), EvalJs(shell(), script));
}

// Verify that the id assertion endpoint can request a pop-up window.
IN_PROC_BROWSER_TEST_F(WebIdAuthzBrowserTest, Authz_openPopUpWindow) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  // Points the id assertion endpoint to a servlet.
  config_details.id_assertion_endpoint_url = "/authz/id_assertion_endpoint.php";

  // Points to the relative url of the authorization servlet.
  std::string continue_on = "/authz.html";

  // Add a servlet to serve a response for the id assertoin endpoint.
  config_details.servlets["/authz/id_assertion_endpoint.php"] =
      base::BindRepeating(
          [](std::string url,
             const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
            std::string content;
            content += "client_id=client_id_1&";
            content += "nonce=12345&";
            content += "account_id=not_real_account&";
            content += "disclosure_text_shown=false&";
            content += "is_auto_selected=false&";
            content += "mode=passive&";
            content += "fields=locale";

            EXPECT_EQ(request.content, content);

            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/json");
            // fields=locale was requested, so need to
            // return a continuation url instead of a token.
            auto body = R"({"continue_on": ")" + url + R"("})";
            response->set_content(body);
            DCHECK(request.headers.contains("Origin"));
            response->AddCustomHeader(
                network::cors::header_names::kAccessControlAllowOrigin,
                request.headers.at("Origin"));
            response->AddCustomHeader(
                network::cors::header_names::kAccessControlAllowCredentials,
                "true");
            return response;
          },
          continue_on);

  idp_server()->SetConfigResponseDetails(config_details);

  // Create a WebContents that represents the modal dialog, specifically
  // the structure that the Identity Registry hangs to.
  Shell* modal = CreateBrowser();
  auto config_url = GURL(BaseIdpUrl());

  modal->LoadURL(config_url);
  EXPECT_TRUE(WaitForLoadStop(modal->web_contents()));

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  // Expects the account chooser to be opened. Selects the first account.
  EXPECT_CALL(*controller, ShowAccountsDialog)
      .WillOnce(::testing::WithArg<4>([&config_url](auto on_selected) {
        std::move(on_selected)
            .Run(config_url,
                 /* account_id=*/"not_real_account",
                 /* is_sign_in= */ true);
        return true;
      }));

  base::RunLoop run_loop;
  EXPECT_CALL(*controller, ShowModalDialog)
      .WillOnce(::testing::WithArg<0>(
          [&config_url, continue_on, &modal, &run_loop](const GURL& url) {
            // Expect that the relative continue_on url will be resolved
            // before opening the dialog.
            EXPECT_EQ(url.spec(), config_url.Resolve(continue_on));
            // When the pop-up window is opened, resolve it immediately by
            // returning a test web contents, which can then later be used
            // to refer to the identity registry.
            run_loop.Quit();
            return modal->web_contents();
          }));

  std::string script = R"(
          var result = navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
                fields: [
                  'locale'
                ],
              }]
            }
         }).then(({token}) => token);
    )";

  // Kick off the identity credential request and deliberately
  // leave the promise hanging, since it requires UX permission
  // prompts to be accepted later.
  EXPECT_TRUE(content::ExecJs(shell(), script,
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the modal dialog to be resolved.
  run_loop.Run();

  std::string token = "--fake-token-from-pop-up-window--";

  base::RunLoop run_loop2;
  EXPECT_CALL(*controller, CloseModalDialog).WillOnce([&run_loop2]() {
    run_loop2.Quit();
  });

  // Resolve the hanging token request by notifying the registry.
  EXPECT_TRUE(content::ExecJs(
      modal, R"(IdentityProvider.resolve(')" + token + R"('))"));
  run_loop2.Run();

  // Finally, wait for the promise to resolve and compare its result
  // to the expected token that was provided in the modal dialog.
  EXPECT_EQ(token, EvalJs(shell(), "result"));
}

IN_PROC_BROWSER_TEST_F(WebIdAuthzBrowserTest, IdpLoginCallsResolve) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  // Mark us as signed out from this IdP.
  GURL url{IdpRootUrl() + "/header/signout"};
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));

  // This will be used for the login dialog.
  Shell* modal = CreateBrowser();
  auto config_url = GURL(BaseIdpUrl());

  modal->LoadURL(config_url);
  EXPECT_TRUE(WaitForLoadStop(modal->web_contents()));

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  MockIdentityRequestDialogController* controller =
      static_cast<MockIdentityRequestDialogController*>(
          test_browser_client_->GetIdentityRequestDialogControllerForTests());

  base::RunLoop modal_dialog_loop;
  EXPECT_CALL(*controller, ShowModalDialog)
      .WillOnce(WithArgs<0, 2>(
          [&modal, &modal_dialog_loop](
              const GURL& url,
              content::IdentityRequestDialogController::DismissCallback cb) {
            modal_dialog_loop.Quit();
            return modal->web_contents();
          }));
  EXPECT_CALL(*controller, ShowLoadingDialog).WillOnce(Return(true));

  // Now run the actual test.
  std::string script = R"(
      promise = navigator.credentials.get({
        identity: {
          providers: [{
            configURL: ')" +
                       BaseIdpUrl() + R"(',
            clientId: 'client_id_1',
            nonce: '12345',
          }],
          mode: 'active'
        }
      });
    )";

  // Initiate the FedCM call
  EXPECT_TRUE(ExecJs(shell(), script, EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the popup window to open.
  modal_dialog_loop.Run();

  // IdentityProvider.resolve() should be ignored and not crash.
  std::string expected_error =
      "NotAllowedError: Not allowed to provide a token.";
  EXPECT_EQ(
      expected_error,
      ExtractJsError(EvalJs(modal, R"(IdentityProvider.resolve('token'))")));
}

// Verify that an IdentityCredentialError exception is returned.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, IdentityCredentialError) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  // Points the id assertion endpoint to a servlet.
  config_details.id_assertion_endpoint_url = "/error/id_assertion_endpoint.php";

  // Add a servlet to serve a response for the id assertion endpoint.
  config_details.servlets["/error/id_assertion_endpoint.php"] =
      base::BindRepeating([](const HttpRequest& request)
                              -> std::unique_ptr<HttpResponse> {
        auto response = std::make_unique<BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/json");
        response->set_content(
            R"({"error": {"code": "invalid_request", "url": "https://idp.com/error"}})");
        DCHECK(request.headers.contains("Origin"));
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowOrigin,
            request.headers.at("Origin"));
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowCredentials,
            "true");
        return response;
      });

  idp_server()->SetConfigResponseDetails(config_details);

  std::string expected_error =
      "IdentityCredentialError: Error retrieving a token.";
  EXPECT_EQ(expected_error,
            ExtractJsError(EvalJs(shell(), GetBasicRequestString())));
}

// Verify that an CORSError is returned.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, CorsError) {
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();

  // Points the id assertion endpoint to a servlet.
  config_details.id_assertion_endpoint_url = "/error/id_assertion_endpoint.php";

  // Add a servlet to serve a response for the id assertion endpoint.
  config_details.servlets["/error/id_assertion_endpoint.php"] =
      base::BindRepeating(
          [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_FORBIDDEN);
            response->set_content_type("text/json");
            return response;
          });

  idp_server()->SetConfigResponseDetails(config_details);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Server did not send the correct CORS headers.");
  std::string expected_error =
      "IdentityCredentialError: Error retrieving a token.";
  EXPECT_EQ(expected_error,
            ExtractJsError(EvalJs(shell(), GetBasicRequestString())));
  ASSERT_TRUE(console_observer.Wait());
}

// Verify that auto re-authn can be triggered if the Rp is on the
// approved_clients list and the IdP has third party cookies access.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest,
                       IdpHas3PCAccessAndAddsRPInApprovedClients) {
  // Does not manually select any account. If auto re-authn is not triggered,
  // the test will time out.
  SetTestIdentityRequestDialogController(
      /*dialog_selected_account=*/std::nullopt);

  // The client id `client_id_1` is on the `approved_clients` list defined in
  // content/test/data/fedcm/accounts_endpoint.json so by exempting the IdP from
  // the check, auto re-authn can be triggered and a token can be returned.
  static_cast<InMemoryFederatedPermissionContext*>(
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetFederatedIdentityPermissionContext())
      ->SetHasThirdPartyCookiesAccessForTesting(BaseIdpUrl(), BaseRpUrl());

  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  EXPECT_EQ(std::string(kToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify that using mediation in the wrong place adds log to console.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest,
                       MediationInIdentityCredentialRequestOptions) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }],
              mediation: 'required'
            }
          }));
          return x.token;
        }) ()
    )";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The 'mediation' parameter should be used outside of 'identity' in the "
      "FedCM API call.");
  EXPECT_EQ(std::string(kToken), EvalJs(shell(), script));
  ASSERT_TRUE(console_observer.Wait());
}

// Verify that using mediation in the right place does not add log to console.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest,
                       NoConsoleWarningWithProperMediationCall) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }],
            },
            mediation: 'required'
          }));
          return x.token;
        }) ()
    )";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  EXPECT_EQ(std::string(kToken), EvalJs(shell(), script));
  // TODO(crbug.com/451219310): Remove when FedCM deprecation warnings removed.
  // EXPECT_TRUE(console_observer.messages().empty());
}

using WebIdModeBrowserTest = WebIdBrowserTest;

std::vector<uint8_t> TestSha256(std::string_view data) {
  std::string str = crypto::SHA256HashString(data);
  std::vector<uint8_t> result(str.begin(), str.end());
  return result;
}

class WebIdDelegationBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(features::kFedCm);
    features.push_back(features::kFedCmDelegation);
    scoped_feature_list_.InitWithFeatures(features, {});
  }

 protected:
  void SetVcIssuanceConfigDetails(base::RunLoop* run_loop) {
    IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();
    config_details.vc_issuance_endpoint_url = "/vc_issuance_endpoint.json";

    config_details.servlets["/vc_issuance_endpoint.json"] =
        base::BindLambdaForTesting([&](const HttpRequest& request)
                                       -> std::unique_ptr<HttpResponse> {
          EXPECT_EQ(request.method, HttpMethod::METHOD_POST);
          EXPECT_EQ(request.has_content, true);

          // Assert that the Verifier's origin isn't passed to the issuer.
          EXPECT_TRUE(request.headers.contains("Origin"));
          EXPECT_EQ("null", request.headers.at("Origin"));

          GURL query_url("http://localhost/?" + request.content);

          // Assert that the format type is a supported one by the issuer.
          std::string format;
          EXPECT_TRUE(net::GetValueForKeyInQuery(query_url, "format", &format));
          EXPECT_EQ("vc+sd-jwt", format);

          // Expects a holder_key JWK as a parameter.
          std::string holder_key_json;
          EXPECT_TRUE(net::GetValueForKeyInQuery(query_url, "holder_key",
                                                 &holder_key_json));

          auto holder_key_value = base::JSONReader::ReadDict(
              holder_key_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
          EXPECT_TRUE(holder_key_value);
          auto holder_key = sdjwt::Jwk::From(*holder_key_value);
          EXPECT_TRUE(holder_key);

          std::string account_id;
          EXPECT_TRUE(
              net::GetValueForKeyInQuery(query_url, "account_id", &account_id));

          auto response = std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/json");

          // Issues a real signed SD-JWT with a "sub" and "name" disclosures.
          sdjwt::Disclosure sub;
          sub.name = "sub";
          sub.value = account_id;
          sub.salt = sdjwt::Disclosure::CreateSalt();

          sdjwt::Disclosure name;
          name.name = "name";
          name.value = "Sam";
          name.salt = sdjwt::Disclosure::CreateSalt();

          sdjwt::Header header;
          sdjwt::Payload payload;
          sdjwt::ConfirmationKey confirmation;

          // Binds the holder's public key to the issued JWT.
          confirmation.jwk = *holder_key;
          payload.cnf = confirmation;
          payload._sd = {
              *sub.Digest(base::BindRepeating(TestSha256)),
              *name.Digest(base::BindRepeating(TestSha256)),
          };
          sdjwt::Jwt jwt;
          jwt.header = *header.ToJson();
          jwt.payload = *payload.ToJson();
          jwt.Sign(sdjwt::CreateJwtSigner(private_key_));

          sdjwt::SdJwt sd_jwt;
          sd_jwt.jwt = jwt;
          sd_jwt.disclosures = {*sub.ToJson(), *name.ToJson()};

          response->set_content(R"({"token": ")" + sd_jwt.Serialize() +
                                R"("})");
          return response;
        });

    idp_server()->SetConfigResponseDetails(config_details);
  }

  crypto::keypair::PrivateKey private_key_{
      crypto::keypair::PrivateKey::GenerateEcP256()};
};

IN_PROC_BROWSER_TEST_F(WebIdDelegationBrowserTest, IssueVCs) {
  base::RunLoop run_loop;
  SetVcIssuanceConfigDetails(&run_loop);

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/fedcm/sd_jwt.html")));

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                format: 'vc+sd-jwt',
                fields: ['name'],
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }],
              mode: 'active'
            },
          }));
          return x.token;
        }) ()
    )";

  auto token = EvalJs(shell(), script).ExtractString();

  auto public_key = sdjwt::ExportPublicKey(private_key_);

  EXPECT_TRUE(public_key);

  // Load the token into a string
  ASSERT_TRUE(ExecJs(shell(), "var token = '" + token + "';"));

  // Load the key into an object
  ASSERT_TRUE(ExecJs(shell(), "var key = " + *public_key->Serialize() + ";"));

  // Load the audience into a string
  ASSERT_TRUE(ExecJs(shell(), "var aud = '" + BaseRpUrl() + "';"));

  // Verify the SD-JWT+KB.
  EXPECT_THAT(
      EvalJs(shell(), "main(token, key, aud, '12345')").TakeValue().TakeList(),
      testing::UnorderedElementsAre("Sam"));
}

IN_PROC_BROWSER_TEST_F(WebIdDelegationBrowserTest, ConditionalMediation) {
  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  // Keep a copy of the pointer before the std::move.
  MockIdentityRequestDialogController* controller = mock.get();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  base::RunLoop modal_loop;
  auto configURL = BaseIdpUrl();
  EXPECT_CALL(*controller, ShowAccountsDialog)
      .WillOnce(
          ::testing::WithArg<4>([&modal_loop, &configURL](auto on_selected) {
            std::move(on_selected)
                .Run(GURL(configURL),
                     /*account_id=*/"not_real_account",
                     /*is_sign_in=*/true);

            modal_loop.Quit();

            return true;
          }));

  EXPECT_CALL(*controller, ShowLoadingDialog).WillOnce(Return(true));

  base::RunLoop run_loop;
  SetVcIssuanceConfigDetails(&run_loop);

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/fedcm/sd_jwt.html")));

  std::string script = R"(
    var token = navigator.credentials.get({
      mediation: 'conditional',
      identity: {
        providers: [{
          format: 'vc+sd-jwt',
          fields: ['name'],
          configURL: ')" +
                       configURL + R"(',
          clientId: 'client_id_1',
          nonce: '12345',
        }],
      },
    }).then(({token}) => token)
  )";

  // Await until the accounts are available for autofill.
  EXPECT_CALL(*controller, NotifyAutofillSourceReadyForTesting)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  auto promise = EvalJs(shell(), script, EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  run_loop.Run();

  // Gets the pending conditional request.
  auto* source = webid::AutofillSource::FromPage(
      shell()->web_contents()->GetPrimaryPage());

  EXPECT_TRUE(source != nullptr);

  // Gets all the autofill suggestion and selects the first one.
  auto suggestions = source->GetAutofillSuggestions();
  EXPECT_TRUE(suggestions);
  EXPECT_EQ(suggestions->size(), 1ul);

  auto account = (*suggestions)[0];

  EXPECT_EQ(account->identity_provider->format, blink::mojom::Format::kSdJwt);

  source->NotifyAutofillSuggestionAccepted(
      account->identity_provider->idp_metadata.config_url, account->id,
      /*show_modal=*/true, base::NullCallback());

  // Wait for the user to accept the prompt.
  modal_loop.Run();

  // Verify that the token is correct.
  auto public_key = sdjwt::ExportPublicKey(private_key_);
  EXPECT_TRUE(public_key);

  // Load the key into an object
  ASSERT_TRUE(ExecJs(shell(), "var key = " + *public_key->Serialize() + ";"));

  // Load the audience into a string
  ASSERT_TRUE(ExecJs(shell(), "var aud = '" + BaseRpUrl() + "';"));

  // Verify the SD-JWT+KB.
  EXPECT_THAT(
      EvalJs(shell(), "(async () => main(await token, key, aud, '12345'))()")
          .TakeValue()
          .TakeList(),
      testing::UnorderedElementsAre("Sam"));
}

// Flaky on mac, https://crbug.com/415953689
#if BUILDFLAG(IS_MAC)
#define MAYBE_ConditionalMediationForMediatedRequest \
  DISABLED_ConditionalMediationForMediatedRequest
#else
#define MAYBE_ConditionalMediationForMediatedRequest \
  ConditionalMediationForMediatedRequest
#endif
IN_PROC_BROWSER_TEST_F(WebIdDelegationBrowserTest,
                       MAYBE_ConditionalMediationForMediatedRequest) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  // Keep a copy of the pointer before the std::move.
  MockIdentityRequestDialogController* controller = mock.get();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  auto configURL = BaseIdpUrl();

  base::RunLoop run_loop;

  EXPECT_TRUE(NavigateToURL(shell(), GURL(configURL)));

  std::string script = R"(
    var token = navigator.credentials.get({
      mediation: 'conditional',
      identity: {
        providers: [{
          fields: ['name'],
          configURL: ')" +
                       configURL + R"(',
          clientId: 'client_id_1',
          nonce: '12345',
        }],
      },
    }).then(({token}) => token)
  )";

  // Await until the accounts are available for autofill.
  EXPECT_CALL(*controller, NotifyAutofillSourceReadyForTesting)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  auto promise = EvalJs(shell(), script, EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  run_loop.Run();

  // Gets the pending conditional request.
  auto* source = webid::AutofillSource::FromPage(
      shell()->web_contents()->GetPrimaryPage());

  EXPECT_TRUE(source != nullptr);

  // Gets all the autofill suggestion and selects the first one.
  auto suggestions = source->GetAutofillSuggestions();
  EXPECT_TRUE(suggestions);
  EXPECT_EQ(suggestions->size(), 1ul);

  auto account = (*suggestions)[0];

  // Mediated FedCM has an empty format.
  EXPECT_EQ(account->identity_provider->format, std::nullopt);

  base::RunLoop callback;

  source->NotifyAutofillSuggestionAccepted(
      account->identity_provider->idp_metadata.config_url, account->id,
      /*show_modal=*/false,
      base::BindLambdaForTesting([&callback](bool accepted) {
        EXPECT_TRUE(accepted);
        callback.Quit();
      }));

  // Wait for the identity provider to return a token.
  callback.Run();

  // Assert that the conditional mediation request resolved and that
  // the right token was provided.
  EXPECT_EQ(std::string(kToken), EvalJs(shell(), "token"));
}

class WebIdMetricsBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(features::kFedCmMetricsEndpoint);
  }

 protected:
  enum TestType { kSuccess, kAccountsFailure, kLoginFailure };
  void SetMetricsConfigDetails(base::RunLoop* run_loop, TestType type) {
    IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();
    if (type == kAccountsFailure) {
      config_details.accounts_endpoint_url = "/404";
    }
    if (type == kLoginFailure) {
      // Just load a file that does not call IdentityProvider.close.
      config_details.login_url = "/blue.html";
    }
    config_details.metrics_endpoint_url = "/metrics";
    config_details.servlets["/metrics"] = base::BindRepeating(
        [](WebIdMetricsBrowserTest* test, base::RunLoop* run_loop,
           const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
          EXPECT_EQ(request.method, HttpMethod::METHOD_POST);
          EXPECT_EQ(request.has_content, true);

          if (request.headers.contains("Origin")) {
            test->metrics_request_origin_ = request.headers.at("Origin");
          } else {
            test->metrics_request_origin_.reset();
          }
          auto parameters = base::SplitStringPiece(request.content, "&",
                                                   base::TRIM_WHITESPACE,
                                                   base::SPLIT_WANT_NONEMPTY);
          for (const auto& param : parameters) {
            auto pair = base::SplitStringOnce(param, '=');
            if (pair) {
              test->metrics_parameters_.emplace(*pair);
            }
          }

          auto response = std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/json");
          response->set_content("{}");

          run_loop->Quit();
          return response;
        },
        this, base::Unretained(run_loop));

    idp_server()->SetConfigResponseDetails(config_details);
  }

  std::map<std::string, std::string> metrics_parameters_;
  std::optional<std::string> metrics_request_origin_;
};

IN_PROC_BROWSER_TEST_F(WebIdMetricsBrowserTest, Success) {
  base::RunLoop run_loop;
  SetMetricsConfigDetails(&run_loop, kSuccess);

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345'
              }]
            }
          }));
          return x.token;
        }) ()
    )";

  EXPECT_EQ(std::string("[not a real token]"), EvalJs(shell(), script));
  run_loop.Run();
  ASSERT_TRUE(metrics_request_origin_);
  EXPECT_TRUE(metrics_request_origin_->starts_with("https://rp.example:"));
  EXPECT_EQ("success", metrics_parameters_["outcome"]);
  EXPECT_EQ(1ul, metrics_parameters_.count("time_to_show_ui"));
  EXPECT_EQ(1ul, metrics_parameters_.count("time_to_continue"));
  EXPECT_EQ(1ul, metrics_parameters_.count("time_to_receive_token"));
  EXPECT_EQ(1ul, metrics_parameters_.count("turnaround_time"));
  EXPECT_EQ(0ul, metrics_parameters_.count("error_code"));
  EXPECT_EQ(0ul, metrics_parameters_.count("did_show_ui"));
}

IN_PROC_BROWSER_TEST_F(WebIdMetricsBrowserTest, IdpLoginClosed) {
  // This will be used for the login dialog.
  Shell* modal = CreateBrowser();

  // Mark us as signed out from this IdP.
  GURL url{IdpRootUrl() + "/header/signout"};
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url));

  auto mock = std::make_unique<
      ::testing::NiceMock<MockIdentityRequestDialogController>>();
  // Keep a copy of the pointer before the std::move.
  MockIdentityRequestDialogController* controller = mock.get();
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));
  base::RunLoop modal_dialog_loop;
  content::IdentityRequestDialogController::DismissCallback saved_cb;
  EXPECT_CALL(*controller, ShowModalDialog)
      .WillOnce(WithArgs<0, 2>(
          [&modal, &modal_dialog_loop, &saved_cb](
              const GURL& url,
              content::IdentityRequestDialogController::DismissCallback cb) {
            modal->LoadURL(url);
            saved_cb = std::move(cb);
            modal_dialog_loop.Quit();
            return modal->web_contents();
          }));
  EXPECT_CALL(*controller, ShowLoadingDialog).WillOnce(Return(true));
  EXPECT_CALL(*controller, DidShowUi).WillRepeatedly(Return(true));

  // Now run the actual test.
  base::RunLoop run_loop;
  SetMetricsConfigDetails(&run_loop, kLoginFailure);

  std::string script = R"(
      promise = navigator.credentials.get({
        identity: {
          providers: [{
            configURL: ')" +
                       BaseIdpUrl() + R"(',
            clientId: 'client_id_1',
            nonce: '12345'
          }],
          mode: 'active'
        }
      });
    )";

  // Initiate the FedCM call
  EXPECT_TRUE(ExecJs(shell(), script, EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the popup window to open.
  modal_dialog_loop.Run();
  // Close the dialog and notify the callback.
  modal->Close();
  std::move(saved_cb).Run(
      IdentityRequestDialogController::DismissReason::kOther);

  std::string expected_error = "NetworkError: Error retrieving a token.";
  EXPECT_EQ(expected_error,
            ExtractJsError(EvalJs(shell(), "(async () => await promise)()")));

  // Wait for the metrics endpoint result
  run_loop.Run();

  EXPECT_EQ("null", metrics_request_origin_);
  EXPECT_EQ("failure", metrics_parameters_["outcome"]);
  // In the failure case we should not send timing data.
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_show_ui"));
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_continue"));
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_receive_token"));
  EXPECT_EQ(0ul, metrics_parameters_.count("turnaround_time"));
  EXPECT_EQ("200", metrics_parameters_["error_code"]);
  EXPECT_EQ("true", metrics_parameters_["did_show_ui"]);
}

IN_PROC_BROWSER_TEST_F(WebIdMetricsBrowserTest, Failure) {
  base::RunLoop run_loop;
  SetMetricsConfigDetails(&run_loop, kAccountsFailure);

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            identity: {
              providers: [{
                configURL: ')" +
                       BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345'
              }]
            }
          }));
          return x.token;
        }) ()
    )";

  std::string expected_error = "NetworkError: Error retrieving a token.";
  EXPECT_EQ(expected_error, ExtractJsError(EvalJs(shell(), script)));
  run_loop.Run();
  EXPECT_EQ("null", metrics_request_origin_);
  EXPECT_EQ("failure", metrics_parameters_["outcome"]);
  // In the failure case we should not send timing data.
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_show_ui"));
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_continue"));
  EXPECT_EQ(0ul, metrics_parameters_.count("time_to_receive_token"));
  EXPECT_EQ(0ul, metrics_parameters_.count("turnaround_time"));
  EXPECT_EQ("301", metrics_parameters_["error_code"]);
  EXPECT_EQ("false", metrics_parameters_["did_show_ui"]);
}

// Verify that stored accounts via login.setStatus can be used to complete
// a signin flow with an empty accounts endpoint.
IN_PROC_BROWSER_TEST_F(WebIdLightweightFedcmBrowserTest,
                       IdpSigninTopLevelSetViaJs) {
  GURL configURL = GURL(BaseIdpUrl());
  IdpTestServer::ConfigDetails config_details = BuildValidConfigDetails();
  config_details.accounts_endpoint_url = "";
  idp_server()->SetConfigResponseDetails(config_details);

  EXPECT_TRUE(NavigateToURL(shell(), configURL));

  EXPECT_FALSE(sharing_context()
                   ->GetIdpSigninStatus(url::Origin::Create(configURL))
                   .has_value());
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), configURL));
  base::RunLoop run_loop;

  sharing_context()->SetIdpStatusClosureForTesting(run_loop.QuitClosure());

  static constexpr char script[] = R"(
    (async () => {
      await navigator.login.setStatus("logged-in", {accounts: [
        {id: "12345", name: "User", email: "user@idp.example"}
      ]});
      return true;
    })()
  )";

  EXPECT_EQ(true, EvalJs(shell(), script));
  run_loop.Run();

  std::optional<bool> value =
      sharing_context()->GetIdpSigninStatus(url::Origin::Create(configURL));
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(*value);
  base::Value::List accounts =
      sharing_context()->GetAccounts(url::Origin::Create(configURL));
  ASSERT_EQ(1U, accounts.size());
  EXPECT_EQ("12345", *accounts[0].GetDict().FindString("id"));
  EXPECT_EQ("User", *accounts[0].GetDict().FindString("name"));
  EXPECT_EQ("user@idp.example", *accounts[0].GetDict().FindString("email"));

  EXPECT_TRUE(NavigateToURL(
      shell(), https_server().GetURL(kRpHostName, "/title1.html")));

  SetTestIdentityRequestDialogController("12345");
  EXPECT_EQ(std::string(kToken), EvalJs(shell(), GetBasicRequestString()));
}

}  // namespace content

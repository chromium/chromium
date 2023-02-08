// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_federated_permission_context.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::EmbeddedTestServer;
using net::HttpStatusCode;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace content {

namespace {

constexpr char kRpHostName[] = "rp.example";

// Use localhost for IDP so that the well-known file can be fetched from the
// test server's custom port. IdpNetworkRequestManager::ComputeWellKnownUrl()
// does not enforce a specific port if the IDP is localhost.
constexpr char kIdpOrigin[] = "https://127.0.0.1";

constexpr char kExpectedConfigPath[] = "/fedcm.json";
constexpr char kExpectedWellKnownPath[] = "/.well-known/web-identity";
constexpr char kTestContentType[] = "application/json";
constexpr char kIdpForbiddenHeader[] = "Sec-FedCM-CSRF";

// TODO(crbug.com/1381501): Replace these with a standardized header once
// we collected enough metrics.
static constexpr char kGoogleSigninHeader[] = "Google-Accounts-SignIn";
static constexpr char kGoogleSignoutHeader[] = "Google-Accounts-SignOut";
static constexpr char kGoogleHeaderValue[] =
    "email=\"foo@example.com\", sessionindex=0, obfuscatedid=123";

// Token value in //content/test/data/id_assertion_endpoint.json
constexpr char kToken[] = "[not a real token]";

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
  };

  IdpTestServer() = default;
  ~IdpTestServer() = default;

  IdpTestServer(const IdpTestServer&) = delete;
  IdpTestServer& operator=(const IdpTestServer&) = delete;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // RP files are fetched from the /test base directory. Assume anything
    // to other paths is directed to the IdP.
    if (request.relative_url.rfind("/test", 0) == 0)
      return nullptr;

    if (request.relative_url.rfind("/header/", 0) == 0)
      return BuildIdpHeaderResponse(request);

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

    return nullptr;
  }

  std::unique_ptr<HttpResponse> BuildIdpHeaderResponse(
      const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url.find("/header/gsignin") != std::string::npos) {
      response->AddCustomHeader(kGoogleSigninHeader, kGoogleHeaderValue);
    } else if (request.relative_url.find("/header/gsignout") !=
               std::string::npos) {
      response->AddCustomHeader(kGoogleSignoutHeader, kGoogleHeaderValue);
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
    std::string content = ConvertToJsonDictionary(
        {{"accounts_endpoint", details.accounts_endpoint_url},
         {"client_metadata_endpoint", details.client_metadata_endpoint_url},
         {"id_assertion_endpoint", details.id_assertion_endpoint_url}});
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
      const std::map<std::string, std::string>& data) {
    std::string out = "{";
    for (auto it : data) {
      out += "\"" + it.first + "\":\"" + it.second + "\",";
    }
    if (!out.empty()) {
      out[out.length() - 1] = '}';
    }
    return out;
  }

  ConfigDetails config_details_;
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
    https_server().SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server().RegisterRequestHandler(base::BindRepeating(
        &IdpTestServer::HandleRequest, base::Unretained(idp_server_.get())));
    ASSERT_TRUE(https_server().Start());

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server().GetURL(kRpHostName, "/title1.html")));

    test_browser_client_ = std::make_unique<WebIdTestContentBrowserClient>();
    SetTestIdentityRequestDialogController("not_real_account");
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

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  std::string BaseIdpUrl() {
    return std::string(kIdpOrigin) + ":" +
           base::NumberToString(https_server().port()) + "/fedcm.json";
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
    return {net::HTTP_OK, kTestContentType, accounts_endpoint_url,
            client_metadata_endpoint_url, id_assertion_endpoint_url};
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  void SetTestIdentityRequestDialogController(
      const std::string& dialog_selected_account) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        dialog_selected_account);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<IdpTestServer> idp_server_;
  std::unique_ptr<WebIdTestContentBrowserClient> test_browser_client_;
};

class WebIdIdpSigninStatusBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFedCm,
        {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ShellFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<ShellFederatedPermissionContext*>(
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
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ShellFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<ShellFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

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

  std::string expected_error =
      "a JavaScript error: \"NetworkError: Error "
      "retrieving a token.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), script).error);
}

// Verify that an IdP can register itself.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RegisterIdP) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

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
}

// Verify that the RP cannot register the IdP across origins.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, RpCantRegisterIdP) {
  std::string script = R"(
        (async () => {
          return await IdentityProvider.register(')" +
                       BaseIdpUrl() + R"(');
        }) ()
    )";

  // TODO(crbug.com/1406698): make this error message more
  // developer friendly, since this was a call error rather
  // than a user declining the permission error.
  std::string expected_error =
      "a JavaScript error: \"NotAllowedError: "
      "User declined the permission to register the Identity Provider.\"\n";

  EXPECT_EQ(expected_error, EvalJs(shell(), script).error);
}

// Verify that an IdP can unregister itself.
IN_PROC_BROWSER_TEST_F(WebIdIdPRegistryBrowserTest, UnregisterIdP) {
  GURL configURL = GURL(BaseIdpUrl());
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

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
}

// Verify that IDP sign-in headers work.
IN_PROC_BROWSER_TEST_F(WebIdIdpSigninStatusBrowserTest, IdpSigninToplevel) {
  GURL url = https_server().GetURL(kRpHostName, "/header/gsignin");
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
  GURL url = https_server().GetURL(kRpHostName, "/header/gsignout");
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
      var resp = await fetch('/header/gsign%s');
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

}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/test/mock_digital_credential_provider.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::base::test::IsJson;
using net::EmbeddedTestServer;
using net::HttpStatusCode;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::WithArg;
using ::testing::WithArgs;

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
static constexpr char kSetLoginHeader[] = "Set-Login";
static constexpr char kLoggedInHeaderValue[] = "logged-in";
static constexpr char kLoggedOutHeaderValue[] = "logged-out";

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
    std::string login_url;
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
    std::string content = ConvertToJsonDictionary(
        {{"accounts_endpoint", details.accounts_endpoint_url},
         {"client_metadata_endpoint", details.client_metadata_endpoint_url},
         {"id_assertion_endpoint", details.id_assertion_endpoint_url},
         {"login_url", details.login_url}});
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

class TestFederatedIdentityModalDialogViewDelegate
    : public NiceMock<MockModalDialogViewDelegate> {
 public:
  base::OnceClosure closure_;
  bool closed_{false};

  void SetClosure(base::OnceClosure closure) { closure_ = std::move(closure); }

  void NotifyClose() override {
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
    https_server().SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server().RegisterRequestHandler(base::BindRepeating(
        &IdpTestServer::HandleRequest, base::Unretained(idp_server_.get())));
    ASSERT_TRUE(https_server().Start());

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server().GetURL(kRpHostName, "/title1.html")));

    test_browser_client_ = std::make_unique<WebIdTestContentBrowserClient>();
    SetTestIdentityRequestDialogController("not_real_account");
    SetTestDigitalCredentialProvider();
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
    std::string login_url = "/fedcm/login.html";
    return {net::HTTP_OK,
            kTestContentType,
            accounts_endpoint_url,
            client_metadata_endpoint_url,
            id_assertion_endpoint_url,
            login_url};
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  void SetTestIdentityRequestDialogController(
      const std::string& dialog_selected_account) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        dialog_selected_account);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

  void SetTestDigitalCredentialProvider() {
    auto provider = std::make_unique<MockDigitalCredentialProvider>();
    test_browser_client_->SetDigitalCredentialProvider(std::move(provider));
  }

  void SetTestModalDialogViewDelegate() {
    test_modal_dialog_view_delegate_ =
        std::make_unique<TestFederatedIdentityModalDialogViewDelegate>();
    test_browser_client_->SetIdentityRegistry(
        shell()->web_contents(), test_modal_dialog_view_delegate_->GetWeakPtr(),
        url::Origin::Create(GURL(BaseIdpUrl())));
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
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFedCmIdpSigninStatusEnabled);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ShellFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<ShellFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

class WebIdIdpSigninStatusForFetchKeepAliveBrowserTest
    : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(
        {features::kFedCmIdpSigninStatusEnabled,
         blink::features::kKeepAliveInBrowserMigration},
        {});
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

class WebIdAuthzBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(features::kFedCmAuthz);
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
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

#if BUILDFLAG(IS_ANDROID)
  // On Android, IdentityProvider.close() should invoke CloseModalDialog() on
  // the dialog controller.
  auto controller = std::make_unique<MockIdentityRequestDialogController>();
  base::RunLoop run_loop;
  EXPECT_CALL(*controller, CloseModalDialog).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  test_browser_client_->SetIdentityRequestDialogController(
      std::move(controller));

  // Run the script.
  EXPECT_EQ(true, EvalJs(shell(), script));
  run_loop.Run();
#else
  // On desktop, IdentityProvider.close() should invoke NotifyClose() on the
  // delegate set on the identity registry. Check that modal dialog is not
  // closed.
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
#endif  // BUILDFLAG(IS_ANDROID)
}

class WebIdDigitalCredentialsBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kWebIdentityDigitalCredentials);
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ShellFederatedPermissionContext* sharing_context() {
    BrowserContext* context = shell()->web_contents()->GetBrowserContext();
    return static_cast<ShellFederatedPermissionContext*>(
        context->GetFederatedIdentityPermissionContext());
  }
};

// Test that a Verifiable Credential can be requested via the JS API.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       RequestDigitalCredentials) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalCredentialProvider* digital_credential_provider =
      static_cast<MockDigitalCredentialProvider*>(
          test_browser_client_->GetDigitalCredentialProviderForTests());

  const char request[] = R"(
  {
   "providers": [ {
      "params": {
         "extraParamAsNeededByDigitalCredentials": "true",
         "nonce": "1234",
         "readerPublicKey": "test_reader_public_key"
      },
      "responseFormat": [ "mdoc" ],
      "selector": {
         "fields": [ {
            "equals": "org.iso.18013.5.1.mDL",
            "name": "doctype"
         }, {
            "name": "org.iso.18013.5.1.family_name"
         }, {
            "name": "org.iso.18013.5.1.portrait"
         } ]
      }
   } ]
  }
  )";

  EXPECT_CALL(*digital_credential_provider,
              RequestDigitalCredential(_, _, IsJson(request), _))
      .WillOnce(WithArg<3>(
          [](DigitalCredentialProvider::DigitalCredentialCallback callback) {
            std::move(callback).Run("test-mdoc");
          }));

  std::string script = R"(
        (async () => {
          const {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                holder: {
                  selector: {
                    format: ['mdoc'],
                    doctype: 'org.iso.18013.5.1.mDL',
                    fields: [
                      'org.iso.18013.5.1.family_name',
                      'org.iso.18013.5.1.portrait',
                    ]
                  },
                  params: {
                    nonce: '1234',
                    readerPublicKey: 'test_reader_public_key',
                    extraParamAsNeededByDigitalCredentials: true,
                  },
                },
              }],
            },
          });
          return token;
        }) ()
    )";

  EXPECT_EQ("test-mdoc", EvalJs(shell(), script));
}

// Test that when there's a pending mdoc request, a second `get` call should be
// rejected.
IN_PROC_BROWSER_TEST_F(WebIdDigitalCredentialsBrowserTest,
                       OnlyOneInFlightDigitalCredentialRequestIsAllowed) {
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());
  MockDigitalCredentialProvider* digital_credential_provider =
      static_cast<MockDigitalCredentialProvider*>(
          test_browser_client_->GetDigitalCredentialProviderForTests());

  std::string script = R"(
        (async () => {
          const {token} = await navigator.credentials.get({
            identity: {
              providers: [{
                holder: {
                  selector: {
                    format: ["mdoc"],
                    doctype: 'org.iso.18013.5.1.mDL',
                    fields: [
                      'org.iso.18013.5.1.family_name',
                      'org.iso.18013.5.1.portrait',
                    ],
                  },
                  params: {
                    nonce: '1234',
                    readerPublicKey: 'test_reader_public_key',
                    extraParamAsNeededByDigitalCredentials: true,
                  },
                },
              }],
            },
          });
          return token;
        }) ()
    )";

  EXPECT_CALL(*digital_credential_provider,
              RequestDigitalCredential(_, _, _, _))
      .WillOnce(WithArg<3>(
          [&](DigitalCredentialProvider::DigitalCredentialCallback callback) {
            EXPECT_EQ(
                "a JavaScript error: \"AbortError: Only one "
                "navigator.credentials.get request may be outstanding at one "
                "time.\"\n",
                EvalJs(shell(), script).error);
            std::move(callback).Run("test-mdoc");
          }));

  EXPECT_EQ("test-mdoc", EvalJs(shell(), script));
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
            // Asserts that the scope, response_type and params parameters
            // were passed correctly to the id assertion endpoint.
            content += "scope=name+email+picture&";
            content += "response_type=id_token+code&";
            content += "%3F+gets+://=%26+escaped+!&";
            content += "foo=bar&";
            content += "hello=world";

            EXPECT_EQ(request.content, content);

            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/json");
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
                scope: [
                  'name',
                  'email',
                  'picture',
                ],
                responseType: [
                  'id_token',
                  'code'
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
            content += "scope=calendar.readonly";

            EXPECT_EQ(request.content, content);

            auto response = std::make_unique<BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/json");
            // scope=calendar.readonly was requested, so need to
            // return a continuation url instead of a token.
            auto body = R"({"continue_on": ")" + url + R"("})";
            response->set_content(body);
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
  EXPECT_CALL(*controller, ShowAccountsDialog(_, _, _, _, _, _, _))
      .WillOnce(::testing::WithArg<5>([&config_url](auto on_selected) {
        std::move(on_selected)
            .Run(config_url,
                 /* account_id=*/"not_real_account",
                 /* is_sign_in= */ true);
      }));

  base::RunLoop run_loop;
  EXPECT_CALL(*controller, ShowModalDialog(_, _))
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
                scope: [
                  'calendar.readonly'
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

  // Resolve the hanging token request by notifying the registry.
  EXPECT_TRUE(content::ExecJs(
      modal, R"(IdentityProvider.resolve(')" + token + R"('))"));

  // Finally, wait for the promise to resolve and compare its result
  // to the expected token that was provided in the modal dialog.
  EXPECT_EQ(token, EvalJs(shell(), "result"));
}

class WebIdErrorBrowserTest : public WebIdBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRef> features;
    features.push_back(features::kFedCmError);
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

// Verify that an IdentityCredentialError exception is returned.
IN_PROC_BROWSER_TEST_F(WebIdErrorBrowserTest, IdentityCredentialError) {
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
        return response;
      });

  idp_server()->SetConfigResponseDetails(config_details);

  std::string expected_error =
      "a JavaScript error: \"IdentityCredentialError: Error "
      "retrieving a token.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), GetBasicRequestString()).error);
}

}  // namespace content

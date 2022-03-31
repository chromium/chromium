// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webid/test/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
constexpr char kIdpOrigin[] = "https://idp.example.org";
constexpr char kExpectedManifestPath[] = "/fedcm.json";
constexpr char kTestContentType[] = "application/json";
constexpr char kIdpForbiddenHeader[] = "Sec-FedCM-CSRF";

// Id token value in //content/test/data/id_token_endpoint.json
constexpr char kIdToken[] = "[not a real token]";

// This class implements the IdP logic, and responds to requests sent to the
// test HTTP server.
class IdpTestServer {
 public:
  struct ManifestDetails {
    HttpStatusCode status_code;
    std::string content_type;
    std::string accounts_endpoint_url;
    std::string client_metadata_endpoint_url;
    std::string id_token_endpoint_url;
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

    if (request.all_headers.find(kIdpForbiddenHeader) != std::string::npos) {
      EXPECT_EQ(request.headers.at(kIdpForbiddenHeader), "?1");
    }

    auto response = std::make_unique<BasicHttpResponse>();
    if (IsManifestRequest(request)) {
      BuildManifestResponseFromDetails(*response.get(), manifest_details_);
      return response;
    }

    return nullptr;
  }

  void SetManifestResponseDetails(ManifestDetails details) {
    manifest_details_ = details;
  }

 private:
  bool IsManifestRequest(const HttpRequest& request) {
    if (request.method == HttpMethod::METHOD_GET &&
        request.relative_url == kExpectedManifestPath) {
      return true;
    }
    return false;
  }

  void BuildManifestResponseFromDetails(BasicHttpResponse& response,
                                        const ManifestDetails& details) {
    std::string content = ConvertToJsonDictionary(
        {{"accounts_endpoint", details.accounts_endpoint_url},
         {"client_metadata_endpoint", details.client_metadata_endpoint_url},
         {"id_token_endpoint", details.id_token_endpoint_url}});
    response.set_code(details.status_code);
    response.set_content(content);
    response.set_content_type(details.content_type);
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

  ManifestDetails manifest_details_;
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
    old_client_ = SetBrowserClientForTesting(test_browser_client_.get());
  }

  void TearDown() override {
    CHECK_EQ(SetBrowserClientForTesting(old_client_),
             test_browser_client_.get());
    ContentBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::Feature> features;

    // kSplitCacheByNetworkIsolationKey feature is needed to verify
    // that the network shard for fetching the fedcm manifest file is different
    // from that used for other IdP transactions, to prevent data leakage.
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kFedCm);
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  std::string BaseIdpUrl() {
    return std::string(kIdpOrigin) + ":" +
           base::NumberToString(https_server().port());
  }

  std::string GetBasicRequestString() {
    return R"(
        (async () => {
          var x = (await navigator.credentials.get({
            federated: {
              providers: [{
                url: ')" +
           BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
              }]
            }
          }));
          return (await x.login({nonce: '12345'})).idToken;
        }) ()
    )";
  }

  IdpTestServer::ManifestDetails BuildValidManifestDetails() {
    std::string accounts_endpoint_url = "/fedcm/accounts_endpoint.json";
    std::string client_metadata_endpoint_url =
        "/fedcm/client_metadata_endpoint.json";
    std::string id_token_endpoint_url = "/fedcm/id_token_endpoint.json";
    return {net::HTTP_OK, kTestContentType, accounts_endpoint_url,
            client_metadata_endpoint_url, id_token_endpoint_url};
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  void SetTestIdentityRequestDialogController(
      const std::string& dialog_selected_account) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        dialog_selected_account);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<IdpTestServer> idp_server_;
  std::unique_ptr<WebIdTestContentBrowserClient> test_browser_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

// Verify a standard login flow with IdP sign-in page.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FullLoginFlow) {
  idp_server()->SetManifestResponseDetails(BuildValidManifestDetails());

  EXPECT_EQ(std::string(kIdToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify full login flow where the IdP uses absolute rather than relative
// URLs.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, AbsoluteURLs) {
  IdpTestServer::ManifestDetails manifest_details = BuildValidManifestDetails();
  manifest_details.accounts_endpoint_url = "/fedcm/accounts_endpoint.json";
  manifest_details.client_metadata_endpoint_url =
      "/fedcm/client_metadata_endpoint.json";
  manifest_details.id_token_endpoint_url = "/fedcm/id_token_endpoint.json";

  idp_server()->SetManifestResponseDetails(manifest_details);

  EXPECT_EQ(std::string(kIdToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify an attempt to invoke FedCM with an insecure IDP path fails.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FailsOnHTTP) {
  idp_server()->SetManifestResponseDetails(BuildValidManifestDetails());

  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            federated: {
              providers: [{
                url: 'http://idp.example)" +
                       base::NumberToString(https_server().port()) + R"(',
                clientId: 'client_id_1',
              }]
            }
          }));
          return await x.login({nonce: '12345'});
        }) ()
    )";

  std::string expected_error =
      "a JavaScript error: \"NetworkError: Error "
      "retrieving an id token.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), script).error);
}

}  // namespace content

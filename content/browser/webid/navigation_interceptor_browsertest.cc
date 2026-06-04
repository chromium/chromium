// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithoutArgs;

namespace content {

namespace {

class TestIdP : public net::EmbeddedTestServer {
 public:
  TestIdP() : net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS) {
    RegisterRequestHandler(
        base::BindRepeating(&TestIdP::HandleRequest, base::Unretained(this)));
  }

  void SetUp() {
    // Standard FedCM IdP endpoints.
    AddJsonResponse("/fedcm.json", R"({
      "accounts_endpoint": "/accounts.json",
      "client_metadata_endpoint": "/metadata.json",
      "id_assertion_endpoint": "/assertion.json",
      "login_url": "/login.html"
    })");

    AddJsonResponse("/.well-known/web-identity",
                    R"({"provider_urls": ["/fedcm.json"]})");

    AddJsonResponse("/accounts.json", R"({
      "accounts": [{
        "id": "1234",
        "given_name": "John",
        "name": "John Doe",
        "email": "john@doe.com",
        "picture": "https://example.com/picture.jpg"
      }]
    })");

    AddJsonResponse("/metadata.json", R"({
      "privacy_policy_url": "https://idp.example/privacy.html",
      "terms_of_service_url": "https://idp.example/terms.html"
    })");

    // Standard interception endpoint.
    // config_url must be an absolute URL for FedCM to accept the interception
    // header.
    AddResponse("/oauth", "interception response", "text/plain",
                {{"FedCM-Intercept-Navigation",
                  base::StringPrintf("config_url=\"%s\", client_id=\"1234\"",
                                     GetURL("/fedcm.json").spec().c_str())}});

    AddResponse("/oauth-initiate", "interception response", "text/plain",
                {{"Federation-Initiate-Request",
                  base::StringPrintf("config_url=\"%s\", client_id=\"1234\"",
                                     GetURL("/fedcm.json").spec().c_str())}});
  }

 private:
  void AddHtmlResponse(
      const std::string& path,
      const std::string& content,
      const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    AddResponse(path, content, "text/html", headers);
  }

  void AddJsonResponse(
      const std::string& path,
      const std::string& content,
      const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    AddResponse(path, content, "application/json", headers);
  }

  void AddResponse(
      const std::string& path,
      const std::string& content,
      const std::string& content_type,
      const std::vector<std::pair<std::string, std::string>>& headers) {
    test_handlers_[path] = base::BindLambdaForTesting(
        [content, content_type, headers](const HttpRequest&) {
          auto response = std::make_unique<BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content(content);
          response->set_content_type(content_type);
          response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
          for (const auto& header : headers) {
            response->AddCustomHeader(header.first, header.second);
          }
          return std::unique_ptr<HttpResponse>(std::move(response));
        });
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    auto it = test_handlers_.find(request.relative_url);
    if (it != test_handlers_.end()) {
      return it->second.Run(request);
    }
    return nullptr;
  }

  using HandlerMap =
      std::map<std::string,
               base::RepeatingCallback<std::unique_ptr<HttpResponse>(
                   const HttpRequest&)>>;
  HandlerMap test_handlers_;
};

class NavigationInterceptorBrowserTest : public ContentBrowserTest {
 public:
  NavigationInterceptorBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kFedCm, features::kFedCmNavigationInterception}, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(idp_server_.Start());
    idp_server_.SetUp();

    // Use the built-in server for the RP.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&NavigationInterceptorBrowserTest::HandleRpRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    test_browser_client_ =
        std::make_unique<webid::WebIdTestContentBrowserClient>();
  }

  void TearDownOnMainThread() override {
    test_browser_client_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetRpPageContent(const std::string& content) {
    rp_page_content_ = content;
  }

  std::unique_ptr<HttpResponse> HandleRpRequest(const HttpRequest& request) {
    if (request.relative_url != "/rp") {
      return nullptr;
    }
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(rp_page_content_);
    return response;
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestIdP idp_server_;
  std::unique_ptr<webid::WebIdTestContentBrowserClient> test_browser_client_;
  std::string rp_page_content_;
};

// Scenario 1: target="_blank" (default noopener)
// FIXED: Now avoids interception and succeeds in navigating to the IDP.
IN_PROC_BROWSER_TEST_F(NavigationInterceptorBrowserTest, TargetBlank) {
  SetRpPageContent(
      base::StringPrintf(R"(
    <!-- RP page with a link that opens in a new tab without an opener. -->
    <html>
      <body>
        <a id='link' href='%s' target='_blank'>Click</a>
      </body>
    </html>
  )",
                         idp_server_.GetURL("/oauth").spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/rp")));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.getElementById('link').click();"));

  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  WaitForLoadStop(new_contents);

  EXPECT_EQ(new_contents->GetLastCommittedURL(), idp_server_.GetURL("/oauth"));
}

// Scenario 2: target="_blank" with rel="opener"
// Succeeds in intercepting because the initial empty document inherits the
// opener's origin.
IN_PROC_BROWSER_TEST_F(NavigationInterceptorBrowserTest,
                       TargetBlankWithOpener) {
  base::RunLoop run_loop;
  auto mock = std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  ON_CALL(*mock, ShowLoadingDialog).WillByDefault(Return(true));
  EXPECT_CALL(*mock, ShowAccountsDialog).WillOnce(WithoutArgs([&run_loop]() {
    run_loop.Quit();
    return true;
  }));
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  SetRpPageContent(
      base::StringPrintf(R"(
    <!-- RP page with a link that opens in a new tab with an explicit -->
    <!-- opener. -->
    <html>
      <body>
        <a id='link' href='%s' target='_blank' rel='opener'>Click</a>
      </body>
    </html>
  )",
                         idp_server_.GetURL("/oauth").spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/rp")));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.getElementById('link').click();"));

  Shell* new_shell = new_shell_observer.GetShell();
  run_loop.Run();
  new_shell->web_contents()->Close();
}

// Scenario 3: Normal same-window navigation.
// Succeeds in intercepting because it uses the existing document's origin.
IN_PROC_BROWSER_TEST_F(NavigationInterceptorBrowserTest, NormalNavigation) {
  base::RunLoop run_loop;
  auto mock = std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  ON_CALL(*mock, ShowLoadingDialog).WillByDefault(Return(true));
  EXPECT_CALL(*mock, ShowAccountsDialog).WillOnce(WithoutArgs([&run_loop]() {
    run_loop.Quit();
    return true;
  }));
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  SetRpPageContent(
      base::StringPrintf(R"(
    <!-- RP page with a normal same-window link. -->
    <html>
      <body>
        <a id='link' href='%s'>Click</a>
      </body>
    </html>
  )",
                         idp_server_.GetURL("/oauth").spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/rp")));
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.getElementById('link').click();"));

  run_loop.Run();
}

// Scenario 4: window.open (JS Popup)
// Succeeds in intercepting because it has an opener and inherits its origin.
IN_PROC_BROWSER_TEST_F(NavigationInterceptorBrowserTest, Popup) {
  base::RunLoop run_loop;
  auto mock = std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  ON_CALL(*mock, ShowLoadingDialog).WillByDefault(Return(true));
  EXPECT_CALL(*mock, ShowAccountsDialog).WillOnce(WithoutArgs([&run_loop]() {
    run_loop.Quit();
    return true;
  }));
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  SetRpPageContent(
      base::StringPrintf(R"(
    <!-- RP page with a link that triggers a JS popup. -->
    <html>
      <body>
        <a id='link' href='#'
           onclick="window.open('%s', '_blank', 'popup=1');">Click</a>
      </body>
    </html>
  )",
                         idp_server_.GetURL("/oauth").spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/rp")));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.getElementById('link').click();"));

  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  run_loop.Run();
  new_shell->web_contents()->Close();
}

// Scenario 5: Normal same-window navigation with Federation-Initiate-Request.
IN_PROC_BROWSER_TEST_F(NavigationInterceptorBrowserTest,
                       NormalNavigationWithFederationInitiateRequest) {
  base::RunLoop run_loop;
  auto mock = std::make_unique<NiceMock<MockIdentityRequestDialogController>>();
  ON_CALL(*mock, ShowLoadingDialog).WillByDefault(Return(true));
  EXPECT_CALL(*mock, ShowAccountsDialog).WillOnce(WithoutArgs([&run_loop]() {
    run_loop.Quit();
    return true;
  }));
  test_browser_client_->SetIdentityRequestDialogController(std::move(mock));

  SetRpPageContent(
      base::StringPrintf(R"(
    <!-- RP page with a normal same-window link. -->
    <html>
      <body>
        <a id='link' href='%s'>Click</a>
      </body>
    </html>
  )",
                         idp_server_.GetURL("/oauth-initiate").spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/rp")));
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.getElementById('link').click();"));

  run_loop.Run();
}

}  // namespace

}  // namespace content

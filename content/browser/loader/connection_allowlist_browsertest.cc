// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/preloading/prefetch/prefetch_key.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/request_service.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/browser/webid/webid_utils.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "services/network/public/cpp/connection_allowlist_metrics.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

using blink::mojom::EmailVerificationRequestResult;
using blink::mojom::FederatedRequestResult;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::ResultOf;

constexpr char kSameOriginAllowlistedPage[] = "/response_origin.html";
constexpr char kCrossOriginAllowlistedPage[] =
    "/response_and_cross_origin.html";

// These variables are for FedCM API and Email Verification Protocol tests.
constexpr char kFedCmScript[] = R"(navigator.credentials.get({
  identity: {
    providers: [{
      configURL: $1,
      clientId: '1234'
    }]
  }
}).then(
  token => 'success',
  error => error.name + ': ' + error.message
))";
constexpr char kFedCmDisconnectScript[] = R"(IdentityCredential.disconnect({
  configURL: $1,
  clientId: '1234',
  accountHint: '1234'
}).then(
  () => 'success',
  error => error.name + ': ' + error.message
))";
constexpr char kIdpHost[] = "b.test";
constexpr char kConfigPath[] = "/fedcm.json";
constexpr char kWellKnownPath[] = "/.well-known/web-identity";
constexpr char kTokenPath[] = "/token";
constexpr char kAvatarPath[] = "/avatar.png";
constexpr char kLoginPath[] = "/login";
constexpr char kAccountPath[] = "/accounts";
constexpr char kClientMetadataPath[] = "/client_metadata";
constexpr char kDisconnectPath[] = "/disconnect";
constexpr char kEmailVerificationWellKnownPath[] =
    "/.well-known/email-verification";
constexpr char kJwksPath[] = "/jwks";
constexpr char kDnsPath[] = "/dns";
constexpr char kAccountID[] = "1234";
constexpr char kImageBytes[] = "01010101001010101010101010101";
constexpr char kConfigFileStr[] = "config file";
constexpr char kWellKnownFileStr[] = "well-known file";
constexpr char kTokenStr[] = "id assertion endpoint";
constexpr char kAccountStr[] = "accounts endpoint";

Matcher<WebContentsConsoleObserver::Message> HasConsoleMessage(
    const std::string& expected_substr) {
  return Field(
      &WebContentsConsoleObserver::Message::message,
      ResultOf([](const std::u16string& s) { return base::UTF16ToUTF8(s); },
               HasSubstr(expected_substr)));
}

bool IsPrerender2FallbackPrefetchSpecRulesEnabled() {
  return base::FeatureList::IsEnabled(
      features::kPrerender2FallbackPrefetchSpecRules);
}

bool MatchesNetworkRequest(const std::string& expected_url,
                           const std::string& expected_method,
                           const base::DictValue& params) {
  const std::string* url = params.FindStringByDottedPath("request.url");
  const std::string* method = params.FindStringByDottedPath("request.method");
  return url && *url == expected_url && method && *method == expected_method;
}

struct ResponseEntry {
  std::string content;
  absl::flat_hash_map<std::string, std::string> headers;
};

class ConnectionAllowlistContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  MOCK_METHOD(void,
              LogWebFeatureForCurrentPage,
              (content::RenderFrameHost*, blink::mojom::WebFeature),
              (override));
};

// TODO(crbug.com/486121443): Once the test flakiness due to the issue in
// WebPrescientNetworkingImpl is resolved, add a test covering preconnect from
// the link header response.
class ConnectionAllowlistTest : public ContentBrowserTest {
 public:
  ConnectionAllowlistTest()
      : prerender_helper_(
            base::BindRepeating(&ConnectionAllowlistTest::GetWebContents,
                                base::Unretained(this))) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::kConnectionAllowlists},
        /*disabled_features=*/{});
  }
  ~ConnectionAllowlistTest() override { content_browser_client_.reset(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    prerender_helper_.RegisterServerRequestMonitor(
        embedded_https_test_server());

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(&embedded_https_test_server());
    net::test_server::InstallDefaultWebSocketHandlers(
        &embedded_https_test_server());
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &ConnectionAllowlistTest::ServeResponses, base::Unretained(this)));

    content_browser_client_ =
        std::make_unique<ConnectionAllowlistContentBrowserClient>();
  }

  void RegisterResponse(const std::string& relative_url,
                        ResponseEntry&& entry) {
    response_map_[relative_url] = std::move(entry);
  }

  void UnregisterResponse(const std::string& relative_url) {
    response_map_.erase(relative_url);
  }

  WebContents* GetWebContents() { return shell()->web_contents(); }

  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

  bool WaitForSpeculationRulesPrefetch(const GURL& url,
                                       PrefetchContainer::LoadState load_state,
                                       PrefetchStatus prefetch_status) {
    RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
    PrefetchService* prefetch_service =
        PrefetchService::GetFromFrameTreeNodeId(rfh->GetFrameTreeNodeId());
    if (!prefetch_service) {
      return false;
    }

    PrefetchKey key(
        static_cast<const RenderFrameHostImpl*>(rfh)->GetDocumentToken(), url);

    return base::test::RunUntil([&]() {
      base::WeakPtr<PrefetchContainer> prefetch_container =
          prefetch_service->MatchUrl(key);

      return prefetch_container &&
             prefetch_container->GetLoadState() == load_state &&
             prefetch_container->GetPrefetchStatus() == prefetch_status;
    });
  }

  void ExpectRequestsSucceeded(URLLoaderMonitor& monitor,
                               const std::vector<GURL>& urls) {
    for (const GURL& url : urls) {
      EXPECT_EQ(monitor.WaitForRequestCompletion(url).error_code, net::OK);
    }
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    auto it = response_map_.find(request.relative_url);
    if (it == response_map_.end()) {
      it = response_map_.find(request.GetURL().path());
    }
    if (it != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(it->second.content);
      for (const auto& [key, value] : it->second.headers) {
        if (key == "Content-Type") {
          response->set_content_type(value);
        } else {
          response->AddCustomHeader(key, value);
        }
      }

      // Adding the required response headers for CORS requests.
      auto origin_it = request.headers.find("origin");
      if (origin_it != request.headers.end()) {
        auto mode_it = request.headers.find("sec-fetch-mode");
        if (mode_it != request.headers.end() && mode_it->second == "cors") {
          response->AddCustomHeader("Access-Control-Allow-Origin",
                                    origin_it->second);
          response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
        }
      }

      return response;
    }

    return nullptr;
  }

  test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  absl::flat_hash_map<std::string, ResponseEntry> response_map_;
  std::unique_ptr<ConnectionAllowlistContentBrowserClient>
      content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkPrefetch) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(R"(
            var allowed_link = document.createElement('link');
            allowed_link.href = $1;
            allowed_link.rel = 'prefetch';

            var denied_link = document.createElement('link');
            denied_link.href = $2;
            denied_link.rel = 'prefetch';

            document.body.appendChild(allowed_link);
            document.body.appendChild(denied_link);
          )",
                                        allowed_url, denied_url)));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kPrefetch));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkHeaderPrefetch) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(
          "<html><body>Hello</body></html>",
          {{"Connection-Allowlist", "(response-origin)"},
           {"Link", absl::StrFormat("<%s>; rel=prefetch, <%s>; rel=prefetch",
                                    allowed_url.spec(), denied_url.spec())}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kPrefetch));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkPreload) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(R"(
            var allowed_link = document.createElement('link');
            allowed_link.href = $1;
            allowed_link.rel = 'preload';
            allowed_link.as = 'script';

            var denied_link = document.createElement('link');
            denied_link.href = $2;
            denied_link.rel = 'preload';
            denied_link.as = 'script';

            document.body.appendChild(allowed_link);
            document.body.appendChild(denied_link);
          )",
                                        allowed_url, denied_url)));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kScript));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkHeaderPreload) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(
          "<html><body>Hello</body></html>",
          {{"Connection-Allowlist", "(response-origin)"},
           {"Link",
            absl::StrFormat(
                "<%s>; rel=preload; as=script, <%s>; rel=preload; as=script",
                allowed_url.spec(), denied_url.spec())}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kScript));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkModulePreload) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(R"(
            var allowed_link = document.createElement('link');
            allowed_link.href = $1;
            allowed_link.rel = 'modulepreload';

            var denied_link = document.createElement('link');
            denied_link.href = $2;
            denied_link.rel = 'modulepreload';

            document.body.appendChild(allowed_link);
            document.body.appendChild(denied_link);
          )",
                                        allowed_url, denied_url)));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kScript));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkHeaderModulepreload) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(
          "<html><body>Hello</body></html>",
          {{"Connection-Allowlist", "(response-origin)"},
           {"Link",
            absl::StrFormat("<%s>; rel=modulepreload, <%s>; rel=modulepreload",
                            allowed_url.spec(), denied_url.spec())}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kScript));
}

class AlwaysPreconnectContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool ShouldPreconnectNavigation(RenderFrameHost* render_frame_host) override {
    return true;
  }
};

// TODO(https://crbug.com/497205155): Fix flakiness and enable this test.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       DISABLED_NavigationRequestPreconnectAllowed) {
  net::test_server::ConnectionTracker connection_tracker(
      &embedded_https_test_server());
  AlwaysPreconnectContentBrowserClient client;

  std::string_view title_page{"/title.html"};
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      std::string{title_page},
      ResponseEntry("<html><head><title>Title</title></head></html>", {}));

  // Use `StartAndReturnHandle()` to start the server; this ensures graceful
  // shutdown when the test finishes. Otherwise, a socket read may occur after
  // the connection tracker is destroyed, invoking a callback via a dangling
  // pointer and crashing the test.
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "a.test", kSameOriginAllowlistedPage)));

  connection_tracker.ResetCounts();
  // Navigation to url allowed by connection allowlist succeeds.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      shell()->web_contents(),
      embedded_https_test_server().GetURL("a.test", title_page)));

  // Preconnect to the same url also succeeds.
  connection_tracker.WaitForAcceptedConnections(1u);
  EXPECT_EQ(connection_tracker.GetAcceptedSocketCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       NavigationRequestPreconnectBlocked) {
  net::test_server::ConnectionTracker connection_tracker(
      &embedded_https_test_server());
  AlwaysPreconnectContentBrowserClient client;

  std::string_view title_page{"/title.html"};
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      std::string{title_page},
      ResponseEntry("<html><head><title>Title</title></head></html>", {}));

  // Use `StartAndReturnHandle()` to start the server; this ensures graceful
  // shutdown when the test finishes. Otherwise, a socket read may occur after
  // the connection tracker is destroyed, invoking a callback via a dangling
  // pointer and crashing the test.
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "a.test", kSameOriginAllowlistedPage)));

  connection_tracker.ResetCounts();
  // Navigation to url blocked by connection allowlist fails.
  EXPECT_FALSE(NavigateToURLFromRenderer(
      shell()->web_contents(),
      embedded_https_test_server().GetURL("b.test", title_page)));

  // Preconnect to the same url also gets blocked.
  EXPECT_EQ(connection_tracker.GetAcceptedSocketCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       IframeNavigationRequestPreconnectAllowed) {
  net::test_server::ConnectionTracker connection_tracker(
      &embedded_https_test_server());
  AlwaysPreconnectContentBrowserClient client;

  std::string_view title_page{"/title.html"};
  std::string_view nested_page{"/nested.html"};
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(JsReplace(R"(
        <html>
          <body>
            <iframe id="iframe" src=$1>
          </body>
        </html>
      )",
                              nested_page),
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      std::string{nested_page},
      ResponseEntry("<html><head><title>Nested</title></head></html>",
                    {{"Connection-Allowlist", "()"}}));
  RegisterResponse(
      std::string{title_page},
      ResponseEntry("<html><head><title>Title</title></head></html>", {}));

  // Use `StartAndReturnHandle()` to start the server; this ensures graceful
  // shutdown when the test finishes. Otherwise, a socket read may occur after
  // the connection tracker is destroyed, invoking a callback via a dangling
  // pointer and crashing the test.
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "a.test", kSameOriginAllowlistedPage)));

  RenderFrameHost* child_frame =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);

  connection_tracker.ResetCounts();

  // Navigating the iframe to url allowed by the initiator connection allowlist
  // succeeds. Note the iframe document has an empty connection allowlist, which
  // blocks all network connections. However, it is the initiator connection
  // allowlist that should be enforced.
  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      JsReplace("document.getElementById('iframe').src = $1", title_page)));

  // Preconnect to the same url also succeeds.
  connection_tracker.WaitForAcceptedConnections(1u);
  EXPECT_EQ(connection_tracker.GetAcceptedSocketCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       IframeNavigationRequestPreconnectDenied) {
  net::test_server::ConnectionTracker connection_tracker(
      &embedded_https_test_server());
  AlwaysPreconnectContentBrowserClient client;

  std::string_view title_page{"/title.html"};
  std::string_view nested_page{"/nested.html"};
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(JsReplace(R"(
        <html>
          <body>
            <iframe id="iframe" src=$1>
          </body>
        </html>
      )",
                              nested_page),
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      std::string{nested_page},
      ResponseEntry("<html><head><title>Nested</title></head></html>",
                    {{"Connection-Allowlist", "(*title*)"}}));
  RegisterResponse(
      std::string{title_page},
      ResponseEntry("<html><head><title>Title</title></head></html>", {}));

  // Use `StartAndReturnHandle()` to start the server; this ensures graceful
  // shutdown when the test finishes. Otherwise, a socket read may occur after
  // the connection tracker is destroyed, invoking a callback via a dangling
  // pointer and crashing the test.
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "a.test", kSameOriginAllowlistedPage)));

  RenderFrameHost* child_frame =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);

  connection_tracker.ResetCounts();

  // Navigating the iframe to url blocked by the initiator connection allowlist
  // fails. Note the iframe document has a connection allowlist that matches the
  // navigation url. However, it is the initiator connection allowlist that
  // should be enforced.
  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      JsReplace("document.getElementById('iframe').src = $1",
                embedded_https_test_server().GetURL("b.test", title_page))));

  // Preconnect to the same url also gets blocked.
  EXPECT_EQ(connection_tracker.GetAcceptedSocketCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkPreconnect) {
  // Create a separate server for receiving preconnect.
  net::test_server::EmbeddedTestServer cross_origin_server;
  net::test_server::ConnectionTracker connection_tracker(&cross_origin_server);

  // Note: the URL pattern in allowlist should be surrounded by double quotes.
  RegisterResponse(kCrossOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist",
                                   R"((response-origin "*://b.test:*/*"))"}}));

  // Use `StartAndReturnHandle()` to start the servers; this ensures graceful
  // shutdown when the test finishes. Otherwise, a socket read may occur after
  // the connection tracker is destroyed, invoking a callback via a dangling
  // pointer and crashing the test.
  auto cross_origin_server_handle = cross_origin_server.StartAndReturnHandle();
  ASSERT_TRUE(cross_origin_server_handle);
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL allowed_url = cross_origin_server.GetURL("b.test", "/allow.js");
  GURL denied_url = cross_origin_server.GetURL("c.test", "/deny.js");

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "a.test", kCrossOriginAllowlistedPage)));

  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), JsReplace(R"(
            var allowed_link = document.createElement('link');
            allowed_link.href = $1;
            allowed_link.rel = 'preconnect';
            allowed_link.crossorigin= 'anonymous';

            var denied_link = document.createElement('link');
            denied_link.href = $2;
            denied_link.rel = 'preconnect';
            denied_link.crossorigin= 'anonymous';

            document.body.appendChild(allowed_link);
            document.body.appendChild(denied_link);
          )",
                                                allowed_url, denied_url)));

  connection_tracker.WaitForAcceptedConnections(1u);
  EXPECT_EQ(1u, connection_tracker.GetAcceptedSocketCount());
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, EnforceHistogramForDocument) {
  base::HistogramTester histogram_tester;
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  histogram_tester.ExpectTotalCount(network::kConnectionAllowlistTypeHistogram,
                                    1);
  histogram_tester.ExpectBucketCount(
      network::kConnectionAllowlistTypeHistogram,
      network::ConnectionAllowlistType::kEnforced, 1);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ReportOnlyHistogramForDocument) {
  base::HistogramTester histogram_tester;
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist-Report-Only",
                                   "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  histogram_tester.ExpectTotalCount(network::kConnectionAllowlistTypeHistogram,
                                    1);
  histogram_tester.ExpectBucketCount(
      network::kConnectionAllowlistTypeHistogram,
      network::ConnectionAllowlistType::kReportOnly, 1);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, EnforceHistogramForWorker) {
  base::HistogramTester histogram_tester;
  RegisterResponse(
      "/worker.js",
      ResponseEntry("onmessage = async (e) => { postMessage('end'); }",
                    {{"Connection-Allowlist", "(response-origin)"},
                     {"Content-Type", "text/javascript"}}));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(R"(<html><body>Hello</body></html>)", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // To ensure that fetching the worker (and its separate Connection-Allowlist)
  // completes, we create a Promise that only resolves when the worker is
  // running.
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     R"(
            (async () => {
              await new Promise((resolve) => {
                window.myworker = new Worker('../worker.js', { type: 'module'});
                window.myworker.onmessage = async (e) => {
                  resolve();
                };
                window.myworker.postMessage('start');
              });
            })();
          )"));

  histogram_tester.ExpectTotalCount(network::kConnectionAllowlistTypeHistogram,
                                    1);
  histogram_tester.ExpectBucketCount(
      network::kConnectionAllowlistTypeHistogram,
      network::ConnectionAllowlistType::kEnforced, 1);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, ReportOnlyHistogramForWorker) {
  base::HistogramTester histogram_tester;
  RegisterResponse(
      "/worker.js",
      ResponseEntry("onmessage = async (e) => { postMessage('end'); }",
                    {{"Connection-Allowlist-Report-Only", "(response-origin)"},
                     {"Content-Type", "text/javascript"}}));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(R"(<html><body>Hello</body></html>)", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // To ensure that fetching the worker (and its separate
  // Connection-Allowlist-Report-Only) completes, we create a Promise that only
  // resolves when the worker is running.
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     R"(
            (async () => {
              await new Promise((resolve) => {
                window.myworker = new Worker('../worker.js', { type: 'module'});
                window.myworker.onmessage = async (e) => {
                  resolve();
                };
                window.myworker.postMessage('start');
              });
            })();
          )"));

  histogram_tester.ExpectTotalCount(network::kConnectionAllowlistTypeHistogram,
                                    1);
  histogram_tester.ExpectBucketCount(
      network::kConnectionAllowlistTypeHistogram,
      network::ConnectionAllowlistType::kReportOnly, 1);
}

// Verifies that WebSocket connections are subject to Connection-Allowlist
// enforcement. A cross-origin WebSocket should be blocked when the page is
// served with Connection-Allowlist: (response-origin).
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, WebSocketBlocked) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Same-origin WebSocket should be allowed.
  GURL allowed_ws_url = net::test_server::GetWebSocketURL(
      embedded_https_test_server(), "a.test", "/echo-with-no-extension");
  EXPECT_EQ("open", EvalJs(shell()->web_contents(), JsReplace(R"(
    new Promise(resolve => {
      const ws = new WebSocket($1);
      ws.onopen = () => { ws.close(); resolve('open'); };
      ws.onerror = () => resolve('error');
    });
  )",
                                                              allowed_ws_url)));

  // Cross-origin WebSocket should be blocked by Connection-Allowlist.
  GURL denied_ws_url = net::test_server::GetWebSocketURL(
      embedded_https_test_server(), "b.test", "/echo-with-no-extension");
  EXPECT_EQ("error", EvalJs(shell()->web_contents(), JsReplace(R"(
    new Promise(resolve => {
      const ws = new WebSocket($1);
      ws.onopen = () => { ws.close(); resolve('open'); };
      ws.onerror = () => resolve('error');
    });
  )",
                                                               denied_ws_url)));
}
// Verifies that when an iframe with Connection-Allowlist is redirected from
// same-origin to cross-origin, the navigation is subject to the initiator's
// Connection-Allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       IframeSameOriginRedirectToCrossOrigin) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL iframe_url =
      embedded_https_test_server().GetURL("a.test", "/iframe.html");
  GURL final_url = embedded_https_test_server().GetURL("b.test", "/final.html");
  GURL redirect_url = embedded_https_test_server().GetURL(
      "a.test", "/cross-site/b.test/final.html");

  RegisterResponse(
      "/main.html",
      ResponseEntry(JsReplace("<html><body><iframe id='test_iframe' "
                              "src=$1></iframe></body></html>",
                              iframe_url),
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      "/iframe.html",
      ResponseEntry("<html><body>Hello from iframe</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse("/final.html",
                   ResponseEntry("<html><body>Final page</body></html>", {}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe->GetLastCommittedURL(), iframe_url);
  EXPECT_EQ(iframe->GetLastCommittedOrigin(),
            main_frame->GetLastCommittedOrigin());

  // Navigate the iframe to a same-origin URL that redirects to cross-origin.
  // The initiator is the iframe's content, which has (response-origin).
  // Redirect to b.test should be blocked.
  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(iframe, JsReplace("location.href = $1", redirect_url)));
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, nav_observer.last_net_error_code());

  // Verify that the final URL was never even requested.
  EXPECT_FALSE(monitor.GetRequestInfo(final_url).has_value());
}

// Ensure that Connection-Allowlist headers are correctly enforced for
// redirects even when the initiator frame is destroyed during the redirect.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    IframeSameOriginRedirectToCrossOriginInitiatorDestroyed) {
  // Setup ControllableHttpResponse for the redirect URL.
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), "/delayed-redirect");

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL initiator_url =
      embedded_https_test_server().GetURL("a.test", "/initiator.html");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/target.html");
  GURL final_url = embedded_https_test_server().GetURL("b.test", "/final.html");
  GURL redirect_url =
      embedded_https_test_server().GetURL("a.test", "/delayed-redirect");

  RegisterResponse(
      "/main.html",
      ResponseEntry(
          JsReplace("<html><body>"
                    "<iframe id='initiator' src=$1></iframe>"
                    "<iframe id='target' name='target_frame' src=$2></iframe>"
                    "</body></html>",
                    initiator_url, target_url),
          {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      "/initiator.html",
      ResponseEntry("<html><body>Initiator iframe</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      "/target.html",
      ResponseEntry("<html><body>Target iframe</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse("/final.html",
                   ResponseEntry("<html><body>Final page</body></html>", {}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* initiator_frame = ChildFrameAt(main_frame, 0);
  RenderFrameHost* target_frame = ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(initiator_frame);
  ASSERT_TRUE(target_frame);

  EXPECT_EQ(main_frame->GetLastCommittedOrigin(),
            initiator_frame->GetLastCommittedOrigin());
  EXPECT_EQ(main_frame->GetLastCommittedOrigin(),
            target_frame->GetLastCommittedOrigin());

  // Trigger navigation in target_iframe initiated by initiator_iframe.
  // The server will redirect it to b.test.
  TestNavigationObserver nav_observer(shell()->web_contents());
  ExecuteScriptAsync(
      initiator_frame,
      JsReplace("window.open($1, 'target_frame')", redirect_url));

  // Wait for the request to reach the server.
  controllable_response.WaitForRequest();

  // Destroy the initiator iframe while the redirect response is pending.
  RenderFrameDeletedObserver deleted(initiator_frame);
  EXPECT_TRUE(
      ExecJs(main_frame, "document.getElementById('initiator').remove();"));
  deleted.WaitUntilDeleted();

  // Send the redirect response now that the initiator is gone.
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      final_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  nav_observer.Wait();

  // The navigation should still fail because the initiator's policies were
  // captured.
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, nav_observer.last_net_error_code());

  // Verify that the final URL was never requested.
  EXPECT_FALSE(monitor.GetRequestInfo(final_url).has_value());
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, UseCounterForWorker) {
  RegisterResponse(
      "/worker.js",
      ResponseEntry("onmessage = async (e) => { postMessage('end'); }",
                    {{"Connection-Allowlist", "(response-origin)"},
                     {"Content-Type", "text/javascript"}}));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(R"(<html><body>Hello</body></html>)", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_CALL(*content_browser_client_,
              LogWebFeatureForCurrentPage(
                  shell()->web_contents()->GetPrimaryMainFrame(),
                  blink::mojom::WebFeature::kConnectionAllowlist));

  // To ensure that fetching the worker (and its separate Connection-Allowlist)
  // completes, we create a Promise that only resolves when the worker is
  // running.
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     R"(
            (async () => {
              await new Promise((resolve) => {
                window.myworker = new Worker('../worker.js', { type: 'module'});
                window.myworker.onmessage = async (e) => {
                  resolve();
                };
                window.myworker.postMessage('start');
              });
            })();
          )"));
}

// TODO(crbug.com/40752428): There is a race condition which makes
// `CreateCrossOriginPrefetchLoaderFactoryBundle()` sometimes called on the
// previous document, before the new document is committed. Once it is fixed,
// add a similar test to "LinkCrossOriginDocumentPrefetch" but use header
// triggered prefetch. Otherwise that test will be flaky.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       LinkCrossOriginDocumentPrefetch) {
  RegisterResponse(kCrossOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist",
                                   R"((response-origin "*://b.test:*/*"))"}}));

  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url = embedded_https_test_server().GetURL(
      "a.test", kCrossOriginAllowlistedPage);
  GURL allowed_url =
      embedded_https_test_server().GetURL("b.test", "/allow.html");
  GURL denied_url = embedded_https_test_server().GetURL("c.test", "/deny.html");

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(R"(
            var allowed_link = document.createElement('link');
            allowed_link.href = $1;
            allowed_link.rel = 'prefetch';
            allowed_link.as = 'document';

            var denied_link = document.createElement('link');
            denied_link.href = $2;
            denied_link.rel = 'prefetch';
            denied_link.as = 'document';

            document.body.appendChild(allowed_link);
            document.body.appendChild(denied_link);
          )",
                                        allowed_url, denied_url)));

  monitor.WaitForUrls({allowed_url, denied_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(denied_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(allowed_url);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kPrefetch));
}

// Regression test: removing an about:blank iframe that inherited the parent's
// network_restrictions_id must not clear the parent's nonce. After the iframe
// is removed, cross-origin requests from the parent should still be blocked.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       RemovingAboutBlankIframeDoesNotClearParentNonce) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Set nonce clear delay to zero so that if the nonce were incorrectly
  // scheduled for clearing, it would fire immediately rather than relying
  // on the default 60-second delay to mask the bug.
  static_cast<StoragePartitionImpl*>(shell()
                                         ->web_contents()
                                         ->GetBrowserContext()
                                         ->GetDefaultStoragePartition())
      ->SetClearNetworkRestrictionsParamsForTesting(base::TimeDelta(),
                                                    base::DoNothing());

  // 1. Create an about:blank iframe (initial empty document inherits the
  //    parent's nonce).
  // 2. Remove it immediately.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), R"(
    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    iframe.remove();
  )"));

  // 3. After the iframe is destroyed, cross-origin requests from the parent
  //    should still be blocked by Connection-Allowlist.
  EXPECT_EQ("blocked",
            EvalJs(shell()->web_contents(), content::JsReplace(R"(
      fetch($1).then(() => 'allowed').catch(() => 'blocked');
    )",
                                                               denied_url)));
}

// Regression test: closing an opener tab must not clear the nonce for a
// popup window that inherited it and remains at about:blank.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ClosingOpenerDoesNotClearPopupNonce) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Set nonce clear delay to zero so incorrect clears fire immediately.
  static_cast<StoragePartitionImpl*>(shell()
                                         ->web_contents()
                                         ->GetBrowserContext()
                                         ->GetDefaultStoragePartition())
      ->SetClearNetworkRestrictionsParamsForTesting(base::TimeDelta(),
                                                    base::DoNothing());

  // 1. Open a popup that stays at about:blank (inherits the opener's nonce).
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();

  // 2. Close the opener tab.
  shell()->Close();

  // 3. The popup should still have its Connection-Allowlist enforcements.
  //    Cross-origin requests should be blocked.
  EXPECT_EQ("blocked",
            EvalJs(popup->web_contents(), content::JsReplace(R"(
      fetch($1).then(() => 'allowed').catch(() => 'blocked');
    )",
                                                             denied_url)));
}

// Regression test: when an about:blank iframe navigates to a new page, the
// old initial empty document's RFH is destroyed. This must not clear the
// parent's nonce -- the ref-counted id should keep the parent's nonce alive.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       NavigatingAboutBlankIframeDoesNotClearParentNonce) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body><iframe id='child'></iframe></body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL iframe_url =
      embedded_https_test_server().GetURL("b.test", "/title1.html");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Set nonce clear delay to zero so incorrect clears fire immediately.
  static_cast<StoragePartitionImpl*>(shell()
                                         ->web_contents()
                                         ->GetBrowserContext()
                                         ->GetDefaultStoragePartition())
      ->SetClearNetworkRestrictionsParamsForTesting(base::TimeDelta(),
                                                    base::DoNothing());

  // 1. The iframe starts at about:blank (initial empty document, inherits
  //    the parent's nonce).
  // 2. Navigate it to a real page -- this commits a new document in a new
  //    RFH and destroys the old initial-empty-document RFH.
  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), "child", iframe_url));

  // 3. After the old about:blank RFH is destroyed, cross-origin requests
  //    from the parent should still be blocked.
  EXPECT_EQ("blocked",
            EvalJs(shell()->web_contents(), content::JsReplace(R"(
      fetch($1).then(() => 'allowed').catch(() => 'blocked');
    )",
                                                               denied_url)));
}

// SpeculationRules API allows specifying the rules in a JSON using the response
// header:
//
// Speculation-Rules: "/rules.json"
//
// It fetches the rules via a subresource request, which is subject to
// connection allowlist. This test verifies the prefetch succeeds if both the
// rules URL and prefetch URL are allowed.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesFetchRulesBaseline) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist", "(response-origin)"},
                                  {"Speculation-Rules", R"("/rules.json")"}}));
  RegisterResponse(
      "/rules.json",
      ResponseEntry(absl::StrFormat(R"(
        {
          "prefetch": [
            {"source": "list", "urls": ["%s"], "eagerness": "immediate"}
          ]
        }
      )",
                                    allowed_url.spec().c_str()),
                    {{"Content-Type", "application/speculationrules+json"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The rules json is fetched, which initiates the prefetch request. Both are
  // allowed by the connection allowlist.
  GURL rules_url = embedded_https_test_server().GetURL("a.test", "/rules.json");
  monitor.WaitForUrls({rules_url, allowed_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(rules_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);
}

// The rules URL is not allowed. The prefetch does not take place.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesFetchRulesBlocked) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL same_origin_url =
      embedded_https_test_server().GetURL("a.test", "/same_origin.js");
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross_origin.js");

  RegisterResponse("/same_origin.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/cross_origin.js",
                   ResponseEntry("console.log('also allow');"));

  // The connection allowlist allows the prefetch URLs, but not the rules URL.
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist",
                      R"(("*://a.test:*/*.js" "*://b.test:*/*.js"))"},
                     {"Speculation-Rules", R"("/rules.json")"}}));
  RegisterResponse(
      "/rules.json",
      ResponseEntry(absl::StrFormat(R"(
        {
          "prefetch": [
            {"source": "list", "urls": ["%s", "%s"], "eagerness": "immediate"}
          ]
        }
      )",
                                    same_origin_url.spec().c_str(),
                                    cross_origin_url.spec().c_str()),
                    {{"Content-Type", "application/speculationrules+json"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The fetch of rules is blocked.
  GURL rules_url = embedded_https_test_server().GetURL("a.test", "/rules.json");
  monitor.WaitForUrls({rules_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(rules_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);

  // Since the rules are not fetched, the prefetch requests do not exist.
  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(rfh->GetFrameTreeNodeId());
  ASSERT_TRUE(prefetch_service);

  const blink::DocumentToken& document_token =
      static_cast<const RenderFrameHostImpl*>(rfh)->GetDocumentToken();
  EXPECT_FALSE(
      prefetch_service->MatchUrl(PrefetchKey(document_token, same_origin_url)));
  EXPECT_FALSE(prefetch_service->MatchUrl(
      PrefetchKey(document_token, cross_origin_url)));
}

// The connection allowlist of the initiator network context is checked for
// Speculation Rules prefetch.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesSameOriginPrefetchBlocked) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("a.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["%s", "%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 allowed_url.spec().c_str(),
                                                 denied_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   R"(("*://a.test:*/allow.js"))"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  monitor.WaitForUrls({allowed_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);

  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      allowed_url, PrefetchContainer::LoadState::kCompleted,
      PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      denied_url, PrefetchContainer::LoadState::kFailedIneligible,
      PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
}

// Speculation Rules prefetch uses an isolated network context when the prefetch
// URL is cross origin. The connection allowlist of the initiator network
// context is checked, instead of the isolated network context.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesCrossOriginPrefetchBlocked) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("b.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["%s", "%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 allowed_url.spec().c_str(),
                                                 denied_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   R"(("*://b.test:*/allow.js"))"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  monitor.WaitForUrls({allowed_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);

  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      allowed_url, PrefetchContainer::LoadState::kCompleted,
      PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      denied_url, PrefetchContainer::LoadState::kFailedIneligible,
      PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
}

// The connection allowlist of the initiator network context is checked for
// Speculation Rules prefetch.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesHeaderSameOriginPrefetchBlocked) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("a.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist",
                      R"(("*://a.test:*/rules.json" "*://a.test:*/allow.js"))"},
                     {"Speculation-Rules", R"("/rules.json")"}}));

  RegisterResponse(
      "/rules.json",
      ResponseEntry(absl::StrFormat(R"(
        {
          "prefetch": [
            {"source": "list", "urls": ["%s", "%s"], "eagerness": "immediate"}
          ]
        }
      )",
                                    allowed_url.spec().c_str(),
                                    denied_url.spec().c_str()),
                    {{"Content-Type", "application/speculationrules+json"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL rules_url = embedded_https_test_server().GetURL("a.test", "/rules.json");
  monitor.WaitForUrls({rules_url, allowed_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(rules_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);

  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      allowed_url, PrefetchContainer::LoadState::kCompleted,
      PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      denied_url, PrefetchContainer::LoadState::kFailedIneligible,
      PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
}

// Speculation Rules prefetch uses an isolated network context when the prefetch
// URL is cross origin. The connection allowlist of the initiator network
// context is checked, instead of the isolated network context.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesHeaderCrossOriginPrefetchBlocked) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("b.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("b.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist",
                      R"(("*://a.test:*/rules.json" "*://b.test:*/allow.js"))"},
                     {"Speculation-Rules", R"("/rules.json")"}}));

  RegisterResponse(
      "/rules.json",
      ResponseEntry(absl::StrFormat(R"(
        {
          "prefetch": [
            {"source": "list", "urls": ["%s", "%s"], "eagerness": "immediate"}
          ]
        }
      )",
                                    allowed_url.spec().c_str(),
                                    denied_url.spec().c_str()),
                    {{"Content-Type", "application/speculationrules+json"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL rules_url = embedded_https_test_server().GetURL("a.test", "/rules.json");
  monitor.WaitForUrls({rules_url, allowed_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(rules_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(allowed_url).error_code, net::OK);

  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      allowed_url, PrefetchContainer::LoadState::kCompleted,
      PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      denied_url, PrefetchContainer::LoadState::kFailedIneligible,
      PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
}

// Speculation Rules prefetch redirect is allowed by connection allowlist with
// `redirects=allow`.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesPrefetchRedirectAllowed) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), "/redirect.js");

  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL redirect_url =
      embedded_https_test_server().GetURL("a.test", "/redirect.js");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/redirect-target.js");

  RegisterResponse("/redirect-target.js",
                   ResponseEntry("console.log('Redirect is allowed');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 redirect_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   "(response-origin);redirects=allow"}}));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      redirect_url, PrefetchContainer::LoadState::kCompleted,
      PrefetchStatus::kPrefetchSuccessful));
}

// Speculation Rules prefetch redirect is blocked by connection allowlist with
// `redirects=block`.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesPrefetchRedirectBlocked) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), "/redirect.js");

  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL redirect_url =
      embedded_https_test_server().GetURL("a.test", "/redirect.js");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/redirect-target.js");

  RegisterResponse("/redirect-target.js",
                   ResponseEntry("console.log('Redirect is blocked');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 redirect_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   "(response-origin);redirects=block"}}));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // The redirect is blocked by the network service, so the prefetch fails with
  // a net error.
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      redirect_url, PrefetchContainer::LoadState::kFailed,
      PrefetchStatus::kPrefetchFailedNetError));
}

// Verifies that if a document is controlled by a Service Worker, and the
// document's Connection-Allowlist blocks a URL, the fetch is blocked in Blink
// before it can be forwarded to the Service Worker.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ServiceWorkerSubresourceFetchBlocked) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('fetch', event => {\n"
          "  if (event.request.url.indexOf('cross-origin-resource') !== -1) "
          "{\n"
          "    event.respondWith(fetch(event.request));\n"
          "  }\n"
          "});",
          {{"Content-Type", "text/javascript"}}));

  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("allowed-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Fetch the cross-origin resource.
  // Since the document has Connection-Allowlist: (response-origin), it should
  // be blocked in Blink before reaching the Service Worker.
  EXPECT_TRUE(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace(R"(
            (async () => {
              try {
                await fetch($1);
                return 'success';
              } catch (e) {
                return 'error';
              }
            })();
          )",
                               cross_origin_url))
                  .ExtractString()
                  .starts_with("error"));
}

// Verifies that if the Service Worker lets the fetch fall back to the
// network, the request is blocked (since the document has a
// Connection-Allowlist blocking it).
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ServiceWorkerSubresourceFetchBlockedByFallback) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('fetch', event => {\n"
          "  // Do not respond, let it fallback to network\n"
          "});",
          {{"Content-Type", "text/javascript"}}));

  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("denied-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Fetch the cross-origin resource.
  // The fetch is blocked early in Blink before reaching the SW or falling
  // back.
  EXPECT_TRUE(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace(R"(
            (async () => {
              try {
                await fetch($1);
                return 'success';
              } catch (e) {
                return 'error';
              }
            })();
          )",
                               cross_origin_url))
                  .ExtractString()
                  .starts_with("error"));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ServiceWorkerSubresourceFetchBlockedForDedicatedWorker) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('fetch', event => {\n"
          "  if (event.request.url.indexOf('cross-origin-resource') !== -1) "
          "{\n"
          "    event.respondWith(fetch(event.request));\n"
          "  }\n"
          "});",
          {{"Content-Type", "text/javascript"}}));

  // The main page has no Connection-Allowlist.
  RegisterResponse("/index.html",
                   ResponseEntry("<html><body>Hello</body></html>", {}));

  // The worker has Connection-Allowlist: (response-origin).
  RegisterResponse(
      "/worker.js",
      ResponseEntry("self.onmessage = async (e) => {\n"
                    "  try {\n"
                    "    await fetch(e.data.url);\n"
                    "    postMessage('success');\n"
                    "  } catch (err) {\n"
                    "    postMessage('error');\n"
                    "  }\n"
                    "};",
                    {{"Content-Type", "text/javascript"},
                     {"Connection-Allowlist", "(response-origin)"}}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("allowed-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/index.html");
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Start the Dedicated Worker and let it fetch the cross-origin resource.
  // Since the worker has Connection-Allowlist: (response-origin), it should
  // be blocked in Blink before reaching the Service Worker.
  EXPECT_EQ("error", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                            JsReplace(R"(
            (async () => {
              const worker = new Worker('/worker.js');
              const result = await new Promise(resolve => {
                worker.onmessage = e => resolve(e.data);
                worker.postMessage({url: $1});
              });
              worker.terminate();
              return result;
            })();
          )",
                                      cross_origin_url)));
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       ServiceWorkerConnectionAllowlistEnforced) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('fetch', event => {\n"
          "  if (event.request.url.indexOf('cross-origin-resource') !== -1) "
          "{\n"
          "    event.respondWith(fetch(event.request));\n"
          "  }\n"
          "});",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", "(response-origin)"}}));

  // Main page has NO Connection-Allowlist.
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>", {}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("allowed-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Fetch the cross-origin resource.
  // The fetch is allowed by the document, but blocked in the Network Service
  // when the Service Worker tries to fetch it.
  EXPECT_TRUE(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                     JsReplace(R"(
            (async () => {
              try {
                await fetch($1);
                return 'success';
              } catch (e) {
                return 'error';
              }
            })();
          )",
                               cross_origin_url))
                  .ExtractString()
                  .starts_with("error"));

  EXPECT_EQ(monitor.WaitForRequestCompletion(cross_origin_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
}

// Verifies that Navigation Preload is subject to the Service Worker's
// Connection-Allowlist. If a Service Worker with a strict
// Connection-Allowlist (e.g. empty allowlist "()" which blocks all
// connections) enables navigation preload, the preload request should be
// blocked, causing the FetchEvent promise to reject and the navigation to
// fail.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerNavigationPreloadEnforcesServiceWorkerAllowlist) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => {\n"
          "  e.waitUntil(Promise.all([\n"
          "    self.registration.navigationPreload.enable(),\n"
          "    self.clients.claim()\n"
          "  ]));\n"
          "});\n"
          "self.addEventListener('fetch', event => {\n"
          "  if (event.request.url.indexOf('controlled-page') !== -1) {\n"
          "    event.respondWith(async function() {\n"
          "      const response = await event.preloadResponse;\n"
          "      if (response) {\n"
          "        return response;\n"
          "      }\n"
          "      return new Response('no-preload-response');\n"
          "    }());\n"
          "  }\n"
          "});",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", "()"}}));

  RegisterResponse("/controlled-page",
                   ResponseEntry("preload-response-content", {}));

  // Main page has NO Connection-Allowlist.
  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL controlled_url =
      embedded_https_test_server().GetURL("a.test", "/controlled-page");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Navigate to the controlled page.
  // The preload request is to a.test/controlled-page.
  // Since the Service Worker's allowlist is empty () (which blocks all
  // connections), the navigation preload request is blocked, causing the
  // fetch event to fail and the navigation to fail with net::ERR_FAILED.
  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("window.location.href = $1", controlled_url)));
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_FAILED, nav_observer.last_net_error_code());
}

// Verifies that a navigation triggered via WindowClient.navigate() is subject
// to the Service Worker's Connection-Allowlist to see if the URL is allowed.
// Once the navigation request starts, it considers the document being
// navigated as the initiator in the current spec and implementation. We keep
// that invariant and check for the document's CA for URL allowed and redirects
// too.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerWindowClientNavigateEnforcesServiceWorkerAndDocumentAllowlists) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('message', event => {\n"
          "  event.waitUntil(async function() {\n"
          "    const clients = await self.clients.matchAll({type: "
          "'window'});\n"
          "    for (const client of clients) {\n"
          "      try {\n"
          "        await client.navigate(event.data.url);\n"
          "        event.source.postMessage({result: 'success'});\n"
          "      } catch (e) {\n"
          "        event.source.postMessage({result: 'failure', error: "
          "e.message});\n"
          "      }\n"
          "    }\n"
          "  }());\n"
          "});",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", "()"}}));

  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>", {}));
  RegisterResponse("/final.html", ResponseEntry("final-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/final.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return !!navigator.serviceWorker.controller;
            })();
          )"));

  // Tell the Service Worker to navigate the client window to target_url.
  // Since the Service Worker's allowlist is empty () (prohibiting all
  // connections), the navigate() promise should reject.
  EXPECT_EQ("failure", EvalJs(shell()->web_contents(), JsReplace(R"(
      new Promise(resolve => {
        navigator.serviceWorker.addEventListener('message', event => {
          resolve(event.data.result);
        }, {once: true});
        navigator.serviceWorker.controller.postMessage({url: $1});
      });
  )",
                                                                 target_url)));
}

// Verifies that a navigation triggered via WindowClient.navigate() is subject
// to the Document's Connection-Allowlist, even when the initial URL is
// allowed but redirects are disallowed (default Connection Allowlist behavior).
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerWindowClientNavigateRedirectObeysDocumentAllowlist) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('message', event => {\n"
          "  event.waitUntil(async function() {\n"
          "    const clients = await self.clients.matchAll({type: 'window'});\n"
          "    for (const client of clients) {\n"
          "      try {\n"
          "        await client.navigate(event.data.url);\n"
          "      } catch (e) {}\n"
          "    }\n"
          "  }());\n"
          "});",
          {{"Content-Type", "text/javascript"}}));

  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist",
                                   R"((response-origin "*://a.test:*/*"))"}}));
  RegisterResponse("/final.html", ResponseEntry("final-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  // Initial URL is on a.test (allowed) but it redirects (disallowed)
  GURL target_url = embedded_https_test_server().GetURL(
      "a.test", "/cross-site/b.test/final.html");
  GURL final_url = embedded_https_test_server().GetURL("b.test", "/final.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
             (async () => {
               const reg = await navigator.serviceWorker.register('/sw.js');
               await new Promise(resolve => {
                 const worker = reg.installing || reg.waiting || reg.active;
                 if (worker.state === 'activated') {
                   resolve();
                 } else {
                   worker.addEventListener('statechange', () => {
                     if (worker.state === 'activated') {
                       resolve();
                     }
                   });
                 }
               });
               return !!navigator.serviceWorker.controller;
             })();
           )"));

  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      JsReplace("navigator.serviceWorker.controller.postMessage({url: $1});",
                target_url)));
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, nav_observer.last_net_error_code());
}

// Verifies that a navigation triggered via WindowClient.navigate() is subject
// to the Document's Connection-Allowlist when the destination URL is allowed
// by the Service Worker's Connection-Allowlist but blocked by the Document's
// Connection-Allowlist.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerWindowClientNavigateObeysDocumentAllowlist) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('message', event => {\n"
          "  event.waitUntil(async function() {\n"
          "    const clients = await self.clients.matchAll({type: 'window'});\n"
          "    for (const client of clients) {\n"
          "      try {\n"
          "        await client.navigate(event.data.url);\n"
          "      } catch (e) {}\n"
          "    }\n"
          "  }());\n"
          "});",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist",
            R"((response-origin "*://a.test:*/*" "*://b.test:*/*"))"}}));

  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist",
                                   R"((response-origin "*://a.test:*/*"))"}}));
  RegisterResponse("/final.html", ResponseEntry("final-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL target_url =
      embedded_https_test_server().GetURL("b.test", "/final.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
             (async () => {
               const reg = await navigator.serviceWorker.register('/sw.js');
               await new Promise(resolve => {
                 const worker = reg.installing || reg.waiting || reg.active;
                 if (worker.state === 'activated') {
                   resolve();
                 } else {
                   worker.addEventListener('statechange', () => {
                     if (worker.state === 'activated') {
                       resolve();
                     }
                   });
                 }
               });
               return !!navigator.serviceWorker.controller;
             })();
           )"));

  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      JsReplace("navigator.serviceWorker.controller.postMessage({url: $1});",
                target_url)));
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_NETWORK_ACCESS_REVOKED,
            nav_observer.last_net_error_code());
}

// Verifies that clients.openWindow() is subject to the Service Worker's
// Connection-Allowlist.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerClientsOpenWindowEnforcesServiceWorkerAllowlist) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          R"(self.addEventListener('install', e => self.skipWaiting());
self.addEventListener('activate', e => {
  e.waitUntil(self.clients.claim());
});
self.addEventListener('notificationclick', event => {
  event.waitUntil(async function() {
    try {
      await self.clients.openWindow(event.notification.body);
      const clients = await self.clients.matchAll({type: 'window'});
      for (const client of clients) {
        client.postMessage({result: 'success'});
      }
    } catch (e) {
      const clients = await self.clients.matchAll({type: 'window'});
      for (const client of clients) {
        client.postMessage({
          result: 'failure',
          error: e.message
        });
      }
    }
  }());
});)",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", "()"}}));

  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>", {}));
  RegisterResponse("/final.html", ResponseEntry("final-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/final.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker, and set up message listener.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
             (async () => {
               const reg = await navigator.serviceWorker.register('/sw.js');
               await new Promise(resolve => {
                 const worker = reg.installing || reg.waiting || reg.active;
                 if (worker.state === 'activated') {
                   resolve();
                 } else {
                   worker.addEventListener('statechange', () => {
                     if (worker.state === 'activated') {
                       resolve();
                     }
                   });
                 }
               });
               window.sw_messages = [];
               navigator.serviceWorker.addEventListener('message', event => {
                 window.sw_messages.push(event.data);
                 if (window.on_sw_message) window.on_sw_message(event.data);
               });
               return !!navigator.serviceWorker.controller;
             })();
           )"));

  // Dispatch notification click.
  StoragePartition* partition = shell()
                                    ->web_contents()
                                    ->GetBrowserContext()
                                    ->GetDefaultStoragePartition();
  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());

  GURL scope_url = embedded_https_test_server().GetURL("a.test", "/");

  // Ensure the service worker is started.
  base::RunLoop run_loop;
  wrapper->StartActiveServiceWorker(
      scope_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_url)),
      base::BindOnce(
          [](base::OnceClosure quit, blink::ServiceWorkerStatusCode status) {
            EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  blink::PlatformNotificationData notification_data;
  notification_data.body = base::UTF8ToUTF16(target_url.spec());

  content::DispatchServiceWorkerNotificationClick(wrapper.get(), scope_url,
                                                  notification_data);

  // Expect failure since the SW CA blocks everything.
  EXPECT_EQ("failure", EvalJs(shell()->web_contents(), R"(
      new Promise(resolve => {
        if (window.sw_messages.length > 0) {
          resolve(window.sw_messages[0].result);
        } else {
          window.on_sw_message = data => {
            resolve(data.result);
          };
        }
      });
  )"));

  // Verify that no new window was opened.
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verifies that clients.openWindow() succeeds when the destination URL is
// allowed by the Service Worker's Connection-Allowlist.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerClientsOpenWindowObeysServiceWorkerAllowlist) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          R"(self.addEventListener('install', e => self.skipWaiting());
self.addEventListener('activate', e => {
  e.waitUntil(self.clients.claim());
});
self.addEventListener('notificationclick', event => {
  event.waitUntil(async function() {
    try {
      await self.clients.openWindow(event.notification.body);
      const clients = await self.clients.matchAll({type: 'window'});
      for (const client of clients) {
        client.postMessage({result: 'success'});
      }
    } catch (e) {
      const clients = await self.clients.matchAll({type: 'window'});
      for (const client of clients) {
        client.postMessage({
          result: 'failure',
          error: e.message
        });
      }
    }
  }());
});)",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", R"((response-origin "*://a.test:*/*"))"}}));

  RegisterResponse("/main.html",
                   ResponseEntry("<html><body>Hello</body></html>", {}));
  RegisterResponse("/final.html", ResponseEntry("final-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url = embedded_https_test_server().GetURL("a.test", "/main.html");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/final.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker, and set up message listener.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
             (async () => {
               const reg = await navigator.serviceWorker.register('/sw.js');
               await new Promise(resolve => {
                 const worker = reg.installing || reg.waiting || reg.active;
                 if (worker.state === 'activated') {
                   resolve();
                 } else {
                   worker.addEventListener('statechange', () => {
                     if (worker.state === 'activated') {
                       resolve();
                     }
                   });
                 }
               });
               window.sw_messages = [];
               navigator.serviceWorker.addEventListener('message', event => {
                 window.sw_messages.push(event.data);
                 if (window.on_sw_message) window.on_sw_message(event.data);
               });
               return !!navigator.serviceWorker.controller;
             })();
           )"));

  // Dispatch notification click.
  StoragePartition* partition = shell()
                                    ->web_contents()
                                    ->GetBrowserContext()
                                    ->GetDefaultStoragePartition();
  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());

  GURL scope_url = embedded_https_test_server().GetURL("a.test", "/");

  // Ensure the service worker is started.
  base::RunLoop run_loop;
  wrapper->StartActiveServiceWorker(
      scope_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_url)),
      base::BindOnce(
          [](base::OnceClosure quit, blink::ServiceWorkerStatusCode status) {
            EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
            std::move(quit).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();

  blink::PlatformNotificationData notification_data;
  notification_data.body = base::UTF8ToUTF16(target_url.spec());

  content::WebContentsAddedObserver new_window_observer;
  TestNavigationObserver nav_observer(target_url);
  nav_observer.StartWatchingNewWebContents();

  content::DispatchServiceWorkerNotificationClick(wrapper.get(), scope_url,
                                                  notification_data);

  // Expect success since the SW CA allows a.test.
  EXPECT_EQ("success", EvalJs(shell()->web_contents(), R"(
      new Promise(resolve => {
        if (window.sw_messages.length > 0) {
          resolve(window.sw_messages[0].result);
        } else {
          window.on_sw_message = data => {
            resolve(data.result);
          };
        }
      });
  )"));

  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(target_url, nav_observer.last_navigation_url());

  // Verify that a new window was opened.
  WebContents* new_window = new_window_observer.GetWebContents();
  EXPECT_TRUE(new_window);
}

class ConnectionAllowlistSyntheticResponseTest
    : public ConnectionAllowlistTest {
 public:
  ConnectionAllowlistSyntheticResponseTest() {
    synthetic_response_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kServiceWorkerSyntheticResponse,
          {{blink::features::kServiceWorkerSyntheticResponseAllowedUrl.name,
            "https://b.test/synthetic_response?query=foo"}}},
         {network::features::kURLLoaderUseProvidedResponseBodyStream, {}},
         {network::features::kServiceWorkerSyntheticResponseHeaderCheck, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList synthetic_response_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistSyntheticResponseTest,
                       SyntheticResponseBlockedByInitiatorAllowlist) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL target_url = embedded_https_test_server().GetURL(
      "b.test", "/synthetic_response?query=foo");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate to a cross-origin URL that is eligible for Synthetic Response.
  // Since the initiator page has Connection-Allowlist: (response-origin),
  // this cross-origin navigation must be blocked and fail with
  // net::ERR_NETWORK_ACCESS_REVOKED.
  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_FALSE(NavigateToURLFromRenderer(shell()->web_contents(), target_url));

  nav_observer.Wait();
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_NETWORK_ACCESS_REVOKED,
            nav_observer.last_net_error_code());
}

IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerRaceNetworkRequestBypassesServiceWorkerAllowlist) {
  // Register the service worker script with Connection-Allowlist which blocks
  // connections except for the service worker script and its imports. The
  // service worker script imports the static router script to configure
  // 'race-network-and-fetch-handler'.
  RegisterResponse(
      "/service_worker/sw.js",
      ResponseEntry(
          "importScripts('/service_worker/static_router_race_match_all.js');",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist",
            R"((response-origin "*://a.test:*/service_worker/*"))"}}));

  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>", {}));

  // The race request will fetch `/service_worker/controlled-page`.
  RegisterResponse("/service_worker/controlled-page",
                   ResponseEntry("race-response-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL controlled_url = embedded_https_test_server().GetURL(
      "a.test", "/service_worker/controlled-page?sw_slow");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register(
                  '/service_worker/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return reg.active && reg.active.state === 'activated';
            })();
          )"));

  // Navigate to the controlled page.
  // The service worker's fetch handler is delayed (sw_slow), so the parallel
  // RaceNetworkRequest will complete first.
  // Although the Service Worker's allowlist is empty () (prohibiting all
  // connections), the RaceNetworkRequest succeeds because it bypasses the
  // SW's allowlist (passes std::nullopt).
  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("window.location.href = $1", controlled_url)));
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(controlled_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ("race-response-content",
            EvalJs(shell()->web_contents(), "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    ServiceWorkerRaceNetworkRequestBlockedByInitiatorAllowlist) {
  // Register the service worker script with Connection-Allowlist which blocks
  // connections except for the service worker script and its imports.
  RegisterResponse(
      "/service_worker/sw.js",
      ResponseEntry(
          "importScripts('/service_worker/static_router_race_match_all.js');",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist",
            R"((response-origin "*://a.test:*/service_worker/*"))"}}));

  // /register.html has no connection allowlist so it can register the Service
  // Worker.
  RegisterResponse(
      "/register.html",
      ResponseEntry("<html><body>Register page</body></html>", {}));

  // /initiator.html has Connection-Allowlist: () which blocks all
  // connections.
  RegisterResponse("/initiator.html",
                   ResponseEntry("<html><body>Initiator page</body></html>",
                                 {{"Connection-Allowlist", "()"}}));

  // The race request will fetch `/service_worker/controlled-page`.
  RegisterResponse("/service_worker/controlled-page",
                   ResponseEntry("race-response-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL register_url =
      embedded_https_test_server().GetURL("a.test", "/register.html");
  GURL initiator_url =
      embedded_https_test_server().GetURL("a.test", "/initiator.html");
  GURL controlled_url = embedded_https_test_server().GetURL(
      "a.test", "/service_worker/controlled-page?sw_slow");

  // Go to the register page.
  EXPECT_TRUE(NavigateToURL(shell(), register_url));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register(
                  '/service_worker/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return reg.active && reg.active.state === 'activated';
            })();
          )"));

  // Navigate to the initiator page, which has Connection-Allowlist: ().
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  // From the initiator page, navigate to the controlled page.
  // Although the RaceNetworkRequest bypasses the Service Worker's allowlist,
  // it must still be blocked by the initiator page's allowlist, failing with
  // net::ERR_NETWORK_ACCESS_REVOKED.
  TestNavigationObserver nav_observer(shell()->web_contents());
  EXPECT_FALSE(
      NavigateToURLFromRenderer(shell()->web_contents(), controlled_url));
  nav_observer.Wait();

  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_NETWORK_ACCESS_REVOKED,
            nav_observer.last_net_error_code());
}

// Observe the error code of prerendering navigation requests. This class is
// required because:
// - `TestNavigationObserver` ignores prerender requests on purpose.
// - `URLLoaderMonitor` fails to monitor prerender requests initiated by the
// URLLoaderFactory obtained by `GetURLLoaderFactoryForBrowserProcess()`.
// `URLLoaderMonitor` needs to be constructed before the URLLoaderFactory is
// created, which is difficult because this URLLoaderFactory is created during
// browser startup.

class PrerenderRequestObserver : public WebContentsObserver {
 public:
  PrerenderRequestObserver(WebContents* web_contents, const GURL& url)
      : WebContentsObserver(web_contents), url_(url) {}
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == url_) {
      future_.SetValue(navigation_handle->GetNetErrorCode());
    }
  }

  net::Error TakeErrorCode() { return future_.Take(); }

 private:
  GURL url_;
  base::test::TestFuture<net::Error> future_;
};

// Note SpeculationRules API currently does not support cross-site prerender.
// Only same-site prerender is tested. See crbug.com/1176054.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, SpeculationRulesPrerender) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("a.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prerender": [
                {
                  "source": "list",
                  "urls": ["%s", "%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 allowed_url.spec().c_str(),
                                                 denied_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   R"(("*://a.test:*/allow.js"))"}}));

  PrerenderRequestObserver allowed_observer(GetWebContents(), allowed_url);
  PrerenderRequestObserver denied_observer(GetWebContents(), denied_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The prefetch to allowed URL ahead of the prerender succeeds. It is then
  // used by the prerendering navigation request.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        allowed_url, PrefetchContainer::LoadState::kCompleted,
        PrefetchStatus::kPrefetchResponseUsed));
  }

  // Verify allowed URL succeeds.
  EXPECT_EQ(allowed_observer.TakeErrorCode(), net::OK);
  prerender_helper().WaitForPrerenderLoadCompletion(allowed_url);
  EXPECT_TRUE(prerender_helper().GetHostForUrl(allowed_url));
  EXPECT_EQ(prerender_helper().GetRequestCount(allowed_url), 1);

  // The prefetch to denied URL ahead of the prerender is blocked.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        denied_url, PrefetchContainer::LoadState::kFailedIneligible,
        PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
  }

  // Verify denied URL fails.
  EXPECT_EQ(denied_observer.TakeErrorCode(), net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_FALSE(prerender_helper().GetHostForUrl(denied_url));
  EXPECT_EQ(prerender_helper().GetRequestCount(denied_url), 0);
}

// Note SpeculationRules API currently does not support cross-site prerender.
// Only same-site prerender is tested. See crbug.com/1176054.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesHeaderPrerender) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_https_test_server().GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_https_test_server().GetURL("a.test", "/deny.js");

  RegisterResponse("/allow.js", ResponseEntry("console.log('allow');"));
  RegisterResponse("/deny.js", ResponseEntry("console.log('deny');"));
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist",
                      R"(("*://a.test:*/rules.json" "*://a.test:*/allow.js"))"},
                     {"Speculation-Rules", R"("/rules.json")"}}));

  RegisterResponse(
      "/rules.json",
      ResponseEntry(absl::StrFormat(R"(
        {
          "prerender": [
            {
              "source": "list",
              "urls": ["%s", "%s"],
              "eagerness": "immediate"
            }
          ]
        }
      )",
                                    allowed_url.spec().c_str(),
                                    denied_url.spec().c_str()),
                    {{"Content-Type", "application/speculationrules+json"}}));

  PrerenderRequestObserver allowed_observer(GetWebContents(), allowed_url);
  PrerenderRequestObserver denied_observer(GetWebContents(), denied_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The prefetch to allowed URL ahead of the prerender succeeds. It is then
  // used by the prerendering navigation request.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        allowed_url, PrefetchContainer::LoadState::kCompleted,
        PrefetchStatus::kPrefetchResponseUsed));
  }

  // Verify allowed URL succeeds.
  EXPECT_EQ(allowed_observer.TakeErrorCode(), net::OK);
  prerender_helper().WaitForPrerenderLoadCompletion(allowed_url);
  EXPECT_TRUE(prerender_helper().GetHostForUrl(allowed_url));
  EXPECT_EQ(prerender_helper().GetRequestCount(allowed_url), 1);

  // The prefetch to denied URL ahead of the prerender is blocked.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        denied_url, PrefetchContainer::LoadState::kFailedIneligible,
        PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));
  }

  // Verify denied URL fails.
  EXPECT_EQ(denied_observer.TakeErrorCode(), net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_FALSE(prerender_helper().GetHostForUrl(denied_url));
  EXPECT_EQ(prerender_helper().GetRequestCount(denied_url), 0);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesPrerenderRedirectAllowed) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), "/redirect.js");

  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL redirect_url =
      embedded_https_test_server().GetURL("a.test", "/redirect.js");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/redirect-target.js");

  RegisterResponse("/redirect-target.js",
                   ResponseEntry("console.log('Redirect is allowed');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prerender": [
                {
                  "source": "list",
                  "urls": ["%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 redirect_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   "(response-origin);redirects=allow"}}));

  PrerenderRequestObserver observer(shell()->web_contents(), target_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // The prefetch ahead of the prerender succeeds. It is then used by the
  // prerendering navigation request.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        redirect_url, PrefetchContainer::LoadState::kCompleted,
        PrefetchStatus::kPrefetchResponseUsed));
  }

  // Verify that the prerender redirect is allowed.
  EXPECT_EQ(observer.TakeErrorCode(), net::OK);
  prerender_helper().WaitForPrerenderLoadCompletion(redirect_url);
  EXPECT_TRUE(prerender_helper().GetHostForUrl(redirect_url));
  EXPECT_EQ(prerender_helper().GetRequestCount(redirect_url), 1);
}

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       SpeculationRulesPrerenderRedirectBlocked) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), "/redirect.js");

  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL redirect_url =
      embedded_https_test_server().GetURL("a.test", "/redirect.js");
  GURL target_url =
      embedded_https_test_server().GetURL("a.test", "/redirect-target.js");

  RegisterResponse("/redirect-target.js",
                   ResponseEntry("console.log('Redirect is blocked');"));
  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry(absl::StrFormat(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prerender": [
                {
                  "source": "list",
                  "urls": ["%s"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Hello</body>
        </html>
      )",
                                                 redirect_url.spec().c_str()),
                                 {{"Connection-Allowlist",
                                   "(response-origin);redirects=block"}}));

  PrerenderRequestObserver observer(shell()->web_contents(), redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // The prefetch ahead of the prerender is blocked by connection allowlist
  // because redirect is not allowed.
  if (IsPrerender2FallbackPrefetchSpecRulesEnabled()) {
    EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
        redirect_url, PrefetchContainer::LoadState::kFailed,
        PrefetchStatus::kPrefetchFailedNetError));
  }

  // If feature `Prerender2FallbackPrefetchSpecRules` is enabled, the prefetch
  // request fails because the redirect is blocked by connection allowlist. The
  // prerender request is aborted without being sent.
  // Otherwise, there is no prefetch ahead of the prerender, the prerender
  // request redirect is blocked with error code `UNSAFE_REDIRECT`.
  EXPECT_EQ(observer.TakeErrorCode(),
            IsPrerender2FallbackPrefetchSpecRulesEnabled()
                ? net::ERR_ABORTED
                : net::ERR_UNSAFE_REDIRECT);
  EXPECT_FALSE(prerender_helper().GetHostForUrl(redirect_url));
}

class ConnectionAllowlistDevToolsTest : public ConnectionAllowlistTest,
                                        public TestDevToolsProtocolClient {
 public:
  ConnectionAllowlistDevToolsTest() = default;
  ~ConnectionAllowlistDevToolsTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistDevToolsTest,
                       DevToolsLoadNetworkResourceEnforcesConnectionAllowlist) {
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry("<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("allowed-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  AttachToWebContents(shell()->web_contents());

  SendCommandSync("Network.enable");

  // Load the cross-origin resource using DevTools
  // Network.loadNetworkResource. Since the page has Connection-Allowlist:
  // (response-origin), which only allows connections to its own origin
  // (a.test), the fetch to b.test via DevTools should fail/be blocked.
  base::DictValue params;
  params.Set("frameId", shell()
                            ->web_contents()
                            ->GetPrimaryMainFrame()
                            ->GetDevToolsFrameToken()
                            .ToString());
  params.Set("url", cross_origin_url.spec());

  base::DictValue options;
  options.Set("disableCache", true);
  options.Set("includeCredentials", false);
  params.Set("options", std::move(options));

  const base::DictValue* response =
      SendCommandSync("Network.loadNetworkResource", std::move(params));
  ASSERT_TRUE(response) << (error() ? error()->DebugString() : "Unknown error");

  const base::DictValue* resource = response->FindDict("resource");
  ASSERT_TRUE(resource);

  // The request is blocked, so success should be false.
  EXPECT_FALSE(resource->FindBool("success").value_or(true));

  EXPECT_EQ(resource->FindInt("netError").value_or(0),
            net::ERR_NETWORK_ACCESS_REVOKED);

  DetachProtocolClient();
}

IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistDevToolsTest,
    DevToolsLoadNetworkResourceEnforcesConnectionAllowlistOnServiceWorker) {
  RegisterResponse(
      "/sw.js",
      ResponseEntry(
          "self.addEventListener('install', e => self.skipWaiting());\n"
          "self.addEventListener('activate', e => "
          "e.waitUntil(self.clients.claim()));\n"
          "self.addEventListener('fetch', event => {});",
          {{"Content-Type", "text/javascript"},
           {"Connection-Allowlist", "(response-origin)"}}));

  RegisterResponse(kSameOriginAllowlistedPage,
                   ResponseEntry("<html><body>Hello</body></html>", {}));

  RegisterResponse(
      "/cross-origin-resource",
      ResponseEntry("allowed-content", {{"Access-Control-Allow-Origin", "*"}}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL cross_origin_url =
      embedded_https_test_server().GetURL("b.test", "/cross-origin-resource");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  AttachToWebContents(shell()->web_contents());

  // Enable auto-attach to attach the Service Worker.
  base::DictValue auto_attach_params;
  auto_attach_params.Set("autoAttach", true);
  auto_attach_params.Set("waitForDebuggerOnStart", false);
  auto_attach_params.Set("flatten", true);
  SendCommandSync("Target.setAutoAttach", std::move(auto_attach_params));

  // Register and activate the Service Worker.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
              (async () => {
                const reg = await navigator.serviceWorker.register('/sw.js');
                await new Promise(resolve => {
                  const worker = reg.installing || reg.waiting || reg.active;
                  if (worker.state === 'activated') {
                    resolve();
                  } else {
                    worker.addEventListener('statechange', () => {
                      if (worker.state === 'activated') {
                        resolve();
                      }
                    });
                  }
                });
                return !!navigator.serviceWorker.controller;
              })();
            )"));

  // Get session id of the service worker target.
  auto notification = WaitForNotification("Target.attachedToTarget", true);
  const std::string* session_id_ptr = notification.FindString("sessionId");
  ASSERT_TRUE(session_id_ptr);
  std::string session_id = *session_id_ptr;

  SendSessionCommand("Network.enable", base::DictValue(), session_id, true);

  // Load the cross-origin resource using DevTools
  // Network.loadNetworkResource from the Service Worker target session.
  // Since the Service Worker has Connection-Allowlist: (response-origin),
  // which only allows connections to its own origin (a.test),
  // the fetch to b.test via DevTools should fail/be blocked.
  base::DictValue load_params;
  load_params.Set("url", cross_origin_url.spec());

  base::DictValue options;
  options.Set("disableCache", true);
  options.Set("includeCredentials", false);
  load_params.Set("options", std::move(options));

  const base::DictValue* response = SendSessionCommand(
      "Network.loadNetworkResource", std::move(load_params), session_id, true);
  ASSERT_TRUE(response) << (error() ? error()->DebugString() : "Unknown error");

  const base::DictValue* resource = response->FindDict("resource");
  ASSERT_TRUE(resource);

  // The request is blocked, so success should be false.
  EXPECT_FALSE(resource->FindBool("success").value_or(true));

  EXPECT_EQ(resource->FindInt("netError").value_or(0),
            net::ERR_NETWORK_ACCESS_REVOKED);

  DetachProtocolClient();
}

// Verifies that if the initiating document's Connection-Allowlist blocks the
// prefetch URL, the speculation rules prefetch is blocked immediately during
// eligibility check. It is not intercepted by the controlling Service Worker.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    SpeculationRulesPrefetchServiceWorkerBlockedByDocumentAllowlist) {
  RegisterResponse("/sw.js",
                   ResponseEntry(R"(
          self.addEventListener('install', e => self.skipWaiting());
          self.addEventListener('activate', e =>
          e.waitUntil(self.clients.claim()));
          self.addEventListener('fetch', event => {
            if (event.request.url.indexOf('controlled-page') !== -1) {
              event.waitUntil(
                caches.open('prefetch-intercepted').then(cache => {
                  return cache.put('/intercepted', new Response('true'));
                })
              );
              event.respondWith(fetch(event.request));
            }
          });
      )",
                                 {{"Content-Type", "text/javascript"}}));

  // /register.html has no connection allowlist so it can register the Service
  // Worker.
  RegisterResponse(
      "/register.html",
      ResponseEntry("<html><body>Register page</body></html>", {}));

  // /initiator.html has Connection-Allowlist: () which blocks all
  // connections (including the prefetch).
  RegisterResponse("/initiator.html",
                   ResponseEntry(R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["/controlled-page"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Initiator page</body>
        </html>
      )",
                                 {{"Connection-Allowlist", "()"}}));

  RegisterResponse("/controlled-page",
                   ResponseEntry("controlled-page-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL register_url =
      embedded_https_test_server().GetURL("a.test", "/register.html");
  GURL initiator_url =
      embedded_https_test_server().GetURL("a.test", "/initiator.html");
  GURL controlled_url =
      embedded_https_test_server().GetURL("a.test", "/controlled-page");

  // Go to the register page.
  EXPECT_TRUE(NavigateToURL(shell(), register_url));
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register(
                  '/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return reg.active && reg.active.state === 'activated';
            })();
          )"));

  // Navigate to the initiator page, which initiates the prefetch.
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  // The prefetch should be blocked by the initiator document's
  // Connection-Allowlist.
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      controlled_url, PrefetchContainer::LoadState::kFailedIneligible,
      PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist));

  // Verify that the Service Worker did not intercept the prefetch request.
  EXPECT_EQ(false, EvalJs(shell()->web_contents(),
                          R"(
                            caches.open('prefetch-intercepted')
                              .then(c => c.match('/intercepted'))
                              .then(r => !!r)
                          )"));
}

// Verifies that a speculation rules prefetch controlled by a Service Worker is
// subject to the Service Worker's Connection-Allowlist. The document does not
// have a connection allow (so it allows the prefetch). It is then intercepted
// by the controlling Service Worker. Then the prefetch is subject to the
// Service Worker's allowlist. The fetch gets blocked because it does not match
// the Service Worker's allowlist.
IN_PROC_BROWSER_TEST_F(
    ConnectionAllowlistTest,
    SpeculationRulesPrefetchServiceWorkerBlockedByServiceWorkerAllowlist) {
  // Service Worker has Connection-Allowlist: () which blocks all connections.
  RegisterResponse("/sw.js", ResponseEntry(R"(
          self.addEventListener('install', e => self.skipWaiting());
          self.addEventListener('activate', e =>
          e.waitUntil(self.clients.claim()));
          self.addEventListener('fetch', event => {
            if (event.request.url.indexOf('controlled-page') !== -1) {
              event.waitUntil(
                caches.open('prefetch-intercepted').then(cache => {
                  return cache.put('/intercepted', new Response('true'));
                })
              );
              event.respondWith(fetch(event.request));
            }
          });
      )",
                                           {{"Content-Type", "text/javascript"},
                                            {"Connection-Allowlist", "()"}}));

  RegisterResponse(
      "/register.html",
      ResponseEntry("<html><body>Register page</body></html>", {}));

  // /initiator.html has no connection allowlist (so it allows the prefetch).
  RegisterResponse("/initiator.html", ResponseEntry(
                                          R"(
        <html>
          <head>
            <script type="speculationrules">
            {
              "prefetch": [
                {
                  "source": "list",
                  "urls": ["/controlled-page"],
                  "eagerness": "immediate"
                }
              ]
            }
            </script>
          </head>
          <body>Initiator page</body>
        </html>
      )",
                                          {}));

  RegisterResponse("/controlled-page",
                   ResponseEntry("controlled-page-content", {}));

  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL register_url =
      embedded_https_test_server().GetURL("a.test", "/register.html");
  GURL initiator_url =
      embedded_https_test_server().GetURL("a.test", "/initiator.html");
  GURL controlled_url =
      embedded_https_test_server().GetURL("a.test", "/controlled-page");

  URLLoaderMonitor monitor;

  // Go to the register page and register the Service Worker.
  EXPECT_TRUE(NavigateToURL(shell(), register_url));
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         R"(
            (async () => {
              const reg = await navigator.serviceWorker.register('/sw.js');
              await new Promise(resolve => {
                const worker = reg.installing || reg.waiting || reg.active;
                if (worker.state === 'activated') {
                  resolve();
                } else {
                  worker.addEventListener('statechange', () => {
                    if (worker.state === 'activated') {
                      resolve();
                    }
                  });
                }
              });
              return reg.active && reg.active.state === 'activated';
            })();
          )"));

  // Navigate to the initiator page, which initiates the prefetch.
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  // The prefetch is intercepted by the Service Worker. Since the Service
  // Worker's allowlist is empty (), the prefetch by the Service Worker is
  // blocked. The speculation rules prefetch container completes with the net
  // error failure status.
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      controlled_url, PrefetchContainer::LoadState::kFailed,
      PrefetchStatus::kPrefetchFailedNetError));
  EXPECT_EQ(monitor.WaitForRequestCompletion(controlled_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);

  // Verify that the Service Worker did intercept the prefetch request.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(shell()->web_contents(),
                  R"(
                           caches.open('prefetch-intercepted')
                             .then(c => c.match('/intercepted'))
                             .then(r => !!r)
                         )")
        .ExtractBool();
  }));
}

// TODO(crbug.com/40256092): Re-enable this test when we can resolve the
// flakiness.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       DISABLED_LinkHeaderRecursivePrefetch) {
  auto server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(server_handle);

  const char prefetch_path[] = "/prefetch.html";
  const char preload_path[] = "/preload.js";

  GURL main_url =
      embedded_https_test_server().GetURL("a.test", kSameOriginAllowlistedPage);
  GURL prefetch_url =
      embedded_https_test_server().GetURL("b.test", prefetch_path);
  GURL preload_url =
      embedded_https_test_server().GetURL("c.test", preload_path);

  // Register the main page response, which performs a prefetch to our
  // cross-origin prefetch_url as a main resource. This satisfies the
  // prerequisite for recursive prefetch to occur. This URL satisfies the
  // provided allowlist because it matches the b.test URLPattern, so this
  // cross-origin main resource prefetch should succeed.
  RegisterResponse(
      kSameOriginAllowlistedPage,
      ResponseEntry(
          "<html><body>Hello</body></html>",
          {
              {"Connection-Allowlist",
               R"((response-origin "*://b.test:*/prefetch.html"))"},
              {"Link", absl::StrFormat("<%s>; rel=prefetch; as=document",
                                       prefetch_url.spec())},
          }));

  // Register the prefetch URL response, which performs a preload to the
  // cross-origin preload_url. This gets converted to a prefetch. It should not
  // be allowed because its origin (c.test) does not satisfy the allowlist.
  RegisterResponse(
      prefetch_path,
      ResponseEntry("<html><body>Prefetch</body></html>",
                    {{"Link", absl::StrFormat("<%s>; rel=preload; as=script",
                                              preload_url.spec())},
                     {"Access-Control-Allow-Origin", "*"}}));

  // Register the preload URL response, which would be fetched by the above
  // recursive prefetch, but that should fail.
  RegisterResponse(preload_path,
                   ResponseEntry("console.log('recursive prefetch')",
                                 {{"Access-Control-Allow-Origin", "*"}}));

  URLLoaderMonitor monitor;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Two cross-origin prefetches fire: the main one, and the recursive one.
  monitor.WaitForUrls({prefetch_url, preload_url});

  // Main prefetch
  EXPECT_EQ(monitor.WaitForRequestCompletion(prefetch_url).error_code, net::OK);
  std::optional<network::ResourceRequest> prefetch_request =
      monitor.GetRequestInfo(prefetch_url);
  ASSERT_TRUE(prefetch_request.has_value());
  EXPECT_EQ(prefetch_request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kPrefetch));

  // Recursive prefetch, which fails because it doesn't match the Connection
  // Allowlist.
  EXPECT_EQ(monitor.WaitForRequestCompletion(preload_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
  std::optional<network::ResourceRequest> preload_request =
      monitor.GetRequestInfo(preload_url);
  ASSERT_TRUE(preload_request.has_value());
  EXPECT_EQ(preload_request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kPrefetch));
}

// `RenderFrameHost::GetConnectionAllowlists()` exposes the Connection-Allowlist
// committed for the document. An enforced allowlist populates `enforced`; a
// report-only allowlist populates `report_only`; a document with no allowlist
// has neither. Browser-process features such as NoStatePrefetch use the
// enforced allowlist to opt out of behavior that is not yet compatible with
// Connection-Allowlist enforcement.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, GetConnectionAllowlists) {
  RegisterResponse(
      "/enforced.html",
      ResponseEntry("<html><body>enforced</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse("/report-only.html",
                   ResponseEntry("<html><body>report-only</body></html>",
                                 {{"Connection-Allowlist-Report-Only",
                                   "(response-origin)"}}));
  RegisterResponse("/none.html",
                   ResponseEntry("<html><body>none</body></html>", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  // An enforced Connection-Allowlist populates `enforced`.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                         "a.test", "/enforced.html")));
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetConnectionAllowlists()
                  .enforced.has_value());

  // A report-only Connection-Allowlist populates `report_only`, not `enforced`.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                         "a.test", "/report-only.html")));
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetPrimaryMainFrame()
                   ->GetConnectionAllowlists()
                   .enforced.has_value());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetConnectionAllowlists()
                  .report_only.has_value());

  // No Connection-Allowlist leaves both empty.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_https_test_server().GetURL("a.test", "/none.html")));
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetPrimaryMainFrame()
                   ->GetConnectionAllowlists()
                   .enforced.has_value());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetPrimaryMainFrame()
                   ->GetConnectionAllowlists()
                   .report_only.has_value());
}

// Connection-Allowlist embedded enforcement (the `connectionallowlist` iframe
// attribute). This fixture enables the runtime feature so the renderer delivers
// the attribute. See https://github.com/WICG/connection-allowlists/issues/1.
class ConnectionAllowlistEmbeddedEnforcementTest
    : public ConnectionAllowlistTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ConnectionAllowlistTest::SetUpCommandLine(command_line);
    // The renderer only delivers the `connectionallowlist` attribute when the
    // ConnectionAllowlistEmbeddedEnforcement runtime feature is enabled, which
    // in turn depends on the ConnectionAllowlist feature.
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "ConnectionAllowlist,ConnectionAllowlistEmbeddedEnforcement");
  }
};

// The renderer parses the `connectionallowlist` attribute into a
// ConnectionAllowlist and delivers it to the browser's FrameTreeNode, and the
// `connectionAllowlist` IDL attribute reflects the content attribute.
// (Enforcement of the delivered value is added in a follow-up CL.)
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       AttributeDeliveredToBrowser) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  constexpr char kAllowlist[] = R"(("https://good.test/" response-origin))";

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.id = 'test_iframe';
        f.setAttribute('connectionallowlist', $1);
        f.src = $2;
        document.body.appendChild(f);
      )",
                                                kAllowlist, child_url)));
  observer.Wait();

  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);
  FrameTreeNode* child =
      static_cast<RenderFrameHostImpl*>(child_rfh)->frame_tree_node();

  // The browser received the parsed ConnectionAllowlist on the child's
  // FrameTreeNode. `response-origin` resolution is deferred (the browser
  // resolves it against the frame's response origin later), so it sets
  // `match_response_origin` rather than appearing in the allowlist.
  ASSERT_TRUE(child->connection_allowlist_attribute().has_value());
  const network::ConnectionAllowlist& parsed =
      child->connection_allowlist_attribute().value();
  EXPECT_EQ(parsed.allowlist, std::vector<std::string>({"https://good.test/"}));
  EXPECT_TRUE(parsed.match_response_origin);

  // The `connectionAllowlist` IDL attribute reflects the content attribute.
  EXPECT_EQ(
      kAllowlist,
      EvalJs(shell()->web_contents(),
             "document.getElementById('test_iframe').connectionAllowlist"));
}

// Once a valid `connectionallowlist` value has been delivered to the browser,
// setting the attribute to an invalid value does not clear the browser's
// requirement (fail closed): the invalid value is dropped in the renderer
// without notifying the browser, so the last valid requirement stays in effect.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       InvalidAttributeKeepsPreviousRequirement) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  constexpr char kAllowlist[] = R"(("https://good.test/"))";

  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.id = 'test_iframe';
        f.setAttribute('connectionallowlist', $1);
        f.src = $2;
        document.body.appendChild(f);
      )",
                                                kAllowlist, child_url)));
  observer.Wait();

  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);
  FrameTreeNode* child =
      static_cast<RenderFrameHostImpl*>(child_rfh)->frame_tree_node();
  ASSERT_TRUE(child->connection_allowlist_attribute().has_value());
  EXPECT_EQ(child->connection_allowlist_attribute().value().allowlist,
            std::vector<std::string>({"https://good.test/"}));

  // Set the attribute to an invalid (header-injecting) value. The renderer
  // drops it and does not notify the browser, so the delivered requirement is
  // unchanged.
  constexpr char kInvalidAllowlist[] = "(response-origin)\nX-Injected: evil";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
      document.getElementById('test_iframe').setAttribute(
          'connectionallowlist', $1);
  )",
                                                        kInvalidAllowlist)));
  // Round-trip through the main frame's renderer to flush any would-be IPC.
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "true"));

  ASSERT_TRUE(child->connection_allowlist_attribute().has_value());
  EXPECT_EQ(child->connection_allowlist_attribute().value().allowlist,
            std::vector<std::string>({"https://good.test/"}));
}

// Tests network requests made by FedCM API are checked against the initiator
// frame's connection allowlist.
//
// The tests exercise the FedCM flow for the following requests:
// - Fetch of the well-known file (and possible redirect).
// - Fetch of the config file.
// - Token.
// - Images (e.g., account avatar).
// - Top-level navigation triggered by the "redirect_to" in the token endpoint
//   response.
// - Client metadata.
// - Accounts.
// - Disconnect.
//
// For each request, the tests verify the request is allowed when the URL
// matches the connection allowlist URL patterns, otherwise the request is
// blocked.

class ConnectionAllowlistFedCmTest : public ConnectionAllowlistTest,
                                     public TestDevToolsProtocolClient {
 public:
  ConnectionAllowlistFedCmTest() = default;
  ~ConnectionAllowlistFedCmTest() override = default;

  void SetUpOnMainThread() override {
    ConnectionAllowlistTest::SetUpOnMainThread();

    test_browser_client_ =
        std::make_unique<webid::WebIdTestContentBrowserClient>();
    SetTestIdentityRequestDialogController(kAccountID);
  }

  void SetTestIdentityRequestDialogController(
      std::optional<std::string> dialog_selected_account) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        std::move(dialog_selected_account), /*web_contents=*/nullptr);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

  // Register the required responses by the FedCM API.
  void RegisterFedCmResponses() {
    ASSERT_TRUE(embedded_https_test_server().Start());

    GURL config_url =
        embedded_https_test_server().GetURL(kIdpHost, kConfigPath);
    GURL avatar_url =
        embedded_https_test_server().GetURL(kIdpHost, kAvatarPath);

    RegisterResponse(
        kWellKnownPath,
        ResponseEntry(
            absl::StrFormat(R"({"provider_urls": ["%s"]})", config_url.spec()),
            {{"Content-Type", "application/json"}}));
    RegisterResponse(kConfigPath,
                     ResponseEntry(R"({
      "accounts_endpoint": "/accounts",
      "id_assertion_endpoint": "/token",
      "login_url": "/login",
      "client_metadata_endpoint": "/client_metadata",
      "disconnect_endpoint": "/disconnect"
    })",
                                   {{"Content-Type", "application/json"}}));

    RegisterResponse(
        kAccountPath,
        ResponseEntry(absl::StrFormat(R"({
          "accounts": [{
            "id": "%s",
            "given_name": "Jane",
            "name": "Jane Doe",
            "email": "jane@example.com",
            "picture": "%s"
          }]
        })",
                                      kAccountID, avatar_url.spec()),
                      {{"Content-Type", "application/json"}}));
    RegisterResponse(kTokenPath,
                     ResponseEntry(R"({"token": "fake_token"})",
                                   {{"Content-Type", "application/json"}}));
    RegisterResponse(kLoginPath, ResponseEntry("login", {}));

    RegisterResponse(
        kClientMetadataPath,
        ResponseEntry("{}", {{"Content-Type", "application/json"}}));
    RegisterResponse(
        kAvatarPath,
        ResponseEntry(kImageBytes, {{"Content-Type", "image/png"},
                                    {"Cache-Control", "max-age=3600"}}));

    RegisterResponse(
        kDisconnectPath,
        ResponseEntry(absl::StrFormat(R"({"account_id": "%s"})", kAccountID),
                      {{"Content-Type", "application/json"}}));
  }

  GURL MainURL() const {
    return embedded_https_test_server().GetURL("a.test",
                                               kSameOriginAllowlistedPage);
  }

  GURL IdpURL() const {
    return embedded_https_test_server().GetURL(kIdpHost,
                                               kSameOriginAllowlistedPage);
  }

  GURL ConfigURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kConfigPath);
  }

  GURL WellKnownURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kWellKnownPath);
  }

  GURL AccountsURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kAccountPath);
  }

  GURL TokenURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kTokenPath);
  }

  GURL AvatarURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kAvatarPath);
  }

  GURL ClientMetadataURL() const {
    return embedded_https_test_server().GetURL(
        kIdpHost,
        base::StrCat({kClientMetadataPath, "?client_id=", kAccountID}));
  }

  GURL DisconnectURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kDisconnectPath);
  }

  void RegisterConnectionAllowlistResponse(std::string_view allowlist) {
    RegisterResponse(
        kSameOriginAllowlistedPage,
        ResponseEntry("<html><body>Hello</body></html>",
                      {{"Connection-Allowlist", std::string(allowlist)}}));
  }

  std::string RunFedCmScript(const ToRenderFrameHost& execution_target) {
    return EvalJs(execution_target, JsReplace(kFedCmScript, ConfigURL()))
        .ExtractString();
  }

  std::string RunFedCmDisconnectScript() {
    return EvalJs(shell()->web_contents(),
                  JsReplace(kFedCmDisconnectScript, ConfigURL()))
        .ExtractString();
  }

  std::unique_ptr<WebContentsConsoleObserver> CreateConsoleObserver(
      std::string_view expected_substr) {
    auto observer =
        std::make_unique<WebContentsConsoleObserver>(shell()->web_contents());
    observer->SetPattern(base::StrCat({"*", expected_substr, "*"}));
    return observer;
  }

 private:
  base::test::ScopedFeatureList fedcm_feature_list_{
      features::kFedCmPreservePortsForTesting};
  std::unique_ptr<webid::WebIdTestContentBrowserClient> test_browser_client_;
};

// FedCM API's fetch of the well-known file is blocked by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmWellKnownBlocked) {
  // FedCM API initiates the fetch of well-known file and the config file in
  // parallel. The connection allowlist allows the config file URL but not the
  // well-known file URL. This ensures the console error from the fetch of
  // well-known file does not get overridden.
  RegisterConnectionAllowlistResponse(R"(("*://*:*/fedcm.json"))");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  AttachToWebContents(shell()->web_contents());
  SendCommandSync("Network.enable");

  std::optional<std::string> well_known_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kWellKnownFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(well_known_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kWellKnownNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(well_known_response_error.value());

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Because FedCM avoids exposing specific errors to the website, all failures
  // lead to "Error retrieving a token".
  EXPECT_THAT(RunFedCmScript(shell()->web_contents()),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Verify that DevTools received Network.requestWillBeSent for the blocked
  // well-known request and that its method is "GET".
  auto matches_well_known_get = [](const std::string& expected_url,
                                   const base::DictValue& params) {
    const std::string* url = params.FindStringByDottedPath("request.url");
    const std::string* method = params.FindStringByDottedPath("request.method");
    return url && *url == expected_url && method && *method == "GET";
  };
  base::DictValue notification = WaitForMatchingNotification(
      "Network.requestWillBeSent",
      base::BindRepeating(matches_well_known_get, WellKnownURL().spec()));
  EXPECT_FALSE(notification.empty());

  DetachProtocolClient();

  // Verify the config file request completed successfully.
  ExpectRequestsSucceeded(monitor, {ConfigURL()});

  // The request to well-known should be blocked, then accounts and token will
  // not be requested.
  EXPECT_FALSE(monitor.GetRequestInfo(WellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the fetch of the well-known file.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// Similar to the test `FedCmWellKnownBlocked`, but runs the FedCM API in a
// cross-origin iframe. FedCM API's fetch of the well-known file initiated from
// the iframe is blocked by the connection allowlist associated with the iframe,
// which does not allow the request URL.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmCrossOriginIframeWellKnownBlocked) {
  // FedCM API initiates the fetch of well-known file and the config file in
  // parallel. The connection allowlist allows the config file URL but not the
  // well-known file URL. This ensures the console error from the fetch of
  // well-known file does not get overridden.
  // Note in this test, it is the cross-origin iframe that will receive the
  // response that contains the connection allowlist.
  RegisterConnectionAllowlistResponse(R"(("*://*:*/fedcm.json"))");

  RegisterResponse(
      "/iframe.html",
      ResponseEntry("<html><body><iframe id='child' "
                    "allow='identity-credentials-get'></iframe></body></html>",
                    {}));

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_https_test_server().GetURL("c.test", "/iframe.html")));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child", MainURL()));

  // The child frame should be cross origin to the main frame.
  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(iframe);
  ASSERT_FALSE(main_frame->GetLastCommittedOrigin().IsSameOriginWith(
      iframe->GetLastCommittedOrigin()));

  std::optional<std::string> well_known_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kWellKnownFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(well_known_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kWellKnownNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(well_known_response_error.value());

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Because FedCM avoids exposing specific errors to the website, all failures
  // lead to "Error retrieving a token".
  EXPECT_THAT(RunFedCmScript(iframe),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Verify the config file request completed successfully.
  ExpectRequestsSucceeded(monitor, {ConfigURL()});

  // The request to well-known should be blocked, then accounts and token will
  // not be requested.
  EXPECT_FALSE(monitor.GetRequestInfo(WellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the fetch of the well-known file.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// FedCM API's fetch of the well-known file is redirected, and redirects are
// allowed by the connection allowlist (`redirects=allow`). Note: The well-known
// file request is the only request that can follow redirect. All other requests
// by FedCM API do not follow redirects.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmWellKnownRedirectAllowed) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), kWellKnownPath);

  // The connection allowlist allows the initial request URL of the well-known
  // file fetch and the redirect target. It also allows redirects as it has
  // `redirects=allow`.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/another/.well-known/web-identity"
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/accounts"
                 "*://b.test:*/token"
               );redirects=allow
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  // Unregister the response registered by the setup helper function because the
  // redirect needs to be done using a ControllableHttpResponse.
  UnregisterResponse(kWellKnownPath);
  RegisterResponse("/another/.well-known/web-identity",
                   ResponseEntry(absl::StrFormat(R"({"provider_urls": ["%s"]})",
                                                 ConfigURL().spec()),
                                 {{"Content-Type", "application/json"}}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Trigger FedCM request and save the promise.
  // ExecuteScriptAsync is used because the request will hang waiting for the
  // ControllableHttpResponse to respond.
  ExecuteScriptAsync(shell()->web_contents(),
                     base::StrCat({"window.fed_cm_promise = ",
                                   JsReplace(kFedCmScript, ConfigURL())}));
  controllable_response.WaitForRequest();

  // Redirect to the target URL.
  GURL target_url = embedded_https_test_server().GetURL(
      kIdpHost, "/another/.well-known/web-identity");
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // Wait for the FedCM's fetch of the well-known file to succeed because the
  // redirect is allowed by connection allowlist (`redirects=allow`).
  EXPECT_EQ(
      EvalJs(shell()->web_contents(), "window.fed_cm_promise").ExtractString(),
      "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // There should not be any console error on the fetch of the well-known file.
  std::optional<std::string> well_known_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kWellKnownFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(well_known_response_error);
  EXPECT_THAT(console_observer.messages(),
              Not(Contains(AnyOf(
                  HasConsoleMessage(webid::GetConsoleErrorMessageFromResult(
                      FederatedRequestResult::kWellKnownNoResponse)),
                  HasConsoleMessage(well_known_response_error.value())))));
}

// FedCM API's fetch of the well-known file is redirected, but redirects are
// blocked by the connection allowlist. Note: The well-known file request is the
// only request that can follow redirect. All other requests by FedCM API do not
// follow redirects.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmWellKnownRedirectBlocked) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), kWellKnownPath);

  // The connection allowlist patterns match both the initial request URL and
  // the redirected URL of the well-known file. But it does not allow any
  // redirect as it has `redirects=block`.
  RegisterConnectionAllowlistResponse(
      R"(
          (
            "*://b.test:*/.well-known/web-identity"
            "*://b.test:*/another/.well-known/web-identity"
            "*://b.test:*/fedcm.json"
            "*://b.test:*/accounts"
            "*://b.test:*/token"
          );redirects=block
      )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  // Unregister the response registered by the setup helper function because the
  // redirect needs to be done using a ControllableHttpResponse.
  UnregisterResponse(kWellKnownPath);
  RegisterResponse("/another/.well-known/web-identity",
                   ResponseEntry(absl::StrFormat(R"({"provider_urls": ["%s"]})",
                                                 ConfigURL().spec()),
                                 {{"Content-Type", "application/json"}}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  std::optional<std::string> well_known_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(kWellKnownFileStr,
                                                      net::ERR_FAILED);
  ASSERT_TRUE(well_known_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kWellKnownNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(well_known_response_error.value());

  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL()});

  // Trigger FedCM request and save the promise.
  // ExecuteScriptAsync is used because the request will hang waiting for the
  // ControllableHttpResponse to respond.
  ExecuteScriptAsync(shell()->web_contents(),
                     base::StrCat({"window.fed_cm_promise = ",
                                   JsReplace(kFedCmScript, ConfigURL())}));

  controllable_response.WaitForRequest();

  // Redirect to the target URL.
  GURL target_url = embedded_https_test_server().GetURL(
      kIdpHost, "/another/.well-known/web-identity");
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\n"
      "Location: " +
      target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // Wait for the FedCM's fetch of the well-known file to fail because the
  // redirect is not allowed by connection allowlist.
  EXPECT_THAT(
      EvalJs(shell()->web_contents(), "window.fed_cm_promise").ExtractString(),
      HasSubstr(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kError)));

  // The initial request to the `well_known_url` was allowed by the connection
  // allowlist, so it should be observed by the URLLoaderMonitor. But it should
  // fail due to the redirect being blocked with the error code net::ERR_FAILED.
  EXPECT_EQ(monitor.WaitForRequestCompletion(WellKnownURL()).error_code,
            net::ERR_FAILED);

  // Even though the config file fetch succeeds, since well-known file fetch is
  // blocked by connection allowlist, accounts and token will not be requested.
  ExpectRequestsSucceeded(monitor, {ConfigURL()});
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the fetch of the well-known file.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// FedCM API's fetch of the config file is blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmConfigBlocked) {
  // FedCM API initiates the fetch of well-known file and the config file in
  // parallel. The connection allowlist allows the well-known file URL but not
  // the config file URL. This ensures the console error from the fetch of
  // config file does not get overridden.
  RegisterConnectionAllowlistResponse(
      R"(("*://*:*/.well-known/web-identity"))");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  std::optional<std::string> config_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kConfigFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(config_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kConfigNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(config_response_error.value());

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Because FedCM avoids exposing specific errors to the website, all failures
  // lead to "Error retrieving a token".
  EXPECT_THAT(RunFedCmScript(shell()->web_contents()),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Verify the well-known file request completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL()});

  // The request to config should be blocked, then accounts and token will not
  // be requested.
  EXPECT_FALSE(monitor.GetRequestInfo(ConfigURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the fetch of the config file.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// FedCM API's fetch of the well-known file and the config file are allowed by
// the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmWellKnownAndConfigAllowed) {
  // Both the well-known file URL and the config file URL are allowed by the
  // connection allowlist.
  RegisterConnectionAllowlistResponse(
      R"(("*://*:*/fedcm.json" "*://*:*/.well-known/web-identity"))");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // The FedCM script still fails because the connection allowlist does not
  // allow subsequent requests to fetch the accounts. Because FedCM avoids
  // exposing specific errors to the website, all failures lead to "Error
  // retrieving a token".
  EXPECT_THAT(RunFedCmScript(shell()->web_contents()),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Both requests to the `well_known_url` and the `config_url` are allowed by
  // the connection allowlist.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL()});

  // The requests to accounts and token are not allowed by the connection
  // allowlist.
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should not be any console error on the fetch of the well-known file
  // and the config file.
  std::optional<std::string> well_known_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kWellKnownFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  std::optional<std::string> config_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kConfigFileStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(well_known_response_error);
  ASSERT_TRUE(config_response_error);
  EXPECT_THAT(console_observer.messages(),
              Not(Contains(AnyOf(
                  HasConsoleMessage(webid::GetConsoleErrorMessageFromResult(
                      FederatedRequestResult::kWellKnownNoResponse)),
                  HasConsoleMessage(well_known_response_error.value()),
                  HasConsoleMessage(webid::GetConsoleErrorMessageFromResult(
                      FederatedRequestResult::kConfigNoResponse)),
                  HasConsoleMessage(config_response_error.value())))));
}

// FedCM API's fetch of the accounts is allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmAccountsAllowed) {
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/token"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/login"
                 "*://b.test:*/accounts"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Trigger FedCM.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // There should not be any console error on the request to the accounts URL.
  std::optional<std::string> accounts_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kAccountStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(accounts_response_error);
  EXPECT_THAT(console_observer.messages(),
              Not(Contains(AnyOf(
                  HasConsoleMessage(webid::GetConsoleErrorMessageFromResult(
                      FederatedRequestResult::kAccountsNoResponse)),
                  HasConsoleMessage(accounts_response_error.value())))));
}

// FedCM API's fetch of the accounts file is blocked by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmAccountsBlocked) {
  // Allow required request URLs but not accounts.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/token"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/login"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  std::optional<std::string> accounts_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kAccountStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(accounts_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kAccountsNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(accounts_response_error.value());

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Trigger FedCM. It should fail because `accounts_url` is blocked by the
  // connection allowlist.
  EXPECT_THAT(RunFedCmScript(shell()->web_contents()),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Verify the requests to the well-known and config completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL()});

  // The request to accounts should be blocked, then the token will not be
  // requested.
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the request to the accounts URL.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// FedCM API's fetch of the token is allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmTokenAllowed) {
  RegisterConnectionAllowlistResponse(R"(
              (
                response-origin
                "*://b.test:*/fedcm.json"
                "*://b.test:*/.well-known/web-identity"
                "*://b.test:*/accounts"
                "*://b.test:*/login"
                "*://b.test:*/token"
              )
            )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // The script should return "success" because the token request is allowed by
  // the connection allowlist.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // There should not be any console error on the fetch of the token.
  std::optional<std::string> token_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kTokenStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(token_response_error);
  EXPECT_THAT(console_observer.messages(),
              Not(Contains(AnyOf(
                  HasConsoleMessage(webid::GetConsoleErrorMessageFromResult(
                      FederatedRequestResult::kIdTokenNoResponse)),
                  HasConsoleMessage(token_response_error.value())))));
}

// FedCM API's fetch of the token is blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmTokenBlocked) {
  RegisterConnectionAllowlistResponse(R"(
              (
                response-origin
                "*://b.test:*/fedcm.json"
                "*://b.test:*/.well-known/web-identity"
                "*://b.test:*/accounts"
                "*://b.test:*/login"
              )
            )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  AttachToWebContents(shell()->web_contents());
  SendCommandSync("Network.enable");

  std::optional<std::string> token_response_error =
      webid::ComputeConsoleMessageForHttpResponseCode(
          kTokenStr, net::ERR_NETWORK_ACCESS_REVOKED);
  ASSERT_TRUE(token_response_error);

  // Observe the console errors.
  auto fedcm_console_observer =
      CreateConsoleObserver(webid::GetConsoleErrorMessageFromResult(
          FederatedRequestResult::kIdTokenNoResponse));
  auto network_console_observer =
      CreateConsoleObserver(token_response_error.value());

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Because FedCM avoids exposing specific errors to the website, all failures
  // lead to "Error retrieving a token".
  EXPECT_THAT(RunFedCmScript(shell()->web_contents()),
              HasSubstr(webid::GetConsoleErrorMessageFromResult(
                  FederatedRequestResult::kError)));

  // Verify that DevTools received Network.requestWillBeSent for the blocked
  // token request and that its method is "POST".
  auto matches_token_post = [](const std::string& expected_url,
                               const base::DictValue& params) {
    const std::string* url = params.FindStringByDottedPath("request.url");
    const std::string* method = params.FindStringByDottedPath("request.method");
    return url && *url == expected_url && method && *method == "POST";
  };
  base::DictValue notification = WaitForMatchingNotification(
      "Network.requestWillBeSent",
      base::BindRepeating(matches_token_post, TokenURL().spec()));
  EXPECT_FALSE(notification.empty());

  DetachProtocolClient();

  // The requests to well-known, config, and accounts should be allowed.
  ExpectRequestsSucceeded(monitor,
                          {WellKnownURL(), ConfigURL(), AccountsURL()});

  // The request to token should be blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());

  // There should be console errors on the fetch of the token.
  EXPECT_TRUE(fedcm_console_observer->Wait());
  EXPECT_TRUE(network_console_observer->Wait());
}

// FedCM API's fetch of the account picture is allowed by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmImageAllowed) {
  RegisterConnectionAllowlistResponse(R"(
              (
                response-origin
                "*://b.test:*/fedcm.json"
                "*://b.test:*/.well-known/web-identity"
                "*://b.test:*/accounts"
                "*://b.test:*/token"
                "*://b.test:*/login"
                "*://b.test:*/avatar.png"
              )
            )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL(), AvatarURL()});

  // Trigger FedCM. The script should return "success".
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL(), AccountsURL(),
                                    TokenURL(), AvatarURL()});
}

// FedCM API's fetch of the account picture is blocked by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmImageBlocked) {
  RegisterConnectionAllowlistResponse(R"(
              (
                response-origin
                "*://b.test:*/fedcm.json"
                "*://b.test:*/.well-known/web-identity"
                "*://b.test:*/accounts"
                "*://b.test:*/token"
                "*://b.test:*/login"
              )
            )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL(), AvatarURL()});

  // Trigger FedCM. The script should return "success" because the image failure
  // is non-fatal.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify other requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // The request to avatar should be blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(AvatarURL()).has_value());
}

// The FedCM `redirect_to` initiates a top-level navigation. The navigation URL
// is allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmRedirectToAllowed) {
  // Allow all required FedCM requests and the redirect_to target.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
                 "*://b.test:*/target.html"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  GURL target_url =
      embedded_https_test_server().GetURL(kIdpHost, "/target.html");

  // Token response: returns redirect_to to target.html.
  RegisterResponse(kTokenPath,
                   ResponseEntry(absl::StrFormat(R"({"redirect_to": "%s"})",
                                                 target_url.spec()),
                                 {{"Content-Type", "application/json"}}));
  RegisterResponse("/target.html", ResponseEntry("target", {}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Force allow redirect_to for testing.
  webid::RequestService::GetOrCreateForCurrentDocument(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->SetForceAllowRedirectToForTesting(true);

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

  // Trigger FedCM. Because there is a top-level navigation by `redirect_to`,
  // the script resolves to "success" immediately.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify FedCM requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Wait for the top-level navigation triggered by `redirect_to`. It should be
  // allowed.
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(navigation_observer.last_net_error_code(), net::OK);
  EXPECT_EQ(navigation_observer.last_navigation_url(), target_url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), target_url);
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetPageType(), PAGE_TYPE_NORMAL);
}

// The FedCM `redirect_to` initiates a top-level navigation. The navigation URL
// is not allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmRedirectToBlocked) {
  // Allow all required FedCM requests, but not the redirect_to target.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  GURL target_url =
      embedded_https_test_server().GetURL(kIdpHost, "/target.html");

  // Token response: returns redirect_to to target.html.
  RegisterResponse(kTokenPath,
                   ResponseEntry(absl::StrFormat(R"({"redirect_to": "%s"})",
                                                 target_url.spec()),
                                 {{"Content-Type", "application/json"}}));
  RegisterResponse("/target.html", ResponseEntry("target", {}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(
      {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Force allow redirect_to for testing.
  webid::RequestService::GetOrCreateForCurrentDocument(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->SetForceAllowRedirectToForTesting(true);

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);

  // Trigger FedCM. Because there is a top-level navigation by `redirect_to`,
  // the script resolves to "success" immediately.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify FedCM requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // Wait for the top-level navigation triggered by `redirect_to`. It should end
  // up being an error page because it is blocked by the connection allowlist.
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(navigation_observer.last_net_error_code(),
            net::ERR_NETWORK_ACCESS_REVOKED);
  EXPECT_EQ(navigation_observer.last_navigation_url(), target_url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), target_url);
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetPageType(), PAGE_TYPE_ERROR);
}

// FedCM API's fetch of the client metadata file is allowed by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmClientMetadataAllowed) {
  // Allow all required FedCM request URLs explicitly.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/accounts"
                 "*://b.test:*/token"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/login"
                 "*://b.test:*/client_metadata*"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AccountsURL(),
                            TokenURL(), ClientMetadataURL()});

  // Trigger FedCM. It should succeed.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL(), AccountsURL(),
                                    TokenURL(), ClientMetadataURL()});
}

// FedCM API's fetch of the client metadata file is blocked by the connection
// allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest,
                       FedCmClientMetadataBlocked) {
  // Allow required request URLs but not client_metadata.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/accounts"
                 "*://b.test:*/token"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/login"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AccountsURL(),
                            TokenURL(), ClientMetadataURL()});

  // Trigger FedCM. It should succeed because client metadata failure is
  // non-fatal.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify other requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // The request to client metadata should be blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(ClientMetadataURL()).has_value());
}

// FedCM API's disconnect request is allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmDisconnectAllowed) {
  // Allow all required FedCM requests explicitly.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
                 "*://b.test:*/disconnect"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AccountsURL(),
                            TokenURL(), DisconnectURL()});

  // Sign in.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Disconnect. It should succeed.
  EXPECT_EQ(RunFedCmDisconnectScript(), "success");

  // Verify all requests completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL(), AccountsURL(),
                                    TokenURL(), DisconnectURL()});

  // There should not be any console error on the disconnect request.
  EXPECT_THAT(
      console_observer.messages(),
      Not(Contains(HasConsoleMessage(webid::GetDisconnectConsoleErrorMessage(
          webid::DisconnectStatus::kDisconnectFailedOnServer)))));
}

// FedCM API's disconnect request is blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmTest, FedCmDisconnectBlocked) {
  // Allow login flow, but not disconnect.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/avatar.png"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  // Observe the console errors.
  auto disconnect_console_observer =
      CreateConsoleObserver(webid::GetDisconnectConsoleErrorMessage(
          webid::DisconnectStatus::kDisconnectFailedOnServer));

  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AccountsURL(),
                            TokenURL(), DisconnectURL()});

  // Sign in.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Try to disconnect. It should fail because the disconnect URL is blocked.
  EXPECT_NE(RunFedCmDisconnectScript(), "success");

  // Verify other requests completed successfully.
  ExpectRequestsSucceeded(
      monitor, {WellKnownURL(), ConfigURL(), AccountsURL(), TokenURL()});

  // The request to disconnect should be blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(DisconnectURL()).has_value());

  // There should be a console error on the disconnect request.
  EXPECT_TRUE(disconnect_console_observer->Wait());
}

// The request for cached account pictures requires enabling FedCM lightweight
// mode feature.
class ConnectionAllowlistFedCmLightweightModeTest
    : public ConnectionAllowlistFedCmTest {
 public:
  ConnectionAllowlistFedCmLightweightModeTest() = default;
  ~ConnectionAllowlistFedCmLightweightModeTest() override = default;

 private:
  base::test::ScopedFeatureList lightweight_mode_feature_list_{
      features::kFedCmLightweightMode};
};

// FedCM API's request for cached account pictures is labelled with load flag:
// `LOAD_ONLY_FROM_CACHE`. Even though it is a request for HTTP cache entries,
// the connection allowlist should still apply.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmLightweightModeTest,
                       FedCmCacheAccountPicturesAllowed) {
  // Note this connection allowlist will be applied to both the `main_url` and
  // the `idp_url`. They have different origins so `response-origin` will be
  // evaluated differently.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
                 "*://b.test:*/avatar.png"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  // Navigate to IDP (b.test) and store the account via setStatus.
  // On IDP, the account image URL "b.test/avatar.png" matches the allowlist
  // pattern (response-origin) and is successfully downloaded and stored in the
  // HTTP cache.
  EXPECT_TRUE(NavigateToURL(shell(), IdpURL()));

  {
    URLLoaderMonitor url_loader_monitor({AvatarURL()});

    // The account image URL is stored.
    EXPECT_EQ(
        EvalJs(shell()->web_contents(), JsReplace(R"((async () => {
                 await navigator.login.setStatus("logged-in", {
                   accounts: [{
                     id: $1,
                     name: "Jane Doe",
                     email: "jane@example.com",
                     picture: $2
                   }]
                 });
                 return true;
               })())",
                                                  kAccountID, AvatarURL())),
        true);

    EXPECT_EQ(
        url_loader_monitor.WaitForRequestCompletion(AvatarURL()).error_code,
        net::OK);
  }

  // Navigate to RP (a.test) and trigger FedCM API.
  // The cached account picture request to "b.test/avatar.png" succeeds because
  // it matches the connection allowlist pattern ("*://b.test:*/avatar.png").
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));
  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AvatarURL()});

  // Trigger FedCM. The script should return "success".
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify FedCM requests completed successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL(), AvatarURL()});
  EXPECT_TRUE(monitor.GetRequestInfo(AvatarURL())->load_flags &
              net::LOAD_ONLY_FROM_CACHE);
}

// FedCM API's request for cached account pictures is labelled with load flag:
// `LOAD_ONLY_FROM_CACHE`. Even though it is a request for HTTP cache entries,
// the connection allowlist should still apply.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistFedCmLightweightModeTest,
                       FedCmCacheAccountPicturesBlocked) {
  // Note this connection allowlist will be applied to both the `main_url` and
  // the `idp_url`. They have different origins so `response-origin` will be
  // evaluated differently.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/fedcm.json"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/client_metadata*"
                 "*://b.test:*/token"
                 "*://b.test:*/login"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterFedCmResponses());

  // Navigate to IDP (b.test) and store the account via setStatus.
  // On IDP, the account image URL "b.test/avatar.png" matches the allowlist
  // pattern (response-origin) and is successfully downloaded and stored in the
  // HTTP cache.
  EXPECT_TRUE(NavigateToURL(shell(), IdpURL()));

  {
    URLLoaderMonitor url_loader_monitor({AvatarURL()});

    // The account image URL is stored.
    EXPECT_EQ(
        EvalJs(shell()->web_contents(), JsReplace(R"((async () => {
                 await navigator.login.setStatus("logged-in", {
                   accounts: [{
                     id: $1,
                     name: "Jane Doe",
                     email: "jane@example.com",
                     picture: $2
                   }]
                 });
                 return true;
               })())",
                                                  kAccountID, AvatarURL())),
        true);

    EXPECT_EQ(
        url_loader_monitor.WaitForRequestCompletion(AvatarURL()).error_code,
        net::OK);
  }

  // Navigate to RP (a.test) and trigger FedCM API.
  // The cached account picture request to "b.test/avatar.png" fails because it
  // is not allowed by the connection allowlist. Note the allowlist allows
  // requests that are same-origin with "a.test" and those paths explicitly
  // specified. None of them matches the cached account picture URL.
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));
  URLLoaderMonitor monitor({WellKnownURL(), ConfigURL(), AvatarURL()});

  // Trigger FedCM. It should succeed because the failure to fetch cached
  // account pictures is non-fatal.
  EXPECT_EQ(RunFedCmScript(shell()->web_contents()), "success");

  // Verify FedCM requests to well-known URL and config URL completed
  // successfully.
  ExpectRequestsSucceeded(monitor, {WellKnownURL(), ConfigURL()});

  // The request for cached account picture should be blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(AvatarURL()).has_value());
}

class EmailVerificationTestContentBrowserClient
    : public webid::WebIdTestContentBrowserClient {
 public:
  EmailVerificationTestContentBrowserClient() = default;
  ~EmailVerificationTestContentBrowserClient() override = default;

  // This allows DNS lookups to the embedded test server.
  std::string GetDnsTxtResolverUrlPrefix() override {
    return dns_txt_resolver_url_prefix_;
  }

  void SetDnsTxtResolverUrlPrefix(std::string prefix) {
    dns_txt_resolver_url_prefix_ = std::move(prefix);
  }

 private:
  std::string dns_txt_resolver_url_prefix_;
};

// Tests for the connection allowlist checks applied to network requests made by
// the Email Verification Protocol (EVP) API.
//
// The EVP API can make the following network requests:
// - DNS query.
// - Email verification well-known file (".well-known/email-verification").
// - ID token request ("issuance_endpoint").
// - JWKS URI fetch ("jwks_uri").
// - WebID well-known file ("/.well-known/web-identity").
// - Accounts endpoint ("/accounts").
//
// For each request, the tests verify the request is allowed when the URL
// matches the connection allowlist URL patterns, otherwise the request is
// blocked.
// For redirect, one test case is added for verifying the redirect is either
// allowed or blocked, depending on the value of connection allowlist's redirect
// directive.
class ConnectionAllowlistEmailVerificationTest
    : public ConnectionAllowlistTest,
      public TestDevToolsProtocolClient {
 public:
  ConnectionAllowlistEmailVerificationTest() {
    email_verification_feature_list_.InitWithFeatures(
        {features::kEmailVerificationProtocol,
         features::kFedCmPreservePortsForTesting},
        {});
  }
  ~ConnectionAllowlistEmailVerificationTest() override = default;

  void SetUpOnMainThread() override {
    ConnectionAllowlistTest::SetUpOnMainThread();

    test_browser_client_ =
        std::make_unique<EmailVerificationTestContentBrowserClient>();
  }

  void RegisterEmailVerificationResponses() {
    ASSERT_TRUE(embedded_https_test_server().Start());

    test_browser_client_->SetDnsTxtResolverUrlPrefix(
        embedded_https_test_server()
            .GetURL(kIdpHost, std::string(kDnsPath) + "?name=")
            .spec());

    RegisterResponse(
        kDnsPath,
        ResponseEntry(absl::StrFormat(R"(
          {
            "Status": 0,
            "Answer": [
              {
                "type": 16,
                "data": "iss=b.test:%d"
              }
            ]
          })",
                                      embedded_https_test_server().port()),
                      {{"Content-Type", "application/json"}}));

    RegisterResponse(
        kEmailVerificationWellKnownPath,
        ResponseEntry(absl::StrFormat(R"(
          {
            "issuance_endpoint": "%s",
            "jwks_uri": "%s",
            "signing_alg_values_supported": [
              "RS256"
            ]
          })",
                                      TokenURL().spec(), JwksURL().spec()),
                      {{"Content-Type", "application/json"}}));

    RegisterResponse(
        kWellKnownPath,
        ResponseEntry(
            R"({"accounts_endpoint": "/accounts","login_url": "/login"})",
            {{"Content-Type", "application/json"}}));

    RegisterResponse(
        kAccountPath,
        ResponseEntry(
            R"({"accounts": [{"id": "1234","email": "jane@example.com"}]})",
            {{"Content-Type", "application/json"}}));
    RegisterResponse(
        kTokenPath,
        ResponseEntry(
            R"({"issuance_token": "e30.e30.sig~","token": "e30.e30.sig~"})",
            {{"Content-Type", "application/json"}}));

    RegisterResponse(kJwksPath,
                     ResponseEntry(R"({"keys": []})",
                                   {{"Content-Type", "application/json"}}));
  }

  GURL MainURL() const {
    return embedded_https_test_server().GetURL("a.test",
                                               kSameOriginAllowlistedPage);
  }

  GURL DnsURL() const {
    return embedded_https_test_server().GetURL(
        kIdpHost,
        std::string(kDnsPath) + "?name=_email-verification.example.com");
  }

  GURL EmailVerificationWellKnownURL() const {
    return embedded_https_test_server().GetURL(kIdpHost,
                                               kEmailVerificationWellKnownPath);
  }

  GURL WebIdentityWellKnownURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kWellKnownPath);
  }

  GURL AccountsURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kAccountPath);
  }

  GURL TokenURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kTokenPath);
  }

  // The JWKS (JSON Web Key Set) URI (`jwks_uri`) endpoint (`/jwks`) returns
  // the public keys of the email verification issuer, which are used to verify
  // the signature of the issued Email Verification Token during
  // `EmailVerifier::Verify()`.
  GURL JwksURL() const {
    return embedded_https_test_server().GetURL(kIdpHost, kJwksPath);
  }

  void RegisterConnectionAllowlistResponse(std::string_view allowlist) {
    RegisterResponse(
        kSameOriginAllowlistedPage,
        ResponseEntry("<html><body>Hello</body></html>",
                      {{"Connection-Allowlist", std::string(allowlist)}}));
  }

  std::optional<webid::EmailVerifier::Result> RunCheckIfVerifiable(
      const std::string& email = "jane@example.com") {
    base::test::TestFuture<std::optional<webid::EmailVerifier::Result>> future;
    webid::EmailVerifier::GetOrCreateForFrame(
        shell()->web_contents()->GetPrimaryMainFrame())
        ->CheckIfVerifiable(email, future.GetCallback());
    return future.Get();
  }

  std::optional<std::string> RunVerify(
      const webid::EmailVerifier::Result& result,
      const std::string& nonce = "test_nonce") {
    base::test::TestFuture<std::optional<std::string>> future;
    webid::EmailVerifier::GetOrCreateForFrame(
        shell()->web_contents()->GetPrimaryMainFrame())
        ->Verify(result, nonce, future.GetCallback());
    return future.Get();
  }

  // Returns the request URLs that are expected once `RunCheckIfVerifiable` is
  // run.
  std::vector<GURL> GetCheckIfVerifiableURLs() const {
    return {DnsURL(), EmailVerificationWellKnownURL(),
            WebIdentityWellKnownURL(), AccountsURL()};
  }

  // Returns the request URLs that are expected for the entire Email
  // Verification flow.
  std::vector<GURL> GetAllRequestURLs() const {
    return {DnsURL(),
            EmailVerificationWellKnownURL(),
            WebIdentityWellKnownURL(),
            AccountsURL(),
            TokenURL(),
            JwksURL()};
  }

  static std::set<GURL> ToSet(const std::vector<GURL>& urls) {
    return {urls.begin(), urls.end()};
  }

 private:
  base::test::ScopedFeatureList email_verification_feature_list_;
  std::unique_ptr<EmailVerificationTestContentBrowserClient>
      test_browser_client_;
};

// Email Verification Protocol API's requests during `CheckIfVerifiable` (dns,
// .well-known/email-verification, .well-known/web-identity, and accounts) are
// allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       CheckIfVerifiableAllowed) {
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));
  base::HistogramTester histogram_tester;

  // `RunCheckIfVerifiable()` checks if the email address is verifiable, which
  // initiates requests to the above 4 URLs.
  auto result = RunCheckIfVerifiable();
  EXPECT_TRUE(result.has_value());

  histogram_tester.ExpectUniqueSample("Blink.Evp.Status.IsVerifiable",
                                      EmailVerificationRequestResult::kSuccess,
                                      1);

  // All 4 URLs are allowed by the connection allowlist.
  ExpectRequestsSucceeded(monitor, GetCheckIfVerifiableURLs());
}

// Email Verification Protocol API's fetch of the DNS record is blocked by the
// connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       DnsRequestBlocked) {
  // Allow all required request URLs except the DNS endpoint.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));
  base::HistogramTester histogram_tester;

  // `RunCheckIfVerifiable()` checks if the email address is verifiable, which
  // initiates requests to the above 4 URLs. It fails because the DNS request
  // URL is not allowed by the connection allowlist.
  EXPECT_FALSE(RunCheckIfVerifiable().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kDnsFetchFailed, 1);
  EXPECT_FALSE(monitor.GetRequestInfo(DnsURL()).has_value());

  // The DNS request is blocked, so no subsequent requests should be made.
  EXPECT_FALSE(
      monitor.GetRequestInfo(EmailVerificationWellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(WebIdentityWellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
}

// Email Verification Protocol API's fetch of the DNS record is redirected,
// and redirects are allowed by the connection allowlist (`redirects=allow`).
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       DnsRequestRedirectAllowed) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), kDnsPath, /*relative_url_is_prefix=*/true);

  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/another/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
               );redirects=allow
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());

  // Unregister the default response to `kDnsPath` registered by
  // `RegisterEmailVerificationResponses()`. This is because this test requires
  // a response that redirects the request, which uses the
  // `ControllableHttpResponse`.
  UnregisterResponse(kDnsPath);
  RegisterResponse(
      "/another/dns",
      ResponseEntry(absl::StrFormat(R"(
                    {
                      "Status": 0,
                      "Answer": [
                      {
                        "type": 16,
                        "data": "iss=b.test:%d"
                      }
                    ]
                  })",
                                    embedded_https_test_server().port()),
                    {{"Content-Type", "application/json"}}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  GURL target_url =
      embedded_https_test_server().GetURL(kIdpHost, "/another/dns");
  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));

  // Check if the email address is verifiable, which initiates requests to the
  // above 4 URLs.
  base::test::TestFuture<std::optional<webid::EmailVerifier::Result>> future;
  webid::EmailVerifier::GetOrCreateForFrame(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->CheckIfVerifiable("jane@example.com", future.GetCallback());

  // Redirect the request from `kDnsPath` to "/another/dns".
  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\nLocation: " + target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // All 4 URLs are allowed by the connection allowlist, including the DNS
  // request which has been redirected.
  EXPECT_TRUE(future.Get().has_value());

  ExpectRequestsSucceeded(monitor, GetCheckIfVerifiableURLs());
}

// Email Verification Protocol API's fetch of the DNS record is redirected,
// but redirects are blocked by the connection allowlist (`redirects=block`).
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       DnsRequestRedirectBlocked) {
  net::test_server::ControllableHttpResponse controllable_response(
      &embedded_https_test_server(), kDnsPath, /*relative_url_is_prefix=*/true);

  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/another/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
               );redirects=block
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());

  // Unregister the default response to `kDnsPath` registered by
  // `RegisterEmailVerificationResponses()`. This is because this test requires
  // a response that redirects the request, which uses the
  // `ControllableHttpResponse`.
  UnregisterResponse(kDnsPath);
  RegisterResponse(
      "/another/dns",
      ResponseEntry(absl::StrFormat(R"(
                    {
                      "Status": 0,
                      "Answer": [
                      {
                        "type": 16,
                        "data": "iss=b.test:%d"
                      }
                    ]
                  })",
                                    embedded_https_test_server().port()),
                    {{"Content-Type", "application/json"}}));

  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  GURL target_url =
      embedded_https_test_server().GetURL(kIdpHost, "/another/dns");
  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));

  base::test::TestFuture<std::optional<webid::EmailVerifier::Result>> future;
  base::HistogramTester histogram_tester;
  webid::EmailVerifier::GetOrCreateForFrame(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->CheckIfVerifiable("jane@example.com", future.GetCallback());

  // Redirect the request from `kDnsPath` to "/another/dns".
  controllable_response.WaitForRequest();
  controllable_response.Send(
      "HTTP/1.1 302 Found\r\nLocation: " + target_url.spec() + "\r\n\r\n");
  controllable_response.Done();

  // The redirect is blocked by the connection allowlist.
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kDnsFetchFailed, 1);

  EXPECT_EQ(monitor.WaitForRequestCompletion(DnsURL()).error_code,
            net::ERR_FAILED);

  // The DNS request is blocked, so no subsequent requests should be made.
  EXPECT_FALSE(
      monitor.GetRequestInfo(EmailVerificationWellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(WebIdentityWellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
}

// Email Verification Protocol API's fetch of the email verification well-known
// file is blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       EmailVerificationWellKnownBlocked) {
  // Allow all required request URLs except the email verification well-known
  // endpoint.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  AttachToWebContents(shell()->web_contents());
  SendCommandSync("Network.enable");

  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));
  base::HistogramTester histogram_tester;

  auto result = RunCheckIfVerifiable();
  EXPECT_FALSE(result.has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kEmailVerificationWellKnownNoResponse, 1);

  // Verify that DevTools received Network.requestWillBeSent for the blocked
  // email verification well-known request and that its method is "GET".
  base::DictValue notification = WaitForMatchingNotification(
      "Network.requestWillBeSent",
      base::BindRepeating(&MatchesNetworkRequest,
                          EmailVerificationWellKnownURL().spec(), "GET"));
  EXPECT_FALSE(notification.empty());

  DetachProtocolClient();

  // Verify the initial DNS request (`DnsURL()`) and the WebID well-known and
  // accounts requests (`WebIdentityWellKnownURL()` and `AccountsURL()`)
  // completed successfully.
  ExpectRequestsSucceeded(monitor,
                          {DnsURL(), WebIdentityWellKnownURL(), AccountsURL()});

  // The email verification well-known request is blocked.
  EXPECT_FALSE(
      monitor.GetRequestInfo(EmailVerificationWellKnownURL()).has_value());
}

// Email Verification Protocol API's fetch of the WebID well-known file is
// blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       WebIdentityWellKnownBlocked) {
  // Allow all required request URLs except the WebID well-known endpoint.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/accounts"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));
  base::HistogramTester histogram_tester;

  auto result = RunCheckIfVerifiable();
  EXPECT_FALSE(result.has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kWellKnownNoResponse, 1);

  // Verify the initial DNS request (`DnsURL()`) and email verification
  // well-known request (`EmailVerificationWellKnownURL()`) completed
  // successfully.
  ExpectRequestsSucceeded(monitor, {DnsURL(), EmailVerificationWellKnownURL()});

  // The WebID well-known request is blocked, so the accounts endpoint should
  // not be requested.
  EXPECT_FALSE(monitor.GetRequestInfo(WebIdentityWellKnownURL()).has_value());
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
}

// Email Verification Protocol API's fetch of the accounts endpoint is blocked
// by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       AccountsRequestBlocked) {
  // Allow all required request URLs except the accounts endpoint.
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetCheckIfVerifiableURLs()));
  base::HistogramTester histogram_tester;

  auto result = RunCheckIfVerifiable();
  EXPECT_FALSE(result.has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.IsVerifiable",
      EmailVerificationRequestResult::kAccountsNoResponse, 1);

  // Verify the initial DNS request (`DnsURL()`), the well-known requests
  // (`EmailVerificationWellKnownURL()` and `WebIdentityWellKnownURL()`)
  // completed successfully.
  ExpectRequestsSucceeded(monitor, {DnsURL(), EmailVerificationWellKnownURL(),
                                    WebIdentityWellKnownURL()});

  // The accounts request is blocked because it is not allowed by the connection
  // allowlist.
  EXPECT_FALSE(monitor.GetRequestInfo(AccountsURL()).has_value());
}

// Email Verification Protocol API's requests during `Verify`
// (`issuance_endpoint` and `jwks_uri`) are allowed by the connection allowlist.
// Note this requires the requests in the preceding `CheckIfVerifiable` (dns,
// .well-known/email-verification, .well-known/web-identity, and accounts) also
// being allowed.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       VerifyAllowed) {
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/token"
                 "*://b.test:*/jwks"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetAllRequestURLs()));
  base::HistogramTester histogram_tester;

  // `RunCheckIfVerifiable()` checks if the email address is verifiable, which
  // initiates requests to dns, .well-known/email-verification,
  // .well-known/web-identity, and accounts. All 4 are allowed by the connection
  // allowlist.
  auto result = RunCheckIfVerifiable();
  ASSERT_TRUE(result.has_value());

  histogram_tester.ExpectUniqueSample("Blink.Evp.Status.IsVerifiable",
                                      EmailVerificationRequestResult::kSuccess,
                                      1);

  ExpectRequestsSucceeded(monitor, GetCheckIfVerifiableURLs());
  auto verify_result = RunVerify(result.value());
  // `verify_result` is `std::nullopt` because the test server returns a dummy
  // "e30.e30.sig~" string that does not verify as a valid SD-JWT. The network
  // requests to `TokenURL()` and `JwksURL()` are verified below to complete
  // successfully (`net::OK`), proving they were allowed by the allowlist.
  EXPECT_FALSE(verify_result.has_value());

  ExpectRequestsSucceeded(monitor, {TokenURL(), JwksURL()});
}

// Email Verification Protocol API's fetch of the ID token (`issuance_endpoint`)
// is blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       TokenRequestBlocked) {
  // Allow all required request URLs except the ID token endpoint (`/token`).
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/jwks"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  AttachToWebContents(shell()->web_contents());
  SendCommandSync("Network.enable");

  URLLoaderMonitor monitor(ToSet(GetAllRequestURLs()));

  // `RunCheckIfVerifiable()` checks if the email address is verifiable, which
  // initiates requests to dns, .well-known/email-verification,
  // .well-known/web-identity, and accounts. All 4 are allowed by the connection
  // allowlist.
  auto result = RunCheckIfVerifiable();
  ASSERT_TRUE(result.has_value());

  ExpectRequestsSucceeded(monitor, GetCheckIfVerifiableURLs());

  base::HistogramTester histogram_tester;

  auto verify_result = RunVerify(result.value());
  EXPECT_FALSE(verify_result.has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.Verify",
      EmailVerificationRequestResult::kTokenNoResponse, 1);

  // Verify that DevTools received Network.requestWillBeSent for the blocked
  // token request and that its method is "POST".
  base::DictValue notification = WaitForMatchingNotification(
      "Network.requestWillBeSent",
      base::BindRepeating(&MatchesNetworkRequest, TokenURL().spec(), "POST"));
  EXPECT_FALSE(notification.empty());

  DetachProtocolClient();

  // The JWKS request is allowed.
  ExpectRequestsSucceeded(monitor, {JwksURL()});

  // The token request is blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(TokenURL()).has_value());
}

// Email Verification Protocol API's fetch of the JWKS URI (`jwks_uri`) is
// blocked by the connection allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmailVerificationTest,
                       JwksRequestBlocked) {
  // Allow all required request URLs except the JWKS endpoint (`/jwks`).
  RegisterConnectionAllowlistResponse(R"(
               (
                 response-origin
                 "*://b.test:*/dns*"
                 "*://b.test:*/.well-known/email-verification"
                 "*://b.test:*/.well-known/web-identity"
                 "*://b.test:*/accounts"
                 "*://b.test:*/token"
               )
             )");

  ASSERT_NO_FATAL_FAILURE(RegisterEmailVerificationResponses());
  EXPECT_TRUE(NavigateToURL(shell(), MainURL()));

  URLLoaderMonitor monitor(ToSet(GetAllRequestURLs()));

  // `RunCheckIfVerifiable()` checks if the email address is verifiable, which
  // initiates requests to dns, .well-known/email-verification,
  // .well-known/web-identity, and accounts. All 4 are allowed by the connection
  // allowlist.
  auto result = RunCheckIfVerifiable();
  ASSERT_TRUE(result.has_value());

  ExpectRequestsSucceeded(monitor, GetCheckIfVerifiableURLs());

  base::HistogramTester histogram_tester;
  auto verify_result = RunVerify(result.value());
  EXPECT_FALSE(verify_result.has_value());
  // For Jwks failures, they share the same error code: `kJwksHttpNotFound`,
  // whether due to the connection allowlist, an HTTP 404 error, or an invalid
  // JSON.
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Status.Verify",
      EmailVerificationRequestResult::kJwksHttpNotFound, 1);

  // The token request is allowed.
  ExpectRequestsSucceeded(monitor, {TokenURL()});

  // The JWKS request is blocked.
  EXPECT_FALSE(monitor.GetRequestInfo(JwksURL()).has_value());
}

// When the framed document opts into blanket enforcement via
// `Allow-Connection-Allowlist-From: *`, the embedder's required allowlist (with
// the `response-origin` token resolved against the frame's origin) is installed
// as the frame's enforced allowlist and actually restricts its connections.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       BlanketOptInEnforcesEmbedderAllowlist) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  TestNavigationManager manager(shell()->web_contents(), child_url);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', '(response-origin)');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                                        child_url)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(manager.was_successful());

  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  // The required allowlist (the embedder's requirement, with `response-origin`
  // resolved against the frame's origin) is propagated to the committed frame.
  auto* child_impl = static_cast<RenderFrameHostImpl*>(child_rfh);
  ASSERT_TRUE(child_impl->required_connection_allowlist().has_value());
  EXPECT_EQ(
      child_impl->required_connection_allowlist()->allowlist,
      std::vector<std::string>({url::Origin::Create(child_url).Serialize()}));

  // The installed allowlist is actually enforced on the frame's connections: a
  // same-origin WebSocket is allowed, a cross-origin one is blocked.
  GURL allowed_ws_url = net::test_server::GetWebSocketURL(
      embedded_https_test_server(), "a.test", "/echo-with-no-extension");
  EXPECT_EQ("open", EvalJs(child_rfh, JsReplace(R"(
    new Promise(resolve => {
      const ws = new WebSocket($1);
      ws.onopen = () => { ws.close(); resolve('open'); };
      ws.onerror = () => resolve('error');
    });
  )",
                                                allowed_ws_url)));

  GURL denied_ws_url = net::test_server::GetWebSocketURL(
      embedded_https_test_server(), "b.test", "/echo-with-no-extension");
  EXPECT_EQ("error", EvalJs(child_rfh, JsReplace(R"(
    new Promise(resolve => {
      const ws = new WebSocket($1);
      ws.onopen = () => { ws.close(); resolve('open'); };
      ws.onerror = () => resolve('error');
    });
  )",
                                                 denied_ws_url)));
}

// A frame that neither opts in nor delivers a satisfying Connection-Allowlist
// is blocked with net::ERR_BLOCKED_BY_RESPONSE.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       NoOptInNoAllowlistBlockedWithErrorCode) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>", {}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  NavigationHandleObserver handle_observer(shell()->web_contents(), child_url);
  TestNavigationManager manager(shell()->web_contents(), child_url);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', '(response-origin)');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                                        child_url)));
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(manager.was_successful());
  EXPECT_TRUE(handle_observer.is_error());
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, handle_observer.net_error_code());
}

// A descendant frame with no `connectionallowlist` attribute inherits its
// ancestor's required allowlist.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       InheritedRequirementEnforcedWithoutAttribute) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  RegisterResponse("/grandchild.html",
                   ResponseEntry("<html><body>grandchild</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  GURL grandchild_url =
      embedded_https_test_server().GetURL("a.test", "/grandchild.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  // Level 1: a frame whose embedder requires `(response-origin)` and which opts
  // in. It now requires that allowlist of any document it frames.
  {
    TestNavigationManager manager(shell()->web_contents(), child_url);
    EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', '(response-origin)');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                                          child_url)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(manager.was_successful());
  }
  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  // Wait for the child document to finish loading so its <body> exists before
  // creating the nested frame inside it below (the navigation having committed
  // does not guarantee the body has been parsed yet).
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Level 2: a frame created by the level-1 frame with no `connectionallowlist`
  // attribute. It inherits the level-1 frame's requirement.
  {
    TestNavigationManager manager(shell()->web_contents(), grandchild_url);
    EXPECT_TRUE(ExecJs(child_rfh, JsReplace(R"(
        const f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                            grandchild_url)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(manager.was_successful());
  }
  RenderFrameHost* grandchild_rfh = ChildFrameAt(child_rfh, 0);
  ASSERT_TRUE(grandchild_rfh);

  auto* grandchild_impl = static_cast<RenderFrameHostImpl*>(grandchild_rfh);
  ASSERT_TRUE(grandchild_impl->required_connection_allowlist().has_value());
  EXPECT_EQ(
      grandchild_impl->required_connection_allowlist()->allowlist,
      std::vector<std::string>({url::Origin::Create(child_url).Serialize()}));
}

// A descendant frame whose `connectionallowlist` attribute would loosen the
// inherited requirement (it additionally lists https://extra.test/) has its
// attribute discarded in favor of the stricter inherited requirement. This also
// guards against the bypass where an attribute's unresolved `response-origin`
// token leaves an empty allowlist that would trivially subsume any requirement.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       NeverLoosenInheritedRequirement) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/child.html",
                   ResponseEntry("<html><body>child</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  RegisterResponse("/grandchild.html",
                   ResponseEntry("<html><body>grandchild</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL child_url = embedded_https_test_server().GetURL("a.test", "/child.html");
  GURL grandchild_url =
      embedded_https_test_server().GetURL("a.test", "/grandchild.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  // Level 1: requires `(response-origin)` and opts in.
  {
    TestNavigationManager manager(shell()->web_contents(), child_url);
    EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', '(response-origin)');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                                          child_url)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(manager.was_successful());
  }
  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  // Wait for the child document to finish loading so its <body> exists before
  // creating the nested frame inside it below (the navigation having committed
  // does not guarantee the body has been parsed yet).
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Level 2: its attribute additionally lists https://extra.test/, which would
  // loosen the inherited requirement. The navigation still commits (it opts
  // in), but the looser attribute is discarded.
  {
    TestNavigationManager manager(shell()->web_contents(), grandchild_url);
    EXPECT_TRUE(ExecJs(child_rfh,
                       JsReplace(R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', $1);
        f.src = $2;
        document.body.appendChild(f);
      )",
                                 R"(("https://extra.test/" response-origin))",
                                 grandchild_url)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(manager.was_successful());
  }
  RenderFrameHost* grandchild_rfh = ChildFrameAt(child_rfh, 0);
  ASSERT_TRUE(grandchild_rfh);

  // Only the inherited embedder origin is required; https://extra.test/ (from
  // the discarded attribute) is not present.
  auto* grandchild_impl = static_cast<RenderFrameHostImpl*>(grandchild_rfh);
  ASSERT_TRUE(grandchild_impl->required_connection_allowlist().has_value());
  EXPECT_EQ(
      grandchild_impl->required_connection_allowlist()->allowlist,
      std::vector<std::string>({url::Origin::Create(child_url).Serialize()}));
}

// A frame with a local-scheme URL (about:srcdoc, about:blank) inherits its
// origin from its initiator rather than deriving one from its URL. Resolving
// its embedder's `(response-origin)` token against `about:srcdoc` would produce
// an opaque origin, which would then be propagated to the frame's descendants
// as their requirement. The token must resolve to the inherited origin instead.
IN_PROC_BROWSER_TEST_F(ConnectionAllowlistEmbeddedEnforcementTest,
                       ResponseOriginResolvesToInheritedOriginForLocalScheme) {
  RegisterResponse("/embedder.html",
                   ResponseEntry("<html><body></body></html>", {}));
  RegisterResponse("/grandchild.html",
                   ResponseEntry("<html><body>grandchild</body></html>",
                                 {{"Allow-Connection-Allowlist-From", "*"}}));
  ASSERT_TRUE(embedded_https_test_server().Start());

  GURL embedder_url =
      embedded_https_test_server().GetURL("a.test", "/embedder.html");
  GURL grandchild_url =
      embedded_https_test_server().GetURL("a.test", "/grandchild.html");
  EXPECT_TRUE(NavigateToURL(shell(), embedder_url));

  // Level 1: an `about:srcdoc` frame whose embedder requires
  // `(response-origin)`. Local schemes allow blanket enforcement, so the
  // requirement is installed on the frame and cascades to its descendants.
  {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell()->web_contents(), R"(
        const f = document.createElement('iframe');
        f.setAttribute('connectionallowlist', '(response-origin)');
        f.srcdoc = '<html><body>child</body></html>';
        document.body.appendChild(f);
      )"));
    observer.Wait();
    ASSERT_TRUE(observer.last_navigation_succeeded());
    ASSERT_EQ(GURL(url::kAboutSrcdocURL), observer.last_navigation_url());
  }
  RenderFrameHost* child_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  // The token resolves to the origin the srcdoc frame actually inherits (its
  // embedder's), not an opaque origin derived from "about:srcdoc".
  auto* child_impl = static_cast<RenderFrameHostImpl*>(child_rfh);
  ASSERT_TRUE(child_impl->required_connection_allowlist().has_value());
  EXPECT_EQ(child_impl->required_connection_allowlist()->allowlist,
            std::vector<std::string>(
                {url::Origin::Create(embedder_url).Serialize()}));

  // Wait for the srcdoc document to finish loading so its <body> exists before
  // creating the nested frame inside it below (the navigation having committed
  // does not guarantee the body has been parsed yet).
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Level 2: a frame created by the srcdoc frame inherits that requirement, so
  // it must see the inherited origin rather than an opaque one.
  {
    TestNavigationManager manager(shell()->web_contents(), grandchild_url);
    EXPECT_TRUE(ExecJs(child_rfh, JsReplace(R"(
        const f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
      )",
                                            grandchild_url)));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    ASSERT_TRUE(manager.was_successful());
  }
  RenderFrameHost* grandchild_rfh = ChildFrameAt(child_rfh, 0);
  ASSERT_TRUE(grandchild_rfh);

  auto* grandchild_impl = static_cast<RenderFrameHostImpl*>(grandchild_rfh);
  ASSERT_TRUE(grandchild_impl->required_connection_allowlist().has_value());
  EXPECT_EQ(grandchild_impl->required_connection_allowlist()->allowlist,
            std::vector<std::string>(
                {url::Origin::Create(embedder_url).Serialize()}));
}

}  // namespace

}  // namespace content

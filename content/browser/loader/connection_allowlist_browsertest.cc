// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kSameOriginAllowlistedPage[] = "/response_origin.html";
constexpr char kCrossOriginAllowlistedPage[] =
    "/response_and_cross_origin.html";
}

struct ResponseEntry {
  std::string content;
  absl::flat_hash_map<std::string, std::string> headers;
};

// TODO(crbug.com/486121443): Once the test flakiness due to the issue in
// WebPrescientNetworkingImpl is resolved, add a test covering preconnect from
// the link header response.
class ConnectionAllowlistTest : public ContentBrowserTest {
 public:
  ConnectionAllowlistTest()
      : scoped_feature_list_(network::features::kConnectionAllowlists) {}
  ~ConnectionAllowlistTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(&embedded_https_test_server());
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &ConnectionAllowlistTest::ServeResponses, base::Unretained(this)));
  }

  void RegisterResponse(const std::string& relative_url,
                        ResponseEntry&& entry) {
    response_map_[relative_url] = std::move(entry);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    if (auto it = response_map_.find(request.relative_url);
        it != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content(it->second.content);
      for (const auto& [key, value] : it->second.headers) {
        response->AddCustomHeader(key, value);
      }

      return response;
    }

    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  absl::flat_hash_map<std::string, ResponseEntry> response_map_;
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

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest,
                       NavigationRequestPreconnectAllowed) {
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

}  // namespace content

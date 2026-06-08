// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_key.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/tokens/tokens.h"
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
  ConnectionAllowlistTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::kConnectionAllowlists,
                              blink::features::
                                  kOverrideConnectionAllowlistOriginTrial},
        /*disabled_features=*/{});
  }
  ~ConnectionAllowlistTest() override { content_browser_client_.reset(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

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

      return response;
    }

    return nullptr;
  }

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

  // For inegligible redirect, the prefetch status is set to
  // `PrefetchStatus::kPrefetchFailedIneligibleRedirect` ignoring the specific
  // `PreloadingEligibility` reason.
  EXPECT_TRUE(WaitForSpeculationRulesPrefetch(
      redirect_url, PrefetchContainer::LoadState::kFailedDeterminedHead,
      PrefetchStatus::kPrefetchFailedIneligibleRedirect));
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

}  // namespace content

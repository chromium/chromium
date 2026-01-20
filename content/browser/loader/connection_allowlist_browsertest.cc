// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kSameOriginAllowlistedPage[] = "/response_origin.html";
}

class ConnectionAllowlistTest : public ContentBrowserTest {
 public:
  ConnectionAllowlistTest()
      : scoped_feature_list_(network::features::kConnectionAllowlists) {}
  ~ConnectionAllowlistTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == kSameOriginAllowlistedPage) {
            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_content("<html><body>Hello</body></html>");
            http_response->AddCustomHeader("Connection-Allowlist",
                                           "(response-origin)");
            return http_response;
          }

          return nullptr;
        }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkPrefetch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url =
      embedded_test_server()->GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_test_server()->GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_test_server()->GetURL("b.test", "/deny.js");

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

IN_PROC_BROWSER_TEST_F(ConnectionAllowlistTest, LinkPreload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url =
      embedded_test_server()->GetURL("a.test", kSameOriginAllowlistedPage);
  GURL allowed_url = embedded_test_server()->GetURL("a.test", "/allow.js");
  GURL denied_url = embedded_test_server()->GetURL("b.test", "/deny.js");

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

}  // namespace content

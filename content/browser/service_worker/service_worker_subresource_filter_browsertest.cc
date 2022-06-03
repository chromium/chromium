// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr char kBaseDataDir[] = "content/test/data/";

// Generate token with the command:
// generate_token.py https://service-worker-subresource-filter.test:443
// ServiceWorkerSubresourceFilter
// --expire-timestamp=2000000000
base::StringPiece origin_trial_token =
    "A1CKeg8m2+M4knvICqx+5ELaI1Bh17J1+2cAfNSKCgL4zmPh4hXikI4YGxbR/QQo"
    "zQyH6JOw/fqwNdWxman2RgQAAACDeyJvcmlnaW4iOiAiaHR0cHM6Ly9zZXJ2aWNl"
    "LXdvcmtlci1zdWJyZXNvdXJjZS1maWx0ZXIudGVzdDo0NDMiLCAiZmVhdHVyZSI6"
    "ICJTZXJ2aWNlV29ya2VyU3VicmVzb3VyY2VGaWx0ZXIiLCAiZXhwaXJ5IjogMjAw"
    "MDAwMDAwMH0=";

const std::string script = R"(
      (async () => {
        const saw_message = new Promise(resolve => {
          navigator.serviceWorker.onmessage = event => {
            resolve(event.data);
          };
        });
        const registration = await navigator.serviceWorker.ready;
        registration.active.postMessage('');
        return await saw_message;
      })();
  )";

class ServiceWorkerSubresourceFilterBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  ServiceWorkerSubresourceFilterBrowserTest() {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed origin,
    // whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_.emplace(base::BindRepeating(
        &ServiceWorkerSubresourceFilterBrowserTest::InterceptRequest,
        base::Unretained(this)));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void NavigateAndFetch(std::string url, bool expect_only_matching_filter) {
    EXPECT_TRUE(NavigateToURL(
        shell(), GetUrl("/service_worker/create_service_worker.html")));
    EXPECT_EQ(
        "DONE",
        EvalJs(
            shell(),
            "register('/service_worker/fetch_event_pass_through.js', '/');"));

    GURL page_url = GetUrl(url);
    GURL fetch_url = GetUrl("/echo");
    GURL fetch_url_with_fragment = GetUrl("/echo#foo");
    GURL fetch_url_with_fragment_substring = GetUrl("/echo#afooz");
    GURL fetch_url_with_other_fragment = GetUrl("/echo#bar");

    EXPECT_TRUE(NavigateToURL(shell(), page_url));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)", fetch_url)));
    EXPECT_EQ("Echo", EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                                fetch_url_with_fragment)));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                        fetch_url_with_fragment_substring)));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                        fetch_url_with_other_fragment)));

    base::Value list(base::Value::Type::LIST);
    if (expect_only_matching_filter) {
      list.Append(page_url.spec());
      list.Append(fetch_url_with_fragment.spec());
      list.Append(fetch_url_with_fragment_substring.spec());
    } else {
      list.Append(page_url.spec());
      list.Append(fetch_url.spec());
      list.Append(fetch_url_with_fragment.spec());
      list.Append(fetch_url_with_fragment_substring.spec());
      list.Append(fetch_url_with_other_fragment.spec());
    }

    EXPECT_EQ(list, EvalJs(shell(), script));
  }

  bool FeatureIsEnabled() { return GetParam(); }

 private:
  static GURL GetUrl(const std::string& path) {
    return GURL("https://service-worker-subresource-filter.test/")
        .Resolve(path);
  }

  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-Type: text/html\n";

    if (params->url_request.url.path() == "/echo") {
      URLLoaderInterceptor::WriteResponse(headers, std::string("Echo"),
                                          params->client.get(),
                                          absl::optional<net::SSLInfo>());
      return true;
    }

    if (FeatureIsEnabled()) {
      base::StrAppend(&headers, {"Origin-Trial: ", origin_trial_token, "\n"});
      headers += '\n';
    }

    if (params->url_request.url.path() == "/filter") {
      base::StrAppend(&headers, {"Service-Worker-Subresource-Filter: foo"});
      headers += '\n';
      URLLoaderInterceptor::WriteResponse(
          base::StrCat({kBaseDataDir, "/service_worker/fetch_from_page.html"}),
          params->client.get(), &headers, absl::optional<net::SSLInfo>());

      return true;
    }

    if (params->url_request.url.path() == "/nofilter") {
      // Do not add any additional headers, but intercept the request.
      URLLoaderInterceptor::WriteResponse(
          base::StrCat({kBaseDataDir, "/service_worker/fetch_from_page.html"}),
          params->client.get(), &headers, absl::optional<net::SSLInfo>());

      return true;
    }

    if (params->url_request.url.path() == "/emptyfilter") {
      base::StrAppend(&headers, {"Service-Worker-Subresource-Filter:"});
      headers += '\n';
      URLLoaderInterceptor::WriteResponse(
          base::StrCat({kBaseDataDir, "/service_worker/fetch_from_page.html"}),
          params->client.get(), &headers, absl::optional<net::SSLInfo>());

      return true;
    }

    URLLoaderInterceptor::WriteResponse(
        base::StrCat({kBaseDataDir, params->url_request.url.path_piece()}),
        params->client.get());
    return true;
  }

  absl::optional<URLLoaderInterceptor> url_loader_interceptor_;
};

INSTANTIATE_TEST_SUITE_P(EnabledDisabled,
                         ServiceWorkerSubresourceFilterBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest, WithFilter) {
  // If the feature is disabled, all URLs should be seen by the Service Worker.
  // If the feature is enabled, only the initial navigation URL and URLs
  // matching the filter should be seen by the Service Worker.
  NavigateAndFetch("/filter", FeatureIsEnabled());
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest,
                       WithoutFilter) {
  // All URLs should be seen by the Service Worker regardless of whether or not
  // the feature is enabled.
  NavigateAndFetch("/nofilter", false);
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest,
                       WithEmptyFilter) {
  // All URLs should be seen by the Service Worker regardless of whether or not
  // the feature is enabled.
  NavigateAndFetch("/emptyfilter", false);
}

}  // namespace
}  // namespace content
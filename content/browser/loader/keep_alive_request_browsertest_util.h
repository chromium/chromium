// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace net::test_server {
class ControllableHttpResponse;
}  // namespace net::test_server

namespace content {

class WebContentsImpl;
class RenderFrameHostImpl;

// `KeepAliveRequestBrowserTestBase` is a base class for performing fetch
// keep-alive request content browser tests.
class KeepAliveRequestBrowserTestBase : public ContentBrowserTest {
 protected:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;
  using DisabledFeaturesType = std::vector<base::test::FeatureRef>;

  static constexpr char kPrimaryHost[] = "a.test";
  static constexpr char kSecondaryHost[] = "b.test";
  static constexpr char kAllowedCspHost[] = "csp.test";
  static constexpr char kKeepAliveEndpoint[] = "/beacon";
  static constexpr char kBeaconId[] = "beacon01";
  static constexpr char k200TextResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "Acked!";
  static constexpr char k301Response[] =
      "HTTP/1.1 301 Moved Permanently\r\n"
      "Location: %s\r\n"
      "\r\n";

  KeepAliveRequestBrowserTestBase();
  ~KeepAliveRequestBrowserTestBase() override;
  // Not Copyable.
  KeepAliveRequestBrowserTestBase(const KeepAliveRequestBrowserTestBase&) =
      delete;
  KeepAliveRequestBrowserTestBase& operator=(
      const KeepAliveRequestBrowserTestBase&) = delete;

  void SetUp() override;
  virtual const FeaturesType& GetEnabledFeatures() = 0;
  virtual const DisabledFeaturesType& GetDisabledFeatures();

  void SetUpOnMainThread() override;

  void TearDownOnMainThread() override;

  // Returns a keepalive endpoint with the given `id`.
  static std::string GetKeepAliveEndpoint(
      std::optional<std::string> id = std::nullopt);

  // Encodes the given `url` using the JS method encodeURIComponent.
  static std::string EncodeURL(const GURL& url);

 protected:
  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls);

  // Returns a cross-origin (kSecondaryHost) URL that causes the following
  // redirect chain:
  //     http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> http(s)://b.test:<port>/server-redirect-307?...
  // --> http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  GURL GetCrossOriginMultipleRedirectsURL(const GURL& target_url) const;

  // Returns a same-origin (kPrimaryHost) URL that causes the following
  // redirect chain:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> http(s)://a.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url`
  GURL GetSameOriginMultipleRedirectsURL(const GURL& target_url) const;

  // Returns a same-origin (kPrimaryHost) URL that leads to cross-origin
  // redirect chain:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url`
  GURL GetSameAndCrossOriginRedirectsURL(const GURL& target_url) const;

  // Returns a same-origin (kPrimaryHost) URL that redirects to `target_url`:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> `target_url`
  GURL GetSameOriginRedirectURL(const GURL& target_url) const;

  using FetchKeepAliveRequestMetricType =
      KeepAliveURLLoader::FetchKeepAliveRequestMetricType;

  // Note: `renderer` is made optional to support the use cases where its
  // loggings happen after unloading a renderer, as the browser process might
  // not be able to fetch UMA logged by renderer in time before the latter is
  // shutting down, as described in https://crbug.com/40109064.
  // It should be made required once the bug is resolved.
  struct ExpectedRequestHistogram {
    int browser = 0;
    std::optional<int> renderer = std::nullopt;
  };
  using ExpectedTotalRequests = ExpectedRequestHistogram;
  using ExpectedStartedRequests = ExpectedRequestHistogram;
  using ExpectedSucceededRequests = ExpectedRequestHistogram;
  using ExpectedFailedRequests = ExpectedRequestHistogram;

  // A helper to assert on the number of UMA logged from both browser and
  // renderer processes.
  void ExpectFetchKeepAliveHistogram(
      const FetchKeepAliveRequestMetricType& expected_sample,
      const ExpectedTotalRequests& total,
      const ExpectedStartedRequests& started_count,
      const ExpectedSucceededRequests& succeeded_count,
      const ExpectedFailedRequests& failed_count,
      size_t retried_count = 0);

  void DisableBackForwardCache(WebContents* web_contents);
  void SetUseHttps();

  WebContentsImpl* web_contents() const;
  RenderFrameHostImpl* current_frame_host();

  KeepAliveURLLoaderService* loader_service();
  KeepAliveURLLoadersTestObserver& loaders_observer();

  net::EmbeddedTestServer* server();
  const net::EmbeddedTestServer* server() const;

  const base::HistogramTester& histogram_tester();

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<KeepAliveURLLoadersTestObserver> loaders_observer_;
  bool use_https_ = false;
  const std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_REQUEST_BROWSERTEST_UTIL_H_

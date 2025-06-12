// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/loader/keep_alive_request_browsertest_util.h"

#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"

namespace content {

KeepAliveRequestBrowserTestBase::KeepAliveRequestBrowserTestBase()
    : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS)) {}
KeepAliveRequestBrowserTestBase::~KeepAliveRequestBrowserTestBase() = default;

void KeepAliveRequestBrowserTestBase::SetUp() {
  feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                              GetDisabledFeatures());
  ContentBrowserTest::SetUp();
}

const KeepAliveRequestBrowserTestBase::DisabledFeaturesType&
KeepAliveRequestBrowserTestBase::GetDisabledFeatures() {
  static const DisabledFeaturesType disabled_features =
      GetDefaultDisabledBackForwardCacheFeaturesForTesting();
  return disabled_features;
}

void KeepAliveRequestBrowserTestBase::SetUpOnMainThread() {
  // Support multiple sites on the test server.
  host_resolver()->AddRule("*", "127.0.0.1");
  if (loader_service()) {
    loaders_observer_ = std::make_unique<KeepAliveURLLoadersTestObserver>(
        web_contents()->GetBrowserContext());
  }

  // Initialize an HTTPS server. Subclass may choose to use HTTPS by calling
  // `SetUseHttps()`.
  https_test_server_->AddDefaultHandlers(GetTestDataFilePath());
  https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

  histogram_tester_ = std::make_unique<base::HistogramTester>();

  ContentBrowserTest::SetUpOnMainThread();
}

void KeepAliveRequestBrowserTestBase::TearDownOnMainThread() {
  histogram_tester_.reset();
  ContentBrowserTest::TearDownOnMainThread();
}

std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
KeepAliveRequestBrowserTestBase::RegisterRequestHandlers(
    const std::vector<std::string>& relative_urls) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      handlers;
  for (const auto& relative_url : relative_urls) {
    handlers.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            server(), relative_url));
  }
  return handlers;
}

GURL KeepAliveRequestBrowserTestBase::GetCrossOriginMultipleRedirectsURL(
    const GURL& target_url) const {
  const auto intermediate_url2 = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         target_url.spec().c_str()));
  const auto intermediate_url1 = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/server-redirect-307?%s",
                                         intermediate_url2.spec().c_str()));
  return server()->GetURL(kSecondaryHost,
                          base::StringPrintf("/no-cors-server-redirect-307?%s",
                                             intermediate_url1.spec().c_str()));
}

GURL KeepAliveRequestBrowserTestBase::GetSameOriginMultipleRedirectsURL(
    const GURL& target_url) const {
  const auto intermediate_url1 = server()->GetURL(
      kPrimaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                       target_url.spec().c_str()));
  return server()->GetURL(kPrimaryHost,
                          base::StringPrintf("/server-redirect-307?%s",
                                             intermediate_url1.spec().c_str()));
}

GURL KeepAliveRequestBrowserTestBase::GetSameAndCrossOriginRedirectsURL(
    const GURL& target_url) const {
  const auto intermediate_url1 = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         target_url.spec().c_str()));
  return server()->GetURL(kPrimaryHost,
                          base::StringPrintf("/server-redirect-307?%s",
                                             intermediate_url1.spec().c_str()));
}

GURL KeepAliveRequestBrowserTestBase::GetSameOriginRedirectURL(
    const GURL& target_url) const {
  return server()->GetURL(
      kPrimaryHost,
      base::StringPrintf("/server-redirect-307?%s", target_url.spec().c_str()));
}

void KeepAliveRequestBrowserTestBase::ExpectFetchKeepAliveHistogram(
    const FetchKeepAliveRequestMetricType& expected_sample,
    const ExpectedTotalRequests& total,
    const ExpectedStartedRequests& started_count,
    const ExpectedSucceededRequests& succeeded_count,
    const ExpectedFailedRequests& failed_count,
    size_t retried_count) {
  // Collect metrics recorded in the renderer processes, if expecting any.
  for (size_t retries = 0;
       retries < 20 &&
       ((total.renderer.has_value() && *total.renderer > 0 &&
         histogram_tester()
             .GetAllSamples("FetchKeepAlive.Requests2.Total.Renderer")
             .empty()) ||
        (started_count.renderer.has_value() && *started_count.renderer > 0 &&
         histogram_tester()
             .GetAllSamples("FetchKeepAlive.Requests2.Started.Renderer")
             .empty()) ||
        (succeeded_count.renderer.has_value() &&
         *succeeded_count.renderer > 0 &&
         histogram_tester()
             .GetAllSamples("FetchKeepAlive.Requests2.Succeeded.Renderer")
             .empty()) ||
        (failed_count.renderer.has_value() && *failed_count.renderer > 0 &&
         histogram_tester()
             .GetAllSamples("FetchKeepAlive.Requests2.Failed.Renderer")
             .empty()));
       retries++) {
    FetchHistogramsFromChildProcesses();
  }

  const int renderer_sample = static_cast<int>(expected_sample);
  const int browser_sample =
      (expected_sample == FetchKeepAliveRequestMetricType::kBeacon ||
       expected_sample == FetchKeepAliveRequestMetricType::kPing ||
       expected_sample == FetchKeepAliveRequestMetricType::kAttribution)
          ? static_cast<int>(FetchKeepAliveRequestMetricType::kPing)
          : static_cast<int>(expected_sample);
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Total.Browser", browser_sample, total.browser);
  if (total.renderer.has_value()) {
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Total.Renderer", renderer_sample,
        *total.renderer);
  }
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Started.Browser", browser_sample,
      started_count.browser);
  if (started_count.renderer.has_value()) {
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Started.Renderer", renderer_sample,
        *started_count.renderer);
  }
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Succeeded.Browser", browser_sample,
      succeeded_count.browser);
  if (succeeded_count.renderer.has_value()) {
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Succeeded.Renderer", renderer_sample,
        *succeeded_count.renderer);
  }
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Failed.Browser", browser_sample,
      failed_count.browser);
  if (failed_count.renderer.has_value()) {
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Failed.Renderer", renderer_sample,
        *failed_count.renderer);
  }
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Retried.Browser", browser_sample,
      retried_count);
  histogram_tester().ExpectUniqueSample(
      "FetchKeepAlive.Requests2.Retried.Renderer", renderer_sample, 0);
}

WebContentsImpl* KeepAliveRequestBrowserTestBase::web_contents() const {
  return static_cast<WebContentsImpl*>(shell()->web_contents());
}
RenderFrameHostImpl* KeepAliveRequestBrowserTestBase::current_frame_host() {
  return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
}
KeepAliveURLLoaderService* KeepAliveRequestBrowserTestBase::loader_service() {
  return static_cast<StoragePartitionImpl*>(
             web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
      ->GetKeepAliveURLLoaderService();
}

void KeepAliveRequestBrowserTestBase::DisableBackForwardCache(
    WebContents* web_contents) {
  DisableBackForwardCacheForTesting(web_contents,
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
}

KeepAliveURLLoadersTestObserver&
KeepAliveRequestBrowserTestBase::loaders_observer() {
  return *loaders_observer_;
}
void KeepAliveRequestBrowserTestBase::SetUseHttps() {
  use_https_ = true;
}
net::EmbeddedTestServer* KeepAliveRequestBrowserTestBase::server() {
  return use_https_ ? https_test_server_.get() : embedded_test_server();
}
const net::EmbeddedTestServer* KeepAliveRequestBrowserTestBase::server() const {
  return use_https_ ? https_test_server_.get() : embedded_test_server();
}

const base::HistogramTester&
KeepAliveRequestBrowserTestBase::histogram_tester() {
  return *histogram_tester_;
}

// static
std::string KeepAliveRequestBrowserTestBase::GetKeepAliveEndpoint(
    std::optional<std::string> id) {
  std::string endpoint = kKeepAliveEndpoint;
  if (id.has_value()) {
    endpoint += "?id=" + *id;
  }
  return endpoint;
}

// static
std::string KeepAliveRequestBrowserTestBase::EncodeURL(const GURL& url) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(url.spec(), &buffer);
  return std::string(buffer.view());
}
}  // namespace content

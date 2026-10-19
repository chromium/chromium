// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PreloadActivationReportManagerTest : public RenderViewHostTestHarness {
 public:
  PreloadActivationReportManagerTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    auto* manager =
        PreloadActivationReportManager::GetOrCreateForBrowserContext(
            web_contents()->GetBrowserContext());
    manager->SetURLLoaderFactoryForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(PreloadActivationReportManagerTest, ReportActivation) {
  base::HistogramTester histogram_tester;
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);

  GURL endpoint("https://example.com/beacon");

  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  manager->ReportActivation(endpoint, main_rfh(),
                            PreloadActivationReportType::kPrefetch);
  EXPECT_EQ(manager->GetLoaderCountForTesting(), 1u);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportPreloadType",
      PreloadActivationReportType::kPrefetch, 1);

  // Verify that a pending request was created in the factory.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(request.url, endpoint);
  EXPECT_EQ(request.method, "GET");
  EXPECT_TRUE(request.devtools_request_id.has_value());

  // Simulate successful response.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      endpoint.spec(), ""));

  // The loader should be destroyed.
  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportSuccess", true, 1);
}

TEST_F(PreloadActivationReportManagerTest, ReportActivationNetworkError) {
  base::HistogramTester histogram_tester;
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);

  GURL endpoint("https://example.com/beacon");

  manager->ReportActivation(endpoint, main_rfh(),
                            PreloadActivationReportType::kPrerender);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportPreloadType",
      PreloadActivationReportType::kPrerender, 1);

  // Simulate failed response.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      endpoint, network::URLLoaderCompletionStatus(net::ERR_FAILED),
      network::mojom::URLResponseHead::New(), ""));

  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportSuccess", false, 1);
}

TEST_F(PreloadActivationReportManagerTest,
       ReportActivationRedirectSameOriginAllowed) {
  base::HistogramTester histogram_tester;
  network::TestURLLoaderFactory custom_factory(
      /*observe_loader_requests=*/true);
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);
  manager->SetURLLoaderFactoryForTesting(custom_factory.GetSafeWeakWrapper());

  GURL endpoint("https://example.com/beacon");
  GURL redirect_endpoint("https://example.com/beacon2");

  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  manager->ReportActivation(endpoint, main_rfh(),
                            PreloadActivationReportType::kPrefetch);
  EXPECT_EQ(manager->GetLoaderCountForTesting(), 1u);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportPreloadType",
      PreloadActivationReportType::kPrefetch, 1);

  // Verify that a pending request was created in the factory.
  ASSERT_EQ(custom_factory.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      custom_factory.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, endpoint);
  EXPECT_EQ(pending_request->request.method, "GET");

  // Simulate redirect.
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_url = redirect_endpoint;
  redirect_info.new_method = "GET";

  auto redirect_head = network::mojom::URLResponseHead::New();
  redirect_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 302 Found\r\n"
                                        "Location: " +
                                        redirect_endpoint.spec() + "\r\n\r\n"));

  pending_request->client->OnReceiveRedirect(redirect_info,
                                             std::move(redirect_head));

  // Wait for the redirect callback to be processed and FollowRedirect to be
  // called.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto* req = custom_factory.GetPendingRequest(0);
    return req && req->test_url_loader &&
           !req->test_url_loader->follow_redirect_params().empty();
  }));

  // Simulate successful response for the redirected request.
  auto final_head = network::mojom::URLResponseHead::New();
  final_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n\r\n");

  // We need to get the pending request again because it might have been
  // modified or we just want to be sure.
  pending_request = custom_factory.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  pending_request->client->OnReceiveResponse(
      std::move(final_head), mojo::ScopedDataPipeConsumerHandle(),
      std::nullopt);
  pending_request->client->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  // The loader should be destroyed because it completed successfully.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetLoaderCountForTesting() == 0; }));
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportSuccess", true, 1);
}

TEST_F(PreloadActivationReportManagerTest,
       ReportActivationRedirectCrossOriginBlocked) {
  base::HistogramTester histogram_tester;
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);

  GURL endpoint("https://example.com/beacon");
  GURL redirect_endpoint("https://cross-origin.com/beacon2");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_url = redirect_endpoint;
  redirect_info.new_method = "GET";

  auto redirect_head = network::mojom::URLResponseHead::New();
  redirect_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 302 Found\r\n"
                                        "Location: " +
                                        redirect_endpoint.spec() + "\r\n\r\n"));

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, std::move(redirect_head));

  auto final_head = network::mojom::URLResponseHead::New();
  final_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n\r\n");

  test_url_loader_factory_.AddResponse(
      endpoint, std::move(final_head), "",
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  manager->ReportActivation(endpoint, main_rfh(),
                            PreloadActivationReportType::kPrefetch);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportPreloadType",
      PreloadActivationReportType::kPrefetch, 1);

  // The loader should be destroyed immediately on redirect due to cross-origin
  // block.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetLoaderCountForTesting() == 0; }));

  // And the redirect_endpoint should NOT have been requested.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(redirect_endpoint.spec()));
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportSuccess", false, 1);
}

TEST_F(PreloadActivationReportManagerTest,
       ReportActivationRedirectSameOriginMethodChangedBlocked) {
  base::HistogramTester histogram_tester;
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);

  GURL endpoint("https://example.com/beacon");
  GURL redirect_endpoint("https://example.com/beacon2");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_url = redirect_endpoint;
  // Change the method to POST, which should be blocked.
  redirect_info.new_method = "POST";

  auto redirect_head = network::mojom::URLResponseHead::New();
  redirect_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 302 Found\r\n"
                                        "Location: " +
                                        redirect_endpoint.spec() + "\r\n\r\n"));

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, std::move(redirect_head));

  auto final_head = network::mojom::URLResponseHead::New();
  final_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n\r\n");

  test_url_loader_factory_.AddResponse(
      endpoint, std::move(final_head), "",
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  EXPECT_EQ(manager->GetLoaderCountForTesting(), 0u);
  manager->ReportActivation(endpoint, main_rfh(),
                            PreloadActivationReportType::kPrerender);
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportPreloadType",
      PreloadActivationReportType::kPrerender, 1);

  // The loader should be destroyed immediately on redirect due to method change
  // block.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetLoaderCountForTesting() == 0; }));

  // And the redirect_endpoint should NOT have been requested.
  EXPECT_FALSE(test_url_loader_factory_.IsPending(redirect_endpoint.spec()));
  histogram_tester.ExpectUniqueSample(
      "Preloading.ActivationBeacon.ReportSuccess", false, 1);
}

}  // namespace content

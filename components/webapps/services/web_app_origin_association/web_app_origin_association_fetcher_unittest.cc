// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"

#include <optional>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kWebAppOriginAssociationFileContent[] =
    R"( {\"https://foo.com/\": {} })";

constexpr char kFetchResultHistogram[] =
    "Webapp.WebAppOriginAssociationFetchResult";
}  // namespace

namespace webapps {

class WebAppOriginAssociationFetcherTest : public testing::Test {
 public:
  WebAppOriginAssociationFetcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Make sure the Network Service is started before making a NetworkContext.
    content::GetNetworkService();

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            network::NetworkService::GetNetworkServiceForTesting(),
            /*is_trusted=*/true);

    fetcher_ = std::make_unique<WebAppOriginAssociationFetcher>(
        shared_url_loader_factory_);

    // Do not retry, otherwise TestSharedURLLoaderFactory.Clone() will be
    // called, which is not implemented.
    fetcher_->SetRetryOptionsForTest(0, network::SimpleURLLoader::RETRY_NEVER);
  }

  void SetUp() override {
    server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppOriginAssociationFetcherTest::HandleRequest,
                            base::Unretained(this)));

    ASSERT_TRUE(test_server_handle_ = server_.StartAndReturnHandle());
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/.well-known/web-app-origin-association")
      return nullptr;

    if (redirect_enabled_) {
      auto redirect_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      redirect_response->set_code(net::HTTP_FOUND);
      redirect_response->AddCustomHeader("Location", "/redirected");
      return redirect_response;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(kWebAppOriginAssociationFileContent);
    return http_response;
  }

  content::BrowserTaskEnvironment task_environment_;
  net::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<WebAppOriginAssociationFetcher> fetcher_;
  base::HistogramTester histogram_tester_;
  bool redirect_enabled_ = false;
};

TEST_F(WebAppOriginAssociationFetcherTest, FileExists) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL(server_.base_url())),
      network::mojom::IPAddressSpace::kLoopback, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(!file_content);
  EXPECT_EQ(*file_content, kWebAppOriginAssociationFileContent);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchSucceed, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest, FileDoesNotExist) {
  base::test::TestFuture<std::optional<std::string>> future;
  GURL url = server_.GetURL("foo.com", "/");

  fetcher_->FetchWebAppOriginAssociationFile(url::Origin::Create(url),
                                             future.GetCallback());
  auto file_content = future.Take();

  ASSERT_TRUE(!file_content);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedNoResponseBody,
      1);
}

TEST_F(WebAppOriginAssociationFetcherTest, FileUrlIsInvalid) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://co.uk")), future.GetCallback());

  auto file_content = future.Take();
  ASSERT_TRUE(!file_content);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest,
       PublicInitiatorCannotFetchPrivateIp) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://192.168.1.1:8443")),
      network::mojom::IPAddressSpace::kPublic, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest,
       PublicInitiatorCannotFetchLoopbackIp) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://127.0.0.1:8443")),
      network::mojom::IPAddressSpace::kPublic, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest, CannotFetchLinkLocalIp) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://169.254.169.254:8443")),
      network::mojom::IPAddressSpace::kLocal, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest, CannotFetchMulticastOrZeroIp) {
  {
    base::test::TestFuture<std::optional<std::string>> future;
    fetcher_->FetchWebAppOriginAssociationFile(
        url::Origin::Create(GURL("https://0.0.0.0:8443")),
        network::mojom::IPAddressSpace::kLocal, future.GetCallback());

    auto file_content = future.Take();
    ASSERT_FALSE(file_content.has_value());
  }
  {
    base::test::TestFuture<std::optional<std::string>> future;
    fetcher_->FetchWebAppOriginAssociationFile(
        url::Origin::Create(GURL("https://224.0.0.1:8443")),
        network::mojom::IPAddressSpace::kLocal, future.GetCallback());

    auto file_content = future.Take();
    ASSERT_FALSE(file_content.has_value());
  }
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 2);
}

TEST_F(WebAppOriginAssociationFetcherTest,
       PublicInitiatorCannotFetchLocalhostDomain) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://localhost:8443")),
      network::mojom::IPAddressSpace::kPublic, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest, InsecureHttpOriginIsRejected) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("http://example.com")),
      network::mojom::IPAddressSpace::kPublic, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest,
       LoopbackInitiatorCanFetchLoopbackIp) {
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL(server_.base_url())),
      network::mojom::IPAddressSpace::kLoopback, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_TRUE(file_content.has_value());
  EXPECT_EQ(*file_content, kWebAppOriginAssociationFileContent);
}

TEST_F(WebAppOriginAssociationFetcherTest, RedirectIsBlocked) {
  redirect_enabled_ = true;
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL(server_.base_url())),
      network::mojom::IPAddressSpace::kLoopback, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(file_content.has_value());
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedNoResponseBody,
      1);
}

class WebAppOriginAssociationFetcherTimeoutTest : public testing::Test {
 public:
  WebAppOriginAssociationFetcherTimeoutTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {
    fetcher_ = std::make_unique<WebAppOriginAssociationFetcher>(
        shared_url_loader_factory_);
    fetcher_->SetRetryOptionsForTest(0, network::SimpleURLLoader::RETRY_NEVER);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<WebAppOriginAssociationFetcher> fetcher_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WebAppOriginAssociationFetcherTimeoutTest, FetchTimeout) {
  base::test::TestFuture<std::optional<std::string>> future;
  GURL url("https://example.com");
  fetcher_->FetchWebAppOriginAssociationFile(url::Origin::Create(url),
                                             future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Fast forward time slightly.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(future.IsReady());

  // Fast forward time to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_TRUE(future.IsReady());

  auto file_content = future.Take();
  ASSERT_TRUE(!file_content);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedNoResponseBody,
      1);
}

TEST_F(WebAppOriginAssociationFetcherTimeoutTest,
       PrivateInitiatorCanFetchPrivateIp) {
  test_factory_.AddResponse(
      "https://192.168.1.1:8443/.well-known/web-app-origin-association",
      kWebAppOriginAssociationFileContent);
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://192.168.1.1:8443")),
      network::mojom::IPAddressSpace::kLocal, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_TRUE(file_content.has_value());
  EXPECT_EQ(*file_content, kWebAppOriginAssociationFileContent);
}

TEST_F(WebAppOriginAssociationFetcherTimeoutTest,
       LocalInitiatorCanFetchLocalDomain) {
  test_factory_.AddResponse(
      "https://app.local:8443/.well-known/web-app-origin-association",
      kWebAppOriginAssociationFileContent);
  base::test::TestFuture<std::optional<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://app.local:8443")),
      network::mojom::IPAddressSpace::kLocal, future.GetCallback());

  auto file_content = future.Take();
  ASSERT_TRUE(file_content.has_value());
  EXPECT_EQ(*file_content, kWebAppOriginAssociationFileContent);
}

}  // namespace webapps

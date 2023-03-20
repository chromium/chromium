// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"

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
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kWebAppOriginAssociationFileContent[] =
    R"({\"web_apps\": ["
  "    { \"web_app_identity\": \"https://foo.com/\"}"
  "]})";

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
            network::NetworkService::GetNetworkServiceForTesting());

    fetcher_ = std::make_unique<WebAppOriginAssociationFetcher>();

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
};

TEST_F(WebAppOriginAssociationFetcherTest, FileExists) {
  base::test::TestFuture<std::unique_ptr<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL(server_.base_url())),
      shared_url_loader_factory_.get(), future.GetCallback());

  auto file_content = future.Take();
  ASSERT_FALSE(!file_content);
  EXPECT_EQ(*file_content, kWebAppOriginAssociationFileContent);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchSucceed, 1);
}

TEST_F(WebAppOriginAssociationFetcherTest, FileDoesNotExist) {
  base::test::TestFuture<std::unique_ptr<std::string>> future;
  GURL url = server_.GetURL("foo.com", "/");

  fetcher_->FetchWebAppOriginAssociationFile(url::Origin::Create(url),
                                             shared_url_loader_factory_.get(),
                                             future.GetCallback());
  auto file_content = future.Take();

  ASSERT_TRUE(!file_content);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedNoResponseBody,
      1);
}

TEST_F(WebAppOriginAssociationFetcherTest, FileUrlIsInvalid) {
  base::test::TestFuture<std::unique_ptr<std::string>> future;
  fetcher_->FetchWebAppOriginAssociationFile(
      url::Origin::Create(GURL("https://co.uk")),
      shared_url_loader_factory_.get(), future.GetCallback());

  auto file_content = future.Take();
  ASSERT_TRUE(!file_content);
  histogram_tester_.ExpectBucketCount(
      kFetchResultHistogram,
      WebAppOriginAssociationMetrics::FetchResult::kFetchFailedInvalidUrl, 1);
}

}  // namespace webapps

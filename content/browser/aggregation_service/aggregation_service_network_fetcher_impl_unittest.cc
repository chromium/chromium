// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kExampleUrl[] =
    "https://helper.test/.well-known/aggregation-service/keys.json";

const aggregation_service::TestHpkeKey kExampleHpkeKey =
    aggregation_service::TestHpkeKey("abcd");
const std::string kExampleValidJson = base::ReplaceStringPlaceholders(
    R"({
          "version": "",
          "keys": [
              {
                  "id": "abcd",
                  "key": "$1"
              }
          ]
       })",
    {kExampleHpkeKey.GetPublicKeyBase64()},
    /*offsets=*/nullptr);
const std::vector<PublicKey> kExamplePublicKeys = {
    kExampleHpkeKey.GetPublicKey()};

constexpr std::string_view kKeyFetcherStatusHistogramName =
    "PrivacySandbox.AggregationService.KeyFetcher.Status2";

constexpr std::string_view kKeyFetcherHttpResponseOrNetErrorCodeHistogramName =
    "PrivacySandbox.AggregationService.KeyFetcher.HttpResponseOrNetErrorCode";

}  // namespace

class AggregationServiceNetworkFetcherTest : public testing::Test {
 public:
  AggregationServiceNetworkFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_fetcher_(AggregationServiceNetworkFetcherImpl::CreateForTesting(
            task_environment_.GetMockClock(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AggregationServiceNetworkFetcherImpl> network_fetcher_;

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AggregationServiceNetworkFetcherTest, RequestAttributes) {
  network_fetcher_->FetchPublicKeys(GURL(kExampleUrl), base::DoNothing());

  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(request.url, kExampleUrl);
  EXPECT_EQ(request.method, net::HttpRequestHeaders::kGetMethod);
  EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);

  int load_flags = request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchPublicKeys_Success) {
  base::HistogramTester histograms;

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_TRUE(keyset.has_value());
        EXPECT_TRUE(aggregation_service::PublicKeysEqual(kExamplePublicKeys,
                                                         keyset->keys));
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, kExampleValidJson));
  task_environment_.RunUntilQuit();

  // kSuccess = 0
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 0, 1);
  histograms.ExpectUniqueSample(
      kKeyFetcherHttpResponseOrNetErrorCodeHistogramName, net::HTTP_OK, 1);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchPublicKeysInvalidKeyFormat_Failed) {
  base::HistogramTester histograms;

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, /*content=*/"{}"));
  task_environment_.RunUntilQuit();

  // kInvalidKeyError = 3
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 3, 1);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchPublicKeysMalformedJson_Failed) {
  base::HistogramTester histograms;

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, /*content=*/"{"));
  task_environment_.RunUntilQuit();

  // kJsonParseError = 2
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 2, 1);
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchPublicKeysLargeBody_Failed) {
  base::HistogramTester histograms;

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  std::string response_body = kExampleValidJson + std::string(1000000, ' ');
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, response_body));
  task_environment_.RunUntilQuit();

  // kDownloadError = 1
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 1, 1);
  histograms.ExpectUniqueSample(
      kKeyFetcherHttpResponseOrNetErrorCodeHistogramName,
      net::ERR_INSUFFICIENT_RESOURCES, 1);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetcherDeletedDuringRequest_NoCrash) {
  network_fetcher_->FetchPublicKeys(GURL(kExampleUrl), base::DoNothing());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network_fetcher_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, kExampleValidJson));
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchRequestHangs_TimesOut) {
  base::HistogramTester histograms;

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        quit_closure.Run();
      }));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, kExampleValidJson));
  task_environment_.RunUntilQuit();

  // kDownloadError = 1
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 1, 1);
  histograms.ExpectUniqueSample(
      kKeyFetcherHttpResponseOrNetErrorCodeHistogramName, net::ERR_TIMED_OUT,
      1);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    base::HistogramTester histograms;

    auto quit_closure = task_environment_.QuitClosure();
    network_fetcher_->FetchPublicKeys(
        GURL(kExampleUrl),
        base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
          EXPECT_FALSE(keyset.has_value());
          quit_closure.Run();
        }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // We should not retry again.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
    task_environment_.RunUntilQuit();

    // kDownloadError = 1
    histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 1, 1);
    histograms.ExpectUniqueSample(
        kKeyFetcherHttpResponseOrNetErrorCodeHistogramName,
        net::ERR_NETWORK_CHANGED, 1);
  }

  // Retry succeeds
  {
    base::HistogramTester histograms;

    auto quit_closure = task_environment_.QuitClosure();
    network_fetcher_->FetchPublicKeys(
        GURL(kExampleUrl),
        base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
          EXPECT_TRUE(keyset.has_value());
          EXPECT_TRUE(aggregation_service::PublicKeysEqual(kExamplePublicKeys,
                                                           keyset->keys));
          quit_closure.Run();
        }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request with respoonse.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleUrl, kExampleValidJson));
    task_environment_.RunUntilQuit();

    // kSuccess = 0
    histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 0, 1);
    histograms.ExpectUniqueSample(
        kKeyFetcherHttpResponseOrNetErrorCodeHistogramName, net::HTTP_OK, 1);
  }
}

TEST_F(AggregationServiceNetworkFetcherTest, HttpError_CallbackRuns) {
  base::HistogramTester histograms;

  GURL url(kExampleUrl);
  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      url, base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        quit_closure.Run();
      }));

  // We should run the callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleUrl, /*content=*/"", net::HTTP_BAD_REQUEST));

  task_environment_.RunUntilQuit();

  // kDownloadError = 1
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 1, 1);
  histograms.ExpectUniqueSample(
      kKeyFetcherHttpResponseOrNetErrorCodeHistogramName, net::HTTP_BAD_REQUEST,
      1);
}

TEST_F(AggregationServiceNetworkFetcherTest, MultipleRequests_AllCallbacksRun) {
  base::HistogramTester histograms;
  auto barrier_closure =
      base::BarrierClosure(10, task_environment_.QuitClosure());

  GURL url(kExampleUrl);
  for (int i = 0; i < 10; i++) {
    network_fetcher_->FetchPublicKeys(
        url,
        base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
          barrier_closure.Run();
        }));
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 10);

  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleUrl, kExampleValidJson));
  }
  task_environment_.RunUntilQuit();

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // kSuccess = 0
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 0, 10);
}

TEST_F(AggregationServiceNetworkFetcherTest, VerifyExpiryTime) {
  base::HistogramTester histograms;

  const base::Clock* clock = task_environment_.GetMockClock();
  ASSERT_TRUE(clock);

  base::Time now = clock->Now();

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_TRUE(keyset.has_value());
        EXPECT_TRUE(aggregation_service::PublicKeysEqual(kExamplePublicKeys,
                                                         keyset->keys));
        EXPECT_EQ(keyset->fetch_time, now);
        EXPECT_EQ(keyset->expiry_time, now + base::Seconds(900));
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  auto response_head =
      network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
  response_head->request_time = now;
  response_head->response_time = now;

  response_head->headers->SetHeader("cache-control", "max-age=1000");
  response_head->headers->SetHeader("age", "100");

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kExampleUrl), network::URLLoaderCompletionStatus(net::OK),
      std::move(response_head), kExampleValidJson));
  task_environment_.RunUntilQuit();

  // kSuccess = 0
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 0, 1);
}

TEST_F(AggregationServiceNetworkFetcherTest, VerifyExpiredKeyOnFetch) {
  base::HistogramTester histograms;

  const base::Clock* clock = task_environment_.GetMockClock();
  ASSERT_TRUE(clock);

  base::Time now = clock->Now();

  auto quit_closure = task_environment_.QuitClosure();
  network_fetcher_->FetchPublicKeys(
      GURL(kExampleUrl),
      base::BindLambdaForTesting([&](std::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        quit_closure.Run();
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  auto response_head =
      network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
  response_head->request_time = now;
  response_head->response_time = now;

  response_head->headers->SetHeader("cache-control", "max-age=1000");
  response_head->headers->SetHeader("age", "1000");

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kExampleUrl), network::URLLoaderCompletionStatus(net::OK),
      std::move(response_head), kExampleValidJson));
  task_environment_.RunUntilQuit();

  // kExpiredKeyError = 4
  histograms.ExpectUniqueSample(kKeyFetcherStatusHistogramName, 4, 1);
}

}  // namespace content

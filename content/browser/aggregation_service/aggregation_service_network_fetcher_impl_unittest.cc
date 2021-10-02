// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kExampleOrigin[] = "https://helper.test/";
const char kExampleOriginKeysUrl[] =
    "https://helper.test/.well-known/aggregation-service/keys.json";

const char kExampleValidJson[] = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234"
                }
            ]
        }
    )";

const std::vector<PublicKey> kExamplePublicKeys = {
    PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes)};

}  // namespace

class AggregationServiceNetworkFetcherTest : public testing::Test {
 public:
  AggregationServiceNetworkFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_fetcher_(std::make_unique<AggregationServiceNetworkFetcherImpl>(
            task_environment_.GetMockClock(),
            /*storage_partition=*/nullptr)),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    network_fetcher_->SetURLLoaderFactoryForTesting(shared_url_loader_factory_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<AggregationServiceNetworkFetcherImpl> network_fetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AggregationServiceNetworkFetcherTest, RequestAttributes) {
  network_fetcher_->FetchPublicKeys(url::Origin::Create(GURL(kExampleOrigin)),
                                    base::DoNothing());

  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(request.url, kExampleOriginKeysUrl);
  EXPECT_EQ(request.method, net::HttpRequestHeaders::kGetMethod);
  EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);

  int load_flags = request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchPublicKeys_Success) {
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      url::Origin::Create(GURL(kExampleOrigin)),
      base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
        EXPECT_TRUE(keyset.has_value());
        EXPECT_TRUE(aggregation_service::PublicKeysEqual(kExamplePublicKeys,
                                                         keyset->keys));
        callback_run = true;
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, kExampleValidJson));
  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchPublicKeysInvalidKeyFormat_Failed) {
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      url::Origin::Create(GURL(kExampleOrigin)),
      base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        callback_run = true;
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, /*content=*/"{}"));
  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchPublicKeysMalformedJson_Failed) {
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      url::Origin::Create(GURL(kExampleOrigin)),
      base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        callback_run = true;
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, /*content=*/"{"));
  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchPublicKeysLargeBody_Failed) {
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      url::Origin::Create(GURL(kExampleOrigin)),
      base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        callback_run = true;
      }));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  std::string response_body = kExampleValidJson + std::string(1000000, ' ');
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, response_body));
  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetcherDeletedDuringRequest_NoCrash) {
  network_fetcher_->FetchPublicKeys(url::Origin::Create(GURL(kExampleOrigin)),
                                    base::DoNothing());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network_fetcher_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, kExampleValidJson));
}

TEST_F(AggregationServiceNetworkFetcherTest, FetchRequestHangs_TimesOut) {
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      url::Origin::Create(GURL(kExampleOrigin)),
      base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
        EXPECT_FALSE(keyset.has_value());
        callback_run = true;
      }));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, kExampleValidJson));
  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest,
       FetchRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    bool callback_run = false;
    network_fetcher_->FetchPublicKeys(
        url::Origin::Create(GURL(kExampleOrigin)),
        base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
          EXPECT_FALSE(keyset.has_value());
          callback_run = true;
        }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleOriginKeysUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleOriginKeysUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // We should not retry again.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
    EXPECT_TRUE(callback_run);
  }

  // Retry succeeds
  {
    bool callback_run = false;
    network_fetcher_->FetchPublicKeys(
        url::Origin::Create(GURL(kExampleOrigin)),
        base::BindLambdaForTesting([&](absl::optional<PublicKeyset> keyset) {
          EXPECT_TRUE(keyset.has_value());
          EXPECT_TRUE(aggregation_service::PublicKeysEqual(kExamplePublicKeys,
                                                           keyset->keys));
          callback_run = true;
        }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleOriginKeysUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request with respoonse.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleOriginKeysUrl, kExampleValidJson));
    EXPECT_TRUE(callback_run);
  }
}

TEST_F(AggregationServiceNetworkFetcherTest, HttpError_CallbackRuns) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  bool callback_run = false;
  network_fetcher_->FetchPublicKeys(
      origin,
      base::BindLambdaForTesting(
          [&](absl::optional<PublicKeyset> keyset) { callback_run = true; }));

  // We should run the callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleOriginKeysUrl, /*content=*/"",
      net::HttpStatusCode::HTTP_BAD_REQUEST));

  EXPECT_TRUE(callback_run);
}

TEST_F(AggregationServiceNetworkFetcherTest, MultipleRequests_AllCallbacksRun) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  int num_callbacks_run = 0;
  for (int i = 0; i < 10; i++) {
    network_fetcher_->FetchPublicKeys(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKeyset> keyset) { ++num_callbacks_run; }));
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 10);

  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleOriginKeysUrl, kExampleValidJson));
  }

  EXPECT_EQ(num_callbacks_run, 10);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

}  // namespace content

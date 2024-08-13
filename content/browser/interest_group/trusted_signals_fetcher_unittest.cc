// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_fetcher.h"

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class TrustedSignalsFetcherTest : public testing::Test {
 public:
  // This is the expected request body that corresponds to the request returned
  // by CreateBasicBiddingSignalsRequest(). It is the deterministic CBOR
  // representation of the following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kBasicBiddingSignalsRequestBody =
      "00000000A9A26A706172746974696F6E7381A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703164746167738172696E74657265737447726F75704E616D6573A26464617461"
      "81646B657931647461677381646B65797372636F6D7072657373696F6E47726F75704964"
      "0071616363657074436F6D7072657373696F6E82646E6F6E6564677A6970000000000000"
      "000000000000000000000000000000000000000000";

  TrustedSignalsFetcherTest() {
    embedded_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_test_server_.AddDefaultHandlers();
    embedded_test_server_.RegisterRequestHandler(
        base::BindRepeating(&TrustedSignalsFetcherTest::HandleSignalsRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server_.Start());
  }

  ~TrustedSignalsFetcherTest() override {
    base::AutoLock auto_lock(lock_);
    // Any request body should have been verified.
    EXPECT_FALSE(bidding_request_body_.has_value());
  }

  GURL TrustedBiddingSignalsUrl() const {
    return embedded_test_server_.GetURL(kTrustedSignalsHost,
                                        kTrustedBiddingSignalsPath);
  }

  // Creates a simple request with one compression group with a single
  // partition with only one key, and no other optional parameters.
  std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
  CreateBasicBiddingSignalsRequest() {
    std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions;
    bidding_partitions.emplace_back();
    bidding_partitions[0].partition_id = 0;
    bidding_partitions[0].hostname = "host.test";
    bidding_partitions[0].interest_group_names = {"group1"};
    bidding_partitions[0].keys = {"key1"};

    std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
        bidding_signals_request;
    bidding_signals_request.emplace(0, std::move(bidding_partitions));
    return bidding_signals_request;
  }

  TrustedSignalsFetcher::SignalsFetchResult
  RequestBiddingSignalsAndWaitForResult(
      const std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>&
          compression_groups,
      std::optional<GURL> signals_url = std::nullopt) {
    base::RunLoop run_loop;
    TrustedSignalsFetcher::SignalsFetchResult out;
    TrustedSignalsFetcher trusted_signals_fetcher;
    trusted_signals_fetcher.FetchBiddingSignals(
        url_loader_factory_.get(),
        signals_url ? *signals_url : TrustedBiddingSignalsUrl(),
        compression_groups,
        base::BindLambdaForTesting(
            [&](TrustedSignalsFetcher::SignalsFetchResult result) {
              out = std::move(result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return out;
  }

  std::string GetBiddingSignalsRequestBody() {
    base::AutoLock auto_lock(lock_);
    CHECK(bidding_request_body_.has_value());
    std::string out = std::move(bidding_request_body_).value();
    bidding_request_body_.reset();
    return out;
  }

  void ValidateRequestBody(std::string_view expected_response_hex) {
    std::string actual_response = GetBiddingSignalsRequestBody();
    EXPECT_EQ(base::HexEncode(actual_response), expected_response_hex);
    // If there's a mismatch, compare the non-hex-encoded string as well. This
    // may give a better idea what's wrong when looking at test output.
    if (HasNonfatalFailure()) {
      std::string expected_response;
      EXPECT_TRUE(
          base::HexStringToString(expected_response_hex, &expected_response));
      EXPECT_EQ(actual_response, expected_response);
    }
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleSignalsRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    if (request.relative_url == kTrustedBiddingSignalsPath) {
      EXPECT_FALSE(bidding_request_body_.has_value());
      EXPECT_THAT(
          request.headers,
          testing::Contains(std::pair(
              "Content-Type", TrustedSignalsFetcher::kRequestMediaType)));
      EXPECT_THAT(request.headers,
                  testing::Contains(std::pair(
                      "Accept", TrustedSignalsFetcher::kResponseMediaType)));
      EXPECT_EQ(request.headers.find("Cookie"), request.headers.end());
      EXPECT_TRUE(request.has_content);
      EXPECT_EQ(request.method_string, net::HttpRequestHeaders::kPostMethod);
      bidding_request_body_ = request.content;
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content_type(response_mime_type_);
      response->set_code(response_status_code_);
      // TODO(crbug.com/333445540): Return a response body, once
      // TrustedSignalsFetcher supports response body parsing.
      return response;
    }

    return nullptr;
  }

  // Need to use an IO thread for the TestSharedURLLoaderFactory, which lives on
  // the thread it's created on, to make network requests.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  const std::string kTrustedBiddingSignalsPath = "/bidder-signals";
  const std::string kTrustedSignalsHost = "a.test";

  // Values returned for requests to the test server for
  // `kTrustedBiddingSignalsPath`.
  std::string response_mime_type_{TrustedSignalsFetcher::kResponseMediaType};
  net::HttpStatusCode response_status_code_{net::HTTP_OK};

  base::Lock lock_;
  std::optional<std::string> bidding_request_body_ GUARDED_BY(lock_);

  net::test_server::EmbeddedTestServer embedded_test_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  // URLLoaderFactory that makes real network requests.
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_{
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true)};
};

TEST_F(TrustedSignalsFetcherTest, BiddingSignals404) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  response_status_code_ = net::HTTP_NOT_FOUND;
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().error_msg,
      base::StringPrintf(
          "Failed to load %s error = net::ERR_HTTP_RESPONSE_CODE_FAILURE.",
          TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBody(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsRedirect) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  GURL server_redirect_url = embedded_test_server_.GetURL(
      kTrustedSignalsHost,
      "/server-redirect?" + TrustedBiddingSignalsUrl().spec());
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request,
                                                      server_redirect_url);
  ASSERT_FALSE(result.has_value());
  // RedirectMode::kError results in ERR_FAILED errors on redirects, which
  // results in rather unhelpful error messages.
  EXPECT_EQ(result.error().error_msg,
            base::StringPrintf("Failed to load %s error = net::ERR_FAILED.",
                               server_redirect_url.spec().c_str()));
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMimeType) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  // Use the request media type instead of the response one.
  response_mime_type_ = TrustedSignalsFetcher::kRequestMediaType;
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().error_msg,
      base::StringPrintf("Rejecting load of %s due to unexpected MIME type.",
                         TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBody(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsCanSetNoCookies) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  // Request trusted bidding signals using a URL that tries to set a cookie.
  GURL set_cookie_url = embedded_test_server_.GetURL(
      kTrustedSignalsHost, "/set-cookie?a=1;Secure;SameSite=None");
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request,
                                                      set_cookie_url);

  // Request should have failed due to a missing MIME type.
  EXPECT_EQ(
      result.error().error_msg,
      base::StringPrintf("Rejecting load of %s due to unexpected MIME type.",
                         set_cookie_url.spec().c_str()));

  // Make sure no cookie was set.
  base::RunLoop run_loop;
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  url_loader_factory_->network_context()->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());
  cookie_manager->GetAllCookies(
      base::BindLambdaForTesting([&](const net::CookieList& cookies) {
        EXPECT_TRUE(cookies.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsHasNoCookies) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  // Set a same-site none cookie on the trusted signals server's origin.
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  url_loader_factory_->network_context()->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());
  net::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      TrustedBiddingSignalsUrl(), "a=1; Secure; SameSite=None",
      base::Time::Now(),
      /*server_time=*/std::nullopt,
      /*cookie_partition_key=*/std::nullopt, net::CookieSourceType::kHTTP,
      &status);
  ASSERT_TRUE(cookie);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      *cookie, TrustedBiddingSignalsUrl(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindLambdaForTesting([&](net::CookieAccessResult result) {
        EXPECT_TRUE(result.status.IsInclude());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Request trusted bidding signals. The request handler will cause the test to
  // fail if it sees a cookie header.
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsNoKeys) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  bidding_signals_request[0][0].keys.clear();

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": []
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000A4A26A706172746974696F6E7381A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703164746167738172696E74657265737447726F75704E616D6573A26464617461"
      "80647461677381646B65797372636F6D7072657373696F6E47726F757049640071616363"
      "657074436F6D7072657373696F6E82646E6F6E6564677A69700000000000000000000000"
      "000000000000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsOneKey) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleKeys) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  bidding_signals_request[0][0].keys = {"key1", "key2", "key3"};

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1", "key2", "key3" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000B3A26A706172746974696F6E7381A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703164746167738172696E74657265737447726F75704E616D6573A26464617461"
      "83646B657931646B657932646B657933647461677381646B65797372636F6D7072657373"
      "696F6E47726F757049640071616363657074436F6D7072657373696F6E82646E6F6E6564"
      "677A69700000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleInterestGroups) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  bidding_signals_request[0][0].interest_group_names = {"group1", "group2",
                                                        "group3"};

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1", "group2", "group3" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000B7A26A706172746974696F6E7381A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461836667"
      "726F7570316667726F7570326667726F75703364746167738172696E7465726573744772"
      "6F75704E616D6573A2646461746181646B657931647461677381646B65797372636F6D70"
      "72657373696F6E47726F757049640071616363657074436F6D7072657373696F6E82646E"
      "6F6E6564677A697000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsOneAdditionalParam) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  bidding_signals_request[0][0].additional_params.Set("foo", "bar");

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test", "foo": "bar" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000B1A26A706172746974696F6E7381A462696400686D65746164617461A263666F"
      "6F6362617268686F73746E616D6569686F73742E7465737469617267756D656E747382A2"
      "6464617461816667726F75703164746167738172696E74657265737447726F75704E616D"
      "6573A2646461746181646B657931647461677381646B65797372636F6D7072657373696F"
      "6E47726F757049640071616363657074436F6D7072657373696F6E82646E6F6E6564677A"
      "697000000000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleAdditionalParams) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  bidding_signals_request[0][0].additional_params.Set("foo", "bar");
  bidding_signals_request[0][0].additional_params.Set("Foo", "bAr");
  bidding_signals_request[0][0].additional_params.Set("oof", "rab");

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "host.test",
  //         "foo": "bar",
  //         "Foo": "bAr",
  //         "oof": "rab",
  //       },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000C1A26A706172746974696F6E7381A462696400686D65746164617461A463466F"
      "6F6362417263666F6F63626172636F6F666372616268686F73746E616D6569686F73742E"
      "7465737469617267756D656E747382A26464617461816667726F75703164746167738172"
      "696E74657265737447726F75704E616D6573A2646461746181646B657931647461677381"
      "646B65797372636F6D7072657373696F6E47726F757049640071616363657074436F6D70"
      "72657373696F6E82646E6F6E6564677A6970000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

// Test a single compression group with a single partition, where neither has
// the index 0.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsNoZeroIndices) {
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions;
  bidding_partitions.emplace_back();
  bidding_partitions[0].partition_id = 7;
  bidding_partitions[0].hostname = "host.test";
  bidding_partitions[0].interest_group_names = {"group7"};
  bidding_partitions[0].keys = {"key1"};
  std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
      bidding_signals_request;
  bidding_signals_request.emplace(3, std::move(bidding_partitions));

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 3,
  //       "id": 7,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group7" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000000A9A26A706172746974696F6E7381A462696407686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703764746167738172696E74657265737447726F75704E616D6573A26464617461"
      "81646B657931647461677381646B65797372636F6D7072657373696F6E47726F75704964"
      "0371616363657074436F6D7072657373696F6E82646E6F6E6564677A6970000000000000"
      "000000000000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultiplePartitions) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto* bidding_partitions = &bidding_signals_request[0];

  auto* bidding_partition2 = &bidding_partitions->emplace_back();
  bidding_partition2->partition_id = 1;
  bidding_partition2->hostname = "host2.test";
  bidding_partition2->interest_group_names = {"group2"};
  bidding_partition2->keys = {"key2"};
  bidding_partition2->additional_params.Set("foo", "bar");

  auto* bidding_partition3 = &bidding_partitions->emplace_back();
  bidding_partition3->partition_id = 2;
  bidding_partition3->hostname = "host3.test";
  bidding_partition3->interest_group_names = {"group1", "group2", "group3"};
  bidding_partition3->keys = {"key1", "key2", "key3"};
  bidding_partition3->additional_params.Set("foo2", "bar2");

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     },
  //     {
  //       "compressionGroupId": 0,
  //       "id": 1,
  //       "metadata": { "hostname": "host2.test", "foo": "bar" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group2" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key2" ]
  //         }
  //       ]
  //     },
  //     {
  //       "compressionGroupId": 0,
  //       "id": 2,
  //       "metadata": { "hostname": "host3.test", "foo2": "bar2"  },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1", "group2", "group3" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1", "key2", "key3" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000001D3A26A706172746974696F6E7383A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703164746167738172696E74657265737447726F75704E616D6573A26464617461"
      "81646B657931647461677381646B65797372636F6D7072657373696F6E47726F75704964"
      "00A462696401686D65746164617461A263666F6F6362617268686F73746E616D656A686F"
      "7374322E7465737469617267756D656E747382A26464617461816667726F757032647461"
      "67738172696E74657265737447726F75704E616D6573A2646461746181646B6579326474"
      "61677381646B65797372636F6D7072657373696F6E47726F7570496400A462696402686D"
      "65746164617461A264666F6F32646261723268686F73746E616D656A686F7374332E7465"
      "737469617267756D656E747382A26464617461836667726F7570316667726F7570326667"
      "726F75703364746167738172696E74657265737447726F75704E616D6573A26464617461"
      "83646B657931646B657932646B657933647461677381646B65797372636F6D7072657373"
      "696F6E47726F757049640071616363657074436F6D7072657373696F6E82646E6F6E6564"
      "677A69700000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleCompressionGroups) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions2;
  auto* bidding_partition2 = &bidding_partitions2.emplace_back();
  bidding_partition2->partition_id = 0;
  bidding_partition2->hostname = "host2.test";
  bidding_partition2->interest_group_names = {"group2"};
  bidding_partition2->keys = {"key2"};
  bidding_partition2->additional_params.Set("foo", "bar");
  bidding_signals_request.emplace(1, std::move(bidding_partitions2));

  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions3;
  auto* bidding_partition3 = &bidding_partitions3.emplace_back();
  bidding_partition3->partition_id = 0;
  bidding_partition3->hostname = "host3.test";
  bidding_partition3->interest_group_names = {"group1", "group2", "group3"};
  bidding_partition3->keys = {"key1", "key2", "key3"};
  bidding_partition3->additional_params.Set("foo2", "bar2");
  bidding_signals_request.emplace(2, std::move(bidding_partitions3));

  // The expected request body is the deterministic CBOR representation of the
  // following, with a prefix and padding added:
  // {
  //   "acceptCompression": [ "none", "gzip" ],
  //   "partitions": [
  //     {
  //       "compressionGroupId": 0,
  //       "id": 0,
  //       "metadata": { "hostname": "host.test" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1" ]
  //         }
  //       ]
  //     },
  //     {
  //       "compressionGroupId": 0,
  //       "id": 1,
  //       "metadata": { "hostname": "host2.test", "foo", "bar" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group2" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key2" ]
  //         }
  //       ]
  //     },
  //     {
  //       "compressionGroupId": 0,
  //       "id": 2,
  //       "metadata": { "hostname": "host3.test", "foo2", "bar2" },
  //       "arguments": [
  //         {
  //           "tags": [ "interestGroupNames" ],
  //           "data": [ "group1", "group2", "group3" ]
  //         },
  //         {
  //           "tags": [ "keys" ],
  //           "data": [ "key1", "key2", "key3" ]
  //         }
  //       ]
  //     }
  //   ]
  // }
  const std::string_view kExpectedRequestBody =
      "00000001D3A26A706172746974696F6E7383A462696400686D65746164617461A168686F"
      "73746E616D6569686F73742E7465737469617267756D656E747382A26464617461816667"
      "726F75703164746167738172696E74657265737447726F75704E616D6573A26464617461"
      "81646B657931647461677381646B65797372636F6D7072657373696F6E47726F75704964"
      "00A462696400686D65746164617461A263666F6F6362617268686F73746E616D656A686F"
      "7374322E7465737469617267756D656E747382A26464617461816667726F757032647461"
      "67738172696E74657265737447726F75704E616D6573A2646461746181646B6579326474"
      "61677381646B65797372636F6D7072657373696F6E47726F7570496401A462696400686D"
      "65746164617461A264666F6F32646261723268686F73746E616D656A686F7374332E7465"
      "737469617267756D656E747382A26464617461836667726F7570316667726F7570326667"
      "726F75703364746167738172696E74657265737447726F75704E616D6573A26464617461"
      "83646B657931646B657932646B657933647461677381646B65797372636F6D7072657373"
      "696F6E47726F757049640271616363657074436F6D7072657373696F6E82646E6F6E6564"
      "677A69700000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000";

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ValidateRequestBody(kExpectedRequestBody);
}

// Test that the expected amount of padding is added.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsPadding) {
  // TODO(crbug.com/333445540): Once encryption is added, test the request body
  // size both before and after encryption.
  const struct {
    size_t interest_group_name_length;
    size_t expected_body_length;
    size_t expected_padding;
  } kTestCases[] = {
      {31, 201, 1},
      {32, 201, 0},
      {33, 457, 255},

      // 286 is 1 less than 31+256 because strings in cbor are length-prefixed.
      {286, 457, 1},
      {287, 457, 0},
      {288, 969, 511},
  };

  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.interest_group_name_length);
    bidding_signals_request[0][0].interest_group_names = {
        std::string(test_case.interest_group_name_length, 'a')};
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    std::string request_body = GetBiddingSignalsRequestBody();
    size_t padding =
        request_body.size() - request_body.find_last_not_of('\0') - 1;
    EXPECT_EQ(request_body.size(), test_case.expected_body_length);
    EXPECT_EQ(padding, test_case.expected_padding);
  }
}

}  // namespace
}  // namespace content

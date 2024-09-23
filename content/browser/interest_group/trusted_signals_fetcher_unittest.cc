// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_fetcher.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/public/test/browser_test_utils.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
const uint8_t kTestPrivateKey[] = {
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
};

const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

const uint8_t kKeyId = 3;

// Helper to create a CompressionGroupResult given all field values.
// `compression_group_data` is a string that will be CBOR encoded to form the
// expected compression group body.
TrustedSignalsFetcher::CompressionGroupResult CreateCompressionGroupResult(
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme,
    std::string_view compression_group_data,
    base::TimeDelta ttl) {
  TrustedSignalsFetcher::CompressionGroupResult out;
  out.compression_scheme = compression_scheme;
  std::optional<std::vector<uint8_t>> content_string =
      cbor::Writer::Write(cbor::Value(compression_group_data));
  CHECK(content_string);
  out.compression_group_data = std::move(content_string).value();
  out.ttl = ttl;
  return out;
}

class TrustedSignalsFetcherTest : public testing::Test {
 public:
  // This is the expected request body that corresponds to the request returned
  // by CreateBasicBiddingSignalsRequest(). Stored as a raw hex string to
  // provide better coverage of padding logic than using
  // CreateKVv2RequestBody(), which uses the same padding code as the fetcher.
  // It is the deterministic CBOR representation of the following, with a prefix
  // and padding added:
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
    SetResponseBodyAndAddHeader(DefaultResponseBody());
  }

  ~TrustedSignalsFetcherTest() override {
    base::AutoLock auto_lock(lock_);
    // Any request body should have been verified.
    EXPECT_FALSE(bidding_request_body_.has_value());
  }

  // CBOR representation of a response with a single compression group. Same for
  // both bidding and scoring signals.
  static std::string DefaultResponseBody() {
    return auction_worklet::test::ToKVv2ResponseCborString(
        R"({
             "compressionGroups": [
               {
                 "compressionGroupId": 0,
                 "ttlMs" : 100,
                 "content" : "compression group content"
               }
             ]
           })");
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
    bidding_partitions.emplace_back(
        /*partition_id=*/0, &kDefaultInterestGroupNames, &kDefaultKeys,
        &kDefaultHostname, &kDefaultAdditionalParams);

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
        BiddingAndAuctionServerKey{
            std::string(reinterpret_cast<const char*>(kTestPublicKey),
                        sizeof(kTestPublicKey)),
            kKeyId},
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

  size_t GetEncryptedRequestBodyLength() {
    base::AutoLock auto_lock(lock_);
    return encrypted_request_body_length_;
  }

  // Checks that the request body matches the provided string, which contains a
  // hex-encoded representation of the expected result.
  void ValidateRequestBodyHex(std::string_view expected_request_hex) {
    std::string actual_response = GetBiddingSignalsRequestBody();
    EXPECT_EQ(base::HexEncode(actual_response), expected_request_hex);
    // If there's a mismatch, compare the non-hex-encoded string as well. This
    // may give a better idea what's wrong when looking at test output.
    if (HasNonfatalFailure()) {
      std::string expected_response;
      EXPECT_TRUE(
          base::HexStringToString(expected_request_hex, &expected_response));
      EXPECT_EQ(actual_response, expected_response);
    }
  }

  // Checks that the request body matches the provided JSON. Converts the JSON
  // input to a cbor string, adds a framing header and padding, and then check
  // that matches the request body.
  void ValidateRequestBodyJson(std::string_view expected_request_json) {
    auto expected_request = auction_worklet::test::CreateKVv2RequestBody(
        auction_worklet::test::ToCborString(expected_request_json));
    ValidateRequestBodyHex(base::HexEncode(expected_request));
  }

  // Sets the response body string.
  void SetResponseBody(std::string response_body, bool use_cleartext = false) {
    base::AutoLock auto_lock(lock_);
    response_body_ = std::move(response_body);
    use_cleartext_response_body_ = use_cleartext;
  }

  // Convenience wrapper that calls CreateKVv2ResponseBody() on the provided
  // values, and sets the resulting string as the response body.
  void SetResponseBodyAndAddHeader(
      std::string_view cbor_response_body,
      std::optional<size_t> advertised_cbor_length = std::nullopt,
      size_t padding_length = 0,
      uint8_t compression_scheme = 0) {
    SetResponseBody(auction_worklet::test::CreateKVv2ResponseBody(
        cbor_response_body, advertised_cbor_length, padding_length,
        compression_scheme));
  }

  // Helper to, in the case of a successfully fetched result, compare `result`
  // to `expected_result`. Has an assertion failure if result indicates a
  // failure.
  void ValidateFetchResult(
      const TrustedSignalsFetcher::SignalsFetchResult& result,
      const TrustedSignalsFetcher::CompressionGroupResultMap& expected_result) {
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), expected_result.size());
    for (auto result_it = result->begin(),
              expected_result_it = expected_result.begin();
         result_it != result->end(); ++result_it, ++expected_result_it) {
      // This is the compression group index of the expected result.
      SCOPED_TRACE(expected_result_it->first);

      EXPECT_EQ(result_it->first, expected_result_it->first);
      EXPECT_EQ(result_it->second.compression_scheme,
                expected_result_it->second.compression_scheme);
      EXPECT_EQ(result_it->second.compression_group_data,
                expected_result_it->second.compression_group_data);
      EXPECT_EQ(result_it->second.ttl, expected_result_it->second.ttl);
    }
  }

  // Checks that the fetch result matches what `DefaultResponseBody()` is
  // expected to be parsed as.
  void ValidateDefaultFetchResult(
      const TrustedSignalsFetcher::SignalsFetchResult& result) {
    TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
    expected_result.try_emplace(
        0, CreateCompressionGroupResult(
               auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
               "compression group content", base::Milliseconds(100)));
    ValidateFetchResult(result, expected_result);
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

      auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
          kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_256_GCM);
      EXPECT_TRUE(config.ok()) << config.status();

      auto ohttp_gateway =
          quiche::ObliviousHttpGateway::Create(
              std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                          sizeof(kTestPrivateKey)),
              config.value())
              .value();
      encrypted_request_body_length_ = request.content.size();
      auto plaintext_ohttp_request_body =
          ohttp_gateway.DecryptObliviousHttpRequest(
              request.content, TrustedSignalsFetcher::kRequestMediaType);
      EXPECT_TRUE(plaintext_ohttp_request_body.ok())
          << plaintext_ohttp_request_body.status();
      bidding_request_body_ = plaintext_ohttp_request_body->GetPlaintextData();

      std::string response_body;
      // Encryption doesn't support empty strings.
      if (response_body_.size() > 0u && !use_cleartext_response_body_) {
        auto context =
            std::move(plaintext_ohttp_request_body).value().ReleaseContext();
        auto ciphertext_ohttp_response_body =
            ohttp_gateway.CreateObliviousHttpResponse(
                response_body_, context,
                TrustedSignalsFetcher::kResponseMediaType);
        EXPECT_TRUE(ciphertext_ohttp_response_body.ok())
            << ciphertext_ohttp_response_body.status();
        response_body =
            ciphertext_ohttp_response_body->EncapsulateAndSerialize();
      } else {
        response_body = response_body_;
      }

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content_type(response_mime_type_);
      response->set_code(response_status_code_);
      response->set_content(response_body);
      return response;
    }

    return nullptr;
  }

  // Need to use an IO thread for the TestSharedURLLoaderFactory, which lives on
  // the thread it's created on, to make network requests.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  const std::string kTrustedBiddingSignalsPath = "/bidder-signals";
  const std::string kTrustedSignalsHost = "a.test";

  // Default values used by CreateBasicBiddingSignalsRequest(). They need to be
  // fields of the test fixture to keep them alive, since the returned
  // BiddingPartition holds onto non-owning raw pointers.
  const std::set<std::string> kDefaultInterestGroupNames{"group1"};
  const std::set<std::string> kDefaultKeys{"key1"};
  const std::string kDefaultHostname{"host.test"};
  const base::Value::Dict kDefaultAdditionalParams;

  // Values returned for requests to the test server for
  // `kTrustedBiddingSignalsPath`.
  std::string response_mime_type_{TrustedSignalsFetcher::kResponseMediaType};
  net::HttpStatusCode response_status_code_{net::HTTP_OK};

  base::Lock lock_;
  // Size of the original encrypted request body.
  size_t encrypted_request_body_length_ GUARDED_BY(lock_);
  // The most recent bidding request body received by the embedded test server,
  // after decyption. Separate bidding / scoring headers to provide basic
  // protections against unexpectedly receiving the wrong type of request.
  std::optional<std::string> bidding_request_body_ GUARDED_BY(lock_);
  // The response body to reply with. Unlike request bodies, not separated for
  // scoring / bidding.
  std::string response_body_ GUARDED_BY(lock_);
  // If true, the response body is not encrypted, which should result in an
  // error.
  bool use_cleartext_response_body_ GUARDED_BY(lock_) = false;

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
      result.error(),
      base::StringPrintf(
          "Failed to load %s error = net::ERR_HTTP_RESPONSE_CODE_FAILURE.",
          TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
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
  EXPECT_EQ(result.error(),
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
      result.error(),
      base::StringPrintf("Rejecting load of %s due to unexpected MIME type.",
                         TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
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
      result.error(),
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
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsNoKeys) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  const std::set<std::string> kNoKeys;
  bidding_signals_request[0][0].keys = kNoKeys;

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": []
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsOneKey) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleKeys) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  const std::set<std::string> kKeys = {"key1", "key2", "key3"};
  bidding_signals_request[0][0].keys = kKeys;

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1", "key2", "key3" ]
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleInterestGroups) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  const std::set<std::string> kInterestGroupNames = {"group1", "group2",
                                                     "group3"};
  bidding_signals_request[0][0].interest_group_names = kInterestGroupNames;

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1", "group2", "group3" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsOneAdditionalParam) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  base::Value::Dict additional_params;
  additional_params.Set("foo", base::Value("bar"));
  bidding_signals_request[0][0].additional_params = additional_params;

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test", "foo": "bar" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleAdditionalParams) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  base::Value::Dict additional_params;
  additional_params.Set("foo", "bar");
  additional_params.Set("Foo", "bAr");
  additional_params.Set("oof", "rab");
  bidding_signals_request[0][0].additional_params = additional_params;

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": {
                 "hostname": "host.test",
                 "foo": "bar",
                 "Foo": "bAr",
                 "oof": "rab",
               },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

// Test a single compression group with a single partition, where neither has
// the index 0.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsNoZeroIndices) {
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions;
  bidding_partitions.emplace_back(/*partition_id=*/7,
                                  &kDefaultInterestGroupNames, &kDefaultKeys,
                                  &kDefaultHostname, &kDefaultAdditionalParams);
  std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
      bidding_signals_request;
  bidding_signals_request.emplace(3, std::move(bidding_partitions));

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 3,
               "id": 7,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             }
           ]
         })";

  // The response similarly only includes information for compression group 3.
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
             "compressionGroups": [
               {
                 "compressionGroupId": 3,
                 "content": "content"
               }
             ]
           })"));

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      3, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content", base::Milliseconds(0)));
  ValidateFetchResult(result, expected_result);
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

// Test that the expected amount of padding is added to requests.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsRequestPadding) {
  const struct {
    size_t interest_group_name_length;
    // Test the encrypted and unecrypted request body.  The encrypted body
    // length, which should always be a power 2, is what's actually publicly
    // visible. The others are useful for debugging.
    size_t expected_encrypted_body_length;
    size_t expected_body_length;
    size_t expected_padding;
  } kTestCases[] = {
      {31, 256, 201, 1},
      {32, 256, 201, 0},
      {33, 512, 457, 255},

      // 286 is 1 less than 31+256 because strings in cbor are length-prefixed.
      {286, 512, 457, 1},
      {287, 512, 457, 0},
      {288, 1024, 969, 511},
  };

  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.interest_group_name_length);
    std::string name = std::string(test_case.interest_group_name_length, 'a');
    std::set<std::string> interest_group_names = {name};
    bidding_signals_request[0][0].interest_group_names = interest_group_names;
    ValidateDefaultFetchResult(
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
    EXPECT_EQ(GetEncryptedRequestBodyLength(),
              test_case.expected_encrypted_body_length);
    std::string request_body = GetBiddingSignalsRequestBody();
    size_t padding =
        request_body.size() - request_body.find_last_not_of('\0') - 1;
    EXPECT_EQ(request_body.size(), test_case.expected_body_length);
    EXPECT_EQ(padding, test_case.expected_padding);

    // Also test the entire request body directly. The above checks provide some
    // protection against issues in CreateKVv2RequestBody(), which is largely
    // copied from TrustedSignalsFetcher.
    EXPECT_EQ(request_body, auction_worklet::test::CreateKVv2RequestBody(
                                auction_worklet::test::ToCborString(JsReplace(
                                    R"(
                                    {
                                      "acceptCompression": [ "none", "gzip" ],
                                      "partitions": [
                                        {
                                          "compressionGroupId": 0,
                                          "id": 0,
                                          "metadata": {
                                            "hostname": "host.test"
                                          },
                                          "arguments": [
                                            {
                                              "tags": [ "interestGroupNames" ],
                                              "data": [ $1 ]
                                            },
                                            {
                                              "tags": [ "keys" ],
                                              "data": [ "key1" ]
                                            }
                                          ]
                                        }
                                      ]
                                    })",
                                    name))));
  }
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsResponseBodyShorterThanHeader) {
  for (int length = 0; length < 5; ++length) {
    SetResponseBody(std::string(length, 0));
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              base::StringPrintf(
                  "Failed to load %s: Response body is shorter than a "
                  "message/ad-auction-trusted-signals-response header.",
                  TrustedBiddingSignalsUrl().spec().c_str()));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsResponseBodyUnencrypted) {
  SetResponseBody(std::string(kBasicBiddingSignalsRequestBody),
                  /*use_plantext=*/true);
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StringPrintf("Failed to load %s: OHTTP decryption failed.",
                               TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

// Receiving CBOR without a header in the response body should result in
// failure.
TEST_F(TrustedSignalsFetcherTest, NoResponseBodyHeader) {
  SetResponseBody(DefaultResponseBody());
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  // Don't check the specific error - it depends on the specific details of the
  // CBOR representation of DefaultResponseBody().
  ASSERT_FALSE(result.has_value());
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

// This test does not actually gzip the body. The fetcher doesn't try to
// decompress anything, so that should be fine.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsCompressionSchemeGzip) {
  SetResponseBodyAndAddHeader(DefaultResponseBody(),
                              /*advertised_cbor_length=*/std::nullopt,
                              /*padding_length=*/0,
                              /*compression_scheme=*/2);
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
             "compression group content", base::Milliseconds(100)));
  ValidateFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request),
      expected_result);
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsCompressionSchemeUnsupported) {
  for (int compression_scheme : {1, 3}) {
    SetResponseBodyAndAddHeader(DefaultResponseBody(),
                                /*advertised_cbor_length=*/std::nullopt,
                                /*padding_length=*/0, compression_scheme);
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        base::StringPrintf(
            "Failed to load %s: Unsupported compression scheme: %u.",
            TrustedBiddingSignalsUrl().spec().c_str(), compression_scheme));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

TEST_F(TrustedSignalsFetcherTest,
       BiddingSignalsCompressionSchemeHighOrderBitsIgnored) {
  // Everything but the low order two bits of the compression scheme should be
  // ignored, so this should be treated as scheme 2 - gzip.
  SetResponseBodyAndAddHeader(DefaultResponseBody(),
                              /*advertised_cbor_length=*/std::nullopt,
                              /*padding_length=*/0,
                              /*compression_scheme=*/0xFE);
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
             "compression group content", base::Milliseconds(100)));
  ValidateFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request),
      expected_result);
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

// If the advertised length is longer than the response, the request should
// fail, even if it's otherwise a valid CBOR response. This test also checks the
// case where the maximum possible length is received, to make sure there are no
// overflow/underflow issues.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsAdvertisedLengthTooLong) {
  const std::string response_body = DefaultResponseBody();
  const size_t kTestCases[] = {response_body.length() + 1,
                               std::numeric_limits<uint32_t>::max()};
  for (size_t advertised_cbor_length : kTestCases) {
    SetResponseBodyAndAddHeader(response_body, advertised_cbor_length);
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              base::StringPrintf(
                  "Failed to load %s: Length header exceeds body size.",
                  TrustedBiddingSignalsUrl().spec().c_str()));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

// If the advertised shorter is longer than the response, the remaining bytes
// should be ignored, even if they make an otherwise valid CBOR response.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsAdvertisedLengthTooShort) {
  SetResponseBodyAndAddHeader(DefaultResponseBody(),
                              DefaultResponseBody().length() / 2 - 1);
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      base::StringPrintf("Failed to load %s: Failed to parse response as CBOR.",
                         TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsResponsePadding) {
  // No need to check length 0 - that's the default amount of padding added by
  // SetResponseBodyAndAddHeader().
  for (size_t padding_length : {1, 2, 16, 1023, 1024}) {
    SetResponseBodyAndAddHeader(DefaultResponseBody(),
                                /*advertised_cbor_length=*/std::nullopt,
                                padding_length);
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    ValidateDefaultFetchResult(
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

// Test the case where there are valid framing headers, but the response body is
// not CBOR.
TEST_F(TrustedSignalsFetcherTest, NotCbor) {
  const std::string_view kTestCases[] = {
      // This is "This is not CBOR." as a hex string.
      "\x54\x68\x69\x73\x20\x69\x73\x20\x6E\x6F\x74\x20\x43\x42\x4F\x52\x2E",

      // Null in CBOR, which is currently rejected as not being CBOR by the CBOR
      // parser, which is a little weird. Seems
      // best to test this case, though, and if we ever do parse it, move it
      // into the next test.
      "\xF6",

      // CBOR has a lot of values that don't map to JSON or even to Javascript
      // objects. This is a very incomplete set of some types of them, without
      // delving into the spec.

      // Undefined in CBOR.
      "\xF7",

      // An unassigned CBOR value.
      "\xF0",

      // A reserved CBOR value.
      "\xF8\x20",
  };

  for (const std::string_view& test_string : kTestCases) {
    SCOPED_TRACE(base::HexEncode(test_string));
    SetResponseBodyAndAddHeader(test_string);
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              base::StringPrintf(
                  "Failed to load %s: Failed to parse response as CBOR.",
                  TrustedBiddingSignalsUrl().spec().c_str()));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

// Test cases where there's a valid framing header, and the response is CBOR,
// but it's not a map.
TEST_F(TrustedSignalsFetcherTest, NotCborMap) {
  const std::string_view kTestCases[] = {
      R"(true)",
      R"("This is a string")",
      R"(42)",
      R"(["array"])",
  };

  for (const std::string_view& test_string : kTestCases) {
    SCOPED_TRACE(test_string);
    SetResponseBodyAndAddHeader(
        auction_worklet::test::ToKVv2ResponseCborString(test_string));
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        base::StringPrintf("Failed to load %s: Response body is not a map.",
                           TrustedBiddingSignalsUrl().spec().c_str()));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

TEST_F(TrustedSignalsFetcherTest, NoCompressionGroupMap) {
  // An empty map.
  SetResponseBodyAndAddHeader(
      auction_worklet::test::ToKVv2ResponseCborString("{}"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      base::StringPrintf(
          "Failed to load %s: Response is missing compressionGroups array.",
          TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, CompressionGroupsNotArray) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({"compressionGroups": {}})"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      base::StringPrintf(
          "Failed to load %s: Response is missing compressionGroups array.",
          TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, NoCompressionGroups) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({"compressionGroups": []})"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  // The fetch succeeds, and the result is an empty map. The cache layer, which
  // maps requests to fetched compression groups, will consider this a failure,
  // but the fetcher considers this a valid result.
  ValidateFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request),
      TrustedSignalsFetcher::CompressionGroupResultMap());
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, CompressionGroupNotMap) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({"compressionGroups": [[]]})"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StringPrintf(
                "Failed to load %s: Compression group is not of type map.",
                TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest,
       CompressionGroupWithBadOrNoCompressionGroupId) {
  const std::string_view kTestCases[] = {
      R"({
           "compressionGroups": [
             {
               "content" : "content"
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": "Jim",
               "content" : "content"
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": -1,
               "content" : "content"
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0.0,
               "content" : "content"
             }
           ]
         })",
  };

  for (const std::string_view test_string : kTestCases) {
    SCOPED_TRACE(test_string);
    SetResponseBodyAndAddHeader(
        auction_worklet::test::ToKVv2ResponseCborString(test_string));
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        base::StringPrintf("Failed to load %s: Compression group must have a "
                           "non-negative integer compressionGroupId.",
                           TrustedBiddingSignalsUrl().spec().c_str()));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

TEST_F(TrustedSignalsFetcherTest, CompressionGroupWithBadOrNoContent) {
  // Each test case uses a different compression group ID to test the error
  // output. Note that TrustedSignalsFetcher has no requirement that returned
  // compression groups match requested compression groups. That's enforced by
  // the cache layer, since it has to match compression groups to requests it
  // sent out, anyways.
  const std::vector<std::string_view> kTestCases = {
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 1,
               "content" : 5
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 2,
               "content" : ["content"]
             }
           ]
         })",

      // This content type is a string instead of a binary string, which should
      // result in an error.
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 3,
               "content" : "content"
             }
           ]
         })",
  };

  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(kTestCases[i]);
    // Note that this uses ToCborString() to convert the JSON to a CBOR string
    // rather than ToKVv2ResponseCborString(). This results in the "content"
    // fields not being encoded as binary strings, but rather as whatever CBOR
    // type corresponds to the JSON type of the "content" field.
    SetResponseBodyAndAddHeader(
        auction_worklet::test::ToCborString(kTestCases[i]));
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              base::StringPrintf("Failed to load %s: Compression group %" PRIuS
                                 " missing binary string \"content\".",
                                 TrustedBiddingSignalsUrl().spec().c_str(), i));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

TEST_F(TrustedSignalsFetcherTest, CompressionGroupWithBadTtl) {
  // Each test case uses a different compression group ID to test the error
  // output. Note that TrustedSignalsFetcher has no requirement that returned
  // compression groups match requested compression groups. That's enforced by
  // the cache layer, since it has to match compression groups to requests it
  // sent out, anyways.
  const std::vector<std::string_view> kTestCases = {
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content",
               "ttlMs": "grapefruit"
             }
           ]
         })",

      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 1,
               "content": "content",
               "ttlMs": 0.5
             }
           ]
         })",
  };

  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(kTestCases[i]);
    SetResponseBodyAndAddHeader(
        auction_worklet::test::ToKVv2ResponseCborString(kTestCases[i]));
    auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
    auto result =
        RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              base::StringPrintf("Failed to load %s: Compression group %" PRIuS
                                 " ttlMs value is not an integer.",
                                 TrustedBiddingSignalsUrl().spec().c_str(), i));
    ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
  }
}

// `ttlMs` is an optional field. When not present, we currently default to a
// value of 0.
TEST_F(TrustedSignalsFetcherTest, CompressionGroupWithNoTtl) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content"
             }
           ]
         })"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content", base::Milliseconds(0)));
  ValidateFetchResult(result, expected_result);
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, CompressionGroupWithZeroTtl) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content",
               "ttlMs": 0
             }
           ]
         })"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content", base::Milliseconds(0)));
  ValidateFetchResult(result, expected_result);
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

// Negative TTLs are allows, and are treated as if they were zero.
TEST_F(TrustedSignalsFetcherTest, CompressionGroupWithNegativeTtl) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content",
               "ttlMs": -1
             }
           ]
         })"));
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content", base::Milliseconds(0)));
  ValidateFetchResult(result, expected_result);
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultiplePartitions) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto* bidding_partitions = &bidding_signals_request[0];

  const std::set<std::string> kInterestGroupNames2{"group2"};
  const std::set<std::string> kKeys2{"key2"};
  const std::string kHostname2{"host2.test"};
  base::Value::Dict additional_params2;
  additional_params2.Set("foo", "bar");
  bidding_partitions->emplace_back(/*partition_id=*/1, &kInterestGroupNames2,
                                   &kKeys2, &kHostname2, &additional_params2);

  const std::set<std::string> kInterestGroupNames3{"group1", "group2",
                                                   "group3"};
  const std::set<std::string> kKeys3{"key1", "key2", "key3"};
  const std::string kHostname3{"host3.test"};
  base::Value::Dict additional_params3;
  additional_params3.Set("foo2", "bar2");
  bidding_partitions->emplace_back(/*partition_id=*/2, &kInterestGroupNames3,
                                   &kKeys3, &kHostname3, &additional_params3);

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             },
             {
               "compressionGroupId": 0,
               "id": 1,
               "metadata": { "hostname": "host2.test", "foo": "bar" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group2" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key2" ]
                 }
               ]
             },
             {
               "compressionGroupId": 0,
               "id": 2,
               "metadata": { "hostname": "host3.test", "foo2": "bar2"  },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1", "group2", "group3" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1", "key2", "key3" ]
                 }
               ]
             }
           ]
         })";

  ValidateDefaultFetchResult(
      RequestBiddingSignalsAndWaitForResult(bidding_signals_request));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

// Test that a fetch fails when there are two compression groups with the same
// ID in the response.
TEST_F(TrustedSignalsFetcherTest, BiddingSignalsDuplicateCompressionGroups) {
  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content"
             },
             {
               "compressionGroupId": 0,
               "content": "content"
             }
           ]
         })"));

  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();
  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StringPrintf("Failed to load %s: Response contains two "
                               "compression groups with id 0.",
                               TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyHex(kBasicBiddingSignalsRequestBody);
}

TEST_F(TrustedSignalsFetcherTest, BiddingSignalsMultipleCompressionGroups) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  const std::set<std::string> kInterestGroupNames2{"group2"};
  const std::set<std::string> kKeys2{"key2"};
  const std::string kHostname2{"host2.test"};
  base::Value::Dict additional_params2;
  additional_params2.Set("foo", "bar");
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions2;
  bidding_partitions2.emplace_back(/*partition_id=*/0, &kInterestGroupNames2,
                                   &kKeys2, &kHostname2, &additional_params2);
  bidding_signals_request.emplace(1, std::move(bidding_partitions2));

  const std::set<std::string> kInterestGroupNames3{"group1", "group2",
                                                   "group3"};
  const std::set<std::string> kKeys3{"key1", "key2", "key3"};
  const std::string kHostname3{"host3.test"};
  base::Value::Dict additional_params3;
  additional_params3.Set("foo2", "bar2");
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions3;
  bidding_partitions3.emplace_back(/*partition_id=*/0, &kInterestGroupNames3,
                                   &kKeys3, &kHostname3, &additional_params3);
  bidding_signals_request.emplace(2, std::move(bidding_partitions3));

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             },
             {
               "compressionGroupId": 1,
               "id": 0,
               "metadata": { "hostname": "host2.test", "foo": "bar" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group2" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key2" ]
                 }
               ]
             },
             {
               "compressionGroupId": 2,
               "id": 0,
               "metadata": { "hostname": "host3.test", "foo2": "bar2" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1", "group2", "group3" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1", "key2", "key3" ]
                 }
               ]
             }
           ]
         })";

  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content1",
               "ttlMs": 10
             },
             {
               "compressionGroupId": 1,
               "content": "content2"
             },
             {
               "compressionGroupId": 2,
               "content": "content3",
               "ttlMs": 150
             }
           ]
         })"));

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  TrustedSignalsFetcher::CompressionGroupResultMap expected_result;
  expected_result.try_emplace(
      0, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content1", base::Milliseconds(10)));
  expected_result.try_emplace(
      1, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content2", base::Milliseconds(0)));
  expected_result.try_emplace(
      2, CreateCompressionGroupResult(
             auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
             "content3", base::Milliseconds(150)));
  ValidateFetchResult(result, expected_result);
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

// Test that the entire fetch fails when one of the requested partitions has an
// error.
TEST_F(TrustedSignalsFetcherTest,
       BiddingSignalsMultipleCompressionGroupsFailsWhenOneBad) {
  auto bidding_signals_request = CreateBasicBiddingSignalsRequest();

  const std::set<std::string> kInterestGroupNames2{"group2"};
  const std::set<std::string> kKeys2{"key2"};
  const std::string kHostname2{"host2.test"};
  base::Value::Dict additional_params2;
  additional_params2.Set("foo", "bar");
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions2;
  bidding_partitions2.emplace_back(/*partition_id=*/0, &kInterestGroupNames2,
                                   &kKeys2, &kHostname2, &additional_params2);
  bidding_signals_request.emplace(1, std::move(bidding_partitions2));

  const std::set<std::string> kInterestGroupNames3{"group1", "group2",
                                                   "group3"};
  const std::set<std::string> kKeys3{"key1", "key2", "key3"};
  const std::string kHostname3{"host3.test"};
  base::Value::Dict additional_params3;
  additional_params3.Set("foo2", "bar2");
  std::vector<TrustedSignalsFetcher::BiddingPartition> bidding_partitions3;
  bidding_partitions3.emplace_back(/*partition_id=*/0, &kInterestGroupNames3,
                                   &kKeys3, &kHostname3, &additional_params3);
  bidding_signals_request.emplace(2, std::move(bidding_partitions3));

  // Request body as a JSON string. Will be converted to CBOR and have a framing
  // header and padding added before beign compared to actual body.
  const std::string_view kExpectedRequestBodyJson =
      R"({
           "acceptCompression": [ "none", "gzip" ],
           "partitions": [
             {
               "compressionGroupId": 0,
               "id": 0,
               "metadata": { "hostname": "host.test" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1" ]
                 }
               ]
             },
             {
               "compressionGroupId": 1,
               "id": 0,
               "metadata": { "hostname": "host2.test", "foo": "bar" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group2" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key2" ]
                 }
               ]
             },
             {
               "compressionGroupId": 2,
               "id": 0,
               "metadata": { "hostname": "host3.test", "foo2": "bar2" },
               "arguments": [
                 {
                   "tags": [ "interestGroupNames" ],
                   "data": [ "group1", "group2", "group3" ]
                 },
                 {
                   "tags": [ "keys" ],
                   "data": [ "key1", "key2", "key3" ]
                 }
               ]
             }
           ]
         })";

  SetResponseBodyAndAddHeader(auction_worklet::test::ToKVv2ResponseCborString(
      R"({
           "compressionGroups": [
             {
               "compressionGroupId": 0,
               "content": "content1",
               "ttlMs": 10
             },
             {
               "compressionGroupId": 1
             },
             {
               "compressionGroupId": 2,
               "content": "content3",
               "ttlMs": 150
             }
           ]
         })"));

  auto result = RequestBiddingSignalsAndWaitForResult(bidding_signals_request);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StringPrintf("Failed to load %s: Compression group 1 missing "
                               "binary string \"content\".",
                               TrustedBiddingSignalsUrl().spec().c_str()));
  ValidateRequestBodyJson(kExpectedRequestBodyJson);
}

}  // namespace
}  // namespace content

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_kvv2_signals.h"

#include <cstddef>

#include "base/containers/span_writer.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {
namespace {

const char kPublisherHostName[] = "publisher.test";
const int kExperimentGroupId = 12345;
const char kTrustedBiddingSignalsSlotSizeParam[] = "slotSize=100,200";
const char kJoiningOrigin[] = "https://foo.test/";
const size_t kFramingHeaderSize = 5;  // bytes
const size_t kOhttpHeaderSize = 55;   // bytes
const char kTrustedSignalsPath[] = "/trusted-signals";
const char kTrustedSignalsHost[] = "a.test";
const uint8_t kKeyId = 0xFF;

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

// Hex string for bidding signals base response, converted from the following
// CBOR data:
// [
//   {
//     "id": 0,
//     "keyGroupOutputs": [
//       {
//         "tags": [
//           "interestGroupNames"
//         ],
//         "keyValues": {
//           "name1": {
//             "value":
//             "{\"priorityVector\":{\"signal1\":1},
//               \"updateIfOlderThanMs\":3600000}"
//           }
//         }
//       },
//       {
//         "tags": [
//           "keys"
//         ],
//         "keyValues": {
//           "key1": {
//             "value": "\"value1\""
//           }
//         }
//       }
//     ]
//   }
// ]
const char kBiddingContentBase[] =
    "81A2626964006F6B657947726F75704F75747075747382A264746167738172696E74657265"
    "737447726F75704E616D6573696B657956616C756573A1656E616D6531A16576616C756578"
    "3F7B227072696F72697479566563746F72223A7B227369676E616C31223A317D2C22757064"
    "61746549664F6C6465725468616E4D73223A20333630303030307DA2647461677381646B65"
    "7973696B657956616C756573A1646B657931A16576616C7565682276616C75653122";

// Hex string for scoring signals base response, converted from the following
// CBOR data:
// [
//   {
//     "id": 0,
//     "keyGroupOutputs": [
//       {
//         "tags": [
//           "renderUrls"
//         ],
//         "keyValues": {
//           "https://foo.test/": {
//             "value": "1"
//           }
//         }
//       },
//       {
//         "tags": [
//           "adComponentRenderUrls"
//         ],
//         "keyValues": {
//           "https://foosub.test/": {
//             "value": "[2]"
//           }
//         }
//       }
//     ]
//   }
// ]
const char kScoringContentBase[] =
    "81A2626964006F6B657947726F75704F75747075747382A26474616773816A72656E646572"
    "55726C73696B657956616C756573A17168747470733A2F2F666F6F2E746573742FA1657661"
    "6C75656131A2647461677381756164436F6D706F6E656E7452656E64657255726C73696B65"
    "7956616C756573A17468747470733A2F2F666F6F7375622E746573742FA16576616C756563"
    "5B325D";

std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
CreateBiddingRequestHelperBuilder() {
  // Create a public key.
  mojom::TrustedSignalsPublicKeyPtr public_key =
      mojom::TrustedSignalsPublicKey::New(
          std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                      sizeof(kTestPublicKey)),
          kKeyId);

  return std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
      kPublisherHostName, kExperimentGroupId, std::move(public_key),
      kTrustedBiddingSignalsSlotSizeParam);
}

std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
CreateScoringRequestHelperBuilder() {
  // Create a public key.
  mojom::TrustedSignalsPublicKeyPtr public_key =
      mojom::TrustedSignalsPublicKey::New(
          std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                      sizeof(kTestPublicKey)),
          kKeyId);

  return std::make_unique<TrustedScoringSignalsKVv2RequestHelperBuilder>(
      kPublisherHostName, kExperimentGroupId, std::move(public_key));
}

class TrustedKVv2SignalsEmbeddedTest : public testing::Test {
 public:
  TrustedKVv2SignalsEmbeddedTest() {
    SetResponseStatusCode(net::HttpStatusCode::HTTP_OK);

    feature_list_.InitWithFeatures(
        {features::kInterestGroupUpdateIfOlderThan,
         blink::features::kFledgeTrustedSignalsKVv2Support},
        {});
    embedded_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &TrustedKVv2SignalsEmbeddedTest::HandleSignalsRequest,
        base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server_.Start());
  }

  ~TrustedKVv2SignalsEmbeddedTest() override {
    base::AutoLock auto_lock(lock_);
    // Wait until idle to ensure all requests have been observed within the
    // `auction_network_events_handler_`.
    task_environment_.RunUntilIdle();
  }

  GURL TrustedSignalsUrl() const {
    return embedded_test_server_.GetURL(kTrustedSignalsHost,
                                        kTrustedSignalsPath);
  }

  // Fetch bidding signals and wait for completion. Return nullptr on
  // failure.
  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
  FetchBiddingSignals(
      std::set<std::string> interest_group_names,
      std::set<std::string> trusted_bidding_signals_keys,
      std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
          request_helper_builder) {
    CHECK(!load_signals_run_loop_);
    DCHECK(!load_kvv2_signals_result_);

    auto bidding_signals = TrustedKVv2Signals::LoadKVv2BiddingSignals(
        url_loader_factory_.get(),
        auction_network_events_handler_.CreateRemote(),
        std::move(interest_group_names),
        std::move(trusted_bidding_signals_keys), TrustedSignalsUrl(),
        std::move(request_helper_builder), v8_helper_,
        base::BindOnce(&TrustedKVv2SignalsEmbeddedTest::LoadKVv2SignalsCallback,
                       base::Unretained(this)));
    WaitForLoadComplete();
    return std::move(load_kvv2_signals_result_);
  }

  // Fetch scoring signals and wait for completion. Return nullptr on
  // failure.
  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
  FetchScoringSignals(
      std::set<std::string> render_urls,
      std::set<std::string> ad_component_render_urls,
      std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
          request_helper_builder) {
    CHECK(!load_signals_run_loop_);
    DCHECK(!load_kvv2_signals_result_);

    auto scoring_signals = TrustedKVv2Signals::LoadKVv2ScoringSignals(
        url_loader_factory_.get(),
        auction_network_events_handler_.CreateRemote(), std::move(render_urls),
        std::move(ad_component_render_urls), TrustedSignalsUrl(),
        std::move(request_helper_builder), v8_helper_,
        base::BindOnce(&TrustedKVv2SignalsEmbeddedTest::LoadKVv2SignalsCallback,
                       base::Unretained(this)));
    WaitForLoadComplete();
    return std::move(load_kvv2_signals_result_);
  }

  // Wait for LoadKVv2SignalsCallback to be invoked.
  void WaitForLoadComplete() {
    // Since LoadKVv2SignalsCallback is always invoked asynchronously, fine to
    // create the RunLoop after creating the TrustedKVv2Signals object, which
    // will ultimately trigger the invocation.
    load_signals_run_loop_ = std::make_unique<base::RunLoop>();
    load_signals_run_loop_->Run();
    load_signals_run_loop_.reset();
  }

  // Return the result of calling TrustedSignals::Result::GetBiddingSignals()
  // with `index` and `trusted_bidding_signals_keys`. Return value as a JSON
  // std::string, for easy testing.
  std::string ExtractBiddingSignals(
      TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap& result_map,
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex& index,
      std::vector<std::string> trusted_bidding_signals_keys) {
    EXPECT_TRUE(result_map.contains(index));
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          v8::Local<v8::Value> value = result_map.at(index)->GetBiddingSignals(
              v8_helper_.get(), context, trusted_bidding_signals_keys);

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Returns the results of calling TrustedSignals::Result::GetScoringSignals()
  // with `index` and `trusted_scoring_signals_keys`. Returns value as a JSON
  // std::string, for easy testing.
  std::string ExtractScoringSignals(
      TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap& result_map,
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex& index,
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls) {
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          v8::Local<v8::Value> value = result_map.at(index)->GetScoringSignals(
              v8_helper_.get(), context, render_url, ad_component_render_urls);

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Set the response status code.
  void SetResponseStatusCode(net::HttpStatusCode code) {
    base::AutoLock auto_lock(lock_);
    response_status_code_ = code;
  }

  // Set the content hex.
  void SetContentHex(std::string hex) {
    base::AutoLock auto_lock(lock_);
    content_hex_ = std::move(hex);
  }

  // Set the response body hex.
  void SetResponseBodyHex(std::string hex) {
    base::AutoLock auto_lock(lock_);
    response_body_hex_ = std::move(hex);
  }

 protected:
  static std::string BuildResponseBody(const std::string& hex_string) {
    std::vector<uint8_t> hex_bytes;
    base::HexStringToBytes(hex_string, &hex_bytes);

    std::string response_body;
    size_t size_before_padding =
        kFramingHeaderSize + kOhttpHeaderSize + hex_bytes.size();
    size_t desired_size = absl::bit_ceil(size_before_padding);
    size_t response_body_size = desired_size - kOhttpHeaderSize;
    response_body.resize(response_body_size, 0x00);

    base::SpanWriter writer(
        base::as_writable_bytes(base::make_span(response_body)));
    writer.WriteU8BigEndian(0x00);
    writer.WriteU32BigEndian(hex_bytes.size());
    writer.Write(base::as_bytes(base::make_span(hex_bytes)));

    return response_body;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSignalsRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    if (!request.headers.contains(net::HttpRequestHeaders::kContentType) ||
        request.headers.at(net::HttpRequestHeaders::kContentType) !=
            kTrustedSignalsKVv2EncryptionRequestMediaType) {
      return nullptr;
    }

    std::vector<uint8_t> compression_group_bytes;
    base::HexStringToBytes(content_hex_, &compression_group_bytes);

    cbor::Value::MapValue compression_group;
    compression_group.try_emplace(cbor::Value("compressionGroupId"),
                                  cbor::Value(0));
    compression_group.try_emplace(cbor::Value("ttlMs"), cbor::Value(100));
    compression_group.try_emplace(cbor::Value("content"),
                                  cbor::Value(compression_group_bytes));

    cbor::Value::ArrayValue compression_groups;
    compression_groups.emplace_back(std::move(compression_group));

    cbor::Value::MapValue body_map;
    body_map.try_emplace(cbor::Value("compressionGroups"),
                         cbor::Value(std::move(compression_groups)));

    cbor::Value body_value(std::move(body_map));
    std::optional<std::vector<uint8_t>> maybe_body_bytes =
        cbor::Writer::Write(body_value);

    std::string response_body = response_body_hex_.empty()
                                    ? BuildResponseBody(base::HexEncode(
                                          std::move(maybe_body_bytes).value()))
                                    : BuildResponseBody(response_body_hex_);

    auto response_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
        kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
        EVP_HPKE_AES_256_GCM);
    CHECK(response_key_config.ok()) << response_key_config.status();

    auto ohttp_gateway =
        quiche::ObliviousHttpGateway::Create(
            std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                        sizeof(kTestPrivateKey)),
            response_key_config.value())
            .value();
    auto received_request = ohttp_gateway.DecryptObliviousHttpRequest(
        request.content, kTrustedSignalsKVv2EncryptionRequestMediaType);
    CHECK(received_request.ok()) << received_request.status();

    auto response_context =
        std::move(received_request).value().ReleaseContext();

    // Encrypt the response body.
    auto maybe_response = ohttp_gateway.CreateObliviousHttpResponse(
        response_body, response_context,
        kTrustedSignalsKVv2EncryptionResponseMediaType);
    CHECK(maybe_response.ok()) << maybe_response.status();

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type(kAdAuctionTrustedSignalsMimeType);
    response->set_content(maybe_response->EncapsulateAndSerialize());
    response->set_code(response_status_code_);
    response->AddCustomHeader("Ad-Auction-Allowed", "true");

    return response;
  }

  void LoadKVv2SignalsCallback(
      std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
          result_map,
      std::optional<std::string> error_msg) {
    DCHECK(!load_kvv2_signals_result_);
    load_kvv2_signals_result_ = std::move(result_map);
    error_msg_ = std::move(error_msg);
    if (!expect_nonfatal_error_) {
      EXPECT_EQ(!load_kvv2_signals_result_.has_value(), error_msg_.has_value());
    } else {
      EXPECT_TRUE(load_kvv2_signals_result_);
      EXPECT_TRUE(error_msg_);
    }
    load_signals_run_loop_->Quit();
  }

  // Need to use an IO thread for the TestSharedURLLoaderFactory, which lives on
  // the thread it's created on, to make network requests.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  // Reusable run loop for loading the signals. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_signals_run_loop_;
  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
      load_kvv2_signals_result_;
  std::optional<std::string> error_msg_;

  // If false, only one of `result_map` or `error_msg` is expected to be
  // received in LoadSignalsCallback().
  bool expect_nonfatal_error_ = false;

  TestAuctionNetworkEventsHandler auction_network_events_handler_;
  scoped_refptr<AuctionV8Helper> v8_helper_{
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())};
  base::test::ScopedFeatureList feature_list_;

  base::Lock lock_;
  net::HttpStatusCode response_status_code_ GUARDED_BY(lock_);
  std::string content_hex_ GUARDED_BY(lock_);
  std::string response_body_hex_ GUARDED_BY(lock_);

  net::test_server::EmbeddedTestServer embedded_test_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  // URLLoaderFactory that makes real network requests.
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_{
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true)};
};

TEST_F(TrustedKVv2SignalsEmbeddedTest, BiddingSignalsBaseResponse) {
  SetContentHex(kBiddingContentBase);
  auto request_helper_builder = CreateBiddingRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      std::string("name1"), std::set<std::string>{"key1"},
      url::Origin::Create(GURL(kJoiningOrigin)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
      result_map = FetchBiddingSignals({"name1"}, {"key1"},
                                       std::move(request_helper_builder));
  ASSERT_TRUE(result_map);
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  EXPECT_EQ(R"({"key1":"value1"})",
            ExtractBiddingSignals(result_map.value(), index, {"key1"}));
  const TrustedSignals::Result::PerGroupData* name1_per_group_data =
      result_map->at(index)->GetPerGroupData("name1");
  ASSERT_NE(name1_per_group_data, nullptr);
  auto priority_vector = name1_per_group_data->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"signal1", 1}}),
            *priority_vector);
  EXPECT_EQ(base::Milliseconds(3600000),
            name1_per_group_data->update_if_older_than);
}

TEST_F(TrustedKVv2SignalsEmbeddedTest,
       BiddingSignalsExpectedEntriesNotPresent) {
  SetContentHex(kBiddingContentBase);
  auto request_helper_builder = CreateBiddingRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      std::string("name2"), std::set<std::string>{"key2"},
      url::Origin::Create(GURL(kJoiningOrigin)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
      result_map = FetchBiddingSignals({"name2"}, {"key2"},
                                       std::move(request_helper_builder));
  ASSERT_TRUE(result_map);
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  EXPECT_EQ(R"({"key2":null})",
            ExtractBiddingSignals(result_map.value(), index, {"key2"}));
  EXPECT_EQ(nullptr, result_map->at(index)->GetPerGroupData("name2"));
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, BiddingSignalsNetworkError) {
  SetContentHex(kBiddingContentBase);
  SetResponseStatusCode(net::HttpStatusCode::HTTP_NOT_FOUND);
  auto request_helper_builder = CreateBiddingRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      std::string("name1"), std::set<std::string>{"key1"},
      url::Origin::Create(GURL(kJoiningOrigin)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  EXPECT_FALSE(FetchBiddingSignals({"name1"}, {"key1"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(),
            base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                               TrustedSignalsUrl().spec().c_str()));
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, FailedToParseBiddingResponseBody) {
  // Random 20 bytes hex string which cannot be parsed as CBOR.
  SetResponseBodyHex("666f421a72ed47aade0c63826288d5d1bbf2dc2a");
  auto request_helper_builder = CreateBiddingRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      std::string("name1"), std::set<std::string>{"key1"},
      url::Origin::Create(GURL(kJoiningOrigin)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  EXPECT_FALSE(FetchBiddingSignals({"name1"}, {"key1"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(), "Failed to parse response body as CBOR.");
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, FailedToParseBiddingSignalsFetchResult) {
  // Random 20 bytes hex string which cannot be parsed as CBOR.
  SetContentHex("666f421a72ed47aade0c63826288d5d1bbf2dc2a");
  auto request_helper_builder = CreateBiddingRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      std::string("name1"), std::set<std::string>{"key1"},
      url::Origin::Create(GURL(kJoiningOrigin)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  EXPECT_FALSE(FetchBiddingSignals({"name1"}, {"key1"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(), "Failed to parse content as CBOR.");
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, ScoringSignalsBaseResponse) {
  SetContentHex(kScoringContentBase);
  auto request_helper_builder = CreateScoringRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      GURL("https://foo.test/"), std::set<std::string>{"https://foosub.test/"},
      url::Origin::Create(GURL("https://owner.test/")),
      url::Origin::Create(GURL(kJoiningOrigin)));

  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
      result_map =
          FetchScoringSignals({"https://foo.test/"}, {"https://foosub.test/"},
                              std::move(request_helper_builder));

  ASSERT_TRUE(result_map);
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  EXPECT_EQ(
      R"({"renderURL":{"https://foo.test/":1},)"
      R"("renderUrl":{"https://foo.test/":1},)"
      R"("adComponentRenderURLs":{"https://foosub.test/":[2]},)"
      R"("adComponentRenderUrls":{"https://foosub.test/":[2]}})",
      ExtractScoringSignals(
          result_map.value(), index, /*render_url=*/GURL("https://foo.test/"),
          /*ad_component_render_urls=*/{"https://foosub.test/"}));
}

TEST_F(TrustedKVv2SignalsEmbeddedTest,
       ScoringSignalsExpectedEntriesNotPresent) {
  SetContentHex(kScoringContentBase);
  auto request_helper_builder = CreateScoringRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      GURL("https://bar.test/"), std::set<std::string>{"https://barsub.test/"},
      url::Origin::Create(GURL("https://owner.test/")),
      url::Origin::Create(GURL(kJoiningOrigin)));

  std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
      result_map =
          FetchScoringSignals({"https://bar.test/"}, {"https://barsub.test/"},
                              std::move(request_helper_builder));

  ASSERT_TRUE(result_map);
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  EXPECT_EQ(
      R"({"renderURL":{"https://bar.test/":null},)"
      R"("renderUrl":{"https://bar.test/":null},)"
      R"("adComponentRenderURLs":{"https://barsub.test/":null},)"
      R"("adComponentRenderUrls":{"https://barsub.test/":null}})",
      ExtractScoringSignals(
          result_map.value(), index, /*render_url=*/GURL("https://bar.test/"),
          /*ad_component_render_urls=*/{"https://barsub.test/"}));
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, ScoringSignalsNetworkError) {
  SetContentHex(kScoringContentBase);
  SetResponseStatusCode(net::HttpStatusCode::HTTP_NOT_FOUND);
  auto request_helper_builder = CreateScoringRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      GURL("https://foo.test/"), std::set<std::string>{"https://foosub.test/"},
      url::Origin::Create(GURL("https://owner.test/")),
      url::Origin::Create(GURL(kJoiningOrigin)));

  EXPECT_FALSE(FetchScoringSignals({"https://foo.test/"},
                                   {"https://foosub.test/"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(),
            base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                               TrustedSignalsUrl().spec().c_str()));
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, FailedToParseScoringResponseBody) {
  // Random 20 bytes hex string which cannot be parsed as CBOR.
  SetResponseBodyHex("666f421a72ed47aade0c63826288d5d1bbf2dc2a");
  auto request_helper_builder = CreateScoringRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      GURL("https://foo.test/"), std::set<std::string>{"https://foosub.test/"},
      url::Origin::Create(GURL("https://owner.test/")),
      url::Origin::Create(GURL(kJoiningOrigin)));

  EXPECT_FALSE(FetchScoringSignals({"https://foo.test/"},
                                   {"https://foosub.test/"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(), "Failed to parse response body as CBOR.");
}

TEST_F(TrustedKVv2SignalsEmbeddedTest, FailedToParseScoringSignalsFetchResult) {
  // Random 20 bytes hex string which cannot be parsed as CBOR.
  SetContentHex("666f421a72ed47aade0c63826288d5d1bbf2dc2a");
  auto request_helper_builder = CreateScoringRequestHelperBuilder();
  request_helper_builder->AddTrustedSignalsRequest(
      GURL("https://foo.test/"), std::set<std::string>{"https://foosub.test/"},
      url::Origin::Create(GURL("https://owner.test/")),
      url::Origin::Create(GURL(kJoiningOrigin)));

  EXPECT_FALSE(FetchScoringSignals({"https://foo.test/"},
                                   {"https://foosub.test/"},
                                   std::move(request_helper_builder)));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(error_msg_.value(), "Failed to parse content as CBOR.");
}

class TrustedKVv2SignalsTest : public testing::Test {
 public:
  TrustedKVv2SignalsTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kFledgeTrustedSignalsKVv2Support);
  }

  ~TrustedKVv2SignalsTest() override { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAuctionNetworkEventsHandler auction_network_events_handler_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_{
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())};
  base::test::ScopedFeatureList feature_list_;
};

// Test case where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before it gets to finish.
TEST_F(TrustedKVv2SignalsTest, BiddingSignalsDeleteBeforeCallback) {
  GURL url = GURL("https://url.test/");
  std::string headers =
      base::StringPrintf("%s\nContent-Type: %s", kAllowFledgeHeader,
                         "message/ad-auction-trusted-signals-request");
  // Parsing process is occurring on the V8 thread, so a non-CBOR response body
  // will not cause any issue.
  AddResponse(&url_loader_factory_, url, kAdAuctionTrustedSignalsMimeType,
              /*charset=*/std::nullopt, "Fake response body", headers);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      request_helper_builder = CreateBiddingRequestHelperBuilder();
  auto bidding_signals = TrustedKVv2Signals::LoadKVv2BiddingSignals(
      &url_loader_factory_, auction_network_events_handler_.CreateRemote(),
      {"name1"}, {"key1"}, url, std::move(request_helper_builder), v8_helper_,
      base::BindOnce([](std::optional<TrustedSignalsKVv2ResponseParser::
                                          TrustedSignalsResultMap> result,
                        std::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  bidding_signals.reset();
  event_handle->Signal();
}

TEST_F(TrustedKVv2SignalsTest, ScoringSignalsDeleteBeforeCallback) {
  GURL url = GURL("https://url.test/");
  std::string headers =
      base::StringPrintf("%s\nContent-Type: %s", kAllowFledgeHeader,
                         "message/ad-auction-trusted-signals-request");
  // Parsing process is occurring on the V8 thread, so a non-CBOR response body
  // will not cause any issue.
  AddResponse(&url_loader_factory_, url, kAdAuctionTrustedSignalsMimeType,
              /*charset=*/std::nullopt, "Fake response body", headers);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
      request_helper_builder = CreateScoringRequestHelperBuilder();
  auto bidding_signals = TrustedKVv2Signals::LoadKVv2ScoringSignals(
      &url_loader_factory_, auction_network_events_handler_.CreateRemote(),
      {"https://foo.test/"}, {"https://foosub.test/"}, url,
      std::move(request_helper_builder), v8_helper_,
      base::BindOnce([](std::optional<TrustedSignalsKVv2ResponseParser::
                                          TrustedSignalsResultMap> result,
                        std::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  bidding_signals.reset();
  event_handle->Signal();
}

}  // namespace
}  // namespace auction_worklet

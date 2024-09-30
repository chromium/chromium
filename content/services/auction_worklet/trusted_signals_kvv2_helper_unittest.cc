// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"

#include <array>

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8-context.h"

namespace auction_worklet {

namespace {

const char kHostName[] = "publisher.test";
const int kExperimentGroupId = 12345;
const char kTrustedBiddingSignalsSlotSizeParam[] = "slotSize=100,200";
const size_t kFramingHeaderSize = 5;  // bytes
const size_t kOhttpHeaderSize = 55;   // bytes
const char kOriginFooUrl[] = "https://foo.test/";
const char kOriginFoosubUrl[] = "https://foosub.test/";
const char kOriginBarUrl[] = "https://bar.test/";
const char kOriginBarsubUrl[] = "https://barsub.test/";
const char kOwnerOriginA[] = "https://owner-a.test/";
const char kOwnerOriginB[] = "https://owner-b.test/";
const char kJoiningOriginA[] = "https://joining-a.test/";
const char kJoiningOriginB[] = "https://joining-b.test/";

const uint8_t kKeyId = 0xff;

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

// Return a public key pointer which is created by kTestPublicKey and kKeyId.
mojom::TrustedSignalsPublicKeyPtr CreatePublicKey() {
  return mojom::TrustedSignalsPublicKey::New(
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey)),
      kKeyId);
}

// Helper to decrypt request body.
std::vector<uint8_t> DecryptRequestBody(const std::string& request_body,
                                        int public_key_id) {
  // Decrypt request body.
  auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      public_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  CHECK(config.ok()) << config.status();

  auto ohttp_gateway =
      quiche::ObliviousHttpGateway::Create(
          std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                      sizeof(kTestPrivateKey)),
          config.value())
          .value();

  auto decrypt_body = ohttp_gateway.DecryptObliviousHttpRequest(
      request_body, kTrustedSignalsKVv2EncryptionRequestMediaType);
  CHECK(decrypt_body.ok()) << decrypt_body.status();

  auto plain_body = decrypt_body->GetPlaintextData();
  std::vector<uint8_t> body_bytes(plain_body.begin(), plain_body.end());

  return body_bytes;
}

// GzipCompress() doesn't support writing to a vector, only a std::string. This
// wrapper provides that capability, at the cost of an extra copy.
std::vector<std::uint8_t> GzipCompressHelper(
    base::span<const std::uint8_t> in) {
  std::string compressed_string;
  EXPECT_TRUE(compression::GzipCompress(in, &compressed_string));
  return std::vector<std::uint8_t>(compressed_string.begin(),
                                   compressed_string.end());
}

void ExpectCompressionGroupMapEquals(
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap& map1,
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap& map2) {
  ASSERT_EQ(map1.size(), map2.size()) << "Maps have different sizes";

  for (const auto& [key, value] : map1) {
    auto it = map2.find(key);
    ASSERT_NE(it, map2.end())
        << "Missing key in compression group map2: " << key;

    // Compare each field in CompressionGroup.
    EXPECT_EQ(value.compression_scheme, it->second.compression_scheme);
    EXPECT_EQ(value.content, it->second.content);
    EXPECT_EQ(value.ttl, it->second.ttl);
  }
}

// Returns the results of calling TrustedSignals::Result::GetBiddingSignals()
// with `trusted_bidding_signals_keys`. Returns value as a JSON std::string,
// for easy testing.
std::string ExtractBiddingSignals(
    AuctionV8Helper* v8_helper,
    TrustedSignals::Result* signals,
    std::vector<std::string> trusted_bidding_signals_keys) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper);
  v8::Isolate* isolate = v8_helper->isolate();
  // Could use the scratch context, but using a separate one more
  // closely resembles actual use.
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> value = signals->GetBiddingSignals(
      v8_helper, context, trusted_bidding_signals_keys);

  std::string result;
  if (v8_helper->ExtractJson(context, value,
                             /*script_timeout=*/nullptr,
                             &result) != AuctionV8Helper::Result::kSuccess) {
    result = "JSON extraction failed.";
  }
  return result;
}

// Check trusted bidding signals' priority vector and bidding signals in json
// format with given interest group names and bidding keys.
void CheckBiddingResult(
    AuctionV8Helper* v8_helper,
    TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap& result_map,
    TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex& index,
    const std::vector<std::string>& interest_group_names,
    const std::vector<std::string>& keys,
    const std::map<std::string, TrustedSignals::Result::PriorityVector>&
        priority_vector_map,
    const std::string& bidding_signals,
    std::optional<uint32_t> data_version) {
  ASSERT_TRUE(result_map.contains(index));
  TrustedSignals::Result* result = result_map.at(index).get();

  for (const auto& name : interest_group_names) {
    std::optional<TrustedSignals::Result::PriorityVector>
        maybe_priority_vector = result->GetPerGroupData(name)->priority_vector;
    ASSERT_TRUE(maybe_priority_vector);
    EXPECT_EQ(priority_vector_map.at(name), *maybe_priority_vector);
  }

  std::string bidding_signals_json =
      ExtractBiddingSignals(v8_helper, result, keys);
  EXPECT_EQ(bidding_signals, bidding_signals_json);
  EXPECT_EQ(data_version, result->GetDataVersion());
}

// Check trusted scoring signals' render urls and ad component signals in json
// format with given render url and ad component render urls.
void CheckScoringResult(
    AuctionV8Helper* v8_helper,
    TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap& result_map,
    TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex& index,
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls,
    const std::string& expected_signals,
    std::optional<uint32_t> data_version) {
  ASSERT_TRUE(result_map.contains(index));
  TrustedSignals::Result* result = result_map.at(index).get();

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper);
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> value = result->GetScoringSignals(
      v8_helper, context, render_url, ad_component_render_urls);
  std::string signals_json;

  if (v8_helper->ExtractJson(context, value, /*script_timeout=*/nullptr,
                             &signals_json) !=
      AuctionV8Helper::Result::kSuccess) {
    signals_json = "JSON extraction failed.";
  }

  EXPECT_EQ(expected_signals, signals_json);
  EXPECT_EQ(data_version, result->GetDataVersion());
}

// Build a response body in string format with a hex string, a given compression
// scheme format byte, and the length of the hex string after it is converted to
// bytes.
std::string BuildResponseBody(const std::string& hex_string,
                              uint8_t compress_scheme = 0x00) {
  std::vector<uint8_t> hex_bytes;
  base::HexStringToBytes(hex_string, &hex_bytes);

  std::string response_body;
  size_t size_before_padding =
      kOhttpHeaderSize + kFramingHeaderSize + hex_bytes.size();
  size_t desired_size = absl::bit_ceil(size_before_padding);
  size_t response_body_size = desired_size - kOhttpHeaderSize;
  response_body.resize(response_body_size, 0x00);

  base::SpanWriter writer(
      base::as_writable_bytes(base::make_span(response_body)));
  writer.WriteU8BigEndian(compress_scheme);
  writer.WriteU32BigEndian(hex_bytes.size());
  writer.Write(base::as_bytes(base::make_span(hex_bytes)));

  return response_body;
}

// Encrypt the response body string by creating a fake encrypted request using a
// public key and saving the encryption context. Return a pair consisting of the
// encrypted response body string and the encryption context. The context will
// be passed to `ParseResponseToSignalsFetchResult` and used in
// `CreateClientObliviousResponse()` for response decryption.
std::pair<std::string, quiche::ObliviousHttpRequest::Context>
EncryptResponseBodyHelper(const std::string& response_body) {
  // Fake a encrypted request.
  std::string public_key =
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey));
  auto request_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  EXPECT_TRUE(request_key_config.ok()) << request_key_config.status();

  auto fake_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          "Fake request.", public_key, request_key_config.value(),
          kTrustedSignalsKVv2EncryptionRequestMediaType);
  std::string fake_request_body = fake_request->EncapsulateAndSerialize();
  auto request_context = std::move(fake_request).value().ReleaseContext();
  EXPECT_TRUE(fake_request.ok()) << fake_request.status();

  // Decrypt the request and get the context.
  auto response_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  EXPECT_TRUE(response_key_config.ok()) << response_key_config.status();

  auto ohttp_gateway =
      quiche::ObliviousHttpGateway::Create(
          std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                      sizeof(kTestPrivateKey)),
          response_key_config.value())
          .value();
  auto received_request = ohttp_gateway.DecryptObliviousHttpRequest(
      fake_request_body, kTrustedSignalsKVv2EncryptionRequestMediaType);
  EXPECT_TRUE(received_request.ok()) << received_request.status();

  auto response_context = std::move(received_request).value().ReleaseContext();

  // Encrypt the response body.
  auto maybe_response = ohttp_gateway.CreateObliviousHttpResponse(
      response_body, response_context,
      kTrustedSignalsKVv2EncryptionResponseMediaType);
  EXPECT_TRUE(maybe_response.ok()) << maybe_response.status();

  return std::pair(maybe_response->EncapsulateAndSerialize(),
                   std::move(request_context));
}

std::string GetErrorMessageFromParseResponseToSignalsFetchResult(
    std::string& hex,
    uint8_t compress_scheme = 0x00) {
  std::string response_body = BuildResponseBody(hex, compress_scheme);
  auto helper_result = EncryptResponseBodyHelper(response_body);

  base::expected<std::map<int, CompressionGroupResult>,
                 TrustedSignalsKVv2ResponseParser::ErrorInfo>
      result =
          TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
              helper_result.first, helper_result.second);
  EXPECT_FALSE(result.has_value());

  return std::move(result.error().error_msg);
}

std::string GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const std::set<std::string>& interest_group_names,
    const std::set<std::string>& keys,
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap&
        compression_group_result_map) {
  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError result =
      TrustedSignalsKVv2ResponseParser::
          ParseBiddingSignalsFetchResultToResultMap(
              v8_helper.get(), interest_group_names, keys,
              compression_group_result_map);
  EXPECT_FALSE(result.has_value());

  return std::move(result.error().error_msg);
}

std::string GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const std::set<std::string>& render_urls,
    const std::set<std::string>& ad_component_render_urls,
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap&
        compression_group_result_map) {
  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError result =
      TrustedSignalsKVv2ResponseParser::
          ParseScoringSignalsFetchResultToResultMap(
              v8_helper.get(), render_urls, ad_component_render_urls,
              compression_group_result_map);
  EXPECT_FALSE(result.has_value());

  return std::move(result.error().error_msg);
}

// Checks that a PartitionMapOrError is not an error and contains exactly the
// listed partitions.
MATCHER_P(PartitionsAre, expected_values, "") {
  if (!arg.has_value()) {
    *result_listener << "is unexpectedly an error: \"" << arg.error() << "\"";
    return false;
  }

  std::vector<int> keys;
  for (const auto& pair : *arg) {
    keys.push_back(pair.first);
  }

  return testing::ExplainMatchResult(
      testing::UnorderedElementsAreArray(expected_values), keys,
      result_listener);
}

// Checks that a PartitionMapOrError is an error with the specified value.
MATCHER_P(IsError, expected_error, "") {
  if (arg.has_value()) {
    *result_listener << "is unexpectedly not an error.";
    return false;
  }

  bool match = testing::ExplainMatchResult(testing::Eq(expected_error),
                                           arg.error(), result_listener);
  if (!match) {
    *result_listener << "Actual error: \"" << arg.error() << "\"";
  }
  return match;
}

}  // namespace

class TrustedSignalsKVv2RequestHelperTest : public testing::Test {
 public:
  explicit TrustedSignalsKVv2RequestHelperTest() {
    public_key_ = CreatePublicKey();
  }
  ~TrustedSignalsKVv2RequestHelperTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  mojom::TrustedSignalsPublicKeyPtr public_key_;
};

TEST_F(TrustedSignalsKVv2RequestHelperTest,
       TrustedBiddingSignalsRequestEncoding) {
  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, kExperimentGroupId, std::move(public_key_),
              kTrustedBiddingSignalsSlotSizeParam);

  helper_builder->AddTrustedSignalsRequest(
      std::string("groupA"), std::set<std::string>{"keyA", "keyAB"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupB"), std::set<std::string>{"keyB", "keyAB"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Another group in kOriginFooUrl, but with execution mode kCompatibilityMode,
  // for scenario of multiple partitions with different keys in one compression
  // group.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupAB"), std::set<std::string>{"key"},
      url::Origin::Create(GURL(kOriginFooUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupC"), std::set<std::string>{"keyC", "keyCD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{"keyD", "keyCD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Test interest group name is merged into one partition with same joining
  // origin and kGroupedByOriginMode.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  // Test bidding keys are merged into one partition with same joining origin
  // and kGroupedByOriginMode.
  helper_builder->AddTrustedSignalsRequest(
      std::string("groupD"), std::set<std::string>{"keyDD"},
      url::Origin::Create(GURL(kOriginBarUrl)),
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);

  std::unique_ptr<TrustedSignalsKVv2RequestHelper> helper =
      helper_builder->Build();

  std::string request_body = helper->TakePostRequestBody();
  std::vector<uint8_t> body_bytes = DecryptRequestBody(request_body, kKeyId);

  // Test if body_bytes size is padded.
  size_t request_length = kOhttpHeaderSize + body_bytes.size();
  // If a number is a power of 2, then the result of performing a bitwise AND
  // operation between the number and the number minus 1 should be 0.
  EXPECT_FALSE(request_length & (request_length - 1));

  // Use cbor.me to convert from
  // {
  //   "partitions": [
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupA",
  //             "groupB"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "keyA",
  //             "keyAB",
  //             "keyB"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 1,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupAB"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "key"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "slotSize": "100,200",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "groupC",
  //             "groupD"
  //           ],
  //           "tags": [
  //             "interestGroupNames"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "keyC",
  //             "keyCD",
  //             "keyD",
  //             "keyDD"
  //           ],
  //           "tags": [
  //             "keys"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 1
  //     }
  //   ],
  //   "acceptCompression": [
  //     "none",
  //     "gzip"
  //   ]
  // }
  const std::string kExpectedBodyHex =
      "A26A706172746974696F6E7383A462696400686D65746164617461A368686F73746E616D"
      "656E7075626C69736865722E7465737468736C6F7453697A65673130302C323030716578"
      "706572696D656E7447726F7570496465313233343569617267756D656E747382A2646461"
      "7461826667726F7570416667726F75704264746167738172696E74657265737447726F75"
      "704E616D6573A2646461746183646B657941656B65794142646B65794264746167738164"
      "6B65797372636F6D7072657373696F6E47726F7570496400A462696401686D6574616461"
      "7461A368686F73746E616D656E7075626C69736865722E7465737468736C6F7453697A65"
      "673130302C323030716578706572696D656E7447726F7570496465313233343569617267"
      "756D656E747382A26464617461816767726F7570414264746167738172696E7465726573"
      "7447726F75704E616D6573A2646461746181636B6579647461677381646B65797372636F"
      "6D7072657373696F6E47726F7570496400A462696400686D65746164617461A368686F73"
      "746E616D656E7075626C69736865722E7465737468736C6F7453697A65673130302C3230"
      "30716578706572696D656E7447726F7570496465313233343569617267756D656E747382"
      "A26464617461826667726F7570436667726F75704464746167738172696E746572657374"
      "47726F75704E616D6573A2646461746184646B657943656B65794344646B657944656B65"
      "794444647461677381646B65797372636F6D7072657373696F6E47726F75704964017161"
      "6363657074436F6D7072657373696F6E82646E6F6E6564677A6970";
  // Prefix hex for `kExpectedBodyHex` which includes the compression format
  // code and the length.
  const std::string kExpectedPrefixHex = "000000025B";
  // Padding zeros.
  const std::string kPaddingString =
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
      "00";

  EXPECT_EQ(base::HexEncode(body_bytes),
            kExpectedPrefixHex + kExpectedBodyHex + kPaddingString);
}

// TODO(crbug.com/337917489): When adding an identical trusted scoring signals
// request, it should use the existing partition instead of creating a new one.
// After the implementation, the EXPECT_EQ() of request I which is duplicated
// from request H, should be failed.
//
// Add the following trusted bidding signals requests:
// Request A[join_origin: foo.test, mode: group-by-origin]
// Request B[join_origin: foo.test, mode: group-by-origin]
// Request C[join_origin: foo.test, mode: compatibility]
// Request D[join_origin: foo.test, mode: compatibility]
// Request E[join_origin: bar.test, mode: compatibility]
// Request F[join_origin: bar.test, mode: group-by-origin]
// Request G[join_origin: bar.test, mode: compatibility]
// Request H[join_origin: bar.test, mode: compatibility]
// Request I[join_origin: bar.test, mode: compatibility]
// will result the following groups:
// Compression: 0 -
//    partition 0: A, B
//    partition 1: C
//    partition 2: D
// Compression: 1 -
//    partition 0: F
//    partition 1: E
//    partition 2: G
//    partition 3: H
//    partition 4: I
TEST_F(TrustedSignalsKVv2RequestHelperTest,
       TrustedBiddingSignalsIsolationIndex) {
  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, kExperimentGroupId, std::move(public_key_),
              kTrustedBiddingSignalsSlotSizeParam);

  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupA"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupB"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupC"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 2),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupD"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginFooUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 1),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupE"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 0),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupF"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 2),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupG"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 3),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupH"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
  EXPECT_EQ(
      TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 4),
      helper_builder->AddTrustedSignalsRequest(
          std::string("groupH"), std::set<std::string>{"key"},
          url::Origin::Create(GURL(kOriginBarUrl)),
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode));
}

TEST_F(TrustedSignalsKVv2RequestHelperTest,
       TrustedScoringSignalsRequestEncoding) {
  std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedScoringSignalsKVv2RequestHelperBuilder>(
              kHostName, kExperimentGroupId, std::move(public_key_));

  helper_builder->AddTrustedSignalsRequest(
      GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
      url::Origin::Create(GURL(kOwnerOriginA)),
      url::Origin::Create(GURL(kJoiningOriginA)));
  helper_builder->AddTrustedSignalsRequest(
      GURL(kOriginBarUrl), std::set<std::string>{kOriginBarsubUrl},
      url::Origin::Create(GURL(kOwnerOriginA)),
      url::Origin::Create(GURL(kJoiningOriginA)));
  helper_builder->AddTrustedSignalsRequest(
      GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
      url::Origin::Create(GURL(kOwnerOriginB)),
      url::Origin::Create(GURL(kJoiningOriginB)));

  std::unique_ptr<TrustedSignalsKVv2RequestHelper> helper =
      helper_builder->Build();

  std::string request_body = helper->TakePostRequestBody();
  std::vector<uint8_t> body_bytes = DecryptRequestBody(request_body, kKeyId);

  // Test if body_bytes size is padded.
  size_t request_length = kOhttpHeaderSize + body_bytes.size();
  // If a number is a power of 2, then the result of performing a bitwise AND
  // operation between the number and the number minus 1 should be 0.
  EXPECT_FALSE(request_length & (request_length - 1));

  // Use cbor.me to convert from
  // {
  //   "partitions": [
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "https://foo.test/"
  //           ],
  //           "tags": [
  //             "renderUrls"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "https://foosub.test/"
  //           ],
  //           "tags": [
  //             "adComponentRenderUrls"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 1,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "https://bar.test/"
  //           ],
  //           "tags": [
  //             "renderUrls"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "https://barsub.test/"
  //           ],
  //           "tags": [
  //             "adComponentRenderUrls"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "id": 0,
  //       "metadata": {
  //         "hostname": "publisher.test",
  //         "experimentGroupId": "12345"
  //       },
  //       "arguments": [
  //         {
  //           "data": [
  //             "https://foo.test/"
  //           ],
  //           "tags": [
  //             "renderUrls"
  //           ]
  //         },
  //         {
  //           "data": [
  //             "https://foosub.test/"
  //           ],
  //           "tags": [
  //             "adComponentRenderUrls"
  //           ]
  //         }
  //       ],
  //       "compressionGroupId": 1
  //     }
  //   ],
  //   "acceptCompression": [
  //     "none",
  //     "gzip"
  //   ]
  // }

  const std::string kExpectedBodyHex =
      "A26A706172746974696F6E7383A462696400686D65746164617461A268686F73746E616D"
      "656E7075626C69736865722E74657374716578706572696D656E7447726F757049646531"
      "3233343569617267756D656E747382A26464617461817168747470733A2F2F666F6F2E74"
      "6573742F6474616773816A72656E64657255726C73A26464617461817468747470733A2F"
      "2F666F6F7375622E746573742F647461677381756164436F6D706F6E656E7452656E6465"
      "7255726C7372636F6D7072657373696F6E47726F7570496400A462696401686D65746164"
      "617461A268686F73746E616D656E7075626C69736865722E74657374716578706572696D"
      "656E7447726F7570496465313233343569617267756D656E747382A26464617461817168"
      "747470733A2F2F6261722E746573742F6474616773816A72656E64657255726C73A26464"
      "617461817468747470733A2F2F6261727375622E746573742F647461677381756164436F"
      "6D706F6E656E7452656E64657255726C7372636F6D7072657373696F6E47726F75704964"
      "00A462696400686D65746164617461A268686F73746E616D656E7075626C69736865722E"
      "74657374716578706572696D656E7447726F7570496465313233343569617267756D656E"
      "747382A26464617461817168747470733A2F2F666F6F2E746573742F6474616773816A72"
      "656E64657255726C73A26464617461817468747470733A2F2F666F6F7375622E74657374"
      "2F647461677381756164436F6D706F6E656E7452656E64657255726C7372636F6D707265"
      "7373696F6E47726F757049640171616363657074436F6D7072657373696F6E82646E6F6E"
      "6564677A6970";
  // Prefix hex for `kExpectedBodyHex` which includes the compression format
  // code and the length.
  const std::string kExpectedPrefixHex = "000000026A";
  // Padding zeros.
  const std::string kPaddingString =
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000";

  EXPECT_EQ(base::HexEncode(body_bytes),
            kExpectedPrefixHex + kExpectedBodyHex + kPaddingString);
}

// TODO(crbug.com/337917489): When adding an identical trusted scoring signals
// request, it should use the existing partition instead of creating a new one.
// After the implementation, the EXPECT_EQ() of request E which is duplicated
// from request A, should be failed.
//
// Add the following trusted bidding signals requests:
// Request A[render_url: foo.test, component_url: foosub.test,
//           owner_origin: owner-a, joining_origin: joining-a]
// Request B[render_url: foo.test, component_url: barsub.test,
//           owner_origin: owner-a, joining_origin: joining-a]
// Request C[render_url: bar.test, component_url: foosub.test,
//           owner_origin: owner-a, joining_origin: joining-a]
// Request D[render_url: bar.test, component_url: barsub.test,
//           owner_origin: owner-a, joining_origin: joining-a]
// Request E[render_url: foo.test, component_url: foosub.test,
//           owner_origin: owner-a, joining_origin: joining-a]
// Request F[render_url: foo.test, component_url: foosub.test,
//           owner_origin: owner-a, joining_origin: joining-b]
// Request G[render_url: foo.test, component_url: foosub.test,
//           owner_origin: owner-b, joining_origin: joining-a]
// Request H[render_url: foo.test, component_url: foosub.test,
//           owner_origin: owner-b, joining_origin: joining-b]
// will result the following groups:
// Compression: 0 -
//    partition 0: A
//    partition 1: B
//    partition 2: C
//    partition 4: D
//    partition 4: E
// Compression: 1 -
//    partition 0: F
// Compression: 2 -
//    partition 0: G
// Compression: 3 -
//    partition 0: H
TEST_F(TrustedSignalsKVv2RequestHelperTest,
       TrustedScoringSignalsIsolationIndex) {
  std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedScoringSignalsKVv2RequestHelperBuilder>(
              kHostName, kExperimentGroupId, std::move(public_key_));

  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginBarsubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 2),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginBarUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 3),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginBarUrl), std::set<std::string>{kOriginBarsubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 4),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 0),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginA)),
                url::Origin::Create(GURL(kJoiningOriginB))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(2, 0),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginB)),
                url::Origin::Create(GURL(kJoiningOriginA))));
  EXPECT_EQ(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(3, 0),
            helper_builder->AddTrustedSignalsRequest(
                GURL(kOriginFooUrl), std::set<std::string>{kOriginFoosubUrl},
                url::Origin::Create(GURL(kOwnerOriginB)),
                url::Origin::Create(GURL(kJoiningOriginB))));
}

class TrustedSignalsKVv2ResponseParserTest : public testing::Test {
 public:
  explicit TrustedSignalsKVv2ResponseParserTest() {
    helper_ = AuctionV8Helper::Create(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    base::RunLoop().RunUntilIdle();
    v8_scope_ =
        std::make_unique<AuctionV8Helper::FullIsolateScope>(helper_.get());
  }

  ~TrustedSignalsKVv2ResponseParserTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> helper_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

// Test trusted bidding signals response parsing with gzip compressed cbor
// bytes.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       TrustedBiddingSignalsResponseParsing) {
  // Used cbor.me to convert from
  // [
  //   {
  //     "id": 0,
  //     "dataVersion": 102,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupA": {
  //             "value": "{\"priorityVector\":{\"signalA\":1}}"
  //           },
  //           "groupB": {
  //             "value": "{\"priorityVector\":{\"signalB\":1}}"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyA": {
  //             "value": "\"valueForA\""
  //           },
  //           "keyB": {
  //             "value": "[\"value1ForB\",\"value2ForB\"]"
  //           }
  //         }
  //       }
  //     ]
  //   },
  //   {
  //     "id": 1,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupC": {
  //             "value": "{\"priorityVector\":{\"signalC\":1}}"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyC": {
  //             "value": "\"valueForC\""
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  const std::string kCompressionGroup0Hex =
      "82A3626964006B6461746156657273696F6E18666F6B657947726F75704F757470757473"
      "82A264746167738172696E74657265737447726F75704E616D6573696B657956616C7565"
      "73A26667726F757041A16576616C756578207B227072696F72697479566563746F72223A"
      "7B227369676E616C41223A317D7D6667726F757042A16576616C756578207B227072696F"
      "72697479566563746F72223A7B227369676E616C42223A317D7DA2647461677381646B65"
      "7973696B657956616C756573A2646B657941A16576616C75656B2276616C7565466F7241"
      "22646B657942A16576616C7565781B5B2276616C756531466F7242222C2276616C756532"
      "466F7242225DA2626964016F6B657947726F75704F75747075747382A264746167738172"
      "696E74657265737447726F75704E616D6573696B657956616C756573A16667726F757043"
      "A16576616C756578207B227072696F72697479566563746F72223A7B227369676E616C43"
      "223A317D7DA2647461677381646B657973696B657956616C756573A1646B657943A16576"
      "616C75656B2276616C7565466F724322";
  std::vector<uint8_t> compression_group0_bytes;
  base::HexStringToBytes(kCompressionGroup0Hex, &compression_group0_bytes);
  std::vector<uint8_t> compressed_group0_bytes =
      GzipCompressHelper(compression_group0_bytes);

  // Used cbor.me to convert from
  // [
  //   {
  //     "id": 2,
  //     "dataVersion": 206,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalD\":1}}"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyD": {
  //             "value": "\"valueForD\""
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  const std::string kCompressionGroup1Hex =
      "81A3626964026B6461746156657273696F6E18CE6F6B657947726F75704F757470757473"
      "82A264746167738172696E74657265737447726F75704E616D6573696B657956616C7565"
      "73A16667726F757044A16576616C756578207B227072696F72697479566563746F72223A"
      "7B227369676E616C44223A317D7DA2647461677381646B657973696B657956616C756573"
      "A1646B657944A16576616C75656B2276616C7565466F724422";
  std::vector<uint8_t> compression_group1_bytes;
  base::HexStringToBytes(kCompressionGroup1Hex, &compression_group1_bytes);
  std::vector<uint8_t> compressed_group1_bytes =
      GzipCompressHelper(compression_group1_bytes);

  // Construct a CBOR body:
  // {
  //   "compressionGroups": [
  //     {
  //       "compressionGroupId": 0,
  //       "ttlMs": 100,
  //       "content": compression_group0_bytes
  //     },
  //     {
  //       "compressionGroupId": 1,
  //       "ttlMs": 200,
  //       "content": compression_group1_bytes
  //     }
  //   ]
  // }
  cbor::Value::MapValue compression_group0;
  compression_group0.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(0));
  compression_group0.try_emplace(cbor::Value("ttlMs"), cbor::Value(100));
  compression_group0.try_emplace(cbor::Value("content"),
                                 cbor::Value(compressed_group0_bytes));

  cbor::Value::MapValue compression_group1;
  compression_group1.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(1));
  compression_group1.try_emplace(cbor::Value("ttlMs"), cbor::Value(200));
  compression_group1.try_emplace(cbor::Value("content"),
                                 cbor::Value(compressed_group1_bytes));

  cbor::Value::ArrayValue compression_groups;
  compression_groups.emplace_back(std::move(compression_group0));
  compression_groups.emplace_back(std::move(compression_group1));

  cbor::Value::MapValue body_map;
  body_map.try_emplace(cbor::Value("compressionGroups"),
                       cbor::Value(std::move(compression_groups)));

  cbor::Value body_value(std::move(body_map));
  std::optional<std::vector<uint8_t>> maybe_body_bytes =
      cbor::Writer::Write(body_value);
  EXPECT_TRUE(maybe_body_bytes.has_value());

  // Set compression format to 0x02 which means gzip.
  std::string response_body = BuildResponseBody(
      base::HexEncode(std::move(maybe_body_bytes).value()), 0x02);

  // Encrypt response body.
  auto helper_result = EncryptResponseBodyHelper(response_body);

  // Check SignalsFetchResult.
  TrustedSignalsKVv2ResponseParser::SignalsFetchResult maybe_fetch_result =
      TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
          helper_result.first, helper_result.second);
  EXPECT_TRUE(maybe_fetch_result.has_value());
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap fetch_result =
      std::move(maybe_fetch_result).value();

  CompressionGroupResult group0 = CompressionGroupResult(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      std::move(compressed_group0_bytes), base::Milliseconds(100));
  CompressionGroupResult group1 = CompressionGroupResult(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      std::move(compressed_group1_bytes), base::Milliseconds(200));
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap
      expected_fetch_result;
  expected_fetch_result.emplace(0, std::move(group0));
  expected_fetch_result.emplace(1, std::move(group1));
  ExpectCompressionGroupMapEquals(expected_fetch_result, fetch_result);

  // Check TrustedSignalsResultMap.
  const std::set<std::string> kInterestGroupNames = {"groupA", "groupB",
                                                     "groupC", "groupD"};
  const std::set<std::string> kKeys = {"keyA", "keyB", "keyC", "keyD"};

  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError
      maybe_result_map = TrustedSignalsKVv2ResponseParser::
          ParseBiddingSignalsFetchResultToResultMap(
              helper_.get(), kInterestGroupNames, kKeys, fetch_result);
  EXPECT_TRUE(maybe_result_map.has_value());
  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap result_map =
      maybe_result_map.value();
  EXPECT_EQ(result_map.size(), 3u);

  std::vector<std::string> expected_names = {"groupA", "groupB"};
  std::vector<std::string> expected_keys = {"keyA", "keyB"};
  uint32_t expected_data_version = 102;
  std::map<std::string, TrustedSignals::Result::PriorityVector>
      priority_vector_map{
          {"groupA", TrustedSignals::Result::PriorityVector{{"signalA", 1}}},
          {"groupB", TrustedSignals::Result::PriorityVector{{"signalB", 1}}}};
  std::string expected_bidding_signals =
      R"({"keyA":"valueForA","keyB":["value1ForB","value2ForB"]})";
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  CheckBiddingResult(helper_.get(), result_map, index, expected_names,
                     expected_keys, priority_vector_map,
                     expected_bidding_signals, expected_data_version);

  expected_names = {"groupC"};
  expected_keys = {"keyC"};
  priority_vector_map = {
      {"groupC", TrustedSignals::Result::PriorityVector{{"signalC", 1}}}};
  expected_bidding_signals = R"({"keyC":"valueForC"})";
  index = TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1);
  CheckBiddingResult(helper_.get(), result_map, index, expected_names,
                     expected_keys, priority_vector_map,
                     expected_bidding_signals, /*data_version=*/std::nullopt);

  expected_names = {"groupD"};
  expected_keys = {"keyD"};
  expected_data_version = 206;
  priority_vector_map = {
      {"groupD", TrustedSignals::Result::PriorityVector{{"signalD", 1}}}};
  expected_bidding_signals = R"({"keyD":"valueForD"})";
  index = TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 2);
  CheckBiddingResult(helper_.get(), result_map, index, expected_names,
                     expected_keys, priority_vector_map,
                     expected_bidding_signals, expected_data_version);
}

// Test trusted bidding signals response parsing with uncompressed CBOR bytes.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       TrustedScoringSignalsResponseParsing) {
  // Used cbor.me to convert from
  // [
  //   {
  //     "id": 0,
  //     "dataVersion": 54,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://bar.test/": {
  //             "value": "1"
  //           },
  //           "https://foo.test/": {
  //             "value": "{\"foo\": [100], \"bar\": \"test\"}"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "adComponentRenderUrls"
  //         ],
  //         "keyValues": {
  //           "https://barsub.test/": {
  //             "value": "2"
  //           },
  //           "https://foosub.test/": {
  //             "value": "[3]"
  //           }
  //         }
  //       }
  //     ]
  //   },
  //   {
  //     "id": 1,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://baz.test/": {
  //             "value": "null"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "adComponentRenderUrls"
  //         ],
  //         "keyValues": {
  //           "https://bazsub.test/": {
  //             "value": "null"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  const std::string kCompressionGroup0Hex =
      "82A3626964006B6461746156657273696F6E18366F6B657947726F75704F757470757473"
      "82A26474616773816A72656E64657255726C73696B657956616C756573A2716874747073"
      "3A2F2F6261722E746573742FA16576616C756561317168747470733A2F2F666F6F2E7465"
      "73742FA16576616C7565781D7B22666F6F223A205B3130305D2C2022626172223A202274"
      "657374227DA2647461677381756164436F6D706F6E656E7452656E64657255726C73696B"
      "657956616C756573A27468747470733A2F2F6261727375622E746573742FA16576616C75"
      "6561327468747470733A2F2F666F6F7375622E746573742FA16576616C7565635B335DA2"
      "626964016F6B657947726F75704F75747075747382A26474616773816A72656E64657255"
      "726C73696B657956616C756573A17168747470733A2F2F62617A2E746573742FA1657661"
      "6C7565646E756C6CA2647461677381756164436F6D706F6E656E7452656E64657255726C"
      "73696B657956616C756573A17468747470733A2F2F62617A7375622E746573742FA16576"
      "616C7565646E756C6C";
  std::vector<uint8_t> compression_group0_bytes;
  base::HexStringToBytes(kCompressionGroup0Hex, &compression_group0_bytes);

  // Used cbor.me to convert from
  // [
  //   {
  //     "id": 2,
  //     "dataVersion": 17,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://qux.test/": {
  //             "value": "[\"3\"]"
  //           }
  //         }
  //       },
  //       {
  //         "tags": [
  //           "adComponentRenderUrls"
  //         ],
  //         "keyValues": {
  //           "https://quxsub.test/": {
  //             "value": "[\"4\"]"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  const std::string kCompressionGroup1Hex =
      "81A3626964026B6461746156657273696F6E116F6B657947726F75704F75747075747382"
      "A26474616773816A72656E64657255726C73696B657956616C756573A17168747470733A"
      "2F2F7175782E746573742FA16576616C7565655B2233225DA2647461677381756164436F"
      "6D706F6E656E7452656E64657255726C73696B657956616C756573A17468747470733A2F"
      "2F7175787375622E746573742FA16576616C7565655B2234225D";
  std::vector<uint8_t> compression_group1_bytes;
  base::HexStringToBytes(kCompressionGroup1Hex, &compression_group1_bytes);

  // Construct a CBOR body:
  // {
  //   "compressionGroups": [
  //     {
  //       "compressionGroupId": 0,
  //       "ttlMs": 100,
  //       "content": compression_group0_bytes
  //     },
  //     {
  //       "compressionGroupId": 1,
  //       "ttlMs": 200,
  //       "content": compression_group1_bytes
  //     }
  //   ]
  // }
  cbor::Value::MapValue compression_group0;
  compression_group0.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(0));
  compression_group0.try_emplace(cbor::Value("ttlMs"), cbor::Value(100));
  compression_group0.try_emplace(cbor::Value("content"),
                                 cbor::Value(compression_group0_bytes));

  cbor::Value::MapValue compression_group1;
  compression_group1.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(1));
  compression_group1.try_emplace(cbor::Value("ttlMs"), cbor::Value(200));
  compression_group1.try_emplace(cbor::Value("content"),
                                 cbor::Value(compression_group1_bytes));

  cbor::Value::ArrayValue compression_groups;
  compression_groups.emplace_back(std::move(compression_group0));
  compression_groups.emplace_back(std::move(compression_group1));

  cbor::Value::MapValue body_map;
  body_map.try_emplace(cbor::Value("compressionGroups"),
                       cbor::Value(std::move(compression_groups)));

  cbor::Value body_value(std::move(body_map));
  std::optional<std::vector<uint8_t>> maybe_body_bytes =
      cbor::Writer::Write(body_value);
  EXPECT_TRUE(maybe_body_bytes.has_value());

  // Set compression format to 0x00 which means uncompressed.
  std::string response_body =
      BuildResponseBody(base::HexEncode(std::move(maybe_body_bytes).value()),
                        /*compress_scheme=*/0x00);

  // Encrypt response body.
  auto helper_result = EncryptResponseBodyHelper(response_body);

  // Check SignalsFetchResult.
  TrustedSignalsKVv2ResponseParser::SignalsFetchResult maybe_fetch_result =
      TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
          helper_result.first, helper_result.second);
  EXPECT_TRUE(maybe_fetch_result.has_value());
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap fetch_result =
      std::move(maybe_fetch_result).value();

  CompressionGroupResult group0 = CompressionGroupResult(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      std::move(compression_group0_bytes), base::Milliseconds(100));
  CompressionGroupResult group1 = CompressionGroupResult(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      std::move(compression_group1_bytes), base::Milliseconds(200));
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap
      expected_fetch_result;
  expected_fetch_result.emplace(0, std::move(group0));
  expected_fetch_result.emplace(1, std::move(group1));
  ExpectCompressionGroupMapEquals(expected_fetch_result, fetch_result);

  // Check TrustedSignalsResultMap.
  const std::set<std::string> kRenderUrls = {
      "https://foo.test/", "https://bar.test/", "https://baz.test/",
      "https://qux.test/"};
  const std::set<std::string> kAdComponentRenderUrls = {
      "https://foosub.test/", "https://barsub.test/", "https://bazsub.test/",
      "https://quxsub.test/"};

  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError
      maybe_result_map = TrustedSignalsKVv2ResponseParser::
          ParseScoringSignalsFetchResultToResultMap(
              helper_.get(), kRenderUrls, kAdComponentRenderUrls, fetch_result);
  EXPECT_TRUE(maybe_result_map.has_value());
  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap result_map =
      maybe_result_map.value();
  EXPECT_EQ(result_map.size(), 3u);

  GURL render_url = GURL("https://foo.test/");
  std::vector<std::string> ad_component_render_urls = {"https://foosub.test/",
                                                       "https://barsub.test/"};
  uint32_t expected_data_version = 54;
  std::string expected_signals =
      R"({"renderURL":{"https://foo.test/":{"foo":[100],"bar":"test"}},)"
      R"("renderUrl":{"https://foo.test/":{"foo":[100],"bar":"test"}},)"
      R"("adComponentRenderURLs":{"https://foosub.test/":[3],"https://barsub.test/":2},)"
      R"("adComponentRenderUrls":{"https://foosub.test/":[3],"https://barsub.test/":2}})";
  TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index(0, 0);
  CheckScoringResult(helper_.get(), result_map, index, render_url,
                     ad_component_render_urls, expected_signals,
                     expected_data_version);

  render_url = GURL("https://baz.test/");
  ad_component_render_urls = {"https://bazsub.test/"};
  expected_signals =
      R"({"renderURL":{"https://baz.test/":null},"renderUrl":{"https://baz.test/":null},)"
      R"("adComponentRenderURLs":{"https://bazsub.test/":null},)"
      R"("adComponentRenderUrls":{"https://bazsub.test/":null}})";
  index = TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1);
  CheckScoringResult(helper_.get(), result_map, index, render_url,
                     ad_component_render_urls, expected_signals,
                     /*data_version=*/std::nullopt);

  render_url = GURL("https://qux.test/");
  ad_component_render_urls = {"https://quxsub.test/"};
  expected_data_version = 17;
  expected_signals =
      R"({"renderURL":{"https://qux.test/":["3"]},"renderUrl":{"https://qux.test/":["3"]},)"
      R"("adComponentRenderURLs":{"https://quxsub.test/":["4"]},)"
      R"("adComponentRenderUrls":{"https://quxsub.test/":["4"]}})";
  index = TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 2);
  CheckScoringResult(helper_.get(), result_map, index, render_url,
                     ad_component_render_urls, expected_signals,
                     expected_data_version);
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, ResponseDecryptionFailure) {
  // Failed to decrypt response body
  // Use a different ID to obtain a public key that differs from the one used in
  // `EncryptResponseBodyHelper()`.
  std::string public_key =
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey));
  auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
                    kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                    EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM)
                    .value();

  auto request = quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
                     "Fake request.", public_key, config,
                     kTrustedSignalsKVv2EncryptionRequestMediaType)
                     .value();
  auto wrong_context = std::move(request).ReleaseContext();

  std::string response_body = "Response body.";
  auto helper_result = EncryptResponseBodyHelper(response_body);
  EXPECT_EQ("Failed to decrypt response body.",
            TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
                helper_result.first, wrong_context)
                .error()
                .error_msg);
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, SignalsFetchResultParseFailure) {
  std::string hex_string;

  // Response shorter than framing header with 4 bytes hex string.
  std::string response_body = std::string({0xA, 0xA, 0xA, 0xA});
  auto helper_result = EncryptResponseBodyHelper(response_body);
  EXPECT_EQ("Response shorter than framing header.",
            TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
                helper_result.first, helper_result.second)
                .error()
                .error_msg);

  // Unsupported compression scheme.
  hex_string = "AA";
  EXPECT_EQ(
      "Unsupported compression scheme.",
      GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string, 0x01));

  // Failed to parse response body as CBOR
  // Random 20 bytes hex string.
  hex_string = "666f421a72ed47aade0c63826288d5d1bbf2dc2a";
  EXPECT_EQ("Failed to parse response body as CBOR.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Response body is not type of map
  // CBOR: [1]
  hex_string = "8101";
  EXPECT_EQ("Response body is not type of map.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Failed to find compression groups in response
  // CBOR:
  // {
  //   "something": "none"
  // }
  hex_string = "A169736F6D657468696E67646E6F6E65";
  EXPECT_EQ("Failed to find compression groups in response.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression groups is not type of array
  // CBOR:
  // {
  //   "compressionGroups": 0
  // }
  hex_string = "A171636F6D7072657373696F6E47726F75707300";
  EXPECT_EQ("Compression groups is not type of array.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group id is already in used
  const std::string kContentHex = "A0";
  std::vector<uint8_t> content_bytes;
  base::HexStringToBytes(kContentHex, &content_bytes);

  // Construct a CBOR body:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "content": content_bytes,
  //       "compressionGroupId": 0
  //     },
  //     {
  //       "ttlMs": 100,
  //       "content": content_bytes,
  //       "compressionGroupId": 0
  //     }
  //   ]
  // }
  cbor::Value::MapValue compression_group0;
  compression_group0.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(0));
  compression_group0.try_emplace(cbor::Value("ttlMs"), cbor::Value(100));
  compression_group0.try_emplace(cbor::Value("content"),
                                 cbor::Value(std::move(content_bytes)));

  cbor::Value::MapValue compression_group1;
  compression_group1.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(0));
  compression_group1.try_emplace(cbor::Value("ttlMs"), cbor::Value(200));
  compression_group1.try_emplace(cbor::Value("content"),
                                 cbor::Value(std::move(content_bytes)));

  cbor::Value::ArrayValue compression_groups;
  compression_groups.emplace_back(std::move(compression_group0));
  compression_groups.emplace_back(std::move(compression_group1));

  cbor::Value::MapValue body_map;
  body_map.try_emplace(cbor::Value("compressionGroups"),
                       cbor::Value(std::move(compression_groups)));

  cbor::Value body_value(std::move(body_map));
  std::optional<std::vector<uint8_t>> maybe_body_bytes =
      cbor::Writer::Write(body_value);
  EXPECT_TRUE(maybe_body_bytes.has_value());

  response_body = BuildResponseBody(
      base::HexEncode(std::move(maybe_body_bytes).value()), 0x00);
  helper_result = EncryptResponseBodyHelper(response_body);
  EXPECT_EQ("Compression group id \"0\" is already in used.",
            TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
                helper_result.first, helper_result.second)
                .error()
                .error_msg);

  // Compression group is not type of map
  // CBOR:
  // {
  //   "compressionGroups": [0]
  // }
  hex_string = "A171636F6D7072657373696F6E47726F7570738100";
  EXPECT_EQ("Compression group is not type of map.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Key "compressionGroupId" is missing in compressionGroups map
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "content": "content"
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A26574746C4D73186467636F6E74656E"
      "7467636F6E74656E74";
  EXPECT_EQ("Key \"compressionGroupId\" is missing in compressionGroups map.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Key "content" is missing in compressionGroups map
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "compressionGroupId": 0
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A26574746C4D73186472636F6D707265"
      "7373696F6E47726F7570496400";
  EXPECT_EQ("Key \"content\" is missing in compressionGroups map.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group id is not type of integer
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "content": "content",
  //       "compressionGroupId": "1"
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A36574746C4D73186467636F6E74656E"
      "7467636F6E74656E7472636F6D7072657373696F6E47726F757049646131";
  EXPECT_EQ("Compression group id is not type of integer.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group id is out of range for int.
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "content": "content",
  //       "compressionGroupId": 2147483648
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A36574746C4D73186467636F6E74656E"
      "7467636F6E74656E7472636F6D7072657373696F6E47726F757049641A80000000";
  EXPECT_EQ("Compression group id is out of range for int.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group ttl is not type of integer
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": "100",
  //       "content": "content",
  //       "compressionGroupId": 1
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A36574746C4D736331303067636F6E74"
      "656E7467636F6E74656E7472636F6D7072657373696F6E47726F7570496401";
  EXPECT_EQ("Compression group ttl is not type of integer.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group content is not type of byte string
  // CBOR:
  // {
  //   "compressionGroups": [
  //     {
  //       "ttlMs": 100,
  //       "content": "content",
  //       "compressionGroupId": 1
  //     }
  //   ]
  // }
  hex_string =
      "A171636F6D7072657373696F6E47726F75707381A36574746C4D73186467636F6E74656E"
      "7467636F6E74656E7472636F6D7072657373696F6E47726F7570496401";
  EXPECT_EQ("Compression group content is not type of byte string.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       SignalsFetchResultMapParseFailure) {
  // Construct a CompressionGroupResultMap with the following hardcoded values.
  std::string hex_string;
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap result_map;
  CompressionGroupResult compression_group_result;
  result_map.emplace(0, std::move(compression_group_result));
  const std::set<std::string> kInterestGroupNames = {"groupA"};
  const std::set<std::string> kBiddingKeys = {"keyA"};
  const std::set<std::string> kRenderUrls = {"https://foo.test/"};
  const std::set<std::string> kAdComponentRenderUrls = {"https://foosub.test/"};

  // Failed to decompress content string with Gzip
  result_map[0].compression_scheme =
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip;
  // []
  hex_string = "80";
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to decompress content string with Gzip.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Failed to decompress content string with Gzip.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Set compression scheme to kNone for the rest of test cases.
  result_map[0].compression_scheme =
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone;

  // Failed to parse content as CBOR
  // Random 20 bytes hex string.
  hex_string = "666f421a72ed47aade0c63826288d5d1bbf2dc2a";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to parse content as CBOR.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Failed to parse content as CBOR.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Content is not type of array
  // "1"
  hex_string = "6131";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Content is not type of array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Content is not type of array.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Partition is not type of map
  // [1]
  hex_string = "8101";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition is not type of map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Partition is not type of map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Key "id" is missing in partition map
  // [
  //   {
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string = "81A16F6B657947726F75704F75747075747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"id\" is missing in partition map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Key \"id\" is missing in partition map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Key "keyGroupOutputs" is missing in partition map
  // [
  //   {
  //     "id": 0
  //   }
  // ]
  hex_string = "81A162696400";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"keyGroupOutputs\" is missing in partition map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Key \"keyGroupOutputs\" is missing in partition map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Partition id is not type of integer
  // [
  //   {
  //     "id": "0",
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string = "81A262696461306F6B657947726F75704F75747075747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition id is not type of integer.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Partition id is not type of integer.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Partition id is out of range for int
  // [
  //   {
  //     "id": 2147483648,
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string = "81A26269641A800000006F6B657947726F75704F75747075747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition id is out of range for int.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Partition id is out of range for int.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Partition key group outputs is not type of array
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": 100
  //   }
  // ]
  hex_string = "81A2626964006F6B657947726F75704F7574707574731864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition key group outputs is not type of array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Partition key group outputs is not type of array.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // DataVersion is not type of integer
  // [
  //   {
  //     "id": 0,
  //     "dataVersion": "102",
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string =
      "81A3626964006B6461746156657273696F6E633130326F6B657947726F75704F75747075"
      "747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("DataVersion is not type of integer.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("DataVersion is not type of integer.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // DataVersion field is out of range for uint32
  // [
  //   {
  //     "id": 0,
  //     "dataVersion": 4294967296,
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string =
      "81A3626964006B6461746156657273696F6E1B00000001000000006F6B657947726F7570"
      "4F75747075747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("DataVersion field is out of range for uint32.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("DataVersion field is out of range for uint32.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Duplicated partition id found in compression group for bidding signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyA": {
  //             "value": "100"
  //           }
  //         }
  //       }
  //     ]
  //   },
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyA": {
  //             "value": "100"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "82A2626964006F6B657947726F75704F75747075747381A2647461677381646B65797369"
      "6B657956616C756573A1646B657941A16576616C756563313030A2626964006F6B657947"
      "726F75704F75747075747381A2647461677381646B657973696B657956616C756573A164"
      "6B657941A16576616C756563313030";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Duplicated partition id \"0\" found in compression group \"0\".",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Duplicated partition id found in compression group for scoring signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://bar.test/": {
  //             "value": "100"
  //           }
  //         }
  //       }
  //     ]
  //   },
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://foo.test/": {
  //             "value": "100"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "82A2626964006F6B657947726F75704F75747075747381A26474616773816A72656E6465"
      "7255726C73696B657956616C756573A17168747470733A2F2F6261722E746573742FA165"
      "76616C756563313030A2626964006F6B657947726F75704F75747075747381A264746167"
      "73816A72656E64657255726C73696B657956616C756573A17168747470733A2F2F666F6F"
      "2E746573742FA16576616C756563313030";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Duplicated partition id \"0\" found in compression group \"0\".",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // KeyGroupOutput value is not type of map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [100]
  //   }
  // ]
  hex_string = "81A2626964006F6B657947726F75704F757470757473811864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("KeyGroupOutput value is not type of map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("KeyGroupOutput value is not type of map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Key "tags" is missing in keyGroupOutputs map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "keyValues": {
  //           "key": {
  //             "value": "value"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A1696B657956616C756573A163"
      "6B6579A16576616C75656576616C7565";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"tags\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Key \"tags\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Key "keyValues" is missing in keyGroupOutputs map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "tag"
  //         ]
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A164746167738163746167";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"keyValues\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Key \"keyValues\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Tags value in keyGroupOutputs map is not type of array
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": "tag",
  //         "keyValues": {
  //           "groupD": {
  //             "value": "value"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677363746167696B65"
      "7956616C756573A16667726F757044A16576616C75656576616C7565";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Tags value in keyGroupOutputs map is not type of array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Tags value in keyGroupOutputs map is not type of array.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Tags array must only have one tag
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": ["tag1","tag2"],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "value"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677382647461673164"
      "74616732696B657956616C756573A16667726F757044A16576616C75656576616C7565";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Tags array must only have one tag.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Tags array must only have one tag.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Tag value in tags array of keyGroupOutputs map is not type of string
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [100],
  //         "keyValues": {
  //           "key": {
  //             "value": "value"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773811864696B6579"
      "56616C756573A1636B6579A16576616C75656576616C7565";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ(
      "Tag value in tags array of keyGroupOutputs map is not type of string.",
      GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
          helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ(
      "Tag value in tags array of keyGroupOutputs map is not type of string.",
      GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
          helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Duplicate tag detected in keyGroupOutputs
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": ["tag"],
  //         "keyValues": {
  //           "key": {
  //             "value": "value"
  //           }
  //         }
  //       },
  //       {
  //         "tags": ["tag"],
  //         "keyValues": {
  //           "key": {
  //             "value": "value"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747382A264746167738163746167696B"
      "657956616C756573A1636B6579A16576616C75656576616C7565A2647461677381637461"
      "67696B657956616C756573A1636B6579A16576616C75656576616C7565";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Duplicate tag \"tag\" detected in keyGroupOutputs.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("Duplicate tag \"tag\" detected in keyGroupOutputs.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // KeyValue value in keyGroupOutputs map is not type of map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "tag"
  //         ],
  //         "keyValues": 100
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738163746167696B"
      "657956616C7565731864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("KeyValue value in keyGroupOutputs map is not type of map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
  EXPECT_EQ("KeyValue value in keyGroupOutputs map is not type of map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Value is not type of map for bidding signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupA": 100
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C756573A16667726F7570411864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Value of \"groupA\" is not type of map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Value is not type of map for scoring signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "renderUrls"
  //         ],
  //         "keyValues": {
  //           "https://foo.test/": 100
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773816A72656E6465"
      "7255726C73696B657956616C756573A17168747470733A2F2F666F6F2E746573742F186"
      "4";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Value of \"https://foo.test/\" is not type of map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Failed to find key "value" in the map for bidding signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupA": {
  //             "val": ""
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C756573A16667726F757041A16376616C"
      "60";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to find key \"value\" in the map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Failed to find key "value" in the map for scoring signals
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
  //             "val": ""
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773816A72656E6465"
      "7255726C73696B657956616C756573A17168747470733A2F2F666F6F2E746573742FA163"
      "76616C60";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to find key \"value\" in the map.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Failed to read value of key "value" as type String for bidding signals
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupA": {
  //             "value": 100
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C756573A16667726F757041A16576616C"
      "75651864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to read value of key \"value\" as type String.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Failed to read value of key "value" as type String for scoring signals
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
  //             "value": 100
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773816A72656E6465"
      "7255726C73696B657956616C756573A17168747470733A2F2F666F6F2E746573742FA165"
      "76616C75651864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to read value of key \"value\" as type String.",
            GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
                helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Failed to create V8 value from key group output data
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": {
  //           "groupA": {
  //             "value": "signal:"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C756573A16667726F757041A16576616C"
      "7565677369676E616C3A";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to create V8 value from key group output data.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Failed to parse key-value string to JSON for bidding keys
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "keys"
  //         ],
  //         "keyValues": {
  //           "keyA": {
  //             "value": "100:"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677381646B65797369"
      "6B657956616C756573A1646B657941A16576616C7565643130303A";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to parse key-value string to JSON for key \"keyA\".",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Failed to parse key-value string to JSON for render URLs
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
  //             "value": "100:"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773816A72656E6465"
      "7255726C73696B657956616C756573A17168747470733A2F2F666F6F2E746573742FA165"
      "76616C7565643130303A";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ(
      "Failed to parse key-value string to JSON for key \"https://foo.test/\".",
      GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
          helper_, kRenderUrls, kAdComponentRenderUrls, result_map));

  // Failed to parse key-value string to JSON for ad component render URLs
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "adComponentRenderUrls"
  //         ],
  //         "keyValues": {
  //           "https://foosub.test/": {
  //             "value": "100:"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677381756164436F6D"
      "706F6E656E7452656E64657255726C73696B657956616C756573A17468747470733A2F2F"
      "666F6F7375622E746573742FA16576616C7565643130303A";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ(
      "Failed to parse key-value string to JSON for key "
      "\"https://foosub.test/\".",
      GetErrorMessageFromParseScoringSignalsFetchResultToResultMap(
          helper_, kRenderUrls, kAdComponentRenderUrls, result_map));
}

////////////////////////////////////////////////////////////////////////////////
// ParseEntireCompressionGroup tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsEmptyData) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone, base::span<uint8_t>());
  EXPECT_THAT(result_or_error, IsError("Failed to parse content as CBOR."));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsNonCborData) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          base::as_bytes(base::make_span("Not CBOR")));
  EXPECT_THAT(result_or_error, IsError("Failed to parse content as CBOR."));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsNotCborArray) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(R"({"this": "is a map."})"));
  EXPECT_THAT(result_or_error, IsError("Content is not type of array."));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsNoPartitions) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector("[]"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{}));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsInvalidPartitions) {
  const struct {
    const char* reponse_as_json;
    const char* expected_error;
  } kTestCases[] = {{"[[]]", "Partition is not type of map."},
                    {"[1]", "Partition is not type of map."},

                    {R"([{ "id": 0 }])",
                     R"(Key "keyGroupOutputs" is missing in partition map.)"},

                    {R"([{ "id": [], "keyGroupOutputs": [] }])",
                     "Partition id is not type of integer."},
                    {R"([{ "id": "Rosebud", "keyGroupOutputs": [] }])",
                     "Partition id is not type of integer."},
                    {R"([{ "id": 0.5, "keyGroupOutputs": [] }])",
                     "Partition id is not type of integer."},

                    {R"([{ "id": 37, "keyGroupOutputs": [] },
                         { "id": 37, "keyGroupOutputs": [] }])",
                     R"(Duplicated partition id "37".)"}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.reponse_as_json);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(test_case.reponse_as_json));
    EXPECT_THAT(result_or_error, IsError(test_case.expected_error));
  }
}

// Empty partitions are allowed, as long as they have a "keyGroupOutputs" array.
TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsEmptyPartition) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(R"([{ "id": 0, "keyGroupOutputs": [] }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  EXPECT_FALSE((*result_or_error)[0]->GetDataVersion());
  EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group1"));
  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[0].get(),
                                  {"key1"}),
            R"({"key1":null})");
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsInvalidDataVersion) {
  const struct {
    const char* data_version;
    const char* expected_error;
  } kTestCases[] = {
      {"1.0", "DataVersion is not type of integer."},
      {"\"1\"", "DataVersion is not type of integer."},
      {"[1]", "DataVersion is not type of integer."},

      {"-1", "DataVersion field is out of range for uint32."},
      // 4294967296 is the minimum invalid integer, but can't be covered in a
      // test that uses ToCborVector(), as it goes through base::Value(), which
      // only supports floats and ints. As a result, it's covered in another
      // test.
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.data_version);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{"id": 0,
                     "dataVersion": %s,
                     "keyGroupOutputs": [] }])",
                test_case.data_version)));
    EXPECT_THAT(result_or_error, IsError(test_case.expected_error));
  }
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsDataVersion) {
  const uint32_t kTestCases[] = {0, 1};
  for (uint32_t test_case : kTestCases) {
    SCOPED_TRACE(test_case);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{"id": 0,
                     "dataVersion": %u,
                     "keyGroupOutputs": [] }])",
                test_case)));
    ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
    EXPECT_EQ((*result_or_error)[0]->GetDataVersion(), test_case);
  }
}

// ToCborVector() uses base::Value, which only supports ints and doubles. The
// maximum DataVersion value is the max uint32, so to test the max value, have
// to construct the cbor::Value directly.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsDataVersionMaxValue) {
  const int64_t kDataVersionMax = std::numeric_limits<uint32_t>::max();

  // Max value should succeed.
  cbor::Value::MapValue max_data_version_compression_group;
  max_data_version_compression_group.emplace(cbor::Value("id"), cbor::Value(0));
  max_data_version_compression_group.emplace(cbor::Value("dataVersion"),
                                             cbor::Value(kDataVersionMax));
  max_data_version_compression_group.emplace(
      cbor::Value("keyGroupOutputs"), cbor::Value(cbor::Value::ArrayValue()));
  cbor::Value::ArrayValue max_data_version_partitions;
  max_data_version_partitions.emplace_back(
      std::move(max_data_version_compression_group));
  cbor::Value max_data_version(std::move(max_data_version_partitions));
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          *cbor::Writer::Write(max_data_version));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  EXPECT_EQ((*result_or_error)[0]->GetDataVersion(), kDataVersionMax);

  // Max value + 1 should fail.
  cbor::Value::MapValue max_data_version_exceeded_compression_group;
  max_data_version_exceeded_compression_group.emplace(cbor::Value("id"),
                                                      cbor::Value(0));
  max_data_version_exceeded_compression_group.emplace(
      cbor::Value("dataVersion"), cbor::Value(kDataVersionMax + 1));
  max_data_version_exceeded_compression_group.emplace(
      cbor::Value("keyGroupOutputs"), cbor::Value(cbor::Value::ArrayValue()));
  cbor::Value::ArrayValue max_data_version_exceeded_partitions;
  max_data_version_exceeded_partitions.emplace_back(
      std::move(max_data_version_exceeded_compression_group));
  cbor::Value max_data_version_exceeded(
      std::move(max_data_version_exceeded_partitions));
  result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          *cbor::Writer::Write(std::move(max_data_version_exceeded)));
  EXPECT_THAT(result_or_error,
              IsError("DataVersion field is out of range for uint32."));
}

// This test covers cases where the structure of the "keyGroupOutputs" or the
// maps the tags or KeyGroupOutputs values it contains are invalid.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsInvalidKeyGroupOutputs) {
  const struct {
    const char* key_group_outputs;
    const char* expected_error;
  } kTestCases[] = {
      // Value not array.
      {"{}", R"(Partition key group outputs is not type of array.)"},
      {"1", R"(Partition key group outputs is not type of array.)"},

      // Array has value of wrong type.
      {"[[]]", R"(KeyGroupOutput value is not type of map.)"},
      {"[1]", R"(KeyGroupOutput value is not type of map.)"},
      // Next two tests have one valid and one invalid entry in the array.
      {R"([{"tags": ["tag1"], "keyValues": {}}, []])",
       R"(KeyGroupOutput value is not type of map.)"},
      {R"([[], {"tags": ["tag1"], "keyValues": {}}])",
       R"(KeyGroupOutput value is not type of map.)"},

      // Missing / invalid tags array test cases.
      {R"([{"keyValues": {}}])",
       R"(Key "tags" is missing in keyGroupOutputs map.)"},
      {R"([{"tags": {"1":"2"}, "keyValues": {}}])",
       R"(Tags value in keyGroupOutputs map is not type of array.)"},
      {R"([{"tags": 1, "keyValues": {}}])",
       R"(Tags value in keyGroupOutputs map is not type of array.)"},
      {R"([{"tags": [1], "keyValues": {}}])",
       "Tag value in tags array of keyGroupOutputs map is not type of string."},
      {R"([{"tags": ["tag1", "tag2"], "keyValues": {}}])",
       R"(Tags array must only have one tag.)"},
      {R"([{"tags": ["tag1", 2], "keyValues": {}}])",
       R"(Tags array must only have one tag.)"},

      // Missing / invalid `keyValues` map test cases.
      {R"([{"tags": ["tag1"]}])",
       R"(Key "keyValues" is missing in keyGroupOutputs map.)"},
      {R"([{"tags": ["tag1"], "keyValues": 1}])",
       R"(KeyValue value in keyGroupOutputs map is not type of map.)"},
      {R"([{"tags": ["tag1"], "keyValues": []}])",
       R"(KeyValue value in keyGroupOutputs map is not type of map.)"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.key_group_outputs);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(
                base::StringPrintf(R"([{ "id": 0, "keyGroupOutputs": %s }])",
                                   test_case.key_group_outputs)));
    EXPECT_THAT(result_or_error, IsError(test_case.expected_error));
  }
}

// Unknown tags are ignored.
TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsUnknownTags) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([{
                "id": 0,
                "keyGroupOutputs": [
                  {"tags": ["foo"], "keyValues": {}},
                  {"tags": ["bar"], "keyValues": {"foo":"bar"}}
                ]
              }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  EXPECT_FALSE((*result_or_error)[0]->GetDataVersion());
  EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group1"));
  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[0].get(),
                                  {"key1"}),
            R"({"key1":null})");
}

// Tests errors related to the "value" entry in the interestGroupNames
// dictionary. In particular, test when it's not present, not JSON, or the wrong
// JSON type.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsInterestGroupNamesValueError) {
  const struct {
    const char* value;
    const char* expected_error;
  } kTestCases[] = {
      {"", R"(Failed to find key "value" in the map.)"},
      {R"("not-value": "{}")", R"(Failed to find key "value" in the map.)"},
      {R"("value": null)",
       R"(Failed to read value of key "value" as type String.)"},
      {R"("value": [42])",
       R"(Failed to read value of key "value" as type String.)"},
      {R"("value": "")",
       "Failed to create V8 value from key group output data."},
      {R"("value": "Not Json")",
       "Failed to create V8 value from key group output data."},
      {R"("value": "\"Not a dictionary\"")",
       "Failed to create V8 value from key group output data."},
      {R"("value": "[\"Also not a dictionary\"]")",
       "Failed to create V8 value from key group output data."},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.value);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["interestGroupNames"],
                    "keyValues": {
                      "group1": { %s }
                    }
                  }]
                }])",
                test_case.value)));
    EXPECT_THAT(result_or_error, IsError(test_case.expected_error));
  }
}

// Test the case where `interestGroupNames` is valid JSON dictionary but has no
// known keys. This case is not an error, but there should be no PerGroupData.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsInterestGroupNamesNoKnownKeys) {
  const char* kTestCases[] = {
      R"("{}")",
      R"("{\"unknown1\":42}")",
      R"("{\"unknown2\":{\"signal1\":1}}")",
  };

  for (const auto* test_case : kTestCases) {
    SCOPED_TRACE(test_case);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["interestGroupNames"],
                    "keyValues": {
                      "group1": { "value": %s }
                    }
                  }]
                }])",
                test_case)));
    ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
    EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group1"));
  }
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsPriorityVectorWrongType) {
  const char* kTestCases[] = {
      R"("{\"priorityVector\":null}")",
      R"("{\"priorityVector\":[]}")",
      R"("{\"priorityVector\":42}")",
  };

  for (const auto* test_case : kTestCases) {
    SCOPED_TRACE(test_case);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["interestGroupNames"],
                    "keyValues": {
                      "group1": { "value": %s }
                    }
                  }]
                }])",
                test_case)));

    // These are currently not considered fatal errors.
    ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
    EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group1"));
  }
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsPriorityVector) {
  const struct {
    const char* priority_vector_json;
    TrustedSignals::Result::PriorityVector expected_value;
  } kTestCases[] = {
      {R"("{\"priorityVector\":{}}")", {}},
      {R"("{\"priorityVector\":{\"signal1\":1}}")", {{"signal1", 1}}},
      {R"("{\"priorityVector\":{\"signal1\":-3, \"signal2\":2.5}}")",
       {{"signal1", -3}, {"signal2", 2.5}}},

      // Invalid values are currently silently ignored, though they do result in
      // a non-null PerGroupData, with a populated `priority_vector`.
      {R"("{\"priorityVector\":{\"signal1\":null}}")", {}},
      {R"("{\"priorityVector\":{\"signal1\":null,\"signal2\":3}}")",
       {{"signal2", 3}}},
      {R"("{\"priorityVector\":{\"signal1\":[2]}}")", {}},
      {R"("{\"priorityVector\":{\"signal1\":[2],\"signal2\":3}}")",
       {{"signal2", 3}}},
      {R"("{\"priorityVector\":{\"signal1\":\"2\"}}")", {}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.priority_vector_json);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["interestGroupNames"],
                    "keyValues": {
                      "group1": { "value": %s }
                    }
                  }]
                }])",
                test_case.priority_vector_json)));
    ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));

    auto* per_group_data = (*result_or_error)[0]->GetPerGroupData("group1");
    ASSERT_TRUE(per_group_data);
    EXPECT_EQ(per_group_data->priority_vector, test_case.expected_value);
    EXPECT_FALSE(per_group_data->update_if_older_than);
  }
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsUpdateIfOlderThanMs) {
  const struct {
    const char* parse_update_if_older_than_json;
    std::optional<base::TimeDelta> expected_value;
  } kTestCases[] = {
      // Invalid values are currently silently ignored.
      {R"("{\"updateIfOlderThanMs\":null}")", std::nullopt},
      {R"("{\"updateIfOlderThanMs\":\"2\"}")", std::nullopt},
      {R"("{\"updateIfOlderThanMs\":[2]}")", std::nullopt},
      {R"("{\"updateIfOlderThanMs\":{}}")", std::nullopt},

      {R"("{\"updateIfOlderThanMs\":2}")", base::Milliseconds(2)},
      {R"("{\"updateIfOlderThanMs\":-3.5}")", base::Milliseconds(-3.5)},
  };

  for (bool enable_feature : {false, true}) {
    SCOPED_TRACE(enable_feature);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(features::kInterestGroupUpdateIfOlderThan,
                                      enable_feature);
    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(test_case.parse_update_if_older_than_json);
      auto result_or_error =
          TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
              helper_.get(),
              TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
              mojom::TrustedSignalsCompressionScheme::kNone,
              test::ToCborVector(base::StringPrintf(
                  R"([{
                    "id": 0,
                    "keyGroupOutputs": [{
                      "tags": ["interestGroupNames"],
                      "keyValues": {
                        "group1": { "value": %s }
                      }
                    }]
                  }])",
                  test_case.parse_update_if_older_than_json)));
      ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));

      auto* per_group_data = (*result_or_error)[0]->GetPerGroupData("group1");
      if (!enable_feature || !test_case.expected_value) {
        // When no value is expected and there is no priority vector,
        // `per_group_data` is nullopt.
        EXPECT_FALSE(per_group_data);
      } else {
        ASSERT_TRUE(per_group_data);
        EXPECT_FALSE(per_group_data->priority_vector);
        EXPECT_EQ(per_group_data->update_if_older_than,
                  test_case.expected_value);
      }
    }
  }
}

// Test that when part of an interest group's JSON is invalid, the rest is
// successfully parsed. e.g., a bad `priorityVectors` doesn't invalidate
// `updateIfOlderThanMs`, and vice versa.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsPerGroupDataHalfBad) {
  // Invalid `priorityVector`, valid `updateIfOlderThanMs`.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInterestGroupUpdateIfOlderThan);
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([{
                "id": 0,
                "keyGroupOutputs": [{
                  "tags": ["interestGroupNames"],
                  "keyValues": {
                    "group1": {
                      "value": "{
                        \"priorityVector\": \"Not valid\",
                        \"updateIfOlderThanMs\": 2
                      }"
                    }
                  }
                }]
              }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  auto* per_group_data = (*result_or_error)[0]->GetPerGroupData("group1");
  ASSERT_TRUE(per_group_data);
  EXPECT_FALSE(per_group_data->priority_vector);
  EXPECT_EQ(per_group_data->update_if_older_than, base::Milliseconds(2));

  // Valid `priorityVector`, invalid `updateIfOlderThanMs`.
  result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([{
                "id": 0,
                "keyGroupOutputs": [{
                  "tags": ["interestGroupNames"],
                  "keyValues": {
                    "group1": {
                      "value": "{
                        \"priorityVector\": {\"signal1\":2},
                        \"updateIfOlderThanMs\": \"Not valid\"
                      }"
                    }
                  }
                }]
              }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  per_group_data = (*result_or_error)[0]->GetPerGroupData("group1");
  ASSERT_TRUE(per_group_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector{
      {"signal1", 2}};
  EXPECT_EQ(per_group_data->priority_vector, kExpectedPriorityVector);
  EXPECT_FALSE(per_group_data->update_if_older_than);
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsPerGroupDataMultipleGroups) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInterestGroupUpdateIfOlderThan);
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([{
                "id": 0,
                "keyGroupOutputs": [{
                  "tags": ["interestGroupNames"],
                  "keyValues": {
                    "group1": {
                      "value": "{
                        \"priorityVector\": {\"signal1\":1, \"signal3\":3},
                        \"updateIfOlderThanMs\":2
                      }"
                    },
                    "group2": {
                      "value": "{
                        \"priorityVector\": {\"signal1\":2, \"signal2\":4},
                        \"updateIfOlderThanMs\":3
                      }"
                    }
                  }
                }]
              }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));

  auto* per_group1_data = (*result_or_error)[0]->GetPerGroupData("group1");
  ASSERT_TRUE(per_group1_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector1{
      {"signal1", 1}, {"signal3", 3}};
  EXPECT_EQ(per_group1_data->priority_vector, kExpectedPriorityVector1);
  EXPECT_EQ(per_group1_data->update_if_older_than, base::Milliseconds(2));

  auto* per_group2_data = (*result_or_error)[0]->GetPerGroupData("group2");
  ASSERT_TRUE(per_group2_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector2{
      {"signal1", 2}, {"signal2", 4}};
  EXPECT_EQ(per_group2_data->priority_vector, kExpectedPriorityVector2);
  EXPECT_EQ(per_group2_data->update_if_older_than, base::Milliseconds(3));

  EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group3"));
}

// Test cases where a `keys` entry of `keyGroupOutputs` has an invalid value.
// Test is named "InvalidKeys" rather than "InvalidKeyValue" because there's a
// field named KeyValue, and general invalid KeyValues are covered by another
// test.
TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsInvalidKeys) {
  const struct {
    const char* key_values_json;
    const char* expected_error;
  } kTestCases[] = {
      {R"()", R"(Failed to find key "value" in the map.)"},
      {R"("not-value":1)", R"(Failed to find key "value" in the map.)"},
      {R"("value":"Not JSON")",
       R"(Failed to parse key-value string to JSON for key "key1".)"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.key_values_json);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["keys"],
                    "keyValues": {
                      "key1": { %s }
                    }
                  }]
                }])",
                test_case.key_values_json)));
    EXPECT_THAT(result_or_error, IsError(test_case.expected_error));
  }
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsKeys) {
  const struct {
    const char* key_values_json;
    std::vector<std::string> keys_to_request;
    const char* expected_value;
  } kTestCases[] = {
      {R"({"key1":{"value":"null"}})", {"key1"}, R"({"key1":null})"},
      {R"({"key1":{"value":"1"}})", {"key1"}, R"({"key1":1})"},
      {R"({"key1":{"value":"-1.5"}})", {"key1"}, R"({"key1":-1.5})"},
      {R"({"key1":{"value":"[]"}})", {"key1"}, R"({"key1":[]})"},
      {R"({"key1":{"value":"[1,\"b\"]"}})", {"key1"}, R"({"key1":[1,"b"]})"},
      {R"({"key1":{"value":"{}"}})", {"key1"}, R"({"key1":{}})"},
      {R"({"key1":{"value":"{\"a\":\"b\",\"c\":1}"}})",
       {"key1"},
       R"({"key1":{"a":"b","c":1}})"},
      {R"({"key1":{"value":"1"}})", {"key2"}, R"({"key2":null})"},
      {R"({"key1":{"value":"1"},"key2":{"value":"3"}})",
       {"key1", "key2"},
       R"({"key1":1,"key2":3})"},

      // Unexpected values are ignored.
      {R"({"key1":{"value":"1","foo":"bar"}})", {"key1"}, R"({"key1":1})"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.key_values_json);
    auto result_or_error =
        TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
            helper_.get(),
            TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
            mojom::TrustedSignalsCompressionScheme::kNone,
            test::ToCborVector(base::StringPrintf(
                R"([{
                  "id": 0,
                  "keyGroupOutputs": [{
                    "tags": ["keys"],
                    "keyValues": %s
                  }]
                }])",
                test_case.key_values_json)));
    ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
    EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group1"));
    EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[0].get(),
                                    test_case.keys_to_request),
              test_case.expected_value);
  }
}

// Test all fields together, for a single partition. Main purpose of this test
// to test keys and interestGroupNames in a single response.
TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsFullyPopulated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInterestGroupUpdateIfOlderThan);
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([{
                "id": 0,
                "dataVersion": 1,
                "keyGroupOutputs": [
                  {
                    "tags": ["interestGroupNames"],
                    "keyValues": {
                      "group1": {
                        "value": "{
                          \"priorityVector\": {\"signal1\":2},
                          \"updateIfOlderThanMs\":3
                        }"
                      }
                    }
                  },
                  {
                    "tags": ["keys"],
                    "keyValues": {
                      "key1": {"value":"\"4\""}
                    }
                  }
                ]
              }])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0}));
  EXPECT_EQ((*result_or_error)[0]->GetDataVersion(), 1);

  auto* per_group1_data = (*result_or_error)[0]->GetPerGroupData("group1");
  ASSERT_TRUE(per_group1_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector{
      {"signal1", 2}};
  EXPECT_EQ(per_group1_data->priority_vector, kExpectedPriorityVector);
  EXPECT_EQ(per_group1_data->update_if_older_than, base::Milliseconds(3));

  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[0].get(),
                                  {"key1"}),
            R"({"key1":"4"})");
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsCompressionSchemeNoneButGzipped) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          GzipCompressHelper(
              test::ToCborVector(R"([{ "id": 0, "keyGroupOutputs": [] }])")));
  EXPECT_THAT(result_or_error, IsError("Failed to parse content as CBOR."));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsCompressionSchemeGzipButNotGzipped) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kGzip,
          // Ideally this would be a valid CBOR compression group, but the gzip
          // code unconditionally allocates memory based on the last 4 bytes of
          // the response, which can be quite large. End this string with 4
          // character 01's to avoid allocating too much memory.
          base::as_bytes(base::make_span("Not gzip.\x1\x1\x1\x1")));
  ASSERT_THAT(result_or_error,
              IsError("Failed to decompress content string with Gzip."));
}

TEST_F(TrustedSignalsKVv2ResponseParserTest,
       BiddingSignalsCompressionSchemeGzip) {
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kGzip,
          GzipCompressHelper(test::ToCborVector(
              R"([{ "id": 37, "dataVersion": 5, "keyGroupOutputs": [] }])")));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{37}));
  EXPECT_EQ((*result_or_error)[37]->GetDataVersion(), 5);
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, BiddingSignalsMultiplePartitions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInterestGroupUpdateIfOlderThan);
  auto result_or_error =
      TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
          helper_.get(),
          TrustedSignalsKVv2ResponseParser::SignalsType::kBidding,
          mojom::TrustedSignalsCompressionScheme::kNone,
          test::ToCborVector(
              R"([
                {
                  "id": 0,
                  "dataVersion": 1,
                  "keyGroupOutputs": [
                    {
                      "tags": ["interestGroupNames"],
                      "keyValues": {
                        "group1": {
                          "value": "{
                            \"priorityVector\": {\"signal1\":2, \"signal2\":3},
                            \"updateIfOlderThanMs\":4
                          }"
                        }
                      }
                    },
                    {
                      "tags": ["keys"],
                      "keyValues": {
                        "key1": {"value":"5"},
                        "key2": {"value":"\"6\""}
                      }
                    }
                  ]
                },
                {
                  "id": 7,
                  "dataVersion": 8,
                  "keyGroupOutputs": [
                    {
                      "tags": ["interestGroupNames"],
                      "keyValues": {
                        "group2": {
                          "value": "{
                            \"priorityVector\": {\"signal1\":9, \"signal3\":10},
                            \"updateIfOlderThanMs\":11
                          }"
                        }
                      }
                    },
                    {
                      "tags": ["keys"],
                      "keyValues": {
                        "key1": {"value":"12"},
                        "key3": {"value":"[13]"},
                      }
                    }
                  ]
                },
                {
                  "id": 14,
                  "keyGroupOutputs": []
                }
              ])"));
  ASSERT_THAT(result_or_error, PartitionsAre(std::vector<int>{0, 7, 14}));

  EXPECT_EQ((*result_or_error)[0]->GetDataVersion(), 1);
  auto* per_group_data = (*result_or_error)[0]->GetPerGroupData("group1");
  ASSERT_TRUE(per_group_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector1{
      {"signal1", 2}, {"signal2", 3}};
  EXPECT_EQ(per_group_data->priority_vector, kExpectedPriorityVector1);
  EXPECT_EQ(per_group_data->update_if_older_than, base::Milliseconds(4));
  EXPECT_FALSE((*result_or_error)[0]->GetPerGroupData("group2"));
  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[0].get(),
                                  {"key1", "key2", "key3"}),
            R"({"key1":5,"key2":"6","key3":null})");

  EXPECT_EQ((*result_or_error)[7]->GetDataVersion(), 8);
  EXPECT_FALSE((*result_or_error)[7]->GetPerGroupData("group1"));
  per_group_data = (*result_or_error)[7]->GetPerGroupData("group2");
  ASSERT_TRUE(per_group_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector2{
      {"signal1", 9}, {"signal3", 10}};
  EXPECT_EQ(per_group_data->priority_vector, kExpectedPriorityVector2);
  EXPECT_EQ(per_group_data->update_if_older_than, base::Milliseconds(11));
  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[7].get(),
                                  {"key1", "key2", "key3"}),
            R"({"key1":12,"key2":null,"key3":[13]})");

  EXPECT_FALSE((*result_or_error)[14]->GetDataVersion());
  EXPECT_FALSE((*result_or_error)[14]->GetPerGroupData("group1"));
  EXPECT_FALSE((*result_or_error)[14]->GetPerGroupData("group2"));
  EXPECT_EQ(ExtractBiddingSignals(helper_.get(), (*result_or_error)[14].get(),
                                  {"key1", "key2", "key3"}),
            R"({"key1":null,"key2":null,"key3":null})");
}

}  // namespace auction_worklet

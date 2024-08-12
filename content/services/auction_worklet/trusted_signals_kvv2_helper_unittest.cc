// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"

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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
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
const char kTrustedSignalsUrl[] = "https://url.test/";
const char kOriginFooUrl[] = "https://foo.test/";
const char kOriginBarUrl[] = "https://bar.test/";

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

// GzipCompress() doesn't support writing to a vector, only a std::string. This
// wrapper provides that capability, at the cost of an extra copy.
std::vector<std::uint8_t> GzipCompressHelper(
    const std::vector<std::uint8_t>& in) {
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

// Check trusted bidding signals' priority vector and bidding signals in json
// format with given interest group names and bididng keys.
void CheckBiddingResult(
    AuctionV8Helper* v8_helper,
    TrustedSignals::Result* result,
    const std::vector<std::string>& interest_group_names,
    const std::vector<std::string>& keys,
    const std::map<std::string, TrustedSignals::Result::PriorityVector>&
        priority_vector_map,
    const std::string& bidding_signals,
    std::optional<uint32_t> data_version) {
  for (const auto& name : interest_group_names) {
    std::optional<TrustedSignals::Result::PriorityVector>
        maybe_priority_vector = result->GetPerGroupData(name)->priority_vector;
    ASSERT_TRUE(maybe_priority_vector);
    EXPECT_EQ(priority_vector_map.at(name), *maybe_priority_vector);
  }

  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> value =
      result->GetBiddingSignals(v8_helper, context, keys);
  std::string bidding_signals_json;
  if (v8_helper->ExtractJson(context, value, /*script_timeout=*/nullptr,
                             &bidding_signals_json) !=
      AuctionV8Helper::Result::kSuccess) {
    bidding_signals_json = "JSON extraction failed.";
  }
  EXPECT_EQ(bidding_signals, bidding_signals_json);
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
  int key_id = 0x00;
  std::string public_key =
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey));
  auto request_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
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
      key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
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
    const std::optional<std::set<std::string>>& interest_group_names,
    const std::optional<std::set<std::string>>& keys,
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap&
        compression_group_result_map) {
  base::expected<
      std::map<TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex,
               scoped_refptr<TrustedSignals::Result>>,
      TrustedSignalsKVv2ResponseParser::ErrorInfo>
      result = TrustedSignalsKVv2ResponseParser::
          ParseBiddingSignalsFetchResultToResultMap(
              v8_helper.get(), interest_group_names, keys,
              compression_group_result_map);
  EXPECT_FALSE(result.has_value());

  return std::move(result.error().error_msg);
}

}  // namespace

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

TEST(TrustedSignalsKVv2RequestHelperTest,
     TrustedBiddingSignalsRequestEncoding) {
  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, GURL(kTrustedSignalsUrl), kExperimentGroupId,
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

  // Generate public key.
  int public_key_id = 0x00;
  mojom::TrustedSignalsPublicKeyPtr public_key =
      mojom::TrustedSignalsPublicKey::New(
          std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                      sizeof(kTestPublicKey)),
          public_key_id);

  std::unique_ptr<TrustedSignalsKVv2RequestHelper> helper =
      helper_builder->Build(std::move(public_key));

  std::string post_body = helper->TakePostRequestBody();

  // Decrypt post body.
  auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      public_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  EXPECT_TRUE(config.ok()) << config.status();

  auto ohttp_gateway =
      quiche::ObliviousHttpGateway::Create(
          std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                      sizeof(kTestPrivateKey)),
          config.value())
          .value();

  auto decrypt_body = ohttp_gateway.DecryptObliviousHttpRequest(
      post_body, kTrustedSignalsKVv2EncryptionRequestMediaType);
  EXPECT_TRUE(decrypt_body.ok()) << decrypt_body.status();

  auto plain_body = decrypt_body->GetPlaintextData();
  std::vector<uint8_t> body_bytes(plain_body.begin(), plain_body.end());

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

// TODO(crbug.com/337917489): When adding an identical IG, it should use the
// existing partition instead of creating a new one. After the implementation,
// the EXPECT_EQ() of second IG H should be failed.
TEST(TrustedSignalsKVv2RequestHelperTest, TrustedBiddingSignalsIsolationIndex) {
  // Add the following interest groups:
  // IG A[join_origin: foo.com, mode: group-by-origin]
  // IG B[join_origin: foo.com, mode: group-by-origin]
  // IG C[join_origin: foo.com, mode: compatibility]
  // IG D[join_origin: foo.com, mode: compatibility]
  // IG E[join_origin: bar.com, mode: compatibility]
  // IG F[join_origin: bar.com, mode: group-by-origin]
  // IG G[join_origin: bar.com, mode: compatibility]
  // IG H[join_origin: bar.com, mode: compatibility]
  // IG H, a dulplicate IG, aiming to test how request builder can handle a
  // identical IG.
  // will result the following groups: Compression: 0 -
  //    partition 0: A, B
  //    partition 1: C
  //    partition 2: D
  // Compression: 1 -
  //    partition 0: F
  //    partition 1: E
  //    partition 2: G
  //    partition 3: H
  //    partition 4: H

  std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
      helper_builder =
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              kHostName, GURL(kTrustedSignalsUrl), kExperimentGroupId,
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

// Test trusted bidding signals response parsing with gzip compressed cbor
// bytes.
TEST_F(TrustedSignalsKVv2ResponseParserTest,
       TrustedBiddingSignalsResponseParsing) {
  // User cbor.me to convert from
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

  // User cbor.me to convert from
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

  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap maybe_result_map =
      TrustedSignalsKVv2ResponseParser::
          ParseBiddingSignalsFetchResultToResultMap(
              helper_.get(), kInterestGroupNames, kKeys, fetch_result);
  EXPECT_TRUE(maybe_result_map.has_value());
  TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap result_map =
      maybe_result_map.value();
  EXPECT_EQ(result_map->size(), 3u);

  std::vector<std::string> expected_names = {"groupA", "groupB"};
  std::vector<std::string> expected_keys = {"keyA", "keyB"};
  uint32_t expected_data_version = 102;
  std::map<std::string, TrustedSignals::Result::PriorityVector>
      priority_vector_map{
          {"groupA", TrustedSignals::Result::PriorityVector{{"signalA", 1}}},
          {"groupB", TrustedSignals::Result::PriorityVector{{"signalB", 1}}}};
  std::string expected_bidding_signals =
      R"({"keyA":"valueForA","keyB":["value1ForB","value2ForB"]})";
  CheckBiddingResult(
      helper_.get(),
      result_map
          ->at(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 0))
          .get(),
      expected_names, expected_keys, priority_vector_map,
      expected_bidding_signals, expected_data_version);

  expected_names = {"groupC"};
  expected_keys = {"keyC"};
  priority_vector_map = {
      {"groupC", TrustedSignals::Result::PriorityVector{{"signalC", 1}}}};
  expected_bidding_signals = R"({"keyC":"valueForC"})";
  CheckBiddingResult(
      helper_.get(),
      result_map
          ->at(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(0, 1))
          .get(),
      expected_names, expected_keys, priority_vector_map,
      expected_bidding_signals,
      /*data_version=*/std::nullopt);

  expected_names = {"groupD"};
  expected_keys = {"keyD"};
  expected_data_version = 206;
  priority_vector_map = {
      {"groupD", TrustedSignals::Result::PriorityVector{{"signalD", 1}}}};
  expected_bidding_signals = R"({"keyD":"valueForD"})";
  CheckBiddingResult(
      helper_.get(),
      result_map
          ->at(TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(1, 2))
          .get(),
      expected_names, expected_keys, priority_vector_map,
      expected_bidding_signals, expected_data_version);
}

TEST_F(TrustedSignalsKVv2ResponseParserTest, ResponseDecryptionFailure) {
  // Failed to decrypt response body
  // Use a different ID to obtain a public key that differs from the one used in
  // `EncryptResponseBodyHelper()`.
  int key_id = 0x01;
  std::string public_key =
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey));
  auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
                    key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
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

  // Response body is not type of Map
  // CBOR: [1]
  hex_string = "8101";
  EXPECT_EQ("Response body is not type of Map.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Failed to find compression groups in response
  // CBOR:
  // {
  //   "something": "none"
  // }
  hex_string = "A169736F6D657468696E67646E6F6E65";
  EXPECT_EQ("Failed to find compression groups in response.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression groups is not type of Array
  // CBOR:
  // {
  //   "compressionGroups": 0
  // }
  hex_string = "A171636F6D7072657373696F6E47726F75707300";
  EXPECT_EQ("Compression groups is not type of Array.",
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

  // Compression group is not type of Map
  // CBOR:
  // {
  //   "compressionGroups": [0]
  // }
  hex_string = "A171636F6D7072657373696F6E47726F7570738100";
  EXPECT_EQ("Compression group is not type of Map.",
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

  // Compression group id is not type of Integer
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
  EXPECT_EQ("Compression group id is not type of Integer.",
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

  // Compression group ttl is not type of Integer
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
  EXPECT_EQ("Compression group ttl is not type of Integer.",
            GetErrorMessageFromParseResponseToSignalsFetchResult(hex_string));

  // Compression group content is not type of Byte String
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
  EXPECT_EQ("Compression group content is not type of Byte String.",
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

  // Failed to decompress content string with Gzip
  result_map[0].compression_scheme =
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip;
  // []
  hex_string = "80";
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to decompress content string with Gzip.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Set compression scheme to kNone for the rest of test cases.
  result_map[0].compression_scheme =
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone;

  // Failed to parse content to CBOR
  // Random 20 bytes hex string.
  hex_string = "666f421a72ed47aade0c63826288d5d1bbf2dc2a";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Failed to parse content to CBOR.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Content is not type of Array
  // "1"
  hex_string = "6131";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Content is not type of Array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Partition is not type of Map
  // [1]
  hex_string = "8101";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition is not type of Map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

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

  // Partition id is not type of Integer
  // [
  //   {
  //     "id": "0",
  //     "keyGroupOutputs": []
  //   }
  // ]
  hex_string = "81A262696461306F6B657947726F75704F75747075747380";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition id is not type of Integer.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

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

  // Partition key group outputs is not type of Array
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": 100
  //   }
  // ]
  hex_string = "81A2626964006F6B657947726F75704F7574707574731864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Partition key group outputs is not type of Array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // DataVersion is not type of Integer
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
  EXPECT_EQ("DataVersion is not type of Integer.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

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

  // KeyGroupOutput value is not type of Map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [100]
  //   }
  // ]
  hex_string = "81A2626964006F6B657947726F75704F757470757473811864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("KeyGroupOutput value is not type of Map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Key "tags" is missing in keyGroupOutputs map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalD\":1}}"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A1696B657956616C756573A166"
      "67726F757044A16576616C756578207B227072696F72697479566563746F72223A7B2273"
      "69676E616C44223A317D7D";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"tags\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Key "keyValues" is missing in keyGroupOutputs map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ]
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A164746167738172696E746572"
      "65737447726F75704E616D6573";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Key \"keyValues\" is missing in keyGroupOutputs map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Tags value in keyGroupOutputs map is not type of Array
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": "interestGroupNames",
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalD\":1}}"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677372696E74657265"
      "737447726F75704E616D6573696B657956616C756573A16667726F757044A16576616C75"
      "6578207B227072696F72697479566563746F72223A7B227369676E616C44223A317D7D";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Tags value in keyGroupOutputs map is not type of Array.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Tags array must only have one tag
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": ["tag1","tag2"],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalD\":1}}"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A2647461677382647461673164"
      "74616732696B657956616C756573A16667726F757044A16576616C756578207B22707269"
      "6F72697479566563746F72223A7B227369676E616C44223A317D7D";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Tags array must only have one tag.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Tag value in tags array of keyGroupOutputs map is not type of String
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [100],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalD\":1}}"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A26474616773811864696B"
      "6579"
      "56616C756573A16667726F757044A16576616C756578207B227072696F7269747956"
      "6563"
      "746F72223A7B227369676E616C44223A317D7D";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ(
      "Tag value in tags array of keyGroupOutputs map is not type of String.",
      GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
          helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Duplicate tag detected in keyGroupOutputs
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": ["interestGroupNames"],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalA\":1}}"
  //           }
  //         }
  //       },
  //       {
  //         "tags": ["interestGroupNames"],
  //         "keyValues": {
  //           "groupD": {
  //             "value": "{\"priorityVector\":{\"signalB\":1}}"
  //           }
  //         }
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747382A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C756573A16667726F757044A16576616C"
      "756578207B227072696F72697479566563746F72223A7B227369676E616C41223A317D7D"
      "A264746167738172696E74657265737447726F75704E616D6573696B657956616C756573"
      "A16667726F757044A16576616C756578207B227072696F72697479566563746F72223A7B"
      "227369676E616C42223A317D7D";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("Duplicate tag \"interestGroupNames\" detected in keyGroupOutputs.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // KeyValue value in keyGroupOutputs map is not type of Map
  // [
  //   {
  //     "id": 0,
  //     "keyGroupOutputs": [
  //       {
  //         "tags": [
  //           "interestGroupNames"
  //         ],
  //         "keyValues": 100
  //       }
  //     ]
  //   }
  // ]
  hex_string =
      "81A2626964006F6B657947726F75704F75747075747381A264746167738172696E746572"
      "65737447726F75704E616D6573696B657956616C7565731864";
  result_map[0].content.clear();
  base::HexStringToBytes(hex_string, &result_map[0].content);
  EXPECT_EQ("KeyValue value in keyGroupOutputs map is not type of Map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Value is not type of Map
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
  EXPECT_EQ("Value of \"groupA\" is not type of Map.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));

  // Failed to find key "value" in the map
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

  // Failed to read value of key "value" as type String
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

  // Failed to parse key-value string to JSON
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
  EXPECT_EQ("Failed to parse key-value string to JSON.",
            GetErrorMessageFromParseBiddingSignalsFetchResultToResultMap(
                helper_, kInterestGroupNames, kBiddingKeys, result_map));
}

}  // namespace auction_worklet

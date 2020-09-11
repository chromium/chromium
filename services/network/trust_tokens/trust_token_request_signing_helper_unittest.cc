// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time_to_iso8601.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/base/request_priority.h"
#include "net/http/structured_headers.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/test/signed_request_verification_util.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace network {

namespace {

using TrustTokenRequestSigningHelperTest = TrustTokenRequestHelperTest;

// FakeSigner returns a successful, nonempty, but meaningless, signature over
// its given signing data. It should be used for tests involving only signing,
// not verification.
class FakeSigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) override {
    return std::vector<uint8_t>{'s', 'i', 'g', 'n', 'e', 'd'};
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    NOTREACHED();
    return false;
  }
};

// IdentitySigner returns a "signature" over given signing data whose value
// equals that of the signing data. This makes verifying the signature easy:
// just check if the signature being provided equals the data it's supposed to
// be signing over.
class IdentitySigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) override {
    return std::vector<uint8_t>(data.begin(), data.end());
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    return std::equal(data.begin(), data.end(), signature.begin());
  }
};

// FailingSigner always fails the Sign and Verify options.
class FailingSigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) override {
    return base::nullopt;
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    return false;
  }
};

// Reconstructs |request|'s canonical request data, extracts the signatures from
// |request|'s Sec-Signature header, and uses the verification algorithm
// provided by the template parameter |Signer| to check that the Sec-Signature
// header's contained signatures verify.
template <typename Signer>
void ReconstructSigningDataAndAssertSignaturesVerify(
    net::URLRequest* request,
    size_t num_expected_signatures) {
  std::string error;

  std::map<std::string, std::string> verification_keys_per_issuer;
  bool success = test::ReconstructSigningDataAndVerifySignatures(
      request->url(), request->extra_request_headers(),
      base::BindRepeating([](base::span<const uint8_t> data,
                             base::span<const uint8_t> signature,
                             base::span<const uint8_t> verification_key) {
        return Signer().Verify(data, signature, verification_key);
      }),
      &error, &verification_keys_per_issuer);

  ASSERT_TRUE(success) << error;
  ASSERT_EQ(verification_keys_per_issuer.size(), num_expected_signatures);
}

// Verifies that |request| has a Sec-Signature header containing signatures and
// extracts the signature for each issuer to |signatures_out|.
void AssertHasSignaturesAndExtract(
    const net::URLRequest& request,
    std::map<std::string, std::string>* signatures_out) {
  std::string signature_header;
  ASSERT_TRUE(request.extra_request_headers().GetHeader("Sec-Signature",
                                                        &signature_header));

  base::Optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(signature_header);
  ASSERT_TRUE(maybe_dictionary);
  ASSERT_TRUE(maybe_dictionary->contains("signatures"));

  for (auto& issuer_and_params : maybe_dictionary->at("signatures").member) {
    net::structured_headers::Item& issuer_item = issuer_and_params.item;
    ASSERT_TRUE(issuer_item.is_string());

    auto signature_iterator = std::find_if(
        issuer_and_params.params.begin(), issuer_and_params.params.end(),
        [](auto& param) { return param.first == "sig"; });

    ASSERT_TRUE(signature_iterator != issuer_and_params.params.end())
        << "Missing signature";
    ASSERT_TRUE(signature_iterator->second.is_byte_sequence());
    signatures_out->emplace(issuer_item.GetString(),
                            signature_iterator->second.GetString());
  }
}

// Assert that the given signing data is a concatenation of the domain separator
// defined in TrustTokenRequestSigningHelper (initially "Trust Token v0") and a
// valid CBOR struct, and that the struct contains a field of name |field_name|;
// extract the corresponding value.
void AssertDecodesToCborAndExtractField(base::StringPiece signing_data,
                                        base::StringPiece field_name,
                                        std::string* field_value_out) {
  base::Optional<cbor::Value> parsed = cbor::Reader::Read(base::as_bytes(
      // Skip over the domain separator (e.g. "Trust Token v0").
      base::make_span(signing_data)
          .subspan(base::size(TrustTokenRequestSigningHelper::
                                  kRequestSigningDomainSeparator))));
  ASSERT_TRUE(parsed);

  const cbor::Value::MapValue& map = parsed->GetMap();
  auto it = map.find(cbor::Value(field_name));
  ASSERT_TRUE(it != map.end());
  const cbor::Value& value = it->second;
  *field_value_out = value.is_string()
                         ? value.GetString()
                         : std::string(value.GetBytestringAsString());
}

MATCHER_P(Header, name, base::StringPrintf("The header %s is present", name)) {
  return arg.extra_request_headers().HasHeader(name);
}
MATCHER_P2(Header,
           name,
           other_matcher,
           "Evaluate the given matcher on the given header, if "
           "present.") {
  std::string header;
  if (!arg.extra_request_headers().GetHeader(name, &header))
    return false;
  return Matches(other_matcher)(header);
}

SuitableTrustTokenOrigin CreateSuitableOriginOrDie(base::StringPiece spec) {
  base::Optional<SuitableTrustTokenOrigin> maybe_origin =
      SuitableTrustTokenOrigin::Create(GURL(spec));
  CHECK(maybe_origin) << "Failed to create a SuitableTrustTokenOrigin!";
  return *maybe_origin;
}

}  // namespace

TEST_F(TrustTokenRequestSigningHelperTest, WontSignIfNoRedemptionRecord) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In failure cases---in particular, in this case where none of the provided
  // issuers has a signed redemption record in storage---the signing helper
  // should return kOk but attach an empty SRR header.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
}

TEST_F(TrustTokenRequestSigningHelperTest, MergesHeaders) {
  // The signing operation should fuse and lowercase the headers from the
  // "Signed-Headers" request header and the additionalSignedHeaders Fetch
  // param.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.additional_headers_to_sign = std::vector<std::string>{"Sec-Time"};

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  my_record.set_body("SRR body");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  my_request->SetExtraRequestHeaderByName(
      "Signed-Headers", "Sec-Signed-Redemption-Record", /*overwrite=*/true);

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  std::string signed_headers_header_value;
  ASSERT_TRUE(my_request->extra_request_headers().GetHeader(
      "Signed-Headers", &signed_headers_header_value));

  // Headers should have been merged and lower-cased.
  EXPECT_THAT(base::SplitString(signed_headers_header_value, ",",
                                base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL),
              UnorderedElementsAre(StrEq("sec-time"),
                                   StrEq("sec-signed-redemption-record")));
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnUnsignableHeaderNameInSignedHeadersHeader) {
  // The signing operation should fail if there's an unsignable header (as
  // specified by TrustTokenRequestSigningHelper::kSignableRequestHeaders) in
  // the "Signed-Headers" request header or the additionalSignedHeaders Fetch
  // param; this tests the former.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  my_request->SetExtraRequestHeaderByName(
      "Signed-Headers",
      "this header name is definitely not in"
      "TrustTokenRequestSigningHelper::kSignableRequestHeaders",
      /*overwrite=*/true);

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In failure cases, the signing helper should return kOk but attach an empty
  // SRR header.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnUnsignableHeaderNameInAdditionalHeadersList) {
  // The signing operation should fail if there's an unsignable header (as
  // specified by TrustTokenRequestSigningHelper::kSignableRequestHeaders) in
  // the "Signed-Headers" request header or the additionalSignedHeaders Fetch
  // param; this tests the latter.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.additional_headers_to_sign = std::vector<std::string>{
      "this header name is definitely not in "
      "TrustTokenRequestSigningHelper::kSignableRequestHeaders"};

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
}

class TrustTokenRequestSigningHelperTestWithMockTime
    : public TrustTokenRequestSigningHelperTest {
 public:
  TrustTokenRequestSigningHelperTestWithMockTime()
      : TrustTokenRequestSigningHelperTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TrustTokenRequestSigningHelperTestWithMockTime() override = default;
};

TEST_F(TrustTokenRequestSigningHelperTestWithMockTime, ProvidesTimeHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.should_add_timestamp = true;

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  my_record.set_body("look at me, I'm an SRR body");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(
      *my_request,
      Header("Sec-Time", StrEq(base::TimeToISO8601(base::Time::Now()))));
}

// Test SRR attachment without request signing:
// - The two issuers with stored redemption records should appear in the header.
// - A third issuer without a corresponding redemption record in storage
// shouldn't appear in the header.
TEST_F(TrustTokenRequestSigningHelperTest,
       RedemptionRecordAttachmentWithoutSigning) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.should_add_timestamp = true;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kOmit;
  params.issuers.push_back(
      *SuitableTrustTokenOrigin::Create(GURL("https://second-issuer.example")));

  SignedTrustTokenRedemptionRecord first_issuer_record;
  first_issuer_record.set_body("look at me! I'm a signed redemption record");
  first_issuer_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             first_issuer_record);

  SignedTrustTokenRedemptionRecord second_issuer_record;
  second_issuer_record.set_body(
      "I'm another signed redemption record, distinct from the first");
  second_issuer_record.set_public_key("some other key");
  store->SetRedemptionRecord(params.issuers.back(), params.toplevel,
                             second_issuer_record);

  // Attempting to sign with an issuer with no redemption record in storage
  // should be fine, resulting in the issuer getting ignored.
  params.issuers.push_back(
      *SuitableTrustTokenOrigin::Create(GURL("https://third-issuer.example")));

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<IdentitySigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  std::string redemption_record_header;
  ASSERT_TRUE(my_request->extra_request_headers().GetHeader(
      "Sec-Signed-Redemption-Record", &redemption_record_header));
  std::map<SuitableTrustTokenOrigin, std::string> redemption_records_per_issuer;
  std::string error;
  ASSERT_TRUE(test::ExtractRedemptionRecordsFromHeader(
      redemption_record_header, &redemption_records_per_issuer, &error))
      << error;

  EXPECT_THAT(
      redemption_records_per_issuer,
      UnorderedElementsAre(
          Pair(CreateSuitableOriginOrDie("https://issuer.com"),
               StrEq(first_issuer_record.body())),
          Pair(CreateSuitableOriginOrDie("https://second-issuer.example"),
               StrEq(second_issuer_record.body()))));
  EXPECT_THAT(*my_request, Header("Sec-Time"));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
}

// Test a round-trip sign-and-verify with no headers.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyMinimal) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  my_record.set_body("look at me, I'm an SRR body");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  // Giving an IdentitySigner to |helper| will mean that |helper| should provide
  // its entire signing data in the request's Sec-Signature header's "sig"
  // field. ReconstructSigningDataAndAssertSignaturesVerify then reproduces
  // this canonical data's construction and checks that the reconstructed data
  // matches what |helper| produced.
  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignaturesVerify<IdentitySigner>(
          my_request.get(), /*num_expected_signatures=*/1));
}

// Test a round-trip sign-and-verify with signed headers and multiple issuers.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyWithHeaders) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel, record);
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  params.issuers.push_back(
      *SuitableTrustTokenOrigin::Create(GURL("https://second-issuer.example")));
  SignedTrustTokenRedemptionRecord other_record;
  other_record.set_body("I am a different signed token redemption record");
  other_record.set_public_key("some other key");
  store->SetRedemptionRecord(params.issuers.back(), params.toplevel,
                             other_record);

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignaturesVerify<IdentitySigner>(
          my_request.get(), /*num_expected_signatures=*/2));
}

// Test a round-trip sign-and-verify with signed headers when adding a timestamp
// header via |should_add_timestamp|.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyTimestampHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.additional_headers_to_sign = std::vector<std::string>{"sec-time"};
  params.should_add_timestamp = true;

  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel, record);

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignaturesVerify<IdentitySigner>(
          my_request.get(), /*num_expected_signatures=*/1));

  // Because we're using an IdentitySigner, each signature will have value
  // equal to the base64-encoded request signing data.
  std::map<std::string, std::string> signatures;
  ASSERT_NO_FATAL_FAILURE(
      AssertHasSignaturesAndExtract(*my_request, &signatures));
  std::string retrieved_timestamp;
  ASSERT_NO_FATAL_FAILURE(AssertDecodesToCborAndExtractField(
      signatures.begin()->second, "sec-time", &retrieved_timestamp));
}

// Test a round-trip sign-and-verify additionally signing over the destination
// eTLD+1 (signRequestData = "include").
TEST_F(TrustTokenRequestSigningHelperTest,
       SignAndVerifyWithHeadersAndDestinationUrl) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kInclude;

  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel, record);
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://sub.destination.com/path?query");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In addition to testing that the signing data equals
  // ReconstructSigningDataAndAssertSignaturesVerify's reconstruction of the
  // data, explicitly check that it contains a "destination" field with the
  // right value.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignaturesVerify<IdentitySigner>(
          my_request.get(), /*num_expected_signatures=*/1));

  // Because we're using an IdentitySigner, each signature will have value
  // equal to the base64-encoded request signing data.
  std::map<std::string, std::string> signatures;
  ASSERT_NO_FATAL_FAILURE(
      AssertHasSignaturesAndExtract(*my_request, &signatures));
  std::string retrieved_url;
  ASSERT_NO_FATAL_FAILURE(AssertDecodesToCborAndExtractField(
      signatures.begin()->second, "destination", &retrieved_url));
  ASSERT_EQ(retrieved_url, "destination.com");
}

// When signing fails, the request should have an empty
// Sec-Signed-Redemption-Record header attached, and none of the other headers
// that could potentially be added during signing.
TEST_F(TrustTokenRequestSigningHelperTest, CatchesSignatureFailure) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  my_record.set_signing_key("signing key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  params.should_add_timestamp = true;
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  // FailingSigner will fail to sign the request, so we should see the operation
  // fail.
  net::RecordingTestNetLog net_log;
  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FailingSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>(),
      net::NetLogWithSource::Make(&net_log,
                                  net::NetLogSourceType::URL_REQUEST));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
  EXPECT_THAT(*my_request, Not(Header("Sec-Time")));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_TRUE(base::ranges::any_of(
      net_log.GetEntriesWithType(
          net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING),
      [](const net::NetLogEntry& entry) {
        base::Optional<std::string> key = net::GetOptionalStringValueFromParams(
            entry, "failed_signing_params.key");
        base::Optional<std::string> issuer =
            net::GetOptionalStringValueFromParams(
                entry, "failed_signing_params.issuer");
        return key && *key == "signing key" && issuer &&
               *issuer == "https://issuer.com";
      }));
}

// Test a round-trip sign-and-verify with signed headers when adding additional
// signing data.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyAdditionalSigningData) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.possibly_unsafe_additional_signing_data =
      "some additional data to sign";

  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel, record);

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignaturesVerify<IdentitySigner>(
          my_request.get(), /*num_expected_signatures=*/1));

  // Because we're using an IdentitySigner, each signature will have value
  // equal to the base64-encoded request signing data.
  std::map<std::string, std::string> signatures;
  ASSERT_NO_FATAL_FAILURE(
      AssertHasSignaturesAndExtract(*my_request, &signatures));
  std::string retrieved_additional_signing_data;
  ASSERT_NO_FATAL_FAILURE(AssertDecodesToCborAndExtractField(
      signatures.begin()->second, "sec-trust-tokens-additional-signing-data",
      &retrieved_additional_signing_data));

  EXPECT_EQ(retrieved_additional_signing_data, "some additional data to sign");
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnOverlongAdditionalSigningData) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.possibly_unsafe_additional_signing_data =
      std::string(kTrustTokenAdditionalSigningDataMaxSizeBytes + 1, 'a');

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In failure cases, the signing helper should return kOk but attach an empty
  // SRR header.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
  EXPECT_THAT(*my_request,
              Not(Header("Sec-Trust-Tokens-Additional-Signing-Data")));
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnAdditionalSigningDataThatIsNotAValidHeaderValue) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestSigningHelper::Params params(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.possibly_unsafe_additional_signing_data = "\r";

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuers.front(), params.toplevel,
                             my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In failure cases, the signing helper should return kOk but attach an empty
  // SRR header.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
  EXPECT_THAT(*my_request,
              Not(Header("Sec-Trust-Tokens-Additional-Signing-Data")));
}

}  // namespace network

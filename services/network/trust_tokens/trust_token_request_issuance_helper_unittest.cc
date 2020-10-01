// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

#include <memory>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnNull;

using TrustTokenRequestIssuanceHelperTest = TrustTokenRequestHelperTest;
using UnblindedTokens =
    TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens;

// FixedKeyCommitmentGetter returns the provided commitment result when
// |Get| is called by the tested code.
class FixedKeyCommitmentGetter : public TrustTokenKeyCommitmentGetter {
 public:
  FixedKeyCommitmentGetter() = default;
  explicit FixedKeyCommitmentGetter(
      const url::Origin& issuer,
      mojom::TrustTokenKeyCommitmentResultPtr result)
      : issuer_(issuer), result_(std::move(result)) {}

  void Get(const url::Origin& origin,
           base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
               on_done) const override {
    EXPECT_EQ(origin, issuer_);
    std::move(on_done).Run(result_.Clone());
  }

 private:
  url::Origin issuer_;
  mojom::TrustTokenKeyCommitmentResultPtr result_;
};

base::NoDestructor<FixedKeyCommitmentGetter> g_fixed_key_commitment_getter{};

// MockCryptographer mocks out the cryptographic operations underlying Trust
// Tokens issuance.
class MockCryptographer
    : public TrustTokenRequestIssuanceHelper::Cryptographer {
 public:
  MOCK_METHOD2(Initialize,
               bool(mojom::TrustTokenProtocolVersion issuer_configured_version,
                    int issuer_configured_batch_size));
  MOCK_METHOD1(AddKey, bool(base::StringPiece key));
  MOCK_METHOD1(BeginIssuance, base::Optional<std::string>(size_t num_tokens));
  MOCK_METHOD1(
      ConfirmIssuance,
      std::unique_ptr<UnblindedTokens>(base::StringPiece response_header));
};

}  // namespace

// Check that issuance fails if it would result in too many issuers being
// configured for the issuance top-level origin.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfTooManyIssuers) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));
  auto toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/"));

  // Associate the toplevel with the cap's worth of issuers different from
  // |issuer|. (The cap is guaranteed to be quite small because of privacy
  // requirements of the Trust Tokens protocol.)
  for (int i = 0; i < kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers; ++i) {
    ASSERT_TRUE(store->SetAssociation(
        *SuitableTrustTokenOrigin::Create(
            GURL(base::StringPrintf("https://issuer%d.com/", i))),
        toplevel));
  }

  TrustTokenRequestIssuanceHelper helper(toplevel, store.get(),
                                         g_fixed_key_commitment_getter.get(),
                                         std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kResourceExhausted);
}

// Check that issuance fails if the number of tokens stored for the issuer is
// already at capacity.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfAtCapacity) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Fill up the store with tokens; issuance should fail the tokens for |issuer|
  // are at capacity.
  store->AddTokens(issuer,
                   std::vector<std::string>(kTrustTokenPerIssuerTokenCapacity),
                   /*issuing_key=*/"");

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), g_fixed_key_commitment_getter.get(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kResourceExhausted);
}

// Check that issuance fails if its key commitment request fails.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfKeyCommitmentFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Have the key commitment getter return nullptr, denoting that the key
  // commitment fetch failed.
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(issuer, nullptr);
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

// Reject if initializing the cryptography delegate fails.
TEST_F(TrustTokenRequestIssuanceHelperTest,
       RejectsIfInitializingCryptographerFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(false));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInternalError);
}

// Reject if one of the keys in the commitment is malformed.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfAddingKeyFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(false));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

// Reject if there's an error getting blinded, unsigned tokens from BoringSSL.
TEST_F(TrustTokenRequestIssuanceHelperTest,
       RejectsIfGettingBlindedTokensFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  // Return nullopt, denoting an error, when the issuance helper requests
  // blinded, unsigned tokens.
  EXPECT_CALL(*cryptographer, BeginIssuance(_)).WillOnce(Return(base::nullopt));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  // This is an internal error because creating blinded tokens is a
  // cryptographic operation not dependent on the inputs provided by the client
  // or the protocol state.
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInternalError);
}

// Check that the issuance helper sets the Sec-Trust-Token and
// Sec-Trust-Token-Version headers on the outgoing request.
TEST_F(TrustTokenRequestIssuanceHelperTest, SetsRequestHeaders) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  // The result of providing blinded, unsigned tokens should be the exact value
  // of the Sec-Trust-Token header attached to the request.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  std::string attached_header;
  EXPECT_TRUE(request->extra_request_headers().GetHeader(
      kTrustTokensSecTrustTokenHeader, &attached_header));
  EXPECT_EQ(attached_header, "this string contains some blinded tokens");

  std::string attached_version_header;
  EXPECT_TRUE(request->extra_request_headers().GetHeader(
      kTrustTokensSecTrustTokenVersionHeader, &attached_version_header));
  EXPECT_EQ(attached_version_header, "TrustTokenV1");
}

// Check that the issuance helper sets the LOAD_BYPASS_CACHE flag on the
// outgoing request.
TEST_F(TrustTokenRequestIssuanceHelperTest, SetsLoadFlag) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  // The result of providing blinded, unsigned tokens should be the exact value
  // of the Sec-Trust-Token header attached to the request.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);
  EXPECT_TRUE(request->load_flags() & net::LOAD_BYPASS_CACHE);
}

// Check that the issuance helper rejects responses lacking the Sec-Trust-Token
// response header.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfResponseOmitsHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kBadResponse);
}

// Check that the issuance helper handles an issuance response rejected by the
// underlying cryptographic library.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfResponseIsUnusable) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));
  // Fail the "confirm issuance" step of validating the server's response
  // within the underlying cryptographic library.
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_)).WillOnce(ReturnNull());

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(
      kTrustTokensSecTrustTokenHeader,
      "response from issuer (this value will be ignored, since "
      "Cryptographer::ConfirmResponse is mocked out)");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kBadResponse);

  // Verify that Finalize correctly stripped the response header.
  EXPECT_FALSE(
      response_head->headers->HasHeader(kTrustTokensSecTrustTokenHeader));
}

// Check that, when preconditions are met and the underlying cryptographic steps
// successfully complete, the begin/finalize methods succeed.
TEST_F(TrustTokenRequestIssuanceHelperTest, Success) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_))
      .WillOnce(Return(ByMove(std::make_unique<UnblindedTokens>())));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(
      kTrustTokensSecTrustTokenHeader,
      "response from issuer (this value will be ignored, since "
      "Cryptographer::ConfirmResponse is mocked out)");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // Verify that Finalize correctly stripped the response header.
  EXPECT_FALSE(
      response_head->headers->HasHeader(kTrustTokensSecTrustTokenHeader));
}

// Check that a successful Begin call associates the issuer with the issuance
// toplevel origin.
TEST_F(TrustTokenRequestIssuanceHelperTest, AssociatesIssuerWithToplevel) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully begun, the issuer and the toplevel
  // should be associated.
  EXPECT_TRUE(store->IsAssociated(issuer, *SuitableTrustTokenOrigin::Create(
                                              GURL("https://toplevel.com/"))));
}

// Check that a successful end-to-end Begin/Finalize flow stores the obtained
// trust tokens in the trust token store.
TEST_F(TrustTokenRequestIssuanceHelperTest, StoresObtainedTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New("key", /*expiry=*/base::Time()));
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  // Have the Trust Tokens issuance conclude by the underlying cryptographic
  // library returning one signed, unblinded token associated with the same
  // returned from the key commitment.
  auto unblinded_tokens = std::make_unique<UnblindedTokens>();
  unblinded_tokens->body_of_verifying_key = "key";
  unblinded_tokens->tokens.push_back("a signed, unblinded token");

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_))
      .WillOnce(Return(ByMove(std::move((unblinded_tokens)))));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(
      kTrustTokensSecTrustTokenHeader,
      "response from issuer (this value will be ignored, since "
      "Cryptographer::ConfirmResponse is mocked out)");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully finished, the trust tokens parsed from
  // the server response should be in the store.
  auto match_all_keys =
      base::BindRepeating([](const std::string&) { return true; });
  EXPECT_THAT(
      store->RetrieveMatchingTokens(issuer, std::move(match_all_keys)),
      ElementsAre(Property(&TrustToken::body, "a signed, unblinded token")));
}

TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsUnsuitableInsecureIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), g_fixed_key_commitment_getter.get(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("http://insecure-issuer.com/");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestIssuanceHelperTest,
       RejectsUnsuitableNonHttpNonHttpsIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), g_fixed_key_commitment_getter.get(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("file:///non-https-issuer.txt");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestIssuanceHelperTest, RespectsMaximumBatchsize) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New("key", /*expiry=*/base::Time()));
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV1;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize + 1);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      issuer, std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));

  // The batch size should be clamped to the configured maximum.
  EXPECT_CALL(*cryptographer,
              BeginIssuance(kMaximumTrustTokenIssuanceBatchSize))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), getter.get(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);
}

}  // namespace network

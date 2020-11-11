// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

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
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

using TrustTokenRequestRedemptionHelperTest = TrustTokenRequestHelperTest;

using ::testing::_;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;

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

// MockCryptographer mocks out the cryptographic operations
// underlying Trust Tokens redemption.
class MockCryptographer
    : public TrustTokenRequestRedemptionHelper::Cryptographer {
 public:
  MOCK_METHOD2(Initialize,
               bool(mojom::TrustTokenProtocolVersion issuer_configured_version,
                    int issuer_configured_batch_size));

  MOCK_METHOD3(
      BeginRedemption,
      base::Optional<std::string>(TrustToken token,
                                  base::StringPiece verification_key,
                                  const url::Origin& top_level_origin));

  MOCK_METHOD1(ConfirmRedemption,
               base::Optional<std::string>(base::StringPiece response_header));
};

class FakeKeyPairGenerator
    : public TrustTokenRequestRedemptionHelper::KeyPairGenerator {
 public:
  bool Generate(std::string*, std::string*) override { return true; }
};

class FailingKeyPairGenerator
    : public TrustTokenRequestRedemptionHelper::KeyPairGenerator {
 public:
  bool Generate(std::string*, std::string*) override { return false; }
};

class MockKeyPairGenerator
    : public TrustTokenRequestRedemptionHelper::KeyPairGenerator {
 public:
  MockKeyPairGenerator(const std::string& signing,
                       const std::string& verification)
      : signing_(signing), verification_(verification) {}
  bool Generate(std::string* s, std::string* v) override {
    s->swap(signing_);
    v->swap(verification_);
    return true;
  }

 private:
  std::string signing_;
  std::string verification_;
};

}  // namespace

// Check that redemption fails if it would result in too many issuers being
// configured for the redemption top-level origin.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfTooManyIssuers) {
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

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kResourceExhausted);
}

// Check that redemption fails if its key commitment request fails.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfKeyCommitmentFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  // Have the key commitment getter return nullptr, denoting that the key
  // commitment fetch failed.
  FixedKeyCommitmentGetter getter(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")), nullptr);
  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &getter,
      std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

// Check that redemption fails with kResourceExhausted if there are no trust
// tokens stored for the (issuer, top-level origin) pair.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfNoTokensToRedeem) {
  // Establish the following state:
  // * Initialize an _empty_ trust token store.
  // * Successfully return from the key commitment query.
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      mojom::TrustTokenKeyCommitmentResult::New());

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kResourceExhausted);
}

// Check that redemption fails with kInternalError if there's an error during
// initializing the cryptography delegate.
TEST_F(TrustTokenRequestRedemptionHelperTest,
       RejectsIfInitializingCryptographerFails) {
  // Establish the following state:
  // * Initialize an _empty_ trust token store.
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Configure the cryptographer to fail to encode the redemption request.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(false));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInternalError);
}

// Check that redemption fails with kInternalError if there's an error during
// encoding of the request header.
TEST_F(TrustTokenRequestRedemptionHelperTest,
       RejectsIfAddingRequestHeaderFails) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Configure the cryptographer to fail to encode the redemption request.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(Return(base::nullopt));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInternalError);
}

// Check that redemption fails with kInternalError if there's an error during
// generating the signing/validation key pair.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfKeyPairGenerationFails) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Provide |helper| a FailingKeyPairGenerator to ensure that key pair
  // generation does not succeed.
  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FailingKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  // Since key pair generation failed, |Begin| should have failed and reported
  // an internal error.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInternalError);
}

namespace {

class TrustTokenBeginRedemptionPostconditionsTest
    : public TrustTokenRequestRedemptionHelperTest {
 public:
  void SetUp() override {
    // Establish the following state:
    // * One key commitment returned from the key commitment registry, with one
    // key, with body "".
    // * One token stored corresponding to the key "" (this will be the token
    // that the redemption request redeems; its key needs to match the key
    // commitment's key so that it does not get evicted from storage after the
    // key commitment is updated to reflect the key commitment result).
    std::unique_ptr<TrustTokenStore> store =
        TrustTokenStore::CreateForTesting();
    store->AddTokens(
        *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
        std::vector<std::string>{"a token"},
        /*key=*/"");

    auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
    key_commitment_result->keys.push_back(
        mojom::TrustTokenVerificationKey::New());
    key_commitment_result->protocol_version =
        mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
    key_commitment_result->id = 1;
    key_commitment_result->batch_size =
        static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
    auto getter = std::make_unique<FixedKeyCommitmentGetter>(
        *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
        std::move(key_commitment_result));

    // The value obtained from the cryptographer should be the exact
    // Sec-Trust-Token header attached to the request.
    auto cryptographer = std::make_unique<MockCryptographer>();
    EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
        .WillOnce(
            Return(std::string("this string contains a redemption request")));

    TrustTokenRequestRedemptionHelper helper(
        *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
        mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
        std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

    request_ = MakeURLRequest("https://issuer.com/");
    request_->set_initiator(
        *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

    mojom::TrustTokenOperationStatus result =
        ExecuteBeginOperationAndWaitForResult(&helper, request_.get());

    EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  }

 protected:
  std::unique_ptr<net::URLRequest> request_;
};

}  // namespace

// Check that the redemption helper sets the Sec-Trust-Token and
// Sec-Trust-Token-Version headers on the outgoing request.
TEST_F(TrustTokenBeginRedemptionPostconditionsTest, SetsHeaders) {
  std::string attached_header;
  EXPECT_TRUE(request_->extra_request_headers().GetHeader(
      kTrustTokensSecTrustTokenHeader, &attached_header));
  EXPECT_TRUE(request_->extra_request_headers().GetHeader(
      kTrustTokensSecTrustTokenVersionHeader, &attached_header));
}

// Check that the redemption helper sets the LOAD_BYPASS_CACHE flag on the
// outgoing request.
TEST_F(TrustTokenBeginRedemptionPostconditionsTest, SetsLoadFlag) {
  EXPECT_TRUE(request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

// Check that the redemption helper rejects responses lacking the
// Sec-Trust-Token response header.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfResponseOmitsHeader) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(
          Return(std::string("this string contains a redemption request")));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  // Verify that the first half of the redemption operation succeeded, as a
  // precursor to testing that the second half works, too.
  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  // Add an empty list of response headers. In particular, this is missing the
  // Sec-Trust-Token redemption response header.
  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");

  // As a consequence, |Finalize| should fail.
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kBadResponse);
}

// Check that the redemption helper handles a redemption response rejected by
// the underlying cryptographic library.
TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsIfResponseIsUnusable) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Configure the cryptographer to reject the response header by returning
  // nullopt on ConfirmRedemption.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(
          Return(std::string("this string contains a redemption request")));
  EXPECT_CALL(*cryptographer, ConfirmRedemption(_))
      .WillOnce(Return(base::nullopt));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  // Verify that the first half of the redemption operation succeeded, as a
  // precursor to testing that the second half works, too.
  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(kTrustTokensSecTrustTokenHeader, "");

  // Since the cryptographer rejected the response header by returning nullopt
  // on ConfirmRedemption, expect to fail with kBadResponse.
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kBadResponse);

  // Processing the response should have stripped the header.
  EXPECT_FALSE(
      response_head->headers->HasHeader(kTrustTokensSecTrustTokenHeader));
}

// Check that, when preconditions are met and the underlying cryptographic steps
// successfully complete, the begin/finalize methods succeed.
TEST_F(TrustTokenRequestRedemptionHelperTest, Success) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Configure the cryptographer to succeed on both the outbound and inbound
  // halves of the operation.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(Return("well-formed redemption request"));
  EXPECT_CALL(*cryptographer, ConfirmRedemption(_))
      .WillOnce(Return("a successfully-extracted RR"));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  // Since this test is testing the behavior on handling the response after
  // successfully constructing a redemption request, sanity check that the setup
  // has correctly caused constructing the request so succeed.
  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(kTrustTokensSecTrustTokenHeader, "");

  // After a successfully constructed request, when the response is well-formed
  // and the delegate accepts the response, Finalize should succeed.
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // Processing the response should have stripped the header.
  EXPECT_FALSE(
      response_head->headers->HasHeader(kTrustTokensSecTrustTokenHeader));
}

// Check that a successful Begin call associates the issuer with the redemption
// toplevel origin.
TEST_F(TrustTokenRequestRedemptionHelperTest, AssociatesIssuerWithToplevel) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  // Configure the cryptographer to succeed on both the outbound and inbound
  // halves of the operation.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(Return("well-formed redemption request"));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<FakeKeyPairGenerator>(), std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  // Since this test is testing the behavior on handling the response after
  // successfully constructing a redemption request, sanity check that the setup
  // has correctly caused constructing the request so succeed.
  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully begun, the issuer and the toplevel
  // should be associated.
  EXPECT_TRUE(store->IsAssociated(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/"))));
}

// Check that a successful end-to-end Begin/Finalize flow stores the obtained
// redemption record (and associated key pair) in the trust token store.
TEST_F(TrustTokenRequestRedemptionHelperTest, StoresObtainedRedemptionRecord) {
  // Establish the following state:
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"token verification key");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "token verification key", /*expiry=*/base::Time::Max()));
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(Return("well-formed redemption request"));
  EXPECT_CALL(*cryptographer, ConfirmRedemption(_))
      .WillOnce(Return("a successfully-extracted RR"));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(), &*getter,
      std::make_unique<MockKeyPairGenerator>("signing key", "verification key"),
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(kTrustTokensSecTrustTokenHeader, "");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully finished, the RR parsed from the
  // server response should be in the store.
  EXPECT_THAT(
      store->RetrieveNonstaleRedemptionRecord(
          *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
          *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/"))),
      Optional(AllOf(
          Property(&TrustTokenRedemptionRecord::body,
                   "a successfully-extracted RR"),
          Property(&TrustTokenRedemptionRecord::public_key, "verification key"),
          Property(&TrustTokenRedemptionRecord::token_verification_key,
                   "token verification key"),
          Property(&TrustTokenRedemptionRecord::signing_key, "signing key"))));
}

// Check that a "refresh" refresh mode is rejected unless the request's
// initiating origin is the issuer origin.
TEST_F(TrustTokenRequestRedemptionHelperTest,
       RejectsRefreshFromNonissuerOrigin) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kRefresh, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  // kRefresh should mean that redemption fails on requests with
  // non-issuer initiators.
  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://not-issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

// On a redemption operation parameterized by kUseCachedRr, if there's an RR
// present in the store for the given issuer-toplevel pair, the request should
// return early with kAlreadyExists.
TEST_F(TrustTokenRequestRedemptionHelperTest, RedemptionRecordCacheHit) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->SetRedemptionRecord(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      TrustTokenRedemptionRecord());

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kAlreadyExists);
}

// Check that a successful end-to-end Begin/Finalize flow with kRefresh
// overwrites the previously stored redemption record (and associated key pair)
// in the trust token store.
TEST_F(TrustTokenRequestRedemptionHelperTest,
       SuccessUsingRefreshRrOverwritesStoredRr) {
  // Establish the following state:
  // * A redemption record is already stored for the issuer, toplevel pair at
  // hand.
  // * One key commitment returned from the key commitment registry, with one
  // key, with body "".
  // * One token stored corresponding to the key "" (this will be the token
  // that the redemption request redeems; its key needs to match the key
  // commitment's key so that it does not get evicted from storage after the key
  // commitment is updated to reflect the key commitment result).
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  store->SetRedemptionRecord(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      TrustTokenRedemptionRecord());
  store->AddTokens(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      std::vector<std::string>{"a token"},
      /*key=*/"");

  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(
      mojom::TrustTokenVerificationKey::New());
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV2Pmb;
  key_commitment_result->id = 1;
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  auto getter = std::make_unique<FixedKeyCommitmentGetter>(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com")),
      std::move(key_commitment_result));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginRedemption(_, _, _))
      .WillOnce(Return("well-formed redemption request"));
  EXPECT_CALL(*cryptographer, ConfirmRedemption(_))
      .WillOnce(Return("a successfully-extracted RR"));

  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kRefresh, store.get(), &*getter,
      std::make_unique<MockKeyPairGenerator>("signing key", "verification key"),
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  // Set the initiator in order to be able to use refresh mode
  // kRefresh.
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, request.get());
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(kTrustTokensSecTrustTokenHeader, "");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully finished, the RR parsed from the
  // server response should be in the store.
  EXPECT_THAT(
      store->RetrieveNonstaleRedemptionRecord(
          *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
          *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/"))),
      Optional(AllOf(
          Property(&TrustTokenRedemptionRecord::body,
                   "a successfully-extracted RR"),
          Property(&TrustTokenRedemptionRecord::public_key, "verification key"),
          Property(&TrustTokenRedemptionRecord::signing_key, "signing key"))));
}

TEST_F(TrustTokenRequestRedemptionHelperTest, RejectsUnsuitableInsecureIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("http://insecure-issuer.com/");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestRedemptionHelperTest,
       RejectsUnsuitableNonHttpNonHttpsIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kUseCached, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("file:///non-https-issuer.txt");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestRedemptionHelperTest, RequiresInitiatorForRrRefresh) {
  // Refresh mode "refresh" requires that the request's initiator to
  // be same-origin with the request's issuer. Test that, in this case, the
  // redemption helper requires that the request have an initiator.
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestRedemptionHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mojom::TrustTokenRefreshPolicy::kRefresh, store.get(),
      &*g_fixed_key_commitment_getter, std::make_unique<FakeKeyPairGenerator>(),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.example");
  request->set_initiator(base::nullopt);

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
}

}  // namespace network

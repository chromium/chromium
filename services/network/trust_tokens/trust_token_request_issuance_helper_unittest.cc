// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/trust_token_test_util.h"
#include "services/network/trust_tokens/operating_system_matching.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
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
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::WithArgs;

using UnblindedTokens =
    TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens;

class TrustTokenRequestIssuanceHelperTest : public TrustTokenRequestHelperTest {
 public:
  explicit TrustTokenRequestIssuanceHelperTest()
      : TrustTokenRequestHelperTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

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
  MOCK_METHOD1(AddKey, bool(std::string_view key));
  MOCK_METHOD1(BeginIssuance, std::optional<std::string>(size_t num_tokens));
  MOCK_METHOD1(
      ConfirmIssuance,
      std::unique_ptr<UnblindedTokens>(std::string_view response_header));
};

class MockExpiryDelegate : public TrustTokenStore::RecordExpiryDelegate {
 public:
  MOCK_METHOD3(IsRecordExpired,
               bool(const TrustTokenRedemptionRecord&,
                    const base::TimeDelta&,
                    const SuitableTrustTokenOrigin&));
};

class MockTrustTokenPersister : public TrustTokenPersister {
 public:
  MOCK_METHOD1(GetIssuerConfig,
               std::unique_ptr<TrustTokenIssuerConfig>(
                   (const SuitableTrustTokenOrigin& issuer)));
  MOCK_METHOD1(GetToplevelConfig,
               std::unique_ptr<TrustTokenToplevelConfig>(
                   const SuitableTrustTokenOrigin& toplevel));
  MOCK_METHOD2(GetIssuerToplevelPairConfig,
               std::unique_ptr<TrustTokenIssuerToplevelPairConfig>(
                   const SuitableTrustTokenOrigin& issuer,
                   const SuitableTrustTokenOrigin& toplevel));
  MOCK_METHOD2(SetIssuerConfig,
               void(const SuitableTrustTokenOrigin& issuer,
                    std::unique_ptr<TrustTokenIssuerConfig> config));
  MOCK_METHOD2(SetToplevelConfig,
               void(const SuitableTrustTokenOrigin& toplevel,
                    std::unique_ptr<TrustTokenToplevelConfig> config));
  MOCK_METHOD3(
      SetIssuerToplevelPairConfig,
      void(const SuitableTrustTokenOrigin& issuer,
           const SuitableTrustTokenOrigin& toplevel,
           std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config));
  MOCK_METHOD2(DeleteIssuerConfig,
               bool(PSTKeyMatcher key_matcher, PSTTimeMatcher time_matcher));
  MOCK_METHOD1(DeleteToplevelConfig, bool(PSTKeyMatcher key_matcher));
  MOCK_METHOD2(DeleteIssuerToplevelPairConfig,
               bool(PSTKeyMatcher key_matcher, PSTTimeMatcher time_matcher));
  MOCK_METHOD0(GetStoredTrustTokenCounts,
               base::flat_map<SuitableTrustTokenOrigin, int>());
  MOCK_METHOD0(GetRedemptionRecords, IssuerRedemptionRecordMap());
};

class MockTrustTokenStore : public TrustTokenStore {
 public:
  MockTrustTokenStore(std::unique_ptr<TrustTokenPersister> p,
                      std::unique_ptr<RecordExpiryDelegate> ed)
      : TrustTokenStore(std::move(p), std::move(ed)) {}

  MOCK_METHOD1(RecordIssuance, void(const SuitableTrustTokenOrigin& issuer));
};

// Returns a key commitment result with reasonable values for all parameters.
mojom::TrustTokenKeyCommitmentResultPtr ReasonableKeyCommitmentResult() {
  auto key_commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment_result->keys.push_back(mojom::TrustTokenVerificationKey::New(
      "key", /*expiry=*/base::Time::Max()));
  key_commitment_result->batch_size =
      static_cast<int>(kMaximumTrustTokenIssuanceBatchSize);
  key_commitment_result->protocol_version =
      mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb;
  key_commitment_result->id = 1;
  return key_commitment_result;
}

FixedKeyCommitmentGetter* ReasonableKeyCommitmentGetter() {
  static base::NoDestructor<FixedKeyCommitmentGetter>
      reasonable_key_commitment_getter{
          *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
          ReasonableKeyCommitmentResult()};
  return reasonable_key_commitment_getter.get();
}

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

  TrustTokenRequestIssuanceHelper helper(
      toplevel, store.get(), g_fixed_key_commitment_getter.get(), std::nullopt,
      std::nullopt, std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kResourceLimited);
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
      store.get(), g_fixed_key_commitment_getter.get(), std::nullopt,
      std::nullopt, std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kResourceLimited);
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
      store.get(), getter.get(), std::nullopt, std::nullopt,
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")));

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kMissingIssuerKeys);
}

// Reject if initializing the cryptography delegate fails.
TEST_F(TrustTokenRequestIssuanceHelperTest,
       RejectsIfInitializingCryptographerFails) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(false));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(false));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  // Return nullopt, denoting an error, when the issuance helper requests
  // blinded, unsigned tokens.
  EXPECT_CALL(*cryptographer, BeginIssuance(_)).WillOnce(Return(std::nullopt));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  // This is an internal error because creating blinded tokens is a
  // cryptographic operation not dependent on the inputs provided by the client
  // or the protocol state.
  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInternalError);
}

// Check that the issuance helper sets the Sec-Private-State-Token and
// Sec-Private-State-Token-Crypto-Version headers on the outgoing request.
TEST_F(TrustTokenRequestIssuanceHelperTest, SetsRequestHeaders) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // The result of providing blinded, unsigned tokens should be the exact value
  // of the Sec-Private-State-Token header attached to the request.
  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  EXPECT_THAT(
      request->extra_request_headers().GetHeader(
          kTrustTokensSecTrustTokenHeader),
      Optional(std::string("this string contains some blinded tokens")));

  EXPECT_THAT(request->extra_request_headers().GetHeader(
                  kTrustTokensSecTrustTokenVersionHeader),
              Optional(std::string("PrivateStateTokenV3PMB")));
}

// Check that the issuance helper rejects responses lacking the
// Sec-Private-State-Token response header.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfResponseOmitsHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

// Check that the issuance helper correctly handles responses bearing empty
// Sec-Private-State-Token headers, which represent "success but no tokens
// issued".
TEST_F(TrustTokenRequestIssuanceHelperTest, TreatsEmptyHeaderAsSuccess) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(kTrustTokensSecTrustTokenHeader, "");
  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // After the operation has successfully finished, the store should still
  // contain no tokens for the issuer.
  auto match_all_keys =
      base::BindRepeating([](const std::string&) { return true; });
  EXPECT_THAT(store->RetrieveMatchingTokens(issuer, std::move(match_all_keys)),
              IsEmpty());
}

// Check that the issuance helper handles an issuance response rejected by the
// underlying cryptographic library.
TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsIfResponseIsUnusable) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

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
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

  // Have the Trust Tokens issuance conclude by the underlying cryptographic
  // library returning one signed, unblinded token associated with the same
  // returned from the key commitment.
  auto unblinded_tokens = std::make_unique<UnblindedTokens>();
  unblinded_tokens->body_of_verifying_key =
      ReasonableKeyCommitmentResult()->keys.front()->body;
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
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

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

// Check that the issuance helper ignores the
// Sec-Private-State-Token-Clear-Data header.
TEST_F(TrustTokenRequestIssuanceHelperTest, ClearDataHeaderIgnored) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  // isser1 must be "https://issuer.com/", this is hard coded to
  // ReasonableKeyCommitmentGetter
  SuitableTrustTokenOrigin issuer1 =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Add tokens that will be discarded with Sec-Private-State-Token-Clear-Data
  // response.
  store->AddTokens(issuer1, std::vector<std::string>{"token1", "token2"},
                   "key");

  auto unblinded_tokens = std::make_unique<UnblindedTokens>();
  unblinded_tokens->body_of_verifying_key =
      ReasonableKeyCommitmentResult()->keys.front()->body;
  unblinded_tokens->tokens.push_back("token3");

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(Return(std::string("this string contains some masked tokens")));
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_))
      .WillOnce(Return(ByMove(std::move((unblinded_tokens)))));

  // ReasonableKeyCommitmentGetter is for issuer1
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  // request is from issuer1
  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer1);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  auto response_head = mojom::URLResponseHead::New();
  response_head->headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
  response_head->headers->SetHeader(
      kTrustTokensSecTrustTokenHeader,
      "response from issuer (this value will be ignored, since "
      "Cryptographer::ConfirmResponse is mocked out)");
  // Add clear data response header.
  response_head->headers->SetHeader("Sec-Private-State-Token-Clear-Data",
                                    "all");

  EXPECT_EQ(ExecuteFinalizeAndWaitForResult(&helper, response_head.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // Trust token parsed from the server response should be in the store.
  auto match_all_keys =
      base::BindRepeating([](const std::string&) { return true; });
  EXPECT_THAT(store->RetrieveMatchingTokens(issuer1, std::move(match_all_keys)),
              ElementsAre(Property(&TrustToken::body, "token1"),
                          Property(&TrustToken::body, "token2"),
                          Property(&TrustToken::body, "token3")));

  // The store should have old and newly issued tokens
  EXPECT_EQ(store->CountTokens(issuer1), 3);
}

TEST_F(TrustTokenRequestIssuanceHelperTest, RejectsUnsuitableInsecureIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), g_fixed_key_commitment_getter.get(), std::nullopt,
      std::nullopt, std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("http://insecure-issuer.com/");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestIssuanceHelperTest,
       RejectsUnsuitableNonHttpNonHttpsIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), g_fixed_key_commitment_getter.get(), std::nullopt,
      std::nullopt, std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("file:///non-https-issuer.txt");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(TrustTokenRequestIssuanceHelperTest, RespectsMaximumBatchsize) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

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
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);
}

// Check that attempting to issue with custom key commitments fails if custom
// key commitments are invalid.
TEST_F(TrustTokenRequestIssuanceHelperTest, BadCustomKeys) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      store.get(), g_fixed_key_commitment_getter.get(), "junk keys",
      std::nullopt, std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

// Check that a successful end-to-end Begin/Finalize flow with custom key
// commitments stores the obtained trust tokens in the trust token store.
TEST_F(TrustTokenRequestIssuanceHelperTest, CustomKeysStoresObtainedTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Have the Trust Tokens issuance conclude by the underlying cryptographic
  // library returning one signed, unblinded token associated with the same
  // returned from the key commitment.
  auto unblinded_tokens = std::make_unique<UnblindedTokens>();
  unblinded_tokens->body_of_verifying_key =
      ReasonableKeyCommitmentResult()->keys.front()->body;
  unblinded_tokens->tokens.push_back("a signed, unblinded token");

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_))
      .WillOnce(Return(ByMove(std::move((unblinded_tokens)))));

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string basic_key = base::StringPrintf(
      R"({ "PrivateStateTokenV3PMB": {
            "protocol_version": "PrivateStateTokenV3PMB", "id": 1,
            "batchsize": 5,
            "keys": {"1": { "Y": "akey", "expiry": "%s" }}
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), basic_key, std::nullopt,
      std::move(cryptographer));

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

// Check that attempting to issue with custom key commitments fails if custom
// key commitments are invalid.
TEST_F(TrustTokenRequestIssuanceHelperTest, BadCustomIssuer) {
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/")),
      store.get(), g_fixed_key_commitment_getter.get(), "junk keys",
      url::Origin::Create(GURL("http://bad-issuer.com")),
      std::make_unique<MockCryptographer>());

  auto request = MakeURLRequest("https://issuer.com/");

  EXPECT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

// Check that a successful end-to-end Begin/Finalize flow with custom key
// commitments stores the obtained trust tokens in the trust token store.
TEST_F(TrustTokenRequestIssuanceHelperTest, CustomIssuerStoresObtainedTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  SuitableTrustTokenOrigin fakeissuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://fakeissuer.com/"));

  SuitableTrustTokenOrigin goodissuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Have the Trust Tokens issuance conclude by the underlying cryptographic
  // library returning one signed, unblinded token associated with the same
  // returned from the key commitment.
  auto unblinded_tokens = std::make_unique<UnblindedTokens>();
  unblinded_tokens->body_of_verifying_key =
      ReasonableKeyCommitmentResult()->keys.front()->body;
  unblinded_tokens->tokens.push_back("a signed, unblinded token");

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));
  EXPECT_CALL(*cryptographer, ConfirmIssuance(_))
      .WillOnce(Return(ByMove(std::move((unblinded_tokens)))));

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string basic_key = base::StringPrintf(
      R"({ "PrivateStateTokenV3PMB": {
            "protocol_version": "PrivateStateTokenV3PMB", "id": 1,
            "batchsize": 5,
            "keys": {"1": { "Y": "akey", "expiry": "%s" }}
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), basic_key,
      url::Origin::Create(GURL("https://issuer.com")),
      std::move(cryptographer));

  auto request = MakeURLRequest("https://fakeissuer.com/");
  request->set_initiator(fakeissuer);

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
      store->RetrieveMatchingTokens(goodissuer, std::move(match_all_keys)),
      ElementsAre(Property(&TrustToken::body, "a signed, unblinded token")));
}

// Check that the last issuance time is recorded for a given issuer.
TEST_F(TrustTokenRequestIssuanceHelperTest, RecordsIssuanceTime) {
  std::unique_ptr<MockTrustTokenPersister> mock_persister =
      std::make_unique<MockTrustTokenPersister>();
  std::unique_ptr<MockExpiryDelegate> mock_delegate =
      std::make_unique<MockExpiryDelegate>();
  std::unique_ptr<MockTrustTokenStore> mock_store =
      std::make_unique<MockTrustTokenStore>(std::move(mock_persister),
                                            std::move(mock_delegate));
  // Make sure we are recording the issuance time when we execute begin.
  EXPECT_CALL(*mock_store, RecordIssuance(_)).Times(1);

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      mock_store.get(), ReasonableKeyCommitmentGetter(), std::nullopt,
      std::nullopt, std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);
}

// Check that the issuance time is updated after it is recorded
// a second time.
TEST_F(TrustTokenRequestIssuanceHelperTest, UpdatesIssuanceTime) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com/"));

  // Record a token issuance, and advance the clock 1 second. Last issuance
  // should be 1 second ago.
  store->RecordIssuance(issuer);
  auto delta = base::Seconds(1);
  env_.AdvanceClock(delta);
  EXPECT_THAT(store->TimeSinceLastIssuance(issuer), Optional(delta));

  auto cryptographer = std::make_unique<MockCryptographer>();
  EXPECT_CALL(*cryptographer, Initialize(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, AddKey(_)).WillOnce(Return(true));
  EXPECT_CALL(*cryptographer, BeginIssuance(_))
      .WillOnce(
          Return(std::string("this string contains some blinded tokens")));

  TrustTokenRequestIssuanceHelper helper(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com/")),
      store.get(), ReasonableKeyCommitmentGetter(), std::nullopt, std::nullopt,
      std::move(cryptographer));

  auto request = MakeURLRequest("https://issuer.com/");
  request->set_initiator(issuer);

  ASSERT_EQ(ExecuteBeginOperationAndWaitForResult(&helper, request.get()),
            mojom::TrustTokenOperationStatus::kOk);

  // Ensure the issuance time was updated (0 seconds since last issuance).
  EXPECT_THAT(store->TimeSinceLastIssuance(issuer), Optional(base::Seconds(0)));
}

}  // namespace network

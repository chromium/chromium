// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_query_answerer.h"

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

class TestTrustTokenKeyCommitmentGetter
    : virtual public SynchronousTrustTokenKeyCommitmentGetter {
 public:
  explicit TestTrustTokenKeyCommitmentGetter(
      std::vector<std::string> const& keys)
      : result_(mojom::TrustTokenKeyCommitmentResult::New()) {
    for (auto const& ki : keys) {
      auto key = mojom::TrustTokenVerificationKey::New();
      key->body = ki;
      result_->keys.push_back(std::move(key));
    }
  }

  mojom::TrustTokenKeyCommitmentResultPtr GetSync(
      const url::Origin& origin) const override {
    return result_->Clone();
  }

 private:
  mojom::TrustTokenKeyCommitmentResultPtr result_;
};

TEST(TrustTokenQueryAnswerer, TokenQueryHandlesInsecureIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store, key_commitment_getter.get());

  mojom::HasTrustTokensResultPtr result;

  // Providing an insecure issuer origin should make the operation fail.
  answerer->HasTrustTokens(
      url::Origin::Create(GURL("http://issuer.com")),
      base::BindLambdaForTesting(
          [&](mojom::HasTrustTokensResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST(TrustTokenQueryAnswerer, TokenQueryHandlesNonHttpNonHttpsIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store, key_commitment_getter.get());

  mojom::HasTrustTokensResultPtr result;

  // Providing a secure but non-HTTP(S) issuer origin should make the operation
  // fail.
  answerer->HasTrustTokens(
      url::Origin::Create(GURL("file:///hello.txt")),
      base::BindLambdaForTesting(
          [&](mojom::HasTrustTokensResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST(TrustTokenQueryAnswerer, TokenQueryHandlesFailureToAssociateIssuer) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  // This max-number-of-issuer limit is expected to be quite small (as of
  // writing, 2).
  for (int i = 0; i < kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers; ++i) {
    ASSERT_TRUE(store->SetAssociation(
        *SuitableTrustTokenOrigin::Create(
            GURL(base::StringPrintf("https://issuer%d.com", i))),
        kToplevel));
  }

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  mojom::HasTrustTokensResultPtr result;

  // Since there's no capacity to associate the issuer with the top-level
  // origin, the operation should fail.
  answerer->HasTrustTokens(
      url::Origin::Create(GURL("https://issuer.com")),
      base::BindLambdaForTesting(
          [&](mojom::HasTrustTokensResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kResourceLimited);
}

TEST(TrustTokenQueryAnswerer, TokenQuerySuccessWithNoTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  mojom::HasTrustTokensResultPtr result;

  answerer->HasTrustTokens(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasTrustTokensResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_trust_tokens);

  // The query should have associated the issuer with the top-level origin.
  EXPECT_TRUE(raw_store->IsAssociated(kIssuer, kToplevel));
}

TEST(TrustTokenQueryAnswerer, TokenQuerySuccessWithTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::string issuing_key = "issuing key";
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{issuing_key});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // Populate the store, giving the issuer a key commitment for the key "issuing
  // key" and a token issued with that key.
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token"}, issuing_key);

  mojom::HasTrustTokensResultPtr result;

  answerer->HasTrustTokens(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasTrustTokensResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_TRUE(result->has_trust_tokens);
}

TEST(TrustTokenQueryAnswerer, TokenQuerySuccessWithNoTokensAllKeysAreInvalid) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::vector<std::string> invalid_issuing_keys = {"key1", "key2"};
  const std::vector<std::string> valid_issuing_keys = {"key3"};

  // create test commitment getter with the valid key
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(valid_issuing_keys);
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // store tokens with invalid keys
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token1"},
                       invalid_issuing_keys[0]);
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token2", "token3"},
                       invalid_issuing_keys[1]);

  mojom::HasTrustTokensResultPtr result;

  // Answerer should return no tokens. It should also prune the three tokens
  // stored in pending_store with invalid keys.
  answerer->HasTrustTokens(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasTrustTokensResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_trust_tokens);
  EXPECT_EQ(raw_store->CountTokens(kIssuer), 0);
}

TEST(TrustTokenQueryAnswerer, TokenQuerySuccessWithTokensSomeKeysAreInvalid) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::vector<std::string> invalid_issuing_keys = {"key1", "key2"};
  const std::vector<std::string> valid_issuing_keys = {"key3", "key4"};

  // create test commitment getter with the valid keys
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(valid_issuing_keys);
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // store three tokens with invalid issuing keys
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token1"},
                       invalid_issuing_keys[0]);
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token2", "token3"},
                       invalid_issuing_keys[1]);
  // store two tokens with valid issuing keys
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token4"},
                       valid_issuing_keys[0]);
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token5"},
                       valid_issuing_keys[1]);

  mojom::HasTrustTokensResultPtr result;

  answerer->HasTrustTokens(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasTrustTokensResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_TRUE(result->has_trust_tokens);
  // token4 and token5 are in store. token1, token2 and token3 are pruned
  EXPECT_EQ(raw_store->CountTokens(kIssuer), 2);
}

TEST(TrustTokenQueryAnswerer,
     TokenQuerySuccessWithNoTokensNoCommitmentsForIssuer) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer1 =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer1.com"));
  const SuitableTrustTokenOrigin kIssuer2 =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer2.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::vector<std::string> issuing_keys = {"key1"};

  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(issuing_keys);
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // store tokens with issuer 1
  raw_store->AddTokens(kIssuer1, std::vector<std::string>{"token1", "token2"},
                       issuing_keys[0]);

  mojom::HasTrustTokensResultPtr result;

  // ask whether issuer 2 has any tokens
  answerer->HasTrustTokens(
      kIssuer2, base::BindLambdaForTesting(
                    [&](mojom::HasTrustTokensResultPtr obtained_result) {
                      result = std::move(obtained_result);
                    }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_trust_tokens);
  // token1 and token2 stored with issuer 1 are in store.
  EXPECT_EQ(raw_store->CountTokens(kIssuer1), 2);
}

TEST(TrustTokenQueryAnswerer, RedemptionQueryHandlesInsecureIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store, key_commitment_getter.get());

  mojom::HasRedemptionRecordResultPtr result;

  // Providing an insecure issuer origin should make the operation fail.
  answerer->HasRedemptionRecord(
      url::Origin::Create(GURL("http://issuer.com")),
      base::BindLambdaForTesting(
          [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST(TrustTokenQueryAnswerer,
     RedemptionQueryHandlesNonHttpNonHttpsIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store, key_commitment_getter.get());

  mojom::HasRedemptionRecordResultPtr result;

  // Providing a secure but non-HTTP(S) issuer origin should make the operation
  // fail.
  answerer->HasRedemptionRecord(
      url::Origin::Create(GURL("file:///hello.txt")),
      base::BindLambdaForTesting(
          [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST(TrustTokenQueryAnswerer, RedemptionQueryHandlesFailureToAssociateIssuer) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  // This max-number-of-issuer limit is expected to be quite small (as of
  // writing, 2).
  for (int i = 0; i < kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers; ++i) {
    ASSERT_TRUE(store->SetAssociation(
        *SuitableTrustTokenOrigin::Create(
            GURL(base::StringPrintf("https://issuer%d.com", i))),
        kToplevel));
  }

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  mojom::HasRedemptionRecordResultPtr result;

  // Since there's no capacity to associate the issuer with the top-level
  // origin, the operation should fail.
  answerer->HasRedemptionRecord(
      url::Origin::Create(GURL("https://issuer.com")),
      base::BindLambdaForTesting(
          [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
            result = std::move(obtained_result);
          }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_redemption_record);
}

TEST(TrustTokenQueryAnswerer,
     RedemptionQuerySuccessWithNoTokensNoRedemptionRecord) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{"issuing key"});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  mojom::HasRedemptionRecordResultPtr result;

  answerer->HasRedemptionRecord(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_redemption_record);
}

TEST(TrustTokenQueryAnswerer,
     RedemptionQuerySuccessWithTokensNoRedemptionRecord) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::string issuing_key = "issuing key";
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{issuing_key});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // Populate the store, giving the issuer a key commitment for the key "issuing
  // key" and a token issued with that key.
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token"}, issuing_key);

  mojom::HasRedemptionRecordResultPtr result;

  answerer->HasRedemptionRecord(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_FALSE(result->has_redemption_record);
}

TEST(TrustTokenQueryAnswerer,
     RedemptionQuerySuccessWithTokensWithRedemptionRecord) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  const std::string issuing_key = "issuing key";
  auto key_commitment_getter =
      std::make_unique<TestTrustTokenKeyCommitmentGetter>(
          std::vector<std::string>{issuing_key});
  auto answerer = std::make_unique<TrustTokenQueryAnswerer>(
      kToplevel, &pending_store, key_commitment_getter.get());

  // Populate the store, giving the issuer a key commitment for the key "issuing
  // key" and a token issued with that key.
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token"}, issuing_key);
  // Set association of issuer and top level origin
  ASSERT_TRUE(raw_store->SetAssociation(kIssuer, kToplevel));

  auto redemption_record = network::TrustTokenRedemptionRecord();
  redemption_record.set_body("rr_body");
  redemption_record.set_token_verification_key("rr_token_verification_key");
  redemption_record.set_lifetime(12345);
  raw_store->SetRedemptionRecord(kIssuer, kToplevel, redemption_record);

  mojom::HasRedemptionRecordResultPtr result;

  answerer->HasRedemptionRecord(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasRedemptionRecordResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_TRUE(result->has_redemption_record);
}

}  // namespace network

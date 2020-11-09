// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/has_trust_tokens_answerer.h"

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

TEST(HasTrustTokensAnswerer, HandlesInsecureIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto answerer = std::make_unique<HasTrustTokensAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store);

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

TEST(HasTrustTokensAnswerer, HandlesNonHttpNonHttpsIssuerOrigin) {
  PendingTrustTokenStore pending_store;
  auto answerer = std::make_unique<HasTrustTokensAnswerer>(
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com")),
      &pending_store);

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

TEST(HasTrustTokensAnswerer, HandlesFailureToAssociateIssuer) {
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
  auto answerer =
      std::make_unique<HasTrustTokensAnswerer>(kToplevel, &pending_store);

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
  EXPECT_EQ(result->status,
            mojom::TrustTokenOperationStatus::kResourceExhausted);
}

TEST(HasTrustTokensAnswerer, SuccessWithNoTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));

  auto answerer =
      std::make_unique<HasTrustTokensAnswerer>(kToplevel, &pending_store);

  mojom::HasTrustTokensResultPtr result;

  // Since there's no capacity to associate the issuer with the top-level
  // origin, the operation should fail.
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

TEST(HasTrustTokensAnswerer, SuccessWithTokens) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();

  const SuitableTrustTokenOrigin kIssuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  const SuitableTrustTokenOrigin kToplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(std::move(store));
  auto answerer =
      std::make_unique<HasTrustTokensAnswerer>(kToplevel, &pending_store);

  // Populate the store, giving the issuer a key commitment for the key "issuing
  // key" and a token issued with that key.
  raw_store->AddTokens(kIssuer, std::vector<std::string>{"token"},
                       "issuing key");

  mojom::HasTrustTokensResultPtr result;

  // Since there's no capacity to associate the issuer with the top-level
  // origin, the operation should fail.
  answerer->HasTrustTokens(
      kIssuer, base::BindLambdaForTesting(
                   [&](mojom::HasTrustTokensResultPtr obtained_result) {
                     result = std::move(obtained_result);
                   }));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->status, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_TRUE(result->has_trust_tokens);
}

}  // namespace network

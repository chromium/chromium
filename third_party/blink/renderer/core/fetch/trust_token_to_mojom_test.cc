// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"

#include <memory>

#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token_version.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(TrustTokenToMojomTest, Issuance) {
  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRequest);
  pt->setVersion(V8PrivateTokenVersion::Enum::k1);

  network::mojom::blink::TrustTokenParamsPtr params =
      network::mojom::blink::TrustTokenParams::New();
  DummyExceptionStateForTesting e;
  EXPECT_TRUE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, {.issuance_enabled = true, .redemption_enabled = true}, &e,
      params.get()));
  EXPECT_EQ(params->operation,
            network::mojom::blink::TrustTokenOperationType::kIssuance);
}

TEST(TrustTokenToMojomTest, IssuanceDenied) {
  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRequest);
  pt->setVersion(V8PrivateTokenVersion::Enum::k1);

  auto params = network::mojom::blink::TrustTokenParams::New();
  DummyExceptionStateForTesting e;
  EXPECT_FALSE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, {.issuance_enabled = false, .redemption_enabled = true}, &e,
      params.get()));
  EXPECT_TRUE(e.HadException());
  EXPECT_EQ(e.CodeAs<DOMExceptionCode>(), DOMExceptionCode::kNotAllowedError);
}

TEST(TrustTokenToMojomTest, Redemption) {
  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRedemption);
  pt->setVersion(V8PrivateTokenVersion::Enum::k1);

  network::mojom::blink::TrustTokenParamsPtr params =
      network::mojom::blink::TrustTokenParams::New();
  DummyExceptionStateForTesting e;
  EXPECT_TRUE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, {.issuance_enabled = true, .redemption_enabled = true}, &e,
      params.get()));
  EXPECT_EQ(params->operation,
            network::mojom::blink::TrustTokenOperationType::kRedemption);
}

TEST(TrustTokenToMojomTest, RedemptionDenied) {
  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRedemption);
  pt->setVersion(V8PrivateTokenVersion::Enum::k1);

  auto params = network::mojom::blink::TrustTokenParams::New();
  DummyExceptionStateForTesting e;
  EXPECT_FALSE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, {.issuance_enabled = true, .redemption_enabled = false}, &e,
      params.get()));
  EXPECT_TRUE(e.HadException());
  EXPECT_EQ(e.CodeAs<DOMExceptionCode>(), DOMExceptionCode::kNotAllowedError);
}

}  // namespace
}  // namespace blink

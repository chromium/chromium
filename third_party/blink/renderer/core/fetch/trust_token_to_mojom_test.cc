// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"

#include <memory>

#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_private_token_version.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "v8-initialization.h"
#include "v8-isolate.h"

namespace blink {
namespace {

TEST(TrustTokenToMojomTest, GoldenPath) {
  V8TestingScope scope;

  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRequest);
  pt->setVersion(V8PrivateTokenVersion::Enum::k1);
  auto params = network::mojom::blink::TrustTokenParams::New();
  ExceptionState& exception_state = scope.GetExceptionState();
  EXPECT_TRUE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, scope.GetExecutionContext(), &exception_state, params.get()));
}

TEST(TrustTokenToMojomTest, BadVersion) {
  V8TestingScope scope;

  PrivateToken* pt = PrivateToken::Create();
  pt->setOperation(V8OperationType::Enum::kTokenRequest);
  pt->setVersion(static_cast<V8PrivateTokenVersion::Enum>(2));
  auto params = network::mojom::blink::TrustTokenParams::New();
  ExceptionState& exception_state = scope.GetExceptionState();
  EXPECT_FALSE(ConvertTrustTokenToMojomAndCheckPermissions(
      *pt, scope.GetExecutionContext(), &exception_state, params.get()));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), static_cast<int>(ESErrorType::kRangeError));
}

}  // namespace
}  // namespace blink

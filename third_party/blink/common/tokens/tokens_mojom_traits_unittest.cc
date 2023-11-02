// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/tokens/tokens_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/tokens/tokens.mojom.h"

namespace mojo {

namespace {

// Tests round-trip serialization for the given TokenType of a given
// MultiTokenType.
template <typename MultiTokenType, typename MojomType, typename TokenType>
void ExpectSerializationWorks() {
  base::UnguessableToken raw_token = base::UnguessableToken::Create();
  TokenType typed_token(raw_token);
  MultiTokenType multi_token(typed_token);

  MultiTokenType deserialized;
  EXPECT_TRUE(::mojo::test::SerializeAndDeserialize<MojomType>(multi_token,
                                                               deserialized));
  EXPECT_TRUE(deserialized.template Is<TokenType>());
  EXPECT_EQ(multi_token, deserialized);
  EXPECT_EQ(multi_token.template GetAs<TokenType>(),
            deserialized.template GetAs<TokenType>());
  EXPECT_EQ(raw_token, deserialized.value());
}

}  // namespace

TEST(FrameTokenTest, MojomTraits) {
  ExpectSerializationWorks<blink::FrameToken, blink::mojom::FrameToken,
                           blink::LocalFrameToken>();
  ExpectSerializationWorks<blink::FrameToken, blink::mojom::FrameToken,
                           blink::RemoteFrameToken>();
}

TEST(WorkerTokenTest, MojomTraits) {
  ExpectSerializationWorks<blink::WorkerToken, blink::mojom::WorkerToken,
                           blink::DedicatedWorkerToken>();
  ExpectSerializationWorks<blink::WorkerToken, blink::mojom::WorkerToken,
                           blink::ServiceWorkerToken>();
  ExpectSerializationWorks<blink::WorkerToken, blink::mojom::WorkerToken,
                           blink::SharedWorkerToken>();
}

TEST(WorkletTokenTest, MojomTraits) {
  ExpectSerializationWorks<blink::WorkletToken, blink::mojom::WorkletToken,
                           blink::AnimationWorkletToken>();
  ExpectSerializationWorks<blink::WorkletToken, blink::mojom::WorkletToken,
                           blink::AudioWorkletToken>();
  ExpectSerializationWorks<blink::WorkletToken, blink::mojom::WorkletToken,
                           blink::LayoutWorkletToken>();
  ExpectSerializationWorks<blink::WorkletToken, blink::mojom::WorkletToken,
                           blink::PaintWorkletToken>();
}

TEST(ExecutionContextTokenTest, MojomTraits) {
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::LocalFrameToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::DedicatedWorkerToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::ServiceWorkerToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::SharedWorkerToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::AnimationWorkletToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::AudioWorkletToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::LayoutWorkletToken>();
  ExpectSerializationWorks<blink::ExecutionContextToken,
                           blink::mojom::ExecutionContextToken,
                           blink::PaintWorkletToken>();
}

}  // namespace mojo

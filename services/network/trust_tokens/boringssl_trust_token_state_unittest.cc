// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_state.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Boringssl allows batch size that can fit into two bytes.
constexpr int kSmallestPositiveInvalidBatchSize = (1 << 16);
constexpr int kLargestAllowedBatchSize = kSmallestPositiveInvalidBatchSize - 1;

TEST(BoringsslTrustTokenState, CreateSuccess) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(/*issuer_configured_batch_size = */ 7);
  EXPECT_THAT(state, testing::NotNull());
}

TEST(BoringsslTrustTokenState, CreateSuccessZeroBatchSize) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(/*issuer_configured_batch_size = */ 0);
  EXPECT_THAT(state, testing::NotNull());
}

TEST(BoringsslTrustTokenState, CreateSuccessWithLargestAllowedBatchSize) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(kLargestAllowedBatchSize);
  EXPECT_THAT(state, testing::NotNull());
}

TEST(BoringsslTrustTokenState, CreateFailureInvalidLargeBatchSize) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(kSmallestPositiveInvalidBatchSize);
  EXPECT_THAT(state, testing::IsNull());
}

TEST(BoringsslTrustTokenState, CreateFailureWithNegativeBatchSize) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(/*issuer_configured_batch_size = */ -1);
  EXPECT_THAT(state, testing::IsNull());
}

TEST(BoringsslTrustTokenState, GetSuccess) {
  std::unique_ptr<BoringsslTrustTokenState> state =
      BoringsslTrustTokenState::Create(/*issuer_configured_batch_size = */ 23);
  ASSERT_THAT(state, testing::NotNull());
  EXPECT_THAT(state->Get(), testing::NotNull());
}

}  // namespace

}  // namespace network

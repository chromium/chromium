// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/pending_trust_token_store.h"

#include "base/test/bind.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(PendingTrustTokenStore, UsesTheStoreThatItWasGiven) {
  PendingTrustTokenStore pending_store;
  bool success = false;
  auto store = TrustTokenStore::CreateForTesting();
  TrustTokenStore* raw_store = store.get();
  pending_store.ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* received_store) {
        success = (received_store == raw_store);
      }));
  pending_store.OnStoreReady(std::move(store));
  EXPECT_TRUE(success);
}

TEST(PendingTrustTokenStore, ExecutesEnqueuedOperationsInFifoOrder) {
  PendingTrustTokenStore pending_store;
  int flag = 0;
  // Pick a couple operations that don't commute.
  pending_store.ExecuteOrEnqueue(base::BindLambdaForTesting(
      [&](TrustTokenStore* received_store) { flag += 4; }));
  pending_store.ExecuteOrEnqueue(base::BindLambdaForTesting(
      [&](TrustTokenStore* received_store) { flag /= 3; }));
  pending_store.OnStoreReady(TrustTokenStore::CreateForTesting());

  EXPECT_EQ(flag, 1);
}

TEST(PendingTrustTokenStore, ExecutesOperationEnqueuedAfterStoreIsReady) {
  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(TrustTokenStore::CreateForTesting());

  int flag = 0;
  pending_store.ExecuteOrEnqueue(base::BindLambdaForTesting(
      [&](TrustTokenStore* received_store) { flag = 1; }));
  EXPECT_EQ(flag, 1);
}

TEST(PendingTrustTokenStore, ExecutesOperationEnqueuedWhileExecutingOperation) {
  PendingTrustTokenStore pending_store;
  pending_store.OnStoreReady(TrustTokenStore::CreateForTesting());

  bool inner_op_success = false;
  bool outer_op_success = false;
  pending_store.ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* received_store) {
        pending_store.ExecuteOrEnqueue(base::BindLambdaForTesting(
            [&](TrustTokenStore*) { inner_op_success = true; }));
        outer_op_success = true;
      }));
  EXPECT_TRUE(inner_op_success);
  EXPECT_TRUE(outer_op_success);
}

TEST(PendingTrustTokenStore, ExecutesOperationEnqueuedDuringOnStoreReady) {
  PendingTrustTokenStore pending_store;

  bool inner_op_success = false;
  bool outer_op_success = false;
  pending_store.ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* received_store) {
        pending_store.ExecuteOrEnqueue(base::BindLambdaForTesting(
            [&](TrustTokenStore*) { inner_op_success = true; }));
        outer_op_success = true;
      }));

  // The inner ExecuteOrEnqueue call will take place while the
  // pre-OnStoreReady operation is being executed, distinguishing this case from
  // the prior test.
  pending_store.OnStoreReady(TrustTokenStore::CreateForTesting());
  EXPECT_TRUE(inner_op_success);
  EXPECT_TRUE(outer_op_success);
}

}  // namespace network

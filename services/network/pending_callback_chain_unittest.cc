// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/pending_callback_chain.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

int SyncReturn(int result, net::CompletionOnceCallback callback) {
  return result;
}

class AsyncReturn {
 public:
  int Wait(net::CompletionOnceCallback callback) {
    callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }

  void Finish(int result) { std::move(callback_).Run(result); }

 private:
  net::CompletionOnceCallback callback_;
};

TEST(PendingCallbackChainTest, SingleSyncResultOk) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(SyncReturn(net::OK, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::OK);
}

TEST(PendingCallbackChainTest, SingleSyncResultError) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(
      SyncReturn(net::ERR_INVALID_ARGUMENT, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_INVALID_ARGUMENT);
}

TEST(PendingCallbackChainTest, SingleAsyncResultOk) {
  int result = net::ERR_UNEXPECTED;
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([&](int r) { result = r; }));

  AsyncReturn async;
  chain->AddResult(async.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async.Finish(net::OK);
  EXPECT_EQ(result, net::OK);
  EXPECT_EQ(chain->GetResult(), net::OK);
}

TEST(PendingCallbackChainTest, SingleAsyncResultError) {
  int result = net::ERR_UNEXPECTED;
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([&](int r) { result = r; }));

  AsyncReturn async;
  chain->AddResult(async.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async.Finish(net::ERR_INVALID_ARGUMENT);
  EXPECT_EQ(result, net::ERR_INVALID_ARGUMENT);
  EXPECT_EQ(chain->GetResult(), net::ERR_INVALID_ARGUMENT);
}

TEST(PendingCallbackChainTest, MultipleSyncResultOk) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(SyncReturn(net::OK, chain->CreateCallback()));
  chain->AddResult(SyncReturn(net::OK, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::OK);
}

TEST(PendingCallbackChainTest, MultipleSyncResultError) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(
      SyncReturn(net::ERR_INVALID_ARGUMENT, chain->CreateCallback()));
  chain->AddResult(SyncReturn(net::OK, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_INVALID_ARGUMENT);
}

TEST(PendingCallbackChainTest, MultipleSyncSameError) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(
      SyncReturn(net::ERR_INVALID_ARGUMENT, chain->CreateCallback()));
  chain->AddResult(
      SyncReturn(net::ERR_INVALID_ARGUMENT, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_INVALID_ARGUMENT);
}

TEST(PendingCallbackChainTest, MultipleSyncResultDifferentError) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([](int r) {}));

  chain->AddResult(
      SyncReturn(net::ERR_INVALID_ARGUMENT, chain->CreateCallback()));
  chain->AddResult(
      SyncReturn(net::ERR_CONNECTION_FAILED, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_FAILED);
}

TEST(PendingCallbackChainTest, SyncAndAsyncResultOk) {
  int result = net::ERR_UNEXPECTED;
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([&](int r) { result = r; }));

  AsyncReturn async;
  chain->AddResult(async.Wait(chain->CreateCallback()));
  chain->AddResult(SyncReturn(net::OK, chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async.Finish(net::OK);
  EXPECT_EQ(result, net::OK);
}

TEST(PendingCallbackChainTest, MultipleAsyncResultOk) {
  int result = net::ERR_UNEXPECTED;
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([&](int r) { result = r; }));

  AsyncReturn async1;
  chain->AddResult(async1.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  AsyncReturn async2;
  chain->AddResult(async2.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async1.Finish(net::OK);
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async2.Finish(net::OK);
  EXPECT_EQ(result, net::OK);
  EXPECT_EQ(chain->GetResult(), net::OK);
}

TEST(PendingCallbackChainTest, MultipleAsyncResultError) {
  int result = net::ERR_UNEXPECTED;
  auto chain = base::MakeRefCounted<PendingCallbackChain>(
      base::BindLambdaForTesting([&](int r) { result = r; }));

  AsyncReturn async1;
  chain->AddResult(async1.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  AsyncReturn async2;
  chain->AddResult(async2.Wait(chain->CreateCallback()));
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async1.Finish(net::OK);
  EXPECT_EQ(chain->GetResult(), net::ERR_IO_PENDING);

  async2.Finish(net::ERR_INVALID_ARGUMENT);
  EXPECT_EQ(result, net::ERR_INVALID_ARGUMENT);
  EXPECT_EQ(chain->GetResult(), net::ERR_INVALID_ARGUMENT);
}

}  // namespace
}  // namespace network

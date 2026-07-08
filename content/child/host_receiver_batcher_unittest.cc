// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/host_receiver_batcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

mojo::GenericPendingReceiver MakeReceiver() {
  mojo::MessagePipe pipe;
  return mojo::GenericPendingReceiver("test.Interface",
                                      std::move(pipe.handle0));
}

class HostReceiverBatcherTest : public testing::Test {
 protected:
  std::unique_ptr<HostReceiverBatcher> MakeBatcher() {
    return std::make_unique<HostReceiverBatcher>(
        base::BindLambdaForTesting(
            [this](std::vector<mojo::GenericPendingReceiver> receivers) {
              batch_sizes_.push_back(receivers.size());
            }),
        task_environment_.GetMainThreadTaskRunner());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::vector<size_t> batch_sizes_;
};

// Receivers added before the flush task runs are coalesced into one batch.
TEST_F(HostReceiverBatcherTest, CoalescesBurst) {
  base::HistogramTester histogram_tester;
  auto batcher = MakeBatcher();
  batcher->AddReceiver(MakeReceiver());
  batcher->AddReceiver(MakeReceiver());
  batcher->AddReceiver(MakeReceiver());

  // Nothing is sent until the posted flush runs.
  EXPECT_TRUE(batch_sizes_.empty());
  ASSERT_TRUE(base::test::RunUntil([&] { return !batch_sizes_.empty(); }));

  ASSERT_EQ(batch_sizes_.size(), 1u);
  EXPECT_EQ(batch_sizes_[0], 3u);
  histogram_tester.ExpectUniqueSample("ChildProcess.BindHostReceiver.BatchSize",
                                      3, 1);
}

// Receivers added in separate task turns flush as separate batches.
TEST_F(HostReceiverBatcherTest, SeparateTurnsSeparateBatches) {
  base::HistogramTester histogram_tester;
  auto batcher = MakeBatcher();
  batcher->AddReceiver(MakeReceiver());
  ASSERT_TRUE(base::test::RunUntil([&] { return batch_sizes_.size() == 1u; }));
  batcher->AddReceiver(MakeReceiver());
  batcher->AddReceiver(MakeReceiver());
  ASSERT_TRUE(base::test::RunUntil([&] { return batch_sizes_.size() == 2u; }));

  EXPECT_EQ(batch_sizes_[0], 1u);
  EXPECT_EQ(batch_sizes_[1], 2u);
  histogram_tester.ExpectTotalCount("ChildProcess.BindHostReceiver.BatchSize",
                                    2);
  histogram_tester.ExpectBucketCount("ChildProcess.BindHostReceiver.BatchSize",
                                     1, 1);
  histogram_tester.ExpectBucketCount("ChildProcess.BindHostReceiver.BatchSize",
                                     2, 1);
}

// Clear() drops buffered receivers; the pending flush then sends nothing.
TEST_F(HostReceiverBatcherTest, ClearDropsPending) {
  base::HistogramTester histogram_tester;
  auto batcher = MakeBatcher();
  batcher->AddReceiver(MakeReceiver());
  batcher->AddReceiver(MakeReceiver());
  batcher->Clear();

  // The flush was already posted (before Clear); drain it via a sentinel task
  // queued behind it, then confirm it sent nothing.
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(batch_sizes_.empty());
  histogram_tester.ExpectTotalCount("ChildProcess.BindHostReceiver.BatchSize",
                                    0);
}

}  // namespace
}  // namespace content

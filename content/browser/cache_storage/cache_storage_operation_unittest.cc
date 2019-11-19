// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_operation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kSlowOperation[] =
    "ServiceWorkerCache.CacheStorage.Scheduler.IsOperationSlow";

class TestTask {
 public:
  TestTask() = default;
  ~TestTask() = default;
  void Run() { callback_count_++; }

  int callback_count() const { return callback_count_; }

 protected:
  int callback_count_ = 0;
};

}  // namespace

class CacheStorageOperationTest : public testing::Test {
 protected:
  CacheStorageOperationTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner()) {
    operation_ = std::make_unique<CacheStorageOperation>(
        base::BindOnce(&TestTask::Run, base::Unretained(&task_)),
        /* id = */ 0, CacheStorageSchedulerClient::kStorage,
        CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kTest,
        CacheStorageSchedulerPriority::kNormal, mock_task_runner_);
  }

  base::HistogramTester histogram_tester_;
  TestTask task_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<CacheStorageOperation> operation_;
};

TEST_F(CacheStorageOperationTest, OperationFast) {
  operation_->Run();
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, task_.callback_count());
  histogram_tester_.ExpectTotalCount(kSlowOperation, 0);

  // Finish the operation.
  operation_.reset();

  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, task_.callback_count());
  histogram_tester_.ExpectTotalCount(kSlowOperation, 1);
  histogram_tester_.ExpectBucketCount(kSlowOperation, false, 1);
}

TEST_F(CacheStorageOperationTest, OperationSlow) {
  operation_->Run();
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, task_.callback_count());
  histogram_tester_.ExpectTotalCount(kSlowOperation, 0);

  // The operation takes 10 seconds.
  mock_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));
  mock_task_runner_->RunUntilIdle();
  histogram_tester_.ExpectTotalCount(kSlowOperation, 1);
  histogram_tester_.ExpectBucketCount(kSlowOperation, true, 1);

  // Finish the operation.
  operation_.reset();
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, task_.callback_count());
  histogram_tester_.ExpectTotalCount(kSlowOperation, 1);
  histogram_tester_.ExpectBucketCount(kSlowOperation, true, 1);
}

}  // namespace content

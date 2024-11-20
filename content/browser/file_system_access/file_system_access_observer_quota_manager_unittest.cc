// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

using UsageChangeResult =
    FileSystemAccessObserverQuotaManager::UsageChangeResult;

class FileSystemAccessObserverQuotaManagerTest : public testing::Test {
 public:
  FileSystemAccessObserverQuotaManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    // TODO(crbug.com/338457523): Pass mock watcher manager and add a test case
    // for destructor.
    observer_quota_manager_ =
        base::MakeRefCounted<FileSystemAccessObserverQuotaManager>(
            blink::StorageKey::CreateFromStringForTesting(
                "https://example.com/test"),
            nullptr);
  }

  void TearDown() override { observer_quota_manager_.reset(); }

 protected:
  scoped_refptr<FileSystemAccessObserverQuotaManager> observer_quota_manager_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FileSystemAccessObserverQuotaManagerTest, OnUsageChange) {
  observer_quota_manager_->SetQuotaLimitForTesting(10);

  // There are two observation groups under the same storage key calling
  // usage change method. All is expected to succeeds except for the observation
  // group 2's call on 6 -> 8 due to unavailable quota.
  //    Observation group 1: 0 -> 4 -> 0
  //    Observation group 2: 0 -> 6 -> 8
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 0);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/4),
      UsageChangeResult::kOk);
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 4);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/6),
      UsageChangeResult::kOk);
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 10);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/6, /*new_usage=*/8),
      UsageChangeResult::kQuotaUnavailable);
  // Total usage should subtract old usage only, but not add new usage if
  // it is `kQuotaUnavailable`.
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 4);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/4, /*new_usage=*/0),
      UsageChangeResult::kOk);
  // The new total usage should not go below 0, even though old usage to
  // subtract is larger than the previous total usage.
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 0);
}

}  // namespace content

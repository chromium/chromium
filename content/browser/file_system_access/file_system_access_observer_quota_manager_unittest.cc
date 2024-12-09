// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
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
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

  FileSystemAccessWatcherManager& watcher_manager() const {
    return manager_->watcher_manager();
  }

  void TearDown() override {
    manager_.reset();
    file_system_context_.reset();
    chrome_blob_context_.reset();
    EXPECT_TRUE(dir_.Delete());
  }

  void ExpectHistogramsOnQuotaManagerDestruction(
      size_t highmark_usage,
      size_t highmark_usage_percentage,
      bool quota_exceeded) {
    histogram_tester_.ExpectUniqueSample(
        "Storage.FileSystemAccess.ObserverUsage", highmark_usage, 1);
    histogram_tester_.ExpectUniqueSample(
        "Storage.FileSystemAccess.ObserverUsageRate", highmark_usage_percentage,
        1);
    histogram_tester_.ExpectUniqueSample(
        "Storage.FileSystemAccess.ObserverUsageQuotaExceeded", quota_exceeded,
        1);
  }

 protected:
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");

  base::ScopedTempDir dir_;
  base::HistogramTester histogram_tester_;
  size_t expected_highmark_usage_histogram_ = 0;
  size_t expected_highmark_usage_percentage_histogram_ = 0;
  bool expect_quota_exceeded_histogram_ = false;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
  BrowserTaskEnvironment task_environment_;
};

TEST_F(FileSystemAccessObserverQuotaManagerTest, OnUsageChange) {
  scoped_refptr<FileSystemAccessObserverQuotaManager> observer_quota_manager_ =
      watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey);
  observer_quota_manager_->SetQuotaLimitForTesting(10);

  // There are two observation groups under the same storage key calling
  // usage change method. All is expected to succeeds except for the observation
  // group 2's call on 6 -> 8 due to unavailable quota.
  //    Observation group 1: 0 -> 4 -> 0
  //    Observation group 2: 0 -> 6 -> 8
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 0U);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/4),
      UsageChangeResult::kOk);
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 4U);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/6),
      UsageChangeResult::kOk);
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 10U);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/6, /*new_usage=*/8),
      UsageChangeResult::kQuotaUnavailable);
  // Total usage should subtract old usage only, but not add new usage if
  // it is `kQuotaUnavailable`.
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 4U);
  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/4, /*new_usage=*/0),
      UsageChangeResult::kOk);
  // The new total usage should not go below 0, even though old usage to
  // subtract is larger than the previous total usage.
  EXPECT_EQ(observer_quota_manager_->GetTotalUsageForTesting(), 0U);

  observer_quota_manager_.reset();
  // 10 usage out of 10 quota limit = 100%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/10,
                                            /*highmark_usage_percentage=*/100,
                                            /*quota_exceeded=*/true);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest, HistogramQuotaExceeded) {
  scoped_refptr<FileSystemAccessObserverQuotaManager> observer_quota_manager_ =
      watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey);
  observer_quota_manager_->SetQuotaLimitForTesting(10);

  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/11),
      UsageChangeResult::kQuotaUnavailable);

  observer_quota_manager_.reset();
  // 0 usage out of 10 quota limit = 0%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/0,
                                            /*highmark_usage_percentage=*/0,
                                            /*quota_exceeded=*/true);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest, HistogramQuotaNotExceeded) {
  scoped_refptr<FileSystemAccessObserverQuotaManager> observer_quota_manager_ =
      watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey);
  observer_quota_manager_->SetQuotaLimitForTesting(10);

  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/1),
      UsageChangeResult::kOk);

  observer_quota_manager_.reset();
  // 1 usage out of 10 quota limit = 10%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/1,
                                            /*highmark_usage_percentage=*/10,
                                            /*quota_exceeded=*/false);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest,
       QuotaManagerRemovedFromWatcherManagerOnDestruction) {
  scoped_refptr<FileSystemAccessObserverQuotaManager> observer_quota_manager_ =
      watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey);
  observer_quota_manager_->SetQuotaLimitForTesting(10);

  EXPECT_EQ(
      observer_quota_manager_->OnUsageChange(/*old_usage=*/0, /*new_usage=*/1),
      UsageChangeResult::kOk);

  EXPECT_NE(watcher_manager().GetQuotaManagerForTesting(kTestStorageKey),
            nullptr);

  observer_quota_manager_.reset();
  EXPECT_EQ(watcher_manager().GetQuotaManagerForTesting(kTestStorageKey),
            nullptr);

  // 1 usage out of 10 quota limit = 10%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/1,
                                            /*highmark_usage_percentage=*/10,
                                            /*quota_exceeded=*/false);
}

}  // namespace content

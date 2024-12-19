// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

using UsageChangeResult =
    FileSystemAccessObserverQuotaManager::UsageChangeResult;

using FileSystemObserver_Usage = ukm::builders::FileSystemObserver_Usage;

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
    // Histogram logging is expected to occur after the destruction of the
    // observer quota manager.
    auto entries = test_ukm_recorder_.GetEntriesByName(
        FileSystemObserver_Usage::kEntryName);
    EXPECT_EQ(entries.size(), 1u);
    if (highmark_usage == 0) {
      EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
          entries[0], FileSystemObserver_Usage::kHighWaterMarkName));
      EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
          entries[0], FileSystemObserver_Usage::kHighWaterMarkPercentageName));
      histogram_tester_.ExpectUniqueSample(
          "Storage.FileSystemAccess.ObserverUsage", highmark_usage, 0);
      histogram_tester_.ExpectUniqueSample(
          "Storage.FileSystemAccess.ObserverUsageRate",
          highmark_usage_percentage, 0);
    } else {
      test_ukm_recorder_.ExpectEntryMetric(
          entries[0], FileSystemObserver_Usage::kHighWaterMarkName,
          ukm::GetExponentialBucketMin(highmark_usage,
                                       FileSystemAccessObserverQuotaManager::
                                           kHighWaterMarkBucketSpacing));
      test_ukm_recorder_.ExpectEntryMetric(
          entries[0], FileSystemObserver_Usage::kHighWaterMarkPercentageName,
          highmark_usage_percentage);
      histogram_tester_.ExpectUniqueSample(
          "Storage.FileSystemAccess.ObserverUsage", highmark_usage, 1);
      histogram_tester_.ExpectUniqueSample(
          "Storage.FileSystemAccess.ObserverUsageRate",
          highmark_usage_percentage, 1);
    }
    test_ukm_recorder_.ExpectEntryMetric(
        entries[0], FileSystemObserver_Usage::kQuotaExceededName,
        quota_exceeded);
    histogram_tester_.ExpectUniqueSample(
        "Storage.FileSystemAccess.ObserverUsageQuotaExceeded", quota_exceeded,
        1);
  }

 protected:
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const ukm::SourceId kSourceId = ukm::AssignNewSourceId();

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
};

TEST_F(FileSystemAccessObserverQuotaManagerTest, OnUsageChange) {
  auto quota_limit_override = FilePathWatcher::SetQuotaLimitForTesting(10);

  {
    FileSystemAccessObserverQuotaManager::Handle observation_group1 =
        watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey,
                                                            kSourceId);
    FileSystemAccessObserverQuotaManager::Handle observation_group2 =
        watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey,
                                                            kSourceId);

    FileSystemAccessObserverQuotaManager* observer_quota_manager =
        observation_group1.GetQuotaManagerForTesting();

    ASSERT_EQ(observer_quota_manager,
              observation_group2.GetQuotaManagerForTesting());

    // The total usage should start at zero.
    EXPECT_EQ(observer_quota_manager->GetTotalUsageForTesting(), 0U);

    // Increasing the usage of an observation group, should increase the quota
    // manager's total usage by the same amount.
    EXPECT_EQ(observation_group1.OnUsageChange(4), UsageChangeResult::kOk);
    EXPECT_EQ(observer_quota_manager->GetTotalUsageForTesting(), 4U);

    // The total usage should be the sum of the observation group's usages.
    EXPECT_EQ(observation_group2.OnUsageChange(6), UsageChangeResult::kOk);
    EXPECT_EQ(observer_quota_manager->GetTotalUsageForTesting(), 10U);

    // We are already at the quota limit, so increasing it would put us over it.
    // This should remove `observation_group2`'s usage from the quota manager's
    // total usage. Leaving just `observation_group1`'s usage remaining.
    EXPECT_EQ(observation_group2.OnUsageChange(8),
              UsageChangeResult::kQuotaUnavailable);
    EXPECT_EQ(observer_quota_manager->GetTotalUsageForTesting(), 4U);

    // Decreasing the usage of an observation group, should decrease the quota
    // manager's total usage by the same amount.
    EXPECT_EQ(observation_group1.OnUsageChange(0), UsageChangeResult::kOk);
    EXPECT_EQ(observer_quota_manager->GetTotalUsageForTesting(), 0U);
  }

  // 10 usage out of 10 quota limit = 100%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/10,
                                            /*highmark_usage_percentage=*/100,
                                            /*quota_exceeded=*/true);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest, HistogramQuotaExceeded) {
  auto quota_limit_override = FilePathWatcher::SetQuotaLimitForTesting(10);

  {
    FileSystemAccessObserverQuotaManager::Handle observation_group =
        watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey,
                                                            kSourceId);

    EXPECT_EQ(observation_group.OnUsageChange(11),
              UsageChangeResult::kQuotaUnavailable);
  }

  // 0 usage out of 10 quota limit = 0%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/0,
                                            /*highmark_usage_percentage=*/0,
                                            /*quota_exceeded=*/true);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest, HistogramQuotaNotExceeded) {
  auto quota_limit_override = FilePathWatcher::SetQuotaLimitForTesting(10);

  {
    FileSystemAccessObserverQuotaManager::Handle observation_group =
        watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey,
                                                            kSourceId);

    EXPECT_EQ(observation_group.OnUsageChange(1), UsageChangeResult::kOk);
  }

  // 1 usage out of 10 quota limit = 10%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/1,
                                            /*highmark_usage_percentage=*/10,
                                            /*quota_exceeded=*/false);
}

TEST_F(FileSystemAccessObserverQuotaManagerTest,
       QuotaManagerRemovedFromWatcherManagerOnDestruction) {
  auto quota_limit_override = FilePathWatcher::SetQuotaLimitForTesting(10);

  {
    FileSystemAccessObserverQuotaManager::Handle observation_group =
        watcher_manager().GetOrCreateQuotaManagerForTesting(kTestStorageKey,
                                                            kSourceId);

    EXPECT_EQ(observation_group.OnUsageChange(1), UsageChangeResult::kOk);

    EXPECT_NE(watcher_manager().GetQuotaManagerForTesting(kTestStorageKey),
              nullptr);
  }
  EXPECT_EQ(watcher_manager().GetQuotaManagerForTesting(kTestStorageKey),
            nullptr);

  // 1 usage out of 10 quota limit = 10%
  ExpectHistogramsOnQuotaManagerDestruction(/*highmark_usage=*/1,
                                            /*highmark_usage_percentage=*/10,
                                            /*quota_exceeded=*/false);
}

}  // namespace content

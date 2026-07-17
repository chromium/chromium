// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/logical_invalidation_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/pickle.h"
#include "net/base/pickle_traits.h"
#include "net/http/http_cache.h"
#include "net/http/http_cache_invalidation_pickle_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

net::HttpCache::InvalidationFilter CreateTestFilter(
    const std::string& url_str) {
  net::HttpCache::InvalidationFilter filter;
  filter.begin_time = base::Time();
  filter.end_time = base::Time::Max();
  filter.filter_type = net::UrlFilterType::kTrueIfMatches;
  filter.origins.insert(url::Origin::Create(GURL(url_str)));
  return filter;
}

}  // namespace

class LogicalInvalidationStoreTest : public testing::Test {
 protected:
  LogicalInvalidationStoreTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = std::make_unique<LogicalInvalidationStore>(temp_dir_.GetPath(),
                                                        file_task_runner_);
  }

  void TearDown() override {
    // Ensure all background threads are idle. The TaskEnvironment destructor
    // does not do this itself. This use of RunUntilIdle() is safe as we are
    // not using it to wait for some condition to become true, just to make
    // sure no background threads are left running.
    task_environment_.RunUntilIdle();
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<LogicalInvalidationStore> store_;
  base::HistogramTester histogram_tester_;
};

TEST_F(LogicalInvalidationStoreTest, SaveAndLoad) {
  LogicalInvalidationStore::InvalidationFilterVector original_filters = {
      CreateTestFilter("https://example.com"),
      CreateTestFilter("https://google.com")};

  // 1. Save filters to disk.
  {
    base::test::TestFuture<void> save_future;
    store_->Save(original_filters, save_future.GetCallback());
    ASSERT_TRUE(save_future.Wait());
  }

  // 2. Load filters from disk and verify.
  {
    base::test::TestFuture<LogicalInvalidationStore::LoadResult,
                           LogicalInvalidationStore::InvalidationFilterVector>
        future;
    store_->Load(future.GetCallback());
    auto [result, loaded] = future.Take();
    EXPECT_EQ(LogicalInvalidationStore::LoadResult::kSuccess, result);
    EXPECT_EQ(original_filters, loaded);
  }

  // Verify histograms are correctly recorded.
  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "Net.HttpCache.LogicalInvalidation.LoadResult",
      static_cast<int>(LogicalInvalidationStore::LoadResult::kSuccess), 1);
  histogram_tester_.ExpectUniqueSample(
      "Net.HttpCache.LogicalInvalidation.LoadedFilterCount", 2, 1);
}

TEST_F(LogicalInvalidationStoreTest, LoadFileNotFound) {
  base::test::TestFuture<LogicalInvalidationStore::LoadResult,
                         LogicalInvalidationStore::InvalidationFilterVector>
      future;
  store_->Load(future.GetCallback());
  auto [result, loaded] = future.Take();
  EXPECT_EQ(LogicalInvalidationStore::LoadResult::kFileNotFound, result);
  EXPECT_TRUE(loaded.empty());

  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "Net.HttpCache.LogicalInvalidation.LoadResult",
      static_cast<int>(LogicalInvalidationStore::LoadResult::kFileNotFound), 1);
  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.LoadedFilterCount", 0);
}

TEST_F(LogicalInvalidationStoreTest, LoadCorruptFile) {
  // 1. Write garbage bytes to the persistence path.
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("invalidation_filters");
  ASSERT_TRUE(base::WriteFile(file_path, "Not a serialized pickle vector!!"));

  // 2. Load filters and verify corruption result.
  base::test::TestFuture<LogicalInvalidationStore::LoadResult,
                         LogicalInvalidationStore::InvalidationFilterVector>
      future;
  store_->Load(future.GetCallback());
  auto [result, loaded] = future.Take();
  EXPECT_EQ(LogicalInvalidationStore::LoadResult::kCorrupt, result);
  EXPECT_TRUE(loaded.empty());

  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "Net.HttpCache.LogicalInvalidation.LoadResult",
      static_cast<int>(LogicalInvalidationStore::LoadResult::kCorrupt), 1);
  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.LoadedFilterCount", 0);
}

TEST_F(LogicalInvalidationStoreTest, LoadTrailingGarbageFile) {
  // 1. Write valid serialized pickle filters followed by trailing garbage
  // bytes.
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("invalidation_filters");
  LogicalInvalidationStore::InvalidationFilterVector original_filters = {
      CreateTestFilter("https://example.com")};
  base::Pickle pickle;
  net::WriteToPickle(pickle, original_filters);
  std::vector<uint8_t> bytes(std::from_range, pickle);
  bytes.insert(bytes.end(), {'g', 'a', 'r', 'b', 'a', 'g', 'e'});
  ASSERT_TRUE(base::WriteFile(file_path, bytes));

  // 2. Load filters and verify it flags kCorrupt due to trailing garbage.
  base::test::TestFuture<LogicalInvalidationStore::LoadResult,
                         LogicalInvalidationStore::InvalidationFilterVector>
      future;
  store_->Load(future.GetCallback());
  auto [result, loaded] = future.Take();
  EXPECT_EQ(LogicalInvalidationStore::LoadResult::kCorrupt, result);
  EXPECT_TRUE(loaded.empty());

  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration", 1);
  histogram_tester_.ExpectUniqueSample(
      "Net.HttpCache.LogicalInvalidation.LoadResult",
      static_cast<int>(LogicalInvalidationStore::LoadResult::kCorrupt), 1);
  histogram_tester_.ExpectTotalCount(
      "Net.HttpCache.LogicalInvalidation.LoadedFilterCount", 0);
}

}  // namespace network

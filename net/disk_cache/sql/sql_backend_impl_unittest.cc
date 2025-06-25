// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

using testing::ElementsAre;
using testing::Pair;

// Default max cache size for tests, 10 MB.
inline constexpr int64_t kDefaultMaxBytes = 10 * 1024 * 1024;

class SqlBackendImplTest : public testing::Test {
 public:
  SqlBackendImplTest() = default;
  ~SqlBackendImplTest() override = default;

  // Sets up a temporary directory and a background task runner for each test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  std::unique_ptr<SqlBackendImpl> CreateBackendAndInit(
      int64_t max_bytes = kDefaultMaxBytes) {
    auto backend = std::make_unique<SqlBackendImpl>(
        temp_dir_.GetPath(), max_bytes, net::CacheType::DISK_CACHE);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    CHECK_EQ(future.Get(), net::OK);
    return backend;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SqlBackendImplTest, MaxFileSizeSmallMax) {
  const int64_t kMaxBytes = 10 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(), kSqlBackendMinFileSizeLimit);
}

TEST_F(SqlBackendImplTest, MaxFileSizeCalculation) {
  const int64_t kLargeMaxBytes = 100 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kLargeMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(),
            kLargeMaxBytes / kSqlBackendMaxFileRatioDenominator);
}

TEST_F(SqlBackendImplTest, CalculateSizeOfAllEntries) {
  auto backend = CreateBackendAndInit();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfAllEntries(callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CalculateSizeOfEntriesBetween) {
  auto backend = CreateBackendAndInit();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfEntriesBetween(
                base::Time(), base::Time::Max(), callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CreateIterator) {
  auto backend = CreateBackendAndInit();
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CreateIterator(), nullptr);
}

TEST_F(SqlBackendImplTest, GetStats) {
  auto backend = CreateBackendAndInit();
  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_THAT(stats, ElementsAre(Pair("Cache type", "SQL Cache")));
}

TEST_F(SqlBackendImplTest, OnExternalCacheHit) {
  auto backend = CreateBackendAndInit();
  // Should not crash.
  backend->OnExternalCacheHit("test_key");
}

}  // namespace
}  // namespace disk_cache

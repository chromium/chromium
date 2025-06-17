// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

using testing::ElementsAre;
using testing::Pair;

class SqlBackendImplTest : public testing::Test {
 public:
  SqlBackendImplTest() = default;
  ~SqlBackendImplTest() override = default;

 protected:
  std::unique_ptr<SqlBackendImpl> CreateBackend() {
    return std::make_unique<SqlBackendImpl>(net::CacheType::DISK_CACHE);
  }
};

TEST_F(SqlBackendImplTest, MaxFileSize) {
  auto backend = CreateBackend();
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->MaxFileSize(), net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, GetEntryCount) {
  auto backend = CreateBackend();
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->GetEntryCount(
                base::BindRepeating([](int32_t result) { NOTREACHED(); })),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, OpenOrCreateEntry) {
  auto backend = CreateBackend();
  TestEntryResultCompletionCallback callback;
  EntryResult result =
      backend->OpenOrCreateEntry("test_key", net::HIGHEST, callback.callback());
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(result.net_error(), net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, OpenEntry) {
  auto backend = CreateBackend();
  TestEntryResultCompletionCallback callback;
  EntryResult result =
      backend->OpenEntry("test_key", net::HIGHEST, callback.callback());
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(result.net_error(), net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CreateEntry) {
  auto backend = CreateBackend();
  TestEntryResultCompletionCallback callback;
  EntryResult result =
      backend->CreateEntry("test_key", net::HIGHEST, callback.callback());
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(result.net_error(), net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, DoomEntry) {
  auto backend = CreateBackend();
  net::TestCompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->DoomEntry("test_key", net::HIGHEST, callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, DoomAllEntries) {
  auto backend = CreateBackend();
  net::TestCompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->DoomAllEntries(callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, DoomEntriesBetween) {
  auto backend = CreateBackend();
  net::TestCompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->DoomEntriesBetween(base::Time(), base::Time::Max(),
                                        callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, DoomEntriesSince) {
  auto backend = CreateBackend();
  net::TestCompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->DoomEntriesSince(base::Time(), callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CalculateSizeOfAllEntries) {
  auto backend = CreateBackend();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfAllEntries(callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CalculateSizeOfEntriesBetween) {
  auto backend = CreateBackend();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfEntriesBetween(
                base::Time(), base::Time::Max(), callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CreateIterator) {
  auto backend = CreateBackend();
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CreateIterator(), nullptr);
}

TEST_F(SqlBackendImplTest, GetStats) {
  auto backend = CreateBackend();
  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_THAT(stats, ElementsAre(Pair("Cache type", "SQL Cache")));
}

TEST_F(SqlBackendImplTest, OnExternalCacheHit) {
  auto backend = CreateBackend();
  // Should not crash.
  backend->OnExternalCacheHit("test_key");
}

}  // namespace
}  // namespace disk_cache

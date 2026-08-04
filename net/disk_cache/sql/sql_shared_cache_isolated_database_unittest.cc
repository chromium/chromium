// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"

#include <limits>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SqlSharedCacheIsolatedDatabaseTest : public testing::TestWithParam<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WalEnabled" : "WalDisabled";
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "true"}}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "false"}}}},
          {});
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SqlSharedCacheIsolatedDatabaseTest,
                         testing::Bool(),
                         &SqlSharedCacheIsolatedDatabaseTest::DescribeParams);

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InitSuccess) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  EXPECT_TRUE(db.Init().has_value());

  // Verify that the isolated database file is created successfully.
  base::FilePath expected_file = temp_dir_.GetPath().AppendASCII("shared_1.db");
  EXPECT_TRUE(base::PathExists(expected_file));
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InitFailureForTesting) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  db.SetSimulateDbFailureCallbackForTesting(base::BindRepeating(
      [](SqlSharedCacheIsolatedDatabase::OperationForTesting op) {
        return true;
      }));
  EXPECT_EQ(db.Init().error(),
            SqlSharedCacheIsolatedDatabase::Error::kFailedForTesting);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InitFailedToOpenVfsFileSet) {
  constexpr std::string_view kNik = "nik";
  SqlSharedCacheDbId db_id(1);

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik), temp_dir_.GetPath(),
                                      db_id);
    EXPECT_TRUE(db.Init().has_value());
  }

  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("shared_1.db");
  ASSERT_TRUE(base::MakeFileUnwritable(db_path));

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik), temp_dir_.GetPath(),
                                      db_id);
    EXPECT_EQ(db.Init().error(),
              SqlSharedCacheIsolatedDatabase::Error::kFailedToOpenVfsFileSet);
  }
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InitializeAndNikMismatch) {
  constexpr std::string_view kNik1 = "nik1";
  constexpr std::string_view kNik2 = "nik2";
  SqlSharedCacheDbId db_id(1);

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  std::optional<SqlSharedCacheRowId> row_id;

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik1), temp_dir_.GetPath(),
                                      db_id);
    EXPECT_TRUE(db.Init().has_value());

    auto row_id_or_error = db.Insert(key, headers, 3, body);
    ASSERT_TRUE(row_id_or_error.has_value());
    row_id = *row_id_or_error;
  }

  {
    // Initialize with the same nik. Data should persist.
    SqlSharedCacheIsolatedDatabase db(std::string(kNik1), temp_dir_.GetPath(),
                                      db_id);
    EXPECT_TRUE(db.Init().has_value());

    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(3);
    EXPECT_TRUE(
        db.Read(key, *row_id, /*body_size=*/3, /*offset=*/0, read_buffer)
            .has_value());
  }

  {
    // Initialize with a different nik. It should wipe the database.
    SqlSharedCacheIsolatedDatabase db(std::string(kNik2), temp_dir_.GetPath(),
                                      db_id);
    EXPECT_TRUE(db.Init().has_value());

    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(3);
    auto read_result =
        db.Read(key, *row_id, /*body_size=*/3, /*offset=*/0, read_buffer);
    EXPECT_FALSE(read_result.has_value());
    EXPECT_EQ(read_result.error(),
              SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
  }
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InsertAndReadSuccess) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  auto row_id_or_error = db.Insert(key, headers, 3, body);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(3);
  auto read_result = db.Read(key, *row_id_or_error, /*body_size=*/3,
                             /*offset=*/0, read_buffer);
  ASSERT_TRUE(read_result.has_value());
  EXPECT_EQ(read_result->read_bytes, 3);
  EXPECT_EQ(read_buffer->span(), body->span());
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, WriteBodyAndRead) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 4, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto body_chunk = base::MakeRefCounted<net::IOBufferWithSize>(2);
  body_chunk->span().copy_from(base::span<const uint8_t>({8, 9}));
  EXPECT_TRUE(db.WriteBody(key, *row_id_or_error, /*offset=*/1, body_chunk,
                           /*set_ready=*/true)
                  .has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(2);
  auto read_result = db.Read(key, *row_id_or_error, /*body_size=*/4,
                             /*offset=*/1, read_buffer);
  ASSERT_TRUE(read_result.has_value());
  EXPECT_EQ(read_result->read_bytes, 2);
  EXPECT_EQ(read_buffer->span(), body_chunk->span());
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, ReadNotReady) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 4, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(2);
  auto read_result = db.Read(key, *row_id_or_error, /*body_size=*/4,
                             /*offset=*/0, read_buffer);
  EXPECT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, ReadKeyMismatch) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 0, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  CacheEntryKey other_key("0/0/https://example.org/");
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(2);
  auto read_result = db.Read(other_key, *row_id_or_error, /*body_size=*/2,
                             /*offset=*/0, read_buffer);
  EXPECT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, InsertBodyTooLarge) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  uint32_t too_large_size =
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) + 1;

  EXPECT_EQ(db.Insert(key, headers, too_large_size, nullptr).error(),
            SqlSharedCacheIsolatedDatabase::Error::kBodyTooLarge);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, WriteBodyInvalidRange) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 4, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(2);
  buffer->span().copy_from(base::span<const uint8_t>({1, 2}));

  EXPECT_EQ(db.WriteBody(key, *row_id_or_error, /*offset=*/-1, buffer,
                         /*set_ready=*/false)
                .error(),
            SqlSharedCacheIsolatedDatabase::Error::kInvalidWriteRange);

  EXPECT_EQ(db.WriteBody(key, *row_id_or_error,
                         /*offset=*/std::numeric_limits<int32_t>::max(), buffer,
                         /*set_ready=*/false)
                .error(),
            SqlSharedCacheIsolatedDatabase::Error::kInvalidWriteRange);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, ReadInvalidRange) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 4, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(2);

  EXPECT_EQ(db.Read(key, *row_id_or_error, /*body_size=*/4, /*offset=*/-1,
                    read_buffer)
                .error(),
            SqlSharedCacheIsolatedDatabase::Error::kInvalidReadRange);

  EXPECT_EQ(db.Read(key, *row_id_or_error, /*body_size=*/4,
                    /*offset=*/std::numeric_limits<int32_t>::max(), read_buffer)
                .error(),
            SqlSharedCacheIsolatedDatabase::Error::kInvalidReadRange);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest,
       WriteBodyMultipleChunksAndReadAcross) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto row_id_or_error = db.Insert(key, headers, 10, nullptr);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto chunk1 = base::MakeRefCounted<net::IOBufferWithSize>(4);
  chunk1->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  EXPECT_TRUE(db.WriteBody(key, *row_id_or_error, /*offset=*/0, chunk1,
                           /*set_ready=*/false)
                  .has_value());

  auto chunk2 = base::MakeRefCounted<net::IOBufferWithSize>(6);
  chunk2->span().copy_from(base::span<const uint8_t>({5, 6, 7, 8, 9, 10}));
  EXPECT_TRUE(db.WriteBody(key, *row_id_or_error, /*offset=*/4, chunk2,
                           /*set_ready=*/true)
                  .has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(6);
  auto read_result = db.Read(key, *row_id_or_error, /*body_size=*/10,
                             /*offset=*/2, read_buffer);
  ASSERT_TRUE(read_result.has_value());
  EXPECT_EQ(read_result->read_bytes, 6);
  EXPECT_EQ(read_buffer->span(), base::span<const uint8_t>({3, 4, 5, 6, 7, 8}));
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, ReadBeyondWrittenBody) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key("0/0/https://example.com/");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(4);
  body->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));

  auto row_id_or_error = db.Insert(key, headers, 4, body);
  ASSERT_TRUE(row_id_or_error.has_value());

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);

  EXPECT_EQ(
      db.Read(key, *row_id_or_error, /*body_size=*/4, /*offset=*/4, read_buffer)
          .error(),
      SqlSharedCacheIsolatedDatabase::Error::kInvalidReadRange);

  auto read_buffer_overflow = base::MakeRefCounted<net::IOBufferWithSize>(3);
  EXPECT_EQ(db.Read(key, *row_id_or_error, /*body_size=*/4, /*offset=*/2,
                    read_buffer_overflow)
                .error(),
            SqlSharedCacheIsolatedDatabase::Error::kInvalidReadRange);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, DeleteEntriesSuccess) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key1("0/0/https://example.com/1");
  CacheEntryKey key2("0/0/https://example.com/2");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  auto row1 = db.Insert(key1, headers, 3, body);
  ASSERT_TRUE(row1.has_value());
  auto row2 = db.Insert(key2, headers, 3, body);
  ASSERT_TRUE(row2.has_value());

  // Delete row 1.
  EXPECT_TRUE(db.DeleteEntries({*row1}).has_value());

  // Verify row 1 cannot be read.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(3);
  EXPECT_EQ(
      db.Read(key1, *row1, /*body_size=*/3, /*offset=*/0, read_buf).error(),
      SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);

  // Verify row 2 can still be read.
  EXPECT_TRUE(db.Read(key2, *row2, /*body_size=*/3, /*offset=*/0, read_buf)
                  .has_value());

  // Delete row 2.
  EXPECT_TRUE(db.DeleteEntries({*row2}).has_value());
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, DeleteEntriesFailureForTesting) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  db.SetSimulateDbFailureCallbackForTesting(base::BindRepeating(
      [](SqlSharedCacheIsolatedDatabase::OperationForTesting op) {
        return op == SqlSharedCacheIsolatedDatabase::OperationForTesting::
                         kDeleteEntries;
      }));

  EXPECT_EQ(db.DeleteEntries({SqlSharedCacheRowId(1)}).error(),
            SqlSharedCacheIsolatedDatabase::Error::kFailedForTesting);
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, DeleteMultipleEntriesAtOnce) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key1("0/0/https://example.com/1");
  CacheEntryKey key2("0/0/https://example.com/2");
  CacheEntryKey key3("0/0/https://example.com/3");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  auto row1 = db.Insert(key1, headers, 3, body);
  ASSERT_TRUE(row1.has_value());
  auto row2 = db.Insert(key2, headers, 3, body);
  ASSERT_TRUE(row2.has_value());
  auto row3 = db.Insert(key3, headers, 3, body);
  ASSERT_TRUE(row3.has_value());

  // Delete row 1 and row 2 simultaneously.
  EXPECT_TRUE(db.DeleteEntries({*row1, *row2}).has_value());

  // Verify row 1 and row 2 cannot be read.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(3);
  EXPECT_EQ(
      db.Read(key1, *row1, /*body_size=*/3, /*offset=*/0, read_buf).error(),
      SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
  EXPECT_EQ(
      db.Read(key2, *row2, /*body_size=*/3, /*offset=*/0, read_buf).error(),
      SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);

  // Verify row 3 can still be read.
  EXPECT_TRUE(db.Read(key3, *row3, /*body_size=*/3, /*offset=*/0, read_buf)
                  .has_value());

  // Delete remaining row 3.
  EXPECT_TRUE(db.DeleteEntries({*row3}).has_value());
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, DeleteNonExistentEntries) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  CacheEntryKey key1("0/0/https://example.com/1");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  auto row1 = db.Insert(key1, headers, 3, body);
  ASSERT_TRUE(row1.has_value());

  const SqlSharedCacheRowId non_existent_row(9999);

  // Delete non-existent row.
  EXPECT_TRUE(db.DeleteEntries({non_existent_row}).has_value());

  // Verify row 1 can still be read.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(3);
  EXPECT_TRUE(db.Read(key1, *row1, /*body_size=*/3, /*offset=*/0, read_buf)
                  .has_value());

  // Delete existing row 1 and non-existent row together.
  EXPECT_TRUE(db.DeleteEntries({*row1, non_existent_row}).has_value());
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, DeleteEntriesEmptyVectorDeath) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
  ASSERT_TRUE(db.Init().has_value());

  EXPECT_CHECK_DEATH(std::ignore = db.DeleteEntries({}));
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest, CleanupDeletesDatabaseFileIfEmpty) {
  SqlSharedCacheDbId db_id(1);
  base::FilePath db_file = temp_dir_.GetPath().AppendASCII("shared_1.db");

  {
    SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
    ASSERT_TRUE(db.Init().has_value());
    EXPECT_TRUE(base::PathExists(db_file));
    db.Cleanup();
  }

  EXPECT_FALSE(base::PathExists(db_file));
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest,
       CleanupPreservesDatabaseFileIfNotEmpty) {
  SqlSharedCacheDbId db_id(1);
  base::FilePath db_file = temp_dir_.GetPath().AppendASCII("shared_1.db");

  {
    SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
    ASSERT_TRUE(db.Init().has_value());

    CacheEntryKey key("0/0/https://example.com/1");
    auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
    headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
    auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
    body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

    auto row = db.Insert(key, headers, 3, body);
    ASSERT_TRUE(row.has_value());

    EXPECT_TRUE(base::PathExists(db_file));
    db.Cleanup();
  }

  EXPECT_TRUE(base::PathExists(db_file));
}

TEST_P(SqlSharedCacheIsolatedDatabaseTest,
       CleanupDeletesFileAfterDeletingAllEntries) {
  SqlSharedCacheDbId db_id(1);
  base::FilePath db_file = temp_dir_.GetPath().AppendASCII("shared_1.db");

  {
    SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id);
    ASSERT_TRUE(db.Init().has_value());

    CacheEntryKey key("0/0/https://example.com/1");
    auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
    headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
    auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
    body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

    auto row = db.Insert(key, headers, 3, body);
    ASSERT_TRUE(row.has_value());

    EXPECT_TRUE(db.DeleteEntries({*row}).has_value());

    EXPECT_TRUE(base::PathExists(db_file));
    db.Cleanup();
  }

  EXPECT_FALSE(base::PathExists(db_file));
}

}  // namespace disk_cache

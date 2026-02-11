// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/streaming_blob_handle.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::ExplainMatchResult;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::PrintToString;

class StreamingBlobHandleTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("streaming_blob_test.sqlite");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

using StreamingBlobHandleDeathTest = StreamingBlobHandleTest;

// Reads the blob from the specified `row` and `column` of `table`, from the
// database `db`. Returns `std::nullopt` if the blob is missing or can't be
// read.
std::optional<base::HeapArray<uint8_t>> ReadBlob(Database& db,
                                                 base::cstring_view table,
                                                 base::cstring_view column,
                                                 int64_t row) {
  std::optional<StreamingBlobHandle> blob =
      db.GetStreamingBlob(table, column, row, /*readonly=*/true);
  if (!blob.has_value()) {
    return std::nullopt;
  }

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob->GetSize());
  if (!blob->Read(0, read_data)) {
    return std::nullopt;
  }
  return read_data;
}

// Matcher validating that a `base::HeapArray` holds the expected content.
// `base::HeapArray` is not copyable and therefore cannot be held by a GMock
// matcher. It can't be compared directly either. Converting the left and right
// hand side of the expectation to `base::span` resolves both problems. This
// matcher is needed to convert the left hand side when nested matchers are
// used. For instance:
//
//   std::optional<base::HeapArray<uint8_t>> optional_array = ...;
//   EXPECT_THAT(optional_array, Optional(HeapArrayIs(expected)));
MATCHER_P(HeapArrayIsImpl,
          expected,
          base::StrCat({"heap array is ", PrintToString(expected)})) {
  return ExplainMatchResult(ElementsAreArray(expected), arg.as_span(),
                            result_listener);
}
auto HeapArrayIs(base::span<const uint8_t> span) {
  return HeapArrayIsImpl(span);
}

TEST_F(StreamingBlobHandleTest, ReadBlob) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, WriteBlob) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));

  auto blob_content = base::HeapArray<uint8_t>::CopiedFrom({0xC0, 0xFF, 0xEE});
  Statement statement(db.GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO foo (data) VALUES (?)"));
  statement.BindBlobForStreaming(0, blob_content.size());
  ASSERT_TRUE(statement.Run());
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));
  ASSERT_TRUE(blob.Write(0, blob_content));

  EXPECT_THAT(ReadBlob(db, "foo", "data", row_id),
              Optional(HeapArrayIs(blob_content)));
}

TEST_F(StreamingBlobHandleTest, WriteAndReadBlobWithSameHandle) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));

  auto blob_content = base::HeapArray<uint8_t>::CopiedFrom({0xC0, 0xFF, 0xEE});
  Statement statement(db.GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO foo (data) VALUES (?)"));
  statement.BindBlobForStreaming(0, blob_content.size());
  ASSERT_TRUE(statement.Run());
  const int64_t row_id = db.GetLastInsertRowId();

  // Write and read back using the same blob handle.
  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));
  ASSERT_TRUE(blob.Write(0, blob_content));

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs(blob_content));
}

TEST_F(StreamingBlobHandleTest, ReadBlobAtOffset) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(
      db.Execute("INSERT INTO foo (data) VALUES (x'DECAFFC0FFEE112233')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  // Read 3 bytes starting at offset 3.
  auto read_data = base::HeapArray<uint8_t>::Uninit(3);
  EXPECT_TRUE(blob.Read(3, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, WriteBlobIncrementally) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));

  Statement statement(db.GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO foo ( data) VALUES (?)"));
  statement.BindBlobForStreaming(0, 3 * 2);  // 3 chunks of 2 bytes each.
  ASSERT_TRUE(statement.Run());
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  // Write 2 bytes at a time.
  ASSERT_TRUE(blob.Write(0, {0x01, 0x23}));
  ASSERT_TRUE(blob.Write(2, {0x45, 0x67}));
  ASSERT_TRUE(blob.Write(4, {0x89, 0xAB}));

  EXPECT_THAT(ReadBlob(db, "foo", "data", row_id),
              Optional(HeapArrayIs({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB})));
}

TEST_F(StreamingBlobHandleTest, ReadBlobIncrementally) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'0123456789AB')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  auto read_data = base::HeapArray<uint8_t>::Uninit(2);
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0x01, 0x23}));

  EXPECT_TRUE(blob.Read(2, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0x45, 0x67}));

  EXPECT_TRUE(blob.Read(4, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0x89, 0xAB}));
}

TEST_F(StreamingBlobHandleTest, ReadOnlyBlobsRemainValidIfTransactionBegins) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, WriteBlobsRemainValidIfTransactionBegins) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());

  EXPECT_TRUE(blob.Write(0, {0xDE, 0xCA, 0xFF}));
  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xDE, 0xCA, 0xFF}));
}

TEST_F(StreamingBlobHandleTest, ReadOnlyBlobsValidAfterTransactionCommit) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  std::optional<StreamingBlobHandle> blob;
  {
    Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());

    blob = db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true);
    ASSERT_TRUE(blob.has_value());

    ASSERT_TRUE(transaction.Commit());
  }

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob->GetSize());
  EXPECT_TRUE(blob->Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, ReadOnlyBlobsValidAfterTransactionRollback) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  std::optional<StreamingBlobHandle> blob;
  {
    Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());

    blob = db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true);
    ASSERT_TRUE(blob.has_value());

    transaction.Rollback();
  }

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob->GetSize());
  EXPECT_TRUE(blob->Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, WriteBlobsBlockTransactionCommit) {
  Database db(test::kTestTag);
  std::vector<int> errors;
  db.set_error_callback(base::BindLambdaForTesting(
      [&](int error, Statement* statement) { errors.push_back(error); }));

  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  // Transaction commit fails.
  EXPECT_FALSE(transaction.Commit());
  EXPECT_THAT(errors, ElementsAre(SQLITE_BUSY));

  // But the blob is now unusable.
  EXPECT_FALSE(blob.Write(0, {0xDE, 0xCA, 0xFF}));
  EXPECT_THAT(errors, ElementsAre(SQLITE_BUSY, SQLITE_ABORT_ROLLBACK));
}

TEST_F(StreamingBlobHandleTest, WriteBlobsInvalidatedByTransactionRollback) {
  Database db(test::kTestTag);
  std::vector<int> errors;
  db.set_error_callback(base::BindLambdaForTesting(
      [&](int error, Statement* statement) { errors.push_back(error); }));

  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  transaction.Rollback();
  EXPECT_THAT(errors, IsEmpty());

  // The blob is now unusable.
  EXPECT_FALSE(blob.Write(0, {0xDE, 0xCA, 0xFF}));
  EXPECT_THAT(errors, ElementsAre(SQLITE_ABORT_ROLLBACK));
}

TEST_F(StreamingBlobHandleTest, BlobInvalidatedIfRowChanges) {
  Database db(test::kTestTag);
  std::vector<int> errors;
  db.set_error_callback(
      base::BindLambdaForTesting([&](int error, Statement* statement) {
        EXPECT_EQ(statement, nullptr);
        errors.push_back(error);
      }));

  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB, timestamp INTEGER)"));
  ASSERT_TRUE(
      db.Execute("INSERT INTO foo (data, timestamp) VALUES (x'C0FFEE', 10)"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  // The blob is readable at first.
  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));

  // The blob handle expires when the row it's in is updated.
  {
    Statement statement(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE foo SET timestamp = 20 WHERE rowid = ?"));
    statement.BindInt64(0, row_id);
    ASSERT_TRUE(statement.Run());
  }

  // The blob is now unusable and will return SQLITE_ABORT.
  EXPECT_THAT(errors, IsEmpty());
  EXPECT_FALSE(blob.Read(0, read_data));
  EXPECT_THAT(errors, ElementsAre(SQLITE_ABORT));

  // A new handle must be recreated to to read the blob.
  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle updated_blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));
  EXPECT_TRUE(updated_blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleTest, CloseDatabaseInBlobErrorCallback) {
  Database db(test::kTestTag);
  std::vector<int> errors;
  db.set_error_callback(
      base::BindLambdaForTesting([&](int error, Statement* statement) {
        errors.push_back(error);

        // Many clients will close the database in the error callback. This call
        // would present a problem if the blob handle were still open.
        db.Close();
      }));

  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB, timestamp INTEGER)"));
  ASSERT_TRUE(
      db.Execute("INSERT INTO foo (data, timestamp) VALUES (x'C0FFEE', 10)"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  {
    // The blob becomes unreadable after updating the row.
    Statement statement(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE foo SET timestamp = 20 WHERE rowid = ?"));
    statement.BindInt64(0, row_id);
    ASSERT_TRUE(statement.Run());
  }

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_FALSE(blob.Read(0, read_data));
  EXPECT_THAT(errors, ElementsAre(SQLITE_ABORT));
}

TEST_F(StreamingBlobHandleTest, DeleteBlobInErrorCallback) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB, timestamp INTEGER)"));
  ASSERT_TRUE(
      db.Execute("INSERT INTO foo (data, timestamp) VALUES (x'C0FFEE', 10)"));
  const int64_t row_id = db.GetLastInsertRowId();

  std::optional<StreamingBlobHandle> blob =
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true);
  ASSERT_TRUE(blob.has_value());

  std::vector<int> errors;
  db.set_error_callback(
      base::BindLambdaForTesting([&](int error, Statement* statement) {
        errors.push_back(error);
        blob.reset();
      }));

  {
    // The blob becomes unreadable after updating the row.
    Statement statement(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE foo SET timestamp = 20 WHERE rowid = ?"));
    statement.BindInt64(0, row_id);
    ASSERT_TRUE(statement.Run());
  }

  auto read_data = base::HeapArray<uint8_t>::Uninit(blob->GetSize());
  EXPECT_FALSE(blob->Read(0, read_data));
  EXPECT_THAT(errors, ElementsAre(SQLITE_ABORT));
  EXPECT_FALSE(blob.has_value());
}

TEST_F(StreamingBlobHandleTest, BlobHandleCanBeMoveConstructed) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle original_blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  StreamingBlobHandle blob(std::move(original_blob));

  // Check that `blob` works for writes and reads.
  EXPECT_TRUE(blob.Write(0, {0x01, 0x23, 0x45}));
  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_TRUE(blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0x01, 0x23, 0x45}));
}

TEST_F(StreamingBlobHandleTest, BlobHandleCanBeMoveAssigned) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'DECAFFC0FFEE')"));
  const int64_t row_0 = db.GetLastInsertRowId();
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'F00D')"));
  const int64_t row_1 = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle row_0_blob,
      db.GetStreamingBlob("foo", "data", row_0, /*readonly=*/false));
  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle row_1_blob,
      db.GetStreamingBlob("foo", "data", row_1, /*readonly=*/false));

  row_1_blob = std::move(row_0_blob);

  // Check that `row_1_blob` now operates on row 0.
  EXPECT_TRUE(row_1_blob.Write(0, {0x0F, 0xF1, 0xCE}));
  auto read_data = base::HeapArray<uint8_t>::Uninit(row_1_blob.GetSize());
  EXPECT_TRUE(row_1_blob.Read(0, read_data));
  EXPECT_THAT(read_data, HeapArrayIs({0x0F, 0xF1, 0xCE, 0xC0, 0xFF, 0xEE}));
}

TEST_F(StreamingBlobHandleDeathTest,
       OriginalHandleIsUnusableAfterMoveConstruction) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle original_blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false));

  StreamingBlobHandle blob(std::move(original_blob));

  // Check that `blob` works for writes and reads.
  auto read_data = base::HeapArray<uint8_t>::Uninit(blob.GetSize());
  EXPECT_CHECK_DEATH((void)original_blob.Read(0, read_data));
  EXPECT_CHECK_DEATH((void)original_blob.Write(0, {0xDE, 0xCA, 0xFF}));
  EXPECT_CHECK_DEATH(original_blob.GetSize());
}

TEST_F(StreamingBlobHandleDeathTest,
       OriginalHandleIsUnusableAfterMoveAssignment) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_0 = db.GetLastInsertRowId();
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'DECAFF')"));
  const int64_t row_1 = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle row_0_blob,
      db.GetStreamingBlob("foo", "data", row_0, /*readonly=*/false));
  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle row_1_blob,
      db.GetStreamingBlob("foo", "data", row_1, /*readonly=*/false));

  row_1_blob = std::move(row_0_blob);

  // Check that `blob` works for writes and reads.
  auto read_data = base::HeapArray<uint8_t>::Uninit(3);
  EXPECT_CHECK_DEATH((void)row_0_blob.Read(0, read_data));
  EXPECT_CHECK_DEATH((void)row_0_blob.Write(0, {0xF0, 0x0D}));
  EXPECT_CHECK_DEATH(row_0_blob.GetSize());
}

TEST_F(StreamingBlobHandleTest, DatabaseCanBeClosedAfterDestroyingBlobs) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  {
    ASSERT_OK_AND_ASSIGN(
        StreamingBlobHandle blob,
        db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));
  }

  db.Close();  // Should not `CHECK`.
}

TEST_F(StreamingBlobHandleDeathTest, DatabaseCannotBeClosedWhileBlobExists) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (data BLOB)"));
  ASSERT_TRUE(db.Execute("INSERT INTO foo (data) VALUES (x'C0FFEE')"));
  const int64_t row_id = db.GetLastInsertRowId();

  ASSERT_OK_AND_ASSIGN(
      StreamingBlobHandle blob,
      db.GetStreamingBlob("foo", "data", row_id, /*readonly=*/true));

  EXPECT_CHECK_DEATH_WITH(
      db.Close(),
      "All StreamingBlobHandles should be destroyed before closing "
      "sql::Database");
}

}  // namespace
}  // namespace sql

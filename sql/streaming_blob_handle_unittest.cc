// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/streaming_blob_handle.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/sqlite_result_code.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

class StreamingBlobHandleTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(
        temp_dir_.GetPath().AppendASCII("streaming_blob_test.sqlite")));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_{test::kTestTag};
};

TEST_F(StreamingBlobHandleTest, Basic) {
  static const size_t kBlobSize = 128;

  std::optional<Transaction> transaction;
  transaction.emplace(&db_);
  ASSERT_TRUE(transaction->Begin());

  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY AUTOINCREMENT, data BLOB, "
      "timestamp INTEGER NOT NULL)";
  ASSERT_TRUE(db_.Execute(kCreateSql));

  // Insert a row with a blob that is yet to be written.
  {
    Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO foo (data, timestamp) VALUES (?, ?)"));
    statement.BindBlobForStreaming(0, kBlobSize);
    statement.BindTime(1, base::Time::Now());
    ASSERT_TRUE(statement.Run());
  }

  const int64_t row_id = db_.GetLastInsertRowId();

  // Write the blob.
  std::optional<StreamingBlobHandle> writing_blob =
      db_.GetStreamingBlob("foo", "data", row_id, /*readonly=*/false);
  ASSERT_TRUE(writing_blob.has_value());

  static const int kChunkSize = 8;
  for (size_t i = 0; i < kBlobSize / kChunkSize; ++i) {
    std::string data(kChunkSize, 'a' + i);
    ASSERT_TRUE(writing_blob->Write(i * kChunkSize, base::as_byte_span(data)));
  }
  writing_blob.reset();

  // Read the blob.
  std::optional<StreamingBlobHandle> reading_blob = db_.GetStreamingBlob(
      "foo", "data", db_.GetLastInsertRowId(), /*readonly=*/true);
  ASSERT_TRUE(reading_blob.has_value());
  std::string read_data(kBlobSize, 'X');
  // Toss in an offset to ensure it works correctly.
  ASSERT_TRUE(reading_blob->Read(
      7, base::as_writable_byte_span(read_data).subspan(10U, kBlobSize - 20)));
  EXPECT_EQ(
      "XXXXXXXXXXabbbbbbbbccccccccddddddddeeeeeeeeffffffffgggggggghhhhhhhhiii"
      "iiiiijjjjjjjjkkkkkkkkllllllllmmmmmmmmnnnnnnnnoooXXXXXXXXXX",
      read_data);

  // Make sure that committing this transaction doesn't affect the validity of
  // the blob handle.
  transaction->Commit();
  transaction.reset();

  EXPECT_TRUE(reading_blob->Read(0, base::as_writable_byte_span(read_data)));
  EXPECT_EQ(
      "aaaaaaaabbbbbbbbccccccccddddddddeeeeeeeeffffffffgggggggghhhhhhhhiiiiii"
      "iijjjjjjjjkkkkkkkkllllllllmmmmmmmmnnnnnnnnoooooooopppppppp",
      read_data);

  // The blob's existence prevents closing the DB. Normally Chromium code
  // closes the DB via Database::Close(); doing so now would hit a DCHECK.
  ASSERT_EQ(ToSqliteResultCode(sqlite3_close(db_.db_.get())),
            SqliteResultCode::kBusy);

  // Coverage for move ctor.
  auto reading_blob_owned =
      std::make_unique<StreamingBlobHandle>(*std::move(reading_blob));

  // The blob handle expires when the row it's in is updated. This means Read()
  // will error out. This scenario is assumed to be a programming error.
  StreamingBlobHandle* reading_blob_ptr = reading_blob_owned.get();
  int sqlite_callback_error = SQLITE_OK;
  db_.set_error_callback(base::BindRepeating(
      base::BindLambdaForTesting(
          [&sqlite_callback_error](
              Database& database,
              std::unique_ptr<StreamingBlobHandle> owned_blob, int sqlite_error,
              Statement* statement) {
            sqlite_callback_error = sqlite_error;
            EXPECT_EQ(statement, nullptr);
            // Many clients will close the database in the error
            // callback. This call would present a problem if the blob
            // handle were still open.
            database.Close();
            // Many clients will also delete the StreamingBlobHandle here,
            // inside the error callback; make sure nothing bad happens.
          }),
      std::ref(db_), base::Passed(std::move(reading_blob_owned))));
  transaction.emplace(&db_);
  ASSERT_TRUE(transaction->Begin());
  {
    Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE foo SET timestamp = ? WHERE id = ?"));
    statement.BindTime(0, base::Time::Now() + base::Milliseconds(100));
    statement.BindInt64(1, row_id);
    ASSERT_TRUE(statement.Run());
  }
  EXPECT_EQ(sqlite_callback_error, SQLITE_OK);
  EXPECT_FALSE(
      reading_blob_ptr->Read(0, base::as_writable_byte_span(read_data)));
  EXPECT_EQ(sqlite_callback_error, SQLITE_ABORT);
}

}  // namespace sql

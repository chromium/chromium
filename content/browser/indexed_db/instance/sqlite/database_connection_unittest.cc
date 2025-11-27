// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store_util.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/meta_table.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-data-view.h"

namespace content::indexed_db::sqlite {

namespace {

// TODO(crbug.com/419272072): de-dupe with backing_store_unittest.cc
BlobWriteCallback CreateBlobWriteCallback(
    bool* succeeded,
    base::OnceClosure on_done = base::DoNothing()) {
  *succeeded = false;
  return base::BindOnce(
      [](bool* succeeded, base::OnceClosure on_done,
         StatusOr<BlobWriteResult> result) {
        *succeeded = result.has_value();
        std::move(on_done).Run();
        return Status::OK();
      },
      succeeded, std::move(on_done));
}

}  // namespace

class MockBlobStorageContext : public ::storage::mojom::BlobStorageContext {
 public:
  MockBlobStorageContext() = default;
  ~MockBlobStorageContext() override = default;

  void RegisterFromDataItem(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                            const std::string& uuid,
                            storage::mojom::BlobDataItemPtr item) override {
    NOTREACHED();
  }
  void RegisterFromMemory(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                          const std::string& uuid,
                          ::mojo_base::BigBuffer data) override {
    NOTREACHED();
  }
  void WriteBlobToFile(mojo::PendingRemote<::blink::mojom::Blob> blob,
                       const base::FilePath& path,
                       bool flush_on_write,
                       std::optional<base::Time> last_modified,
                       WriteBlobToFileCallback callback) override {
    NOTREACHED();
  }
  void Clone(mojo::PendingReceiver<::storage::mojom::BlobStorageContext>
                 receiver) override {
    NOTREACHED();
  }
};

class DatabaseConnectionTest : public testing::Test {
 public:
  static constexpr int kObjectStoreId = 42;
  const blink::IndexedDBKey kKey;
  const IndexedDBValue kValue;

  DatabaseConnectionTest() : kKey("key"), kValue("deadbeef", {}) {}
  ~DatabaseConnectionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create a mock backing store for testing
    backing_store_ =
        std::make_unique<BackingStoreImpl>(temp_dir_.GetPath(), blob_context_);
  }

  void TearDown() override { backing_store_.reset(); }

  DatabaseConnection* GetDatabaseConnection(const std::u16string& name) {
    auto it = backing_store()->open_connections_.find(name);
    return it == backing_store()->open_connections_.end() ? nullptr
                                                          : it->second.get();
  }

 protected:
  BackingStoreImpl* backing_store() {
    return reinterpret_cast<BackingStoreImpl*>(backing_store_.get());
  }

  std::unique_ptr<BackingStore::Database> OpenDb(std::u16string_view name) {
    StatusOr<std::unique_ptr<BackingStore::Database>> db =
        backing_store()->CreateOrOpenDatabase(std::u16string(name));
    EXPECT_TRUE(db.has_value());
    EXPECT_TRUE(db.value().get());
    return std::move(db.value());
  }

  base::FilePath GetDatabasePath(std::u16string_view name) {
    return temp_dir_.GetPath().Append(DatabaseNameToFileName(name));
  }

  // Create an object store with one record in it.
  void InitializeDbWithOneRecord(BackingStore::Database& db) {
    auto vc =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                             blink::mojom::IDBTransactionMode::VersionChange);
    vc->Begin({});
    ASSERT_TRUE(
        vc->CreateObjectStore(kObjectStoreId, u"object store name", {}, true)
            .ok());
    ASSERT_TRUE(vc->PutRecord(kObjectStoreId, kKey.Clone(), kValue.Clone())
                    .has_value());
    ASSERT_TRUE(vc->SetDatabaseVersion(1).ok());
    bool succeeded = false;
    ASSERT_TRUE(
        vc->CommitPhaseOne(CreateBlobWriteCallback(&succeeded), {}).ok());
    EXPECT_TRUE(succeeded);
    ASSERT_TRUE(vc->CommitPhaseTwo().ok());
  }

  base::test::TaskEnvironment task_environment_;
  MockBlobStorageContext blob_context_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BackingStore> backing_store_;
};

// Verifies that a DB which is too new (as determined by the compatible version
// number) is considered an irrecoverable state and deleted.
TEST_F(DatabaseConnectionTest, TooNew) {
  base::HistogramTester histograms;

  // Create DB.
  const std::u16string_view kDbName{u"test db"};
  auto connection = OpenDb(kDbName);
  ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*connection));
  connection.reset();
  const base::FilePath db_path = GetDatabasePath(kDbName);
  ASSERT_TRUE(base::PathExists(db_path));
  histograms.ExpectUniqueSample(
      "IndexedDB.SQLite.SpecificEvent.OnDisk",
      DatabaseConnection::SpecificEvent::kDatabaseOpenAttempt, 1);

  // Simulate a newer version of the browser updating the schema.
  auto sql_db = std::make_unique<sql::Database>(sql::DatabaseOptions()
                                                    .set_exclusive_locking(true)
                                                    .set_wal_mode(true)
                                                    .set_enable_triggers(true),
                                                sql::test::kTestTag);
  ASSERT_TRUE(sql_db->Open(db_path));
  ASSERT_TRUE(sql::MetaTable::DoesTableExist(sql_db.get()));
  int original_version, original_compat_version;
  {
    sql::MetaTable meta_table;
    // Versions ignored since the table already exists.
    EXPECT_TRUE(meta_table.Init(sql_db.get(), /*version=*/42,
                                /*compatible_version=*/42));
    original_version = meta_table.GetVersionNumber();
    original_compat_version = meta_table.GetCompatibleVersionNumber();
    EXPECT_TRUE(meta_table.SetVersionNumber(1000));
    EXPECT_TRUE(meta_table.SetCompatibleVersionNumber(1000));
  }
  sql_db->Close();

  // The database should be nuked because of the compatible version check, but
  // it is automatically recreated.
  connection = OpenDb(kDbName);
  // Note that this would fail if the object store still existed (i.e. if the
  // original DB hadn't been deleted).
  ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*connection));
  connection.reset();

  histograms.ExpectBucketCount(
      "IndexedDB.SQLite.SpecificEvent.OnDisk",
      DatabaseConnection::SpecificEvent::kDatabaseOpenAttempt, 3);
  histograms.ExpectBucketCount(
      "IndexedDB.SQLite.SpecificEvent.OnDisk",
      DatabaseConnection::SpecificEvent::kDatabaseTooNew, 1);
  histograms.ExpectBucketCount(
      "IndexedDB.SQLite.SpecificEvent.OnDisk",
      DatabaseConnection::SpecificEvent::kDatabaseHadSqlError, 0);

  ASSERT_TRUE(sql_db->Open(db_path));
  ASSERT_TRUE(sql::MetaTable::DoesTableExist(sql_db.get()));
  {
    sql::MetaTable meta_table;
    // Versions ignored since the table already exists.
    EXPECT_TRUE(meta_table.Init(sql_db.get(), /*version=*/42,
                                /*compatible_version=*/42));
    // The meta table got recreated with the current version number and
    // compatible version number.
    EXPECT_EQ(original_version, meta_table.GetVersionNumber());
    EXPECT_EQ(original_compat_version, meta_table.GetCompatibleVersionNumber());
  }
}

class DatabaseConnectionCorruptionTest : public DatabaseConnectionTest {
 public:
  DatabaseConnectionCorruptionTest() = default;

  // Writes a record to the DB and reads it back with `read_value_callback`,
  // which should normally succeed. Then corrupts the DB and tries to read
  // again, which is expected to fail gracefully. Then handles the failure by
  // recovering or deleting the DB. In short: the code in `read_value_callback`
  // is being verified for its error reporting, and the rest of the code in this
  // function is verifying DatabaseConnection's error *handling*.
  void VerifyCorruptionHandling(
      base::RepeatingCallback<StatusOr<IndexedDBValue>(
          BackingStore::Transaction&)> read_value_callback) {
    const std::u16string_view kDbName{u"test db"};

    auto db = OpenDb(kDbName);
    ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*db));

    // Make sure that reading the record works.
    auto read_value = [&]() {
      auto ro =
          db->CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                                blink::mojom::IDBTransactionMode::ReadOnly);
      ro->Begin({});
      StatusOr<IndexedDBValue> value = read_value_callback.Run(*ro);
      ro->Rollback();
      return value;
    };

    StatusOr<IndexedDBValue> value = read_value();

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(base::span(value.value().bits), base::span(kValue.bits));

    StatusOr<base::DictValue> contents_before_corruption =
        SnapshotDatabase(*db);
    ASSERT_TRUE(contents_before_corruption.has_value());

    // Close the database and then corrupt it.
    db.reset();
    const base::FilePath db_path = GetDatabasePath(kDbName);
    ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path, "records_by_key"));

    // Reopen the database. The corruption isn't detected until the index is
    // used, which happens when reading from the records table.
    db = OpenDb(kDbName);

    value = read_value();
    ASSERT_FALSE(value.has_value());
    EXPECT_TRUE(value.error().IsCorruption());

    // Closing the database should run the recovery routine.
    db.reset();
    db = OpenDb(kDbName);

    auto verify_recovery = [&]() {
      StatusOr<IndexedDBValue> recovered_value = read_value();
#if BUILDFLAG(IS_FUCHSIA)
      // Read "works" in that it doesn't fail, but the record doesn't exist,
      // since the corrupted DB was deleted and recreated.
      ASSERT_TRUE(recovered_value.has_value());
      EXPECT_TRUE(recovered_value.value().empty());

      // Reinsert the record. If we don't, the database will be deleted the next
      // time the connection is destroyed, as the database is empty.
      ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*db));
      recovered_value = read_value();
#else
      StatusOr<base::DictValue> contents_after_recovery = SnapshotDatabase(*db);
      ASSERT_TRUE(contents_after_recovery.has_value());
      EXPECT_EQ(*contents_after_recovery, *contents_before_corruption);
#endif

      // Read works because the DB was recovered (or, on Fuchsia, was deleted,
      // recreated, and the record inserted again).
      ASSERT_TRUE(recovered_value.has_value());
      EXPECT_EQ(base::span(recovered_value.value().bits),
                base::span(kValue.bits));
    };
    verify_recovery();

    // Now try a different style of corruption which is detected when the DB is
    // first opened. This verifies that such corruptions will be detected and
    // handled on startup.
    db.reset();
    ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path));
    db = OpenDb(kDbName);
    verify_recovery();
  }
};

TEST_F(DatabaseConnectionCorruptionTest, Get) {
  VerifyCorruptionHandling(
      base::BindLambdaForTesting([&](BackingStore::Transaction& ro) {
        return ro.GetRecord(kObjectStoreId, kKey);
      }));
}

TEST_F(DatabaseConnectionCorruptionTest, ObjectStoreCursor) {
  VerifyCorruptionHandling(
      base::BindLambdaForTesting([&](BackingStore::Transaction& ro) {
        return ro
            .OpenObjectStoreCursor(kObjectStoreId, blink::IndexedDBKeyRange(),
                                   blink::mojom::IDBCursorDirection::Next)
            .transform([](std::unique_ptr<BackingStore::Cursor> cursor)
                           -> IndexedDBValue {
              if (!cursor) {
                return {};
              }
              return cursor->GetValue().Clone();
            });
      }));
}

}  // namespace content::indexed_db::sqlite

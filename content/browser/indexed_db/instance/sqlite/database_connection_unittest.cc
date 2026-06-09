// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store_util.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"

namespace content::indexed_db::sqlite {

namespace {

// TODO(crbug.com/419272072): de-dupe with backing_store_unittest.cc
BlobWriteCallback CreateBlobWriteCallback(
    bool* succeeded,
    base::OnceClosure on_done = base::DoNothing()) {
  *succeeded = false;
  return base::BindOnce(
      [](bool* succeeded, base::OnceClosure on_done, Status result) {
        *succeeded = result.ok();
        std::move(on_done).Run();
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
                       uint64_t expected_size,
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
    CreateBackingStore();
  }

  // (Re)creates the backing store on `temp_dir_`.
  void CreateBackingStore() {
    backing_store_ = std::make_unique<BackingStoreImpl>(
        temp_dir_.GetPath(), blob_context_,
        base::BindRepeating(&DatabaseConnectionTest::AcquireDatabaseLocks,
                            base::Unretained(this)),
        base::DoNothing(), base::DoNothing());
  }

  void TearDown() override {
    backing_store_->FlushForTesting();
    backing_store_.reset();
  }

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
    if (!db.has_value()) {
      ADD_FAILURE();
      return nullptr;
    }
    EXPECT_TRUE(db.value().get());
    return std::move(db.value());
  }

  // Drops the connection to a `DatabaseConnection` and makes sure it's fully
  // closed.
  void DropDbAndDestructDatabaseConnection(
      std::unique_ptr<BackingStore::Database> db) {
    const std::u16string name = db->GetMetadata().name;
    db.reset();
    // This step is necessary to get past the closing grace period.
    task_environment_.FastForwardBy(
        DatabaseConnection::GetDestructionGracePeriodForTesting());
    // This step ensures cleanup is done before proceeding.
    AcquireDatabaseLocks(name);
  }

  base::FilePath GetDatabasePath(std::u16string_view name) {
    return temp_dir_.GetPath().Append(DatabaseNameToFileName(name));
  }

  std::vector<PartitionedLock> AcquireDatabaseLocks(
      const std::u16string& name) {
    base::RunLoop loop;
    PartitionedLockHolder locks_receiver;
    lock_manager_.AcquireLocks(
        {{{0, DatabaseNameToFileName(name).MaybeAsASCII()},
          PartitionedLockManager::LockType::kExclusive}},
        locks_receiver, loop.QuitClosure());
    loop.Run();
    return std::move(locks_receiver.locks);
  }

  // Create an object store with one record in it.
  void InitializeDbWithOneRecord(BackingStore::Database& db) {
    auto vc =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                             blink::mojom::IDBTransactionMode::VersionChange);
    std::vector<PartitionedLock> locks;
    locks.emplace_back(PartitionedLock{{}, base::DoNothing()});
    vc->Begin(std::move(locks));
    ASSERT_TRUE(
        vc->CreateObjectStore(kObjectStoreId, u"object store name", {}, true)
            .ok());
    ASSERT_TRUE(vc->PutRecord(kObjectStoreId, kKey.Clone(), kValue.Clone())
                    .has_value());
    ASSERT_TRUE(vc->SetDatabaseVersion(1).ok());
    bool succeeded = false;
    ASSERT_OK_AND_ASSIGN(
        bool async_work_to_be_done,
        vc->CommitPhaseOne(CreateBlobWriteCallback(&succeeded), {}));
    EXPECT_FALSE(async_work_to_be_done);
    EXPECT_FALSE(succeeded);
    ASSERT_TRUE(vc->CommitPhaseTwo().ok());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockBlobStorageContext blob_context_;
  PartitionedLockManager lock_manager_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BackingStore> backing_store_;
};

// Verifies that a DB which is too new (as determined by the compatible version
// number) is considered an irrecoverable state and deleted.
TEST_F(DatabaseConnectionTest, TooNew) {
  base::HistogramTester histograms;

  // Create DB.
  const std::u16string kDbName{u"test db"};
  auto connection = OpenDb(kDbName);
  ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*connection));
  DropDbAndDestructDatabaseConnection(std::move(connection));
  const base::FilePath db_path = GetDatabasePath(kDbName);
  ASSERT_TRUE(base::PathExists(db_path));
  histograms.ExpectUniqueSample(
      "IndexedDB.SQLite.SpecificEvent.OnDisk",
      DatabaseConnection::SpecificEvent::kDatabaseOpenAttempt, 1);

  // Simulate a newer version of the browser updating the schema.
  auto sql_db = std::make_unique<sql::Database>(
      sql::DatabaseOptions().set_wal_mode(true).set_enable_triggers(true),
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
  DropDbAndDestructDatabaseConnection(std::move(connection));

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

TEST_F(DatabaseConnectionTest, CompressionHistograms) {
  base::HistogramTester histograms;

  const std::u16string_view kDbName{u"test db"};
  auto db = OpenDb(kDbName);
  auto vc =
      db->CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                            blink::mojom::IDBTransactionMode::VersionChange);
  std::vector<PartitionedLock> locks;
  locks.emplace_back(PartitionedLock{{}, base::DoNothing()});
  vc->Begin(std::move(locks));
  ASSERT_TRUE(
      vc->CreateObjectStore(kObjectStoreId, u"object store name", {}, true)
          .ok());

  // Compressible data.
  std::string compressible_data(1000, 'a');
  ASSERT_TRUE(
      vc->PutRecord(kObjectStoreId, kKey, IndexedDBValue(compressible_data, {}))
          .has_value());

  histograms.ExpectTotalCount("IndexedDB.SQLite.PutRecord.CompressionRatio", 1);
  histograms.ExpectUniqueSample(
      "IndexedDB.SQLite.PutRecord.PrecompressionValueSize.Compressed",
      compressible_data.size(), 1);
  histograms.ExpectTotalCount(
      "IndexedDB.SQLite.PutRecord.PrecompressionValueSize.Uncompressed", 0);

  // Data that is not effectively compressible.
  std::string incompressible_data(1000, 'a');
  base::RandBytes(base::as_writable_byte_span(incompressible_data));
  ASSERT_TRUE(vc->PutRecord(kObjectStoreId, blink::IndexedDBKey("key2"),
                            IndexedDBValue(incompressible_data, {}))
                  .has_value());
  vc.reset();

  histograms.ExpectTotalCount("IndexedDB.SQLite.PutRecord.CompressionRatio", 2);
  histograms.ExpectTotalCount(
      "IndexedDB.SQLite.PutRecord.PrecompressionValueSize.Compressed", 1);
  histograms.ExpectUniqueSample(
      "IndexedDB.SQLite.PutRecord.PrecompressionValueSize.Uncompressed",
      incompressible_data.size(), 1);
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
  //
  // When `recoverable` is false, the second pass of corruption uses a technique
  // that prevents successful recovery --- the DB file should still be deleted
  // and recreated as a fallback.
  void VerifyCorruptionHandling(
      bool recoverable,
      base::RepeatingCallback<StatusOr<IndexedDBValue>(
          BackingStore::Transaction&)> read_value_callback) {
    const std::u16string kDbName{u"test db"};

    auto db = OpenDb(kDbName);
    ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*db));

    // Make sure that reading the record works.
    auto read_value = [&]() {
      auto ro =
          db->CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                                blink::mojom::IDBTransactionMode::ReadOnly);
      std::vector<PartitionedLock> locks;
      locks.emplace_back(PartitionedLock{{}, base::DoNothing()});
      ro->Begin(std::move(locks));
      StatusOr<IndexedDBValue> value = read_value_callback.Run(*ro);
      ro->Rollback();
      return value;
    };

    StatusOr<IndexedDBValue> value = read_value();

    ASSERT_OK_AND_ASSIGN(IndexedDBValue value_unwrapped, std::move(value));
    EXPECT_EQ(base::span(value_unwrapped.bits), base::span(kValue.bits));

    ASSERT_OK_AND_ASSIGN(base::DictValue contents_before_corruption,
                         SnapshotDatabase(*db));

    // Close the database and then corrupt it.
    DropDbAndDestructDatabaseConnection(std::move(db));
    const base::FilePath db_path = GetDatabasePath(kDbName);
    ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path, "records_by_key"));

    // Reopen the database. The corruption isn't detected until the index is
    // used, which happens when reading from the records table.
    db = OpenDb(kDbName);

    value = read_value();
    ASSERT_FALSE(value.has_value());
    EXPECT_TRUE(value.error().IsCorruption());

    // Closing the database should run the recovery routine.
    DropDbAndDestructDatabaseConnection(std::move(db));
    db = OpenDb(kDbName);

    auto verify_recovery = [&](bool recovery_expected) {
      StatusOr<IndexedDBValue> recovered_value = read_value();
#if BUILDFLAG(IS_FUCHSIA)
      recovery_expected = false;
#endif
      if (recovery_expected) {
        ASSERT_OK_AND_ASSIGN(base::DictValue contents_after_recovery,
                             SnapshotDatabase(*db));
        EXPECT_EQ(contents_after_recovery, contents_before_corruption);
      } else {
        // Read "works" in that it doesn't fail, but the record doesn't exist,
        // since the corrupted DB was deleted and recreated.
        ASSERT_TRUE(recovered_value.has_value());
        EXPECT_TRUE(recovered_value.value().empty());

        // Reinsert the record. If we don't, the database will be deleted the
        // next time the connection is destroyed, as the database is empty.
        ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*db));
        recovered_value = read_value();
      }

      // Read works because the DB was recovered (or, on Fuchsia, was deleted,
      // recreated, and the record inserted again).
      ASSERT_TRUE(recovered_value.has_value());
      EXPECT_EQ(base::span(recovered_value.value().bits),
                base::span(kValue.bits));
    };
    verify_recovery(/*recovery_expected=*/true);

    // Now try a different style of corruption which is detected when the DB is
    // first opened. This verifies that such corruptions will be detected and
    // handled on startup.
    DropDbAndDestructDatabaseConnection(std::move(db));
    if (recoverable) {
      ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path));
    } else {
      std::array<uint8_t, 100> empty_data = {};
      base::File file(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
      ASSERT_TRUE(file.IsValid());
      ASSERT_TRUE(file.WriteAndCheck(0, empty_data));
    }
    db = OpenDb(kDbName);
    verify_recovery(/*recovery_expected=*/recoverable);
    DropDbAndDestructDatabaseConnection(std::move(db));
  }
};

TEST_F(DatabaseConnectionCorruptionTest, Get) {
  VerifyCorruptionHandling(
      /*recoverable=*/true,
      base::BindLambdaForTesting([&](BackingStore::Transaction& ro) {
        return ro.GetRecord(kObjectStoreId, kKey);
      }));
}

TEST_F(DatabaseConnectionCorruptionTest, UnrecoverableGet) {
  VerifyCorruptionHandling(
      /*recoverable=*/false,
      base::BindLambdaForTesting([&](BackingStore::Transaction& ro) {
        return ro.GetRecord(kObjectStoreId, kKey);
      }));
}

TEST_F(DatabaseConnectionCorruptionTest, ObjectStoreCursor) {
  VerifyCorruptionHandling(
      /*recoverable=*/true,
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

class DatabaseConnectionOpenCorruptionTest : public DatabaseConnectionTest {
 public:
  using SpecificEvent = DatabaseConnection::SpecificEvent;

  static constexpr char kSpecificEventHistogram[] =
      "IndexedDB.SQLite.SpecificEvent.OnDisk";
  static constexpr char kOpenRetryResultHistogram[] =
      "IndexedDB.SQLite.OpenRetryResult";

 protected:
  // Sets up a DB and corrupts it with `corrupt`.
  void SetUpAndCorruptDb(
      std::u16string_view name,
      base::FunctionRef<void(const base::FilePath&)> corrupt) {
    std::unique_ptr<BackingStore::Database> db = OpenDb(name);
    ASSERT_NO_FATAL_FAILURE(InitializeDbWithOneRecord(*db));
    DropDbAndDestructDatabaseConnection(std::move(db));
    corrupt(GetDatabasePath(name));
  }

  // Creates a DB, corrupts it with `corrupt`, and expects that it gets
  // recreated when opened.
  void ExpectRecreated(base::FunctionRef<void(const base::FilePath&)> corrupt,
                       SpecificEvent event,
                       bool data_loss_reported = true) {
    ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"db", corrupt));
    base::HistogramTester histograms;
    std::unique_ptr<BackingStore::Database> db = OpenDb(u"db");
    EXPECT_EQ(db->GetDataLossInfo().status,
              data_loss_reported ? blink::mojom::IDBDataLoss::Total
                                 : blink::mojom::IDBDataLoss::None);
    EXPECT_FALSE(db->GetMetadata().object_stores.contains(kObjectStoreId));
    DropDbAndDestructDatabaseConnection(std::move(db));
    histograms.ExpectTotalCount(kSpecificEventHistogram, 3);
    histograms.ExpectBucketCount(kSpecificEventHistogram,
                                 SpecificEvent::kDatabaseOpenAttempt, 2);
    histograms.ExpectBucketCount(kSpecificEventHistogram, event, 1);
    histograms.ExpectUniqueSample(kOpenRetryResultHistogram,
                                  0 /*Status::Type::kOk*/, 1);
  }

  // Calls `mutate` with a raw `sql::Database` opened on `path`.
  static void MutateRawDb(const base::FilePath& path,
                          base::FunctionRef<void(sql::Database&)> mutate) {
    sql::Database db(
        sql::DatabaseOptions().set_wal_mode(true).set_enable_triggers(true),
        sql::test::kTestTag);
    CHECK(db.Open(path));
    mutate(db);
    db.Close();
  }

  static void CorruptEmptyMetadataTable(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      CHECK(db.Execute("DELETE FROM indexed_db_metadata"));
    });
  }

  static void CorruptToTooNew(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::MetaTable meta_table;
      CHECK(meta_table.Init(&db, /*version=*/42, /*compatible_version=*/42));
      CHECK(meta_table.SetCompatibleVersionNumber(42));
    });
  }

  static void CorruptToUnknownSchemaVersion(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::MetaTable meta_table;
      CHECK(meta_table.Init(&db, /*version=*/42, /*compatible_version=*/42));
      CHECK(meta_table.SetVersionNumber(42));
    });
  }

  static void CorruptStoredName(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::Statement statement(
          db.GetUniqueStatement("UPDATE indexed_db_metadata SET name = ?"));
      statement.BindBlob(0, u"corrupt name");
      CHECK(statement.Run());
    });
  }

  static void CorruptBadDataFormatVersion(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::MetaTable meta_table;
      CHECK(meta_table.Init(&db, /*version=*/42, /*compatible_version=*/42));
      CHECK(
          meta_table.SetValue("v8_data_version", int64_t{0x7FFFFFFFFFFFFFFF}));
    });
  }

  // An odd-length name BLOB can't be decoded as UTF-16.
  static void CorruptUnreadableName(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::Statement statement(
          db.GetUniqueStatement("UPDATE indexed_db_metadata SET name = ?"));
      statement.BindBlob(0, base::byte_span_from_cstring("odd"));
      CHECK(statement.Run());
    });
  }

  static void CorruptToEmptyFile(const base::FilePath& path) {
    CHECK(base::WriteFile(path, ""));
  }

  // Leaves the IndexedDB tables but drops the `meta` table, so the DB looks new
  // despite still holding data.
  static void CorruptDropMetaTable(const base::FilePath& path) {
    MutateRawDb(
        path, [](sql::Database& db) { CHECK(db.Execute("DROP TABLE meta")); });
  }

  static void CorruptZeroedHeader(const base::FilePath& path) {
    std::array<uint8_t, 100> zeros = {};
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    CHECK(file.IsValid());
    CHECK(file.WriteAndCheck(0, zeros));
  }

  static void CorruptRecoverableHeader(const base::FilePath& path) {
    CHECK(sql::test::CorruptSizeInHeader(path));
  }

  static void CorruptToZygotic(const base::FilePath& path) {
    MutateRawDb(path, [](sql::Database& db) {
      sql::Statement statement(
          db.GetUniqueStatement("UPDATE indexed_db_metadata SET version = ?"));
      statement.BindInt64(0, blink::IndexedDBDatabaseMetadata::NO_VERSION);
      CHECK(statement.Run());
    });
  }
};

TEST_F(DatabaseConnectionOpenCorruptionTest, EmptyMetadataTable) {
  ExpectRecreated(CorruptEmptyMetadataTable,
                  SpecificEvent::kMissingMetadataTable);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, TooNew) {
  ExpectRecreated(CorruptToTooNew, SpecificEvent::kDatabaseTooNew);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, UnknownSchemaVersion) {
  ExpectRecreated(CorruptToUnknownSchemaVersion,
                  SpecificEvent::kDatabaseSchemaUnknown);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, NameMismatch) {
  ExpectRecreated(CorruptStoredName, SpecificEvent::kDatabaseNameMismatch);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, BadDataFormatVersion) {
  ExpectRecreated(CorruptBadDataFormatVersion,
                  SpecificEvent::kV8FormatTooNewOrMissing);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, UnreadableName) {
  ExpectRecreated(CorruptUnreadableName, SpecificEvent::kUtf16StringUnreadable);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, EmptyFile) {
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"db", CorruptToEmptyFile));
  base::HistogramTester histograms;
  std::unique_ptr<BackingStore::Database> db = OpenDb(u"db");
  // DB just gets created from scratch with no error reported.
  EXPECT_EQ(db->GetDataLossInfo().status, blink::mojom::IDBDataLoss::None);
  EXPECT_FALSE(db->GetMetadata().object_stores.contains(kObjectStoreId));
  DropDbAndDestructDatabaseConnection(std::move(db));
  histograms.ExpectUniqueSample(kSpecificEventHistogram,
                                SpecificEvent::kDatabaseOpenAttempt, 1);
  histograms.ExpectTotalCount(kOpenRetryResultHistogram, 0);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, DropMetaTable) {
  // The data tables survive, so the first open's CreateSchema collides with
  // them and fails; the retry razes the DB and recreates it empty. The failed
  // open logs no SQL-error event because its transaction rollback clears the
  // error code before cleanup sees it.
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"db", CorruptDropMetaTable));
  base::HistogramTester histograms;
  std::unique_ptr<BackingStore::Database> db = OpenDb(u"db");
  EXPECT_EQ(db->GetDataLossInfo().status, blink::mojom::IDBDataLoss::None);
  EXPECT_FALSE(db->GetMetadata().object_stores.contains(kObjectStoreId));
  DropDbAndDestructDatabaseConnection(std::move(db));
  histograms.ExpectUniqueSample(kSpecificEventHistogram,
                                SpecificEvent::kDatabaseOpenAttempt, 2);
  histograms.ExpectUniqueSample(kOpenRetryResultHistogram,
                                0 /*Status::Type::kOk*/, 1);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, ZeroedHeader) {
  // Zeroing the header defeats recovery, so the DB is recreated empty.
  ExpectRecreated(CorruptZeroedHeader, SpecificEvent::kDatabaseHadSqlError,
                  /*data_loss_reported=*/false);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, RecoverableHeader) {
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"db", CorruptRecoverableHeader));
  base::HistogramTester histograms;
  std::unique_ptr<BackingStore::Database> db = OpenDb(u"db");
  EXPECT_EQ(db->GetDataLossInfo().status, blink::mojom::IDBDataLoss::None);
#if BUILDFLAG(IS_FUCHSIA)
  // Recovery isn't supported, so the DB is deleted and recreated empty.
  EXPECT_FALSE(db->GetMetadata().object_stores.contains(kObjectStoreId));
#else
  // Recovery preserves the data.
  EXPECT_TRUE(db->GetMetadata().object_stores.contains(kObjectStoreId));
#endif
  DropDbAndDestructDatabaseConnection(std::move(db));

  // The first open hits a SQL error, then the retry recovers (non-Fuchsia) or
  // recreates (Fuchsia) successfully.
  histograms.ExpectTotalCount(kSpecificEventHistogram, 3);
  histograms.ExpectBucketCount(kSpecificEventHistogram,
                               SpecificEvent::kDatabaseOpenAttempt, 2);
  histograms.ExpectBucketCount(kSpecificEventHistogram,
                               SpecificEvent::kDatabaseHadSqlError, 1);
  histograms.ExpectUniqueSample(kOpenRetryResultHistogram,
                                0 /*Status::Type::kOk*/, 1);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, ZygoticDatabase) {
  // The database is used as-is since this is "content" corruption.
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"db", CorruptToZygotic));
  base::HistogramTester histograms;
  std::unique_ptr<BackingStore::Database> db = OpenDb(u"db");
  EXPECT_EQ(db->GetDataLossInfo().status, blink::mojom::IDBDataLoss::None);
  EXPECT_TRUE(db->GetMetadata().object_stores.contains(kObjectStoreId));
  DropDbAndDestructDatabaseConnection(std::move(db));
  histograms.ExpectUniqueSample(kSpecificEventHistogram,
                                SpecificEvent::kDatabaseOpenAttempt, 1);
  histograms.ExpectTotalCount(kOpenRetryResultHistogram, 0);
}

TEST_F(DatabaseConnectionOpenCorruptionTest, EnumerateAll) {
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"name mismatch db", CorruptStoredName));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"empty file db", CorruptToEmptyFile));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"dropped meta db", CorruptDropMetaTable));
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"too new db", CorruptToTooNew));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"unreadable name db", CorruptUnreadableName));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"unknown schema db", CorruptToUnknownSchemaVersion));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"empty metadata db", CorruptEmptyMetadataTable));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"bad data format db", CorruptBadDataFormatVersion));
  ASSERT_NO_FATAL_FAILURE(
      SetUpAndCorruptDb(u"recoverable db", CorruptRecoverableHeader));
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"zeroed db", CorruptZeroedHeader));
  ASSERT_NO_FATAL_FAILURE(SetUpAndCorruptDb(u"zygotic db", CorruptToZygotic));

  // Re-init the backing store to ensure the database files are read from disk.
  backing_store()->FlushForTesting();
  CreateBackingStore();

  base::HistogramTester histograms;
  ASSERT_OK_AND_ASSIGN(std::vector<blink::mojom::IDBNameAndVersionPtr> entries,
                       backing_store()->GetDatabaseNamesAndVersions());
  std::vector<std::pair<std::u16string, int64_t>> names_and_versions;
  for (const blink::mojom::IDBNameAndVersionPtr& entry : entries) {
    names_and_versions.emplace_back(entry->name, entry->version);
  }

  // Name-mismatch is surfaced under its corrupt stored name (enumeration has no
  // name to validate against). On non-Fuchsia, the recoverable DB is also
  // surfaced since recovery restores it. Everything else is dropped.
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_THAT(names_and_versions,
              testing::UnorderedElementsAre(testing::Pair(u"corrupt name", 1)));
#else
  EXPECT_THAT(names_and_versions, testing::UnorderedElementsAre(
                                      testing::Pair(u"corrupt name", 1),
                                      testing::Pair(u"recoverable db", 1)));
#endif

  // The dropped databases are deleted from disk.
  EXPECT_TRUE(base::PathExists(GetDatabasePath(u"name mismatch db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"empty file db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"dropped meta db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"too new db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"unreadable name db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"unknown schema db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"empty metadata db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"bad data format db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"zeroed db")));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"zygotic db")));
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(base::PathExists(GetDatabasePath(u"recoverable db")));
#else
  EXPECT_TRUE(base::PathExists(GetDatabasePath(u"recoverable db")));
#endif

  histograms.ExpectTotalCount(
      "IndexedDB.SQLite.OpenToReadMetadataResult.OnDisk", 11);
}

}  // namespace content::indexed_db::sqlite

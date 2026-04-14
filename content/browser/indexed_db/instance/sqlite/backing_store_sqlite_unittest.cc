// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/instance/test_blob_consumer.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"

namespace content::indexed_db::sqlite {

namespace {

using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

}  // namespace

class BackingStoreSqliteTest : public BackingStoreTestBase {
 public:
  BackingStoreSqliteTest() : BackingStoreTestBase(/*use_sqlite=*/true) {}

  BackingStoreSqliteTest(const BackingStoreSqliteTest&) = delete;
  BackingStoreSqliteTest& operator=(const BackingStoreSqliteTest&) = delete;

  ~BackingStoreSqliteTest() override = default;

  void PutRecord(BackingStore::Database& db,
                 int64_t object_store_id,
                 const IndexedDBKey& key,
                 const IndexedDBValue& value) {
    auto transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    transaction->Begin(CreateDummyLock());
    EXPECT_TRUE(transaction->PutRecord(object_store_id, key, value.Clone())
                    .has_value());
    CommitTransactionAndVerify(std::move(transaction));
  }

  // Gets the value pointed to by `key` and reads and returns its first blob.
  std::string ReadBlobContents(BackingStore::Database& db,
                               int64_t object_store_id,
                               const IndexedDBKey& key) {
    auto transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadOnly);
    transaction->Begin(CreateDummyLock());
    auto result = transaction->GetRecord(object_store_id, key);
    EXPECT_TRUE(result.has_value());
    IndexedDBValue result_value = std::move(result.value());
    EXPECT_FALSE(result_value.empty());
    EXPECT_EQ(result_value.external_objects.size(), 1U);
    blink::mojom::IDBValuePtr mojo_value =
        transaction->BuildMojoValue(std::move(result_value), base::DoNothing());
    mojo::Remote<blink::mojom::Blob> blob(
        std::move(mojo_value->external_objects[0]->get_blob_or_file()->blob));
    CommitTransactionAndVerify(std::move(transaction));

    // This loop blocks until some of the data has been read, but not all.
    // This allows verifying what happens if a blob is actively being read.
    base::RunLoop some_loop;
    base::test::TestFuture<std::string> output_future;
    TestBlobConsumer::ReadWholeBlob(blob, output_future.GetCallback(),
                                    some_loop.QuitClosure());
    some_loop.Run();

    std::string output_blob_contents = output_future.Get();

    // Verify that it's possible to checkpoint the database. This makes sure
    // that the `ActiveBlobStreamer` represented by `blob` is not holding onto
    // database resources that prevent this operation.
    EXPECT_TRUE(
        reinterpret_cast<BackingStoreDatabaseImpl&>(db).db_->Checkpoint(false));
    return output_blob_contents;
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

  // Creates a database with an object store, writes enough blob data to create
  // many pages, then clears the object store to produce freelist pages.
  std::unique_ptr<BackingStore::Database> CreateDatabaseWithFreelistPages(
      const std::u16string& db_name) {
    const int64_t object_store_id = 1;
    auto result = backing_store()->CreateOrOpenDatabase(db_name);
    CHECK(result.has_value());
    std::unique_ptr<BackingStore::Database> db = std::move(result.value());
    {
      std::unique_ptr<BackingStore::Transaction> transaction =
          CreateAndBeginTransaction(
              *db, blink::mojom::IDBTransactionMode::VersionChange);
      EXPECT_TRUE(transaction
                      ->CreateObjectStore(object_store_id, u"store",
                                          IndexedDBKeyPath(u"key"),
                                          /*auto_increment=*/true)
                      .ok());
      EXPECT_TRUE(transaction->SetDatabaseVersion(1).ok());
      // Blobs are not compressed, so this will increase page usage.
      std::string blob_data(1000, 'x');
      for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(
            transaction
                ->PutRecord(
                    object_store_id,
                    IndexedDBKey(i, blink::mojom::IDBKeyType::Number),
                    IndexedDBValue("non_blob_payload",
                                   {CreateBlobInfo(u"type", blob_data)}))
                .has_value());
      }
      CommitTransactionAndVerify(std::move(transaction));
    }
    {
      std::unique_ptr<BackingStore::Transaction> transaction =
          db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                                blink::mojom::IDBTransactionMode::ReadWrite);
      transaction->Begin(CreateDummyLock());
      EXPECT_TRUE(transaction->ClearObjectStore(object_store_id).ok());
      CommitTransactionAndVerify(std::move(transaction));
    }
    return db;
  }

  base::FilePath GetDatabasePath(std::u16string_view name) {
    return temp_dir_.GetPath().Append(
        GetSqliteDbDirectory(bucket_context_->bucket_locator())
            .Append(DatabaseNameToFileName(name)));
  }

  // Rewrites blobs that are inlined in the SQLite db to instead be stored as
  // standalone files, as if they had been migrated from a LevelDB store.
  std::vector<base::FilePath> ConvertInlinedBlobsToLegacyFileBlobs(
      std::u16string_view name) {
    AcquireDatabaseLocks(std::u16string(name));
    base::FilePath db_path = GetDatabasePath(name);
    sql::Database db(
        sql::DatabaseOptions().set_wal_mode(true).set_enable_triggers(true),
        sql::test::kTestTag);

    EXPECT_TRUE(db.Open(db_path));

    base::FilePath blob_dir = db_path.InsertBeforeExtensionASCII("_");
    base::CreateDirectory(blob_dir);

    std::vector<base::FilePath> blob_files;
    {
      sql::Statement statement(
          db.GetUniqueStatement("SELECT row_id, bytes FROM blobs"));
      while (statement.Step()) {
        int64_t row_id = statement.ColumnInt64(0);
        base::span<const uint8_t> bytes = statement.ColumnBlob(1);

        base::FilePath path =
            blob_dir.AppendASCII(absl::StrFormat("%" PRIx64, row_id));
        EXPECT_TRUE(base::WriteFile(path, bytes));
        blob_files.push_back(path);
      }
    }
    {
      sql::Statement statement(
          db.GetUniqueStatement("UPDATE blobs SET bytes = NULL "));
      EXPECT_TRUE(statement.Run());
    }
    db.Close();
    return blob_files;
  }
};

TEST_F(BackingStoreSqliteTest, BlobBasics) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"name"));

  const int64_t object_store_id = 1;
  const std::string payload("payload");
  IndexedDBKey key(u"key");
  IndexedDBValue value("non_blob_payload", {CreateBlobInfo(u"type", payload)});
  PutRecord(*db, object_store_id, key, value);
  EXPECT_EQ(ReadBlobContents(*db, object_store_id, key), payload);

  // Verify that the blob was served from the SQLite database (not a legacy
  // file).
  histogram_tester.ExpectUniqueSample(
      "IndexedDB.SQLite.BlobServedFromLegacyFile", false, 1);
}

TEST_F(BackingStoreSqliteTest, LegacyBlobBasics) {
  base::HistogramTester histogram_tester;

  const std::u16string db_name(u"name");
  const int64_t object_store_id = 1;
  const std::string payload("payload");
  const IndexedDBKey key1(u"key1");
  const IndexedDBKey key2(u"key2");
  const IndexedDBKey key3(u"key3");

  // Setup: write two blobs into a database.
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));

    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            *db, blink::mojom::IDBTransactionMode::VersionChange);

    EXPECT_TRUE(transaction
                    ->CreateObjectStore(object_store_id, u"object_store_name",
                                        IndexedDBKeyPath(u"object_store_key"),
                                        /*auto_increment=*/true)
                    .ok());
    EXPECT_TRUE(transaction->SetDatabaseVersion(1).ok());

    IndexedDBValue value("non_blob_payload",
                         {CreateBlobInfo(u"type", payload)});
    EXPECT_TRUE(transaction->PutRecord(object_store_id, key1, value.Clone())
                    .has_value());
    EXPECT_TRUE(transaction->PutRecord(object_store_id, key2, value.Clone())
                    .has_value());
    EXPECT_TRUE(transaction->PutRecord(object_store_id, key3, value.Clone())
                    .has_value());
    CommitTransactionAndVerify(std::move(transaction));

    DropDbAndDestructDatabaseConnection(std::move(db));
  }

  // Test hack: convert these blobs to standalone files, as if they'd been
  // migrated from a LevelDB store.
  std::vector<base::FilePath> blob_files =
      ConvertInlinedBlobsToLegacyFileBlobs(db_name);
  ASSERT_EQ(blob_files.size(), 3U);
  EXPECT_TRUE(base::PathExists(blob_files[0]));
  EXPECT_TRUE(base::PathExists(blob_files[1]));
  EXPECT_TRUE(base::PathExists(blob_files[2]));

  {
    // Necessary to reset the "preclose" period, which would normally happen via
    // `BucketContext::Open()`, which is shortcut here.
    auto scoper = SimulateFactoryRequest();

    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
    // Verify that one of these blobs can be read correctly.
    EXPECT_EQ(ReadBlobContents(*db, object_store_id, key2), payload);
    // Verify that the blob was served from a legacy file.
    histogram_tester.ExpectUniqueSample(
        "IndexedDB.SQLite.BlobServedFromLegacyFile", true, 1);

    // Now overwrite two of the rows, such that they don't contain blobs. The
    // blobs should be removed from the backing store.
    IndexedDBValue value_without_blob("non_blob_payload", {});
    PutRecord(*db, object_store_id, key1, value_without_blob);
    PutRecord(*db, object_store_id, key2, value_without_blob);

    // In the case of the blob that was never active (being read), the file can
    // be deleted as soon as the RW txn is finalized. This verifies the
    // `DatabaseConnection::EndTransaction` deletion path.
    EXPECT_FALSE(base::PathExists(blob_files[0]));

    // In the case of the blob that we read above, the file will still be there
    // because `WholeBlobReader` has not released its reference yet. This
    // verifies the `OnBlobBecameInactive` deletion path.
    EXPECT_TRUE(base::PathExists(blob_files[1]));
    // It's eventually deleted after `WholeBlobReader` releases its reference
    // by way of closing the mojo pipe.
    EXPECT_TRUE(base::test::RunUntil(
        [&blob_files]() { return !base::PathExists(blob_files[1]); }));

    // And finally, the blob that was not overwritten is still there.
    EXPECT_TRUE(base::PathExists(blob_files[2]));

    DropDbAndDestructDatabaseConnection(std::move(db));
  }

  // Re-create a blob file, as if it had failed to be deleted at some point.
  // It will be cleaned up when the database is opened then closed again.
  EXPECT_TRUE(base::WriteFile(blob_files[0], "some bytes"));
  {
    auto scoper = SimulateFactoryRequest();

    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
    // Currently cleanup only occurs if there happens to be a Put.
    IndexedDBValue value_without_blob("non_blob_payload", {});
    PutRecord(*db, object_store_id, key2, value_without_blob);
    DropDbAndDestructDatabaseConnection(std::move(db));
    // The artificially resurrected blob is now gone.
    EXPECT_FALSE(base::PathExists(blob_files[0]));
    // The blob that was not resurrected is still gone.
    EXPECT_FALSE(base::PathExists(blob_files[1]));
    // The existing blob is unaffected.
    EXPECT_TRUE(base::PathExists(blob_files[2]));
  }
}

// Tests that legacy blob files are cleaned up when the database is deleted.
TEST_F(BackingStoreSqliteTest, DeleteDatabaseCleansUpLegacyBlobs) {
  const std::u16string db_name(u"name");
  const int64_t object_store_id = 1;
  const std::string payload("payload");
  const IndexedDBKey key(u"key");

  // Setup: write a blob into a database.
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));

    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            *db, blink::mojom::IDBTransactionMode::VersionChange);

    EXPECT_TRUE(transaction
                    ->CreateObjectStore(object_store_id, u"object_store_name",
                                        IndexedDBKeyPath(u"object_store_key"),
                                        /*auto_increment=*/true)
                    .ok());
    EXPECT_TRUE(transaction->SetDatabaseVersion(1).ok());

    IndexedDBValue value("non_blob_payload",
                         {CreateBlobInfo(u"type", payload)});
    EXPECT_TRUE(transaction->PutRecord(object_store_id, key, value.Clone())
                    .has_value());
    CommitTransactionAndVerify(std::move(transaction));

    DropDbAndDestructDatabaseConnection(std::move(db));
  }

  // Convert this blob to a standalone file, as if it had been migrated from a
  // LevelDB store.
  std::vector<base::FilePath> blob_files =
      ConvertInlinedBlobsToLegacyFileBlobs(db_name);
  ASSERT_EQ(blob_files.size(), 1U);
  EXPECT_TRUE(base::PathExists(blob_files[0]));

  // Delete the database.
  {
    auto scoper = SimulateFactoryRequest();
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
    EXPECT_TRUE(
        db->DeleteDatabase(AcquireDatabaseLocks(db_name), base::DoNothing())
            .ok());
    // Wait for the delete operation to release locks.
    AcquireDatabaseLocks(db_name);
  }

  // The legacy blob file should be deleted.
  EXPECT_FALSE(base::PathExists(blob_files[0]));
}

// Regression test for https://crbug.com/454824963. Tests that blob IDs are not
// reused, which is important when building `blobs_staged_for_commit_`.
TEST_F(BackingStoreSqliteTest, PutPutCommitBlob) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"name"));

  auto transaction =
      db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                            blink::mojom::IDBTransactionMode::ReadWrite);
  transaction->Begin(CreateDummyLock());

  const int64_t object_store_id = 1;
  const std::string payload("payload");
  IndexedDBKey key(u"key");
  IndexedDBValue value("non_blob_payload", {CreateBlobInfo(u"type", payload)});
  // Put a record with a blob.
  EXPECT_TRUE(
      transaction->PutRecord(object_store_id, key, value.Clone()).has_value());
  IndexedDBValue value2("non_blob_payload_times_2",
                        {CreateBlobInfo(u"type", payload + payload)});
  // *In the same transaction*, put the record again (replace the blob).
  EXPECT_TRUE(
      transaction->PutRecord(object_store_id, key, value2.Clone()).has_value());
  CommitTransactionAndVerify(std::move(transaction));

  EXPECT_EQ(ReadBlobContents(*db, object_store_id, key), payload + payload);
}

// Regression test for https://crbug.com/454824963. Tests that blobs that are
// staged for commit will be discarded if the associated record is deleted
// before committing.
TEST_F(BackingStoreSqliteTest, PutDeleteCommitBlob) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"name"));

  auto transaction =
      db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                            blink::mojom::IDBTransactionMode::ReadWrite);
  transaction->Begin(CreateDummyLock());

  const int64_t object_store_id = 1;
  IndexedDBKey key(u"key");
  // Create a blob with no payload; since there is no `FakeBlob::set_body()`, it
  // will error if it's actually read (at Commit time). Thus, this test verifies
  // that the blob is never actually read since the record containing it is
  // deleted before commit.
  IndexedDBValue value("non_blob_payload",
                       {CreateBlobInfo(u"type", std::nullopt)});

  EXPECT_TRUE(
      transaction->PutRecord(object_store_id, key, value.Clone()).has_value());
  // *In the same transaction*, delete the record.
  EXPECT_TRUE(
      transaction
          ->DeleteRange(1, blink::IndexedDBKeyRange(key.Clone(), key.Clone(),
                                                    /*lower_open=*/false,
                                                    /*upper_open=*/false))
          .ok());
  CommitTransactionAndVerify(std::move(transaction));
}

// Verifies that chunking of blobs too big to fit in a single row works. The
// test tries blobs of various lengths (mostly prime) to attempt to flush out
// errors in the code that slices blobs apart for writing and stitches them back
// together when reading.
TEST_F(BackingStoreSqliteTest, BlobChunking) {
  constexpr int kBlobChunkSizeForTest = 137;
  DatabaseConnection::OverrideMaxBlobSizeForTesting(
      base::ByteSize(kBlobChunkSizeForTest));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"name"));

  const int64_t object_store_id = 1;
  std::array blob_sizes{0,
                        // Even multiples of chunk size.
                        kBlobChunkSizeForTest, kBlobChunkSizeForTest * 4,
                        // Primes.
                        37, 53, 79, 97, 131, 193, 257, 389, 521, 769, 1031,
                        1543, 2053, 3079, 4099, 6151, 8209};
  std::string data;
  data.reserve(blob_sizes.back());
  EXPECT_GT(blob_sizes.back(), TestBlobConsumer::kPipeCapacity);
  for (int i = 0; i < blob_sizes.back(); ++i) {
    data += static_cast<char>('A' + (i % 26));
  }

  for (int blob_size : blob_sizes) {
    ASSERT_LE(blob_size, static_cast<int>(data.size()));

    IndexedDBKey key(base::NumberToString(blob_size));
    std::string_view payload = std::string_view(data).substr(0, blob_size);
    IndexedDBValue value("non_blob_payload",
                         {CreateBlobInfo(u"type", payload)});
    PutRecord(*db, object_store_id, key, value);
    EXPECT_EQ(ReadBlobContents(*db, object_store_id, key), payload);
  }
}

TEST_F(BackingStoreSqliteTest,
       LeftoverDatabaseCleanedAfterGetDatabaseNamesAndVersions) {
  base::HistogramTester histogram_tester;

  // Create a valid database.
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(u"basic_db"));
    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            *db, blink::mojom::IDBTransactionMode::VersionChange);
    EXPECT_TRUE(transaction
                    ->CreateObjectStore(1, u"object_store_name",
                                        IndexedDBKeyPath(u"object_store_key"),
                                        /*auto_increment=*/true)
                    .ok());
    EXPECT_TRUE(transaction->SetDatabaseVersion(1).ok());
    CommitTransactionAndVerify(std::move(transaction));
  }

  // Create a database that's left in a zygotic state. Since we don't run the
  // cleanup task, the database is left behind despite being zygotic. This
  // simulates a previous crash.
  const std::u16string kDbName = u"leftover_db";
  base::FilePath db_path = GetDatabasePath(kDbName);
  EXPECT_FALSE(base::PathExists(db_path));
  BackingStoreImpl* backing_store_impl =
      reinterpret_cast<BackingStoreImpl*>(backing_store());
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DatabaseConnection> connection,
      DatabaseConnection::Open(kDbName, db_path, *backing_store_impl));
  EXPECT_TRUE(base::PathExists(db_path));
  std::ignore = std::move(*connection).GetCleanupTask();
  EXPECT_TRUE(base::PathExists(db_path));

  // `GetDatabaseNamesAndVersions()` won't return the zygotic database, and
  // furthermore erases it.
  ASSERT_OK_AND_ASSIGN(
      std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions,
      backing_store()->GetDatabaseNamesAndVersions());
  ASSERT_EQ(names_and_versions.size(), 1U);
  EXPECT_EQ(names_and_versions[0]->name, u"basic_db");
  EXPECT_FALSE(base::PathExists(db_path));

  // Verify that the histogram was logged when opening the leftover database.
  // Nothing is logged for "basic_db" since it reads from `cached_versions_`.
  histogram_tester.ExpectUniqueSample(
      "IndexedDB.SQLite.OpenToReadMetadataResult.OnDisk", 2 /*kCorruption*/, 1);
}

TEST_F(BackingStoreSqliteTest, VacuumOnClose) {
  const std::u16string db_name(u"vacuum_test");
  base::FilePath db_path = GetDatabasePath(db_name);
  int64_t pre_vacuum_size = 0;

  // Create a database with freelist pages, then force-close to skip vacuum.
  {
    base::HistogramTester histograms;
    std::ignore = CreateDatabaseWithFreelistPages(db_name);
    backing_store_ = nullptr;
    bucket_context_->ForceClose(/*doom=*/false);
    ASSERT_OK_AND_ASSIGN(pre_vacuum_size, base::GetFileSize(db_path));

    histograms.ExpectTotalCount("IndexedDB.SQLite.FreelistPercentageAtClose",
                                1);
#if BUILDFLAG(IS_ANDROID)
    // Autovacuum is enabled by default on Android, so vacuum is not triggered.
    histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);
#else
    histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 2);
    histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent",
                                 1 /*kRequestedOnClose*/, 1);
    histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent",
                                 3 /*kForceClosing*/, 1);
#endif
  }

  // Reopen the database and expect vacuum to succeed on "regular" close.
  {
    base::HistogramTester histograms;
    CreateFactoryAndBackingStore();

    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
    DropDbAndDestructDatabaseConnection(std::move(db));
    ASSERT_OK_AND_ASSIGN(int64_t post_vacuum_size, base::GetFileSize(db_path));

    histograms.ExpectTotalCount("IndexedDB.SQLite.FreelistPercentageAtClose",
                                1);
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(post_vacuum_size, pre_vacuum_size);
    histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);
#else
    EXPECT_LT(post_vacuum_size, pre_vacuum_size);
    histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 2);
    histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent",
                                 1 /*kRequestedOnClose*/, 1);
    histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent",
                                 2 /*kSucceeded*/, 1);
#endif
  }
}

TEST_F(BackingStoreSqliteTest, VacuumOnIdle) {
  const std::u16string db_name(u"long_idle_vacuum_test");
  base::FilePath db_path = GetDatabasePath(db_name);
  base::HistogramTester histograms;

  // Create a database with freelist pages and keep it open.
  std::unique_ptr<BackingStore::Database> db =
      CreateDatabaseWithFreelistPages(db_name);

  // Short idle checkpoints but does not vacuum.
  backing_store()->RunIdleTasks(/*long_idle=*/false);
  ASSERT_OK_AND_ASSIGN(int64_t pre_vacuum_size, base::GetFileSize(db_path));
  histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);

  // Long idle vacuums.
  backing_store()->RunIdleTasks(/*long_idle=*/true);
  ASSERT_OK_AND_ASSIGN(int64_t post_vacuum_size, base::GetFileSize(db_path));

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(post_vacuum_size, pre_vacuum_size);
  histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);
#else
  EXPECT_LT(post_vacuum_size, pre_vacuum_size);
  histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent",
                               7 /*kRequestedOnLongIdle*/, 1);
  histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent", 2 /*kSucceeded*/,
                               1);
#endif
}

// Verifies that writing enough to a database will cause a checkpoint
// independently of "idle" maintenance.
TEST_F(BackingStoreSqliteTest, NonIdleCheckpoint) {
  const std::u16string db_name(u"wal_checkpoint_test");
  const int64_t object_store_id = 1;

  // Create the database and object store.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(db_name));
  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            *db, blink::mojom::IDBTransactionMode::VersionChange);
    EXPECT_TRUE(transaction
                    ->CreateObjectStore(object_store_id, u"store",
                                        IndexedDBKeyPath(u"key"),
                                        /*auto_increment=*/true)
                    .ok());
    EXPECT_TRUE(transaction->SetDatabaseVersion(1).ok());
    CommitTransactionAndVerify(std::move(transaction));
  }

  base::FilePath db_path = GetDatabasePath(db_name);
  base::FilePath wal_path =
      base::FilePath(db_path.value() + FILE_PATH_LITERAL("-wal"));

  // Get the initial database size.
  ASSERT_OK_AND_ASSIGN(int64_t initial_db_size, base::GetFileSize(db_path));

  std::string large_payload = base::RandBytesAsString(50000);
  int64_t wal_size = 0;
  int64_t max_wal_size = 0;

  // Keep writing until we observe the WAL being truncated.
  for (int iteration = 0; iteration < 1000; ++iteration) {
    std::unique_ptr<BackingStore::Transaction> transaction =
        db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                              blink::mojom::IDBTransactionMode::ReadWrite);
    transaction->Begin(CreateDummyLock());

    for (int i = 0; i < 10; ++i) {
      EXPECT_TRUE(
          transaction
              ->PutRecord(
                  object_store_id,
                  IndexedDBKey(iteration, blink::mojom::IDBKeyType::Number),
                  IndexedDBValue(large_payload, {}))
              .has_value());
    }
    CommitTransactionAndVerify(std::move(transaction));

    // Check WAL file size.
    ASSERT_OK_AND_ASSIGN(wal_size, base::GetFileSize(wal_path));
    max_wal_size = std::max(max_wal_size, wal_size);
    if (max_wal_size > 0 && wal_size == 0) {
      break;
    }
  }

  EXPECT_GT(max_wal_size, 10000 * 4096);
  EXPECT_EQ(wal_size, 0U);

  ASSERT_OK_AND_ASSIGN(int64_t final_db_size, base::GetFileSize(db_path));
  EXPECT_GT(final_db_size, initial_db_size)
      << "Database file should have grown after checkpoint";
}

}  // namespace content::indexed_db::sqlite

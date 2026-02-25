// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
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

class WholeBlobReader : public mojo::DataPipeDrainer::Client,
                        public blink::mojom::BlobReaderClient {
 public:
  WholeBlobReader(base::OnceCallback<void(std::string)> on_complete)
      : on_complete_(std::move(on_complete)) {}

  ~WholeBlobReader() override = default;

  void Start(mojo::Remote<blink::mojom::Blob>& blob) {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult result =
        CreateDataPipe(/*options=*/nullptr, producer_handle, consumer_handle);
    EXPECT_EQ(result, MOJO_RESULT_OK);

    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));

    blob->ReadAll(std::move(producer_handle),
                  receiver_.BindNewPipeAndPassRemote());
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_.append(reinterpret_cast<const char*>(data.data()), data.size());
  }
  void OnDataComplete() override {
    std::move(on_complete_).Run(std::move(data_));
    delete this;
  }

  // blink::mojom::BlobReaderClient
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {}

 private:
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_{this};
  base::OnceCallback<void(std::string)> on_complete_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  std::string data_;
};

void ReadWholeBlob(mojo::Remote<blink::mojom::Blob>& blob,
                   base::OnceCallback<void(std::string)> on_complete) {
  (new WholeBlobReader(std::move(on_complete)))->Start(blob);
}

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
    CommitTransactionAndVerify(*transaction);
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
    CommitTransactionAndVerify(*transaction);

    std::string output_blob_contents;
    base::RunLoop run_loop;
    ReadWholeBlob(blob, base::BindLambdaForTesting([&](std::string data) {
                    output_blob_contents = std::move(data);
                    run_loop.Quit();
                  }));
    run_loop.Run();

    // Verify that it's possible to checkpoint the database. This makes sure
    // that the `ActiveBlobStreamer` represented by `blob` is not holding onto
    // database resources that prevent this operation.
    EXPECT_TRUE(reinterpret_cast<BackingStoreDatabaseImpl&>(db)
                    .db_->db_->CheckpointDatabase());

    return output_blob_contents;
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
    CommitTransactionAndVerify(*transaction);
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
  }

  // Let the cleanup complete, which releases database locks.
  AcquireDatabaseLocks(db_name);

  // Re-create a blob file, as if it had failed to be deleted at some point.
  // It will be cleaned up when the database is opened then closed again.
  EXPECT_TRUE(base::WriteFile(blob_files[0], "some bytes"));
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
    // Currently cleanup only occurs if there happens to be a Put.
    IndexedDBValue value_without_blob("non_blob_payload", {});
    PutRecord(*db, object_store_id, key2, value_without_blob);
    db.reset();
    AcquireDatabaseLocks(db_name);
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
    CommitTransactionAndVerify(*transaction);
  }

  // Convert this blob to a standalone file, as if it had been migrated from a
  // LevelDB store.
  std::vector<base::FilePath> blob_files =
      ConvertInlinedBlobsToLegacyFileBlobs(db_name);
  ASSERT_EQ(blob_files.size(), 1U);
  EXPECT_TRUE(base::PathExists(blob_files[0]));

  // Delete the database.
  {
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
  CommitTransactionAndVerify(*transaction);

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
  CommitTransactionAndVerify(*transaction);
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
    CommitTransactionAndVerify(*transaction);
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
  std::ignore = std::move(*connection).GetCleanupTask(/*force_closing=*/true);
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
  base::HistogramTester histograms;

  const std::u16string db_name(u"vacuum_test");
  const int64_t object_store_id = 1;

  // Create an object store and write enough data to create many pages.
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));
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
              ->PutRecord(object_store_id,
                          IndexedDBKey(i, blink::mojom::IDBKeyType::Number),
                          IndexedDBValue("non_blob_payload",
                                         {CreateBlobInfo(u"type", blob_data)}))
              .has_value());
    }
    CommitTransactionAndVerify(*transaction);
  }

  // Wait for database close and measure the size of the database file.
  AcquireDatabaseLocks(db_name);
  base::FilePath db_path = GetDatabasePath(db_name);
  int64_t initial_size = 0;
  ASSERT_OK_AND_ASSIGN(initial_size, base::GetFileSize(db_path));

  histograms.ExpectBucketCount("IndexedDB.SQLite.FreelistPercentageAtClose", 0,
                               1);
  histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);

  // Now clear the object store to create freelist pages.
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                         backing_store()->CreateOrOpenDatabase(db_name));

    std::unique_ptr<BackingStore::Transaction> transaction =
        db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                              blink::mojom::IDBTransactionMode::ReadWrite);
    transaction->Begin(CreateDummyLock());
    EXPECT_TRUE(transaction->ClearObjectStore(object_store_id).ok());
    CommitTransactionAndVerify(*transaction);
  }

  // The database size should have reduced.
  AcquireDatabaseLocks(db_name);
  int64_t post_vacuum_size = 0;
  ASSERT_OK_AND_ASSIGN(post_vacuum_size, base::GetFileSize(db_path));
  EXPECT_LT(post_vacuum_size, initial_size);

  histograms.ExpectTotalCount("IndexedDB.SQLite.FreelistPercentageAtClose", 2);
#if BUILDFLAG(IS_ANDROID)
  // Autovacuum is enabled by default on Android.
  histograms.ExpectBucketCount("IndexedDB.SQLite.FreelistPercentageAtClose", 0,
                               2);
  histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 0);
#else
  histograms.ExpectBucketCount("IndexedDB.SQLite.FreelistPercentageAtClose", 0,
                               1);
  histograms.ExpectTotalCount("IndexedDB.SQLite.VacuumEvent", 3);
  histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent", 0 /*kNeeded*/,
                               1);
  histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent", 2 /*kRequested*/,
                               1);
  histograms.ExpectBucketCount("IndexedDB.SQLite.VacuumEvent", 3 /*kSucceeded*/,
                               1);
#endif
}

}  // namespace content::indexed_db::sqlite

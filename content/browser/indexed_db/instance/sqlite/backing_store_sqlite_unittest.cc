// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "sql/database.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

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
};

TEST_F(BackingStoreSqliteTest, BlobBasics) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  const int64_t object_store_id = 1;
  const std::string payload("payload");
  IndexedDBKey key(u"key");
  IndexedDBValue value("non_blob_payload", {CreateBlobInfo(u"type", payload)});
  PutRecord(db, object_store_id, key, value);
  EXPECT_EQ(ReadBlobContents(db, object_store_id, key), payload);
}

// Regression test for https://crbug.com/454824963. Tests that blob IDs are not
// reused, which is important when building `blobs_staged_for_commit_`.
TEST_F(BackingStoreSqliteTest, PutPutCommitBlob) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  auto transaction =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
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

  EXPECT_EQ(ReadBlobContents(db, object_store_id, key), payload + payload);
}

// Regression test for https://crbug.com/454824963. Tests that blobs that are
// staged for commit will be discarded if the associated record is deleted
// before committing.
TEST_F(BackingStoreSqliteTest, PutDeleteCommitBlob) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  auto transaction =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
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
      base::ByteCount(kBlobChunkSizeForTest));

  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

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
    PutRecord(db, object_store_id, key, value);
    EXPECT_EQ(ReadBlobContents(db, object_store_id, key), payload);
  }
}

}  // namespace content::indexed_db::sqlite

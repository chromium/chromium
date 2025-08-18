// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db::sqlite {

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
  DatabaseConnectionTest() = default;
  ~DatabaseConnectionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create a mock backing store for testing
    auto rv =
        BackingStoreImpl::OpenAndVerify(temp_dir_.GetPath(), blob_context_);
    backing_store_ = std::move(std::get<0>(rv));
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

  std::unique_ptr<BackingStore::Database> OpenDb(const std::u16string& name) {
    StatusOr<std::unique_ptr<BackingStore::Database>> db =
        backing_store()->CreateOrOpenDatabase(name);
    EXPECT_TRUE(db.has_value());
    EXPECT_TRUE(db.value().get());
    return std::move(db.value());
  }

  base::test::TaskEnvironment task_environment_;
  MockBlobStorageContext blob_context_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BackingStore> backing_store_;
};

// TODO(crbug.com/419272072): de-dupe with backing_store_unittest.cc
BlobWriteCallback CreateBlobWriteCallback(
    bool* succeeded,
    base::OnceClosure on_done = base::DoNothing()) {
  *succeeded = false;
  return base::BindOnce(
      [](bool* succeeded, base::OnceClosure on_done, BlobWriteResult result,
         storage::mojom::WriteBlobToFileResult error) {
        switch (result) {
          case BlobWriteResult::kFailure:
            NOTREACHED();
          case BlobWriteResult::kRunPhaseTwoAsync:
          case BlobWriteResult::kRunPhaseTwoAndReturnResult:
            CHECK_EQ(error, storage::mojom::WriteBlobToFileResult::kSuccess);
            *succeeded = true;
            break;
        }
        std::move(on_done).Run();
        return Status::OK();
      },
      succeeded, std::move(on_done));
}

TEST_F(DatabaseConnectionTest, Corruption) {
  constexpr int kObjectStoreId = 42;
  const blink::IndexedDBKey kKey("key");
  const IndexedDBValue kValue("deadbeef", {});
  const std::u16string kDbName{u"test db"};

  auto db = OpenDb(kDbName);

  // Create an object store with one record in it.
  {
    auto vc =
        db->CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
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

  // Make sure that reading the record works.
  auto read_value = [&]() {
    auto ro =
        db->CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                              blink::mojom::IDBTransactionMode::ReadOnly);
    ro->Begin({});
    StatusOr<IndexedDBValue> value =
        ro->GetRecord(kObjectStoreId, kKey.Clone());
    ro->Rollback();
    return value;
  };

  StatusOr<IndexedDBValue> value = read_value();

  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value().bits, kValue.bits);

  // Close the database and then corrupt it.
  db.reset();
  base::FilePath db_path =
      temp_dir_.GetPath().Append(DatabaseNameToFileName(kDbName));
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

  value = read_value();
#if BUILDFLAG(IS_FUCHSIA)
  // Read "works" in that it doesn't fail, but the record doesn't exist,
  // since the corrupted DB was deleted and recreated.
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(value.value().empty());
#else
  // Read works because the DB was recovered.
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value().bits, kValue.bits);
#endif
}

}  // namespace content::indexed_db::sqlite

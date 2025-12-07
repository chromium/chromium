// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_TEST_BASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_TEST_BASE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"
#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace content::indexed_db {

class BackingStoreTestBase : public testing::Test {
 public:
  explicit BackingStoreTestBase(bool use_sqlite);
  BackingStoreTestBase(const BackingStoreTestBase&) = delete;
  BackingStoreTestBase& operator=(const BackingStoreTestBase&) = delete;
  ~BackingStoreTestBase() override;

  void SetUp() override;
  void TearDown() override;

  void CreateFactoryAndBackingStore();

  void UpdateDatabaseVersion(indexed_db::BackingStore::Database& db,
                             int64_t version);

  std::unique_ptr<indexed_db::BackingStore::Transaction>
  CreateAndBeginTransaction(indexed_db::BackingStore::Database& db,
                            blink::mojom::IDBTransactionMode mode);

  // Commits both phase one and two of `transaction`. This also verifies commit
  // steps are successful.
  void CommitTransactionAndVerify(BackingStore::Transaction& transaction);
  // Commits only phase one of `transaction` and returns true iff successful.
  bool CommitTransactionPhaseOneAndVerify(
      BackingStore::Transaction& transaction);

  std::vector<PartitionedLock> CreateDummyLock();

  void DestroyFactoryAndBackingStore();

  BackingStore* backing_store();

  static IndexedDBExternalObject CreateFileInfo(const std::u16string& file_name,
                                                const std::u16string& type,
                                                base::Time last_modified,
                                                std::string_view file_contents);
  // If `blob_data` is nullopt, the FakeBlob will have no body, which means it
  // will return an error when being read.
  static IndexedDBExternalObject CreateBlobInfo(
      const std::u16string& type,
      std::optional<std::string_view> blob_data);

  static IndexedDBExternalObject CreateFileSystemAccessHandle();

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<MockBlobStorageContext> blob_context_;
  std::unique_ptr<test::MockFileSystemAccessContext>
      file_system_access_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  std::unique_ptr<BucketContext> bucket_context_;
  IndexedDBDataLossInfo data_loss_info_;

  // Sample keys and values that are consistent.
  blink::IndexedDBKey key1_;
  blink::IndexedDBKey key2_;
  IndexedDBValue value1_;
  IndexedDBValue value2_;

  raw_ptr<BackingStore> backing_store_ = nullptr;

 private:
  base::AutoReset<std::optional<bool>> sqlite_override_;
};

class BackingStoreWithExternalObjectsTestBase : public BackingStoreTestBase {
 public:
  explicit BackingStoreWithExternalObjectsTestBase(bool use_sqlite);

  BackingStoreWithExternalObjectsTestBase(
      const BackingStoreWithExternalObjectsTestBase&) = delete;
  BackingStoreWithExternalObjectsTestBase& operator=(
      const BackingStoreWithExternalObjectsTestBase&) = delete;

  ~BackingStoreWithExternalObjectsTestBase() override;

  // Children test classes are expected to implement these.
  virtual bool IncludesBlobs() = 0;
  virtual bool IncludesFileSystemAccessHandles() = 0;

  void SetUp() override;

  // This just checks the data that survive getting stored and recalled, e.g.
  // the file path and UUID will change and thus aren't verified.
  bool CheckBlobInfoMatches(
      const std::vector<IndexedDBExternalObject>& reads) const;

  std::vector<IndexedDBExternalObject>& external_objects() {
    return external_objects_;
  }

 protected:
  // Sample keys and values that are consistent.
  blink::IndexedDBKey key3_;
  IndexedDBValue value3_;

  const std::string kBlobFileData1 = "asdfgasdf";
  const std::string kBlobFileData2 = "aaaaaa";

  // Blob details referenced by `value3_`. The various CheckBlob*() methods
  // can be used to verify the state as a test progresses.
  std::vector<IndexedDBExternalObject> external_objects_;
  std::vector<std::string> blob_remote_uuids_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_TEST_BASE_H_

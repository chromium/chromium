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
  BackingStoreTestBase();
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

  void CommitTransaction(indexed_db::BackingStore::Transaction& transaction);

  std::vector<PartitionedLock> CreateDummyLock();

  void DestroyFactoryAndBackingStore();

  BackingStore* backing_store();

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
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_TEST_BASE_H_

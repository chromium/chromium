// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TEST_BASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TEST_BASE_H_

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"
#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

class IndexedDBTestBase : public testing::Test {
 public:
  IndexedDBTestBase(bool use_default_buckets, bool use_sqlite);

  IndexedDBTestBase(const IndexedDBTestBase&) = delete;
  IndexedDBTestBase& operator=(const IndexedDBTestBase&) = delete;

  ~IndexedDBTestBase() override;

  void SetUp() override;
  void TearDown() override;

  bool UseDefaultBuckets() const { return use_default_buckets_; }
  bool IsSqliteBackingStoreEnabled() const { return use_sqlite_; }
  IndexedDBContextImpl* context() const { return context_.get(); }

  void SetUpInMemoryContext();

  // Flushes the main IDB sequence if `bucket_locator` is not provided. Flushes
  // the bucket sequence corresponding to `bucket_locator` if it is provided.
  void RunPostedTasks(
      std::optional<storage::BucketLocator> bucket_locator = std::nullopt);

  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params);
  storage::BucketInfo InitBucket(const blink::StorageKey& storage_key);
  BucketContext& InitBucketContext(const blink::StorageKey& storage_key);
  void BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info);
  blink::StorageKey GetTestStorageKey();
  BucketContext& GetOrCreateBucketContext(const storage::BucketInfo& bucket,
                                          const base::FilePath& data_directory);
  BucketContext* GetBucketContext(storage::BucketId id);

  // TODO(crbug.com/489361938): remove.
  BucketContextHandle CreateBucketHandle();

 protected:
  base::AutoReset<std::optional<bool>> sqlite_override_;
  bool use_default_buckets_;
  bool use_sqlite_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  MockBlobStorageContext blob_storage_context_;
  test::MockFileSystemAccessContext file_system_access_context_;
  std::unique_ptr<IndexedDBContextImpl> context_;
  mojo::Remote<blink::mojom::IDBFactory> factory_remote_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TEST_BASE_H_

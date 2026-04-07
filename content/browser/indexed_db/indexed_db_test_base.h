// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TEST_BASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"
#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
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

  bool IsSqliteBackingStoreEnabled() const { return use_sqlite_; }
  IndexedDBContextImpl* context() const { return context_.get(); }

  void SetUpInMemoryContext();

  // Flushes the main IDB sequence if `bucket_locator` is not provided. Flushes
  // the bucket sequence corresponding to `bucket_locator` if it is provided.
  void RunPostedTasks(
      std::optional<storage::BucketLocator> bucket_locator = std::nullopt);

  // Asks the (fake) quota manager to make a bucket.
  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params);
  // Asks the (fake) quota manager to make a bucket for `storage_key`, the type
  // depending on `use_default_buckets_`.
  storage::BucketInfo GetOrCreateBucket(const blink::StorageKey& storage_key);
  // If `maybe_bucket` is not provided, this will create a bucket for
  // `GetTestStorageKey()`.
  base::WeakPtr<BucketContext> InitBucketContext(
      std::optional<storage::BucketInfo> maybe_bucket = std::nullopt,
      bool create_backing_store = true);
  void BindFactory(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> bucket_info);
  blink::StorageKey GetTestStorageKey();
  BucketContext* GetBucketContext(storage::BucketId id);

  storage::BucketInitParams BucketParamsForStorageKey(
      const blink::StorageKey& storage_key);

  base::FilePath GetFilePathForTesting(
      const storage::BucketLocator& bucket_locator);

  // Creates a new database, commits the versionchange transaction, and waits
  // for success. Fails if UpgradeNeeded is not received (i.e. the database
  // already exists). Returns the open connection.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> CreateDatabase(
      mojo::Remote<blink::mojom::IDBFactory>& factory_remote,
      const std::u16string& name,
      int64_t transaction_id,
      blink::mojom::IDBDataLoss expected_data_loss =
          blink::mojom::IDBDataLoss::None,
      int64_t version = 1);

  // Opens an existing database and waits for success. Fails if UpgradeNeeded is
  // received (i.e. the database doesn't exist). Returns the open connection.
  mojo::AssociatedRemote<blink::mojom::IDBDatabase> OpenDatabase(
      mojo::Remote<blink::mojom::IDBFactory>& factory_remote,
      const std::u16string& name,
      int64_t transaction_id);

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

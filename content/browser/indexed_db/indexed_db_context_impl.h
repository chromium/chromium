// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
class Value;
}

namespace blink {
class StorageKey;
}

namespace storage {
class QuotaClientCallbackWrapper;
}

namespace content {
class IndexedDBConnection;
class IndexedDBFactoryImpl;
class IndexedDBQuotaClient;

class CONTENT_EXPORT IndexedDBContextImpl
    : public base::RefCountedThreadSafe<IndexedDBContextImpl>,
      public storage::mojom::IndexedDBControl,
      public storage::mojom::IndexedDBControlTest {
 public:
  // The indexed db directory.
  static const base::FilePath::CharType kIndexedDBDirectory[];

  // Release |context| on the IDBTaskRunner.
  static void ReleaseOnIDBSequence(
      scoped_refptr<IndexedDBContextImpl>&& context);

  // If |data_path| is empty, nothing will be saved to disk.
  // |task_runner| is optional, and only set during testing.
  // This is *not* called on the IDBTaskRunner, unlike most other functions.
  IndexedDBContextImpl(
      const base::FilePath& data_path,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::Clock* clock,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
          file_system_access_context,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  IndexedDBContextImpl(const IndexedDBContextImpl&) = delete;
  IndexedDBContextImpl& operator=(const IndexedDBContextImpl&) = delete;

  void Bind(mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForStorageKey(const blink::StorageKey& storage_key,
                           DeleteForStorageKeyCallback callback) override;
  void ForceClose(const blink::StorageKey& storage_key,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(const blink::StorageKey& storage_key,
                          GetConnectionCountCallback callback) override;
  void DownloadStorageKeyData(const blink::StorageKey& storage_key,
                              DownloadStorageKeyDataCallback callback) override;
  void GetAllStorageKeysDetails(
      GetAllStorageKeysDetailsCallback callback) override;
  void SetForceKeepSessionState() override;
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // mojom::IndexedDBControlTest implementation:
  void GetBaseDataPathForTesting(
      GetBaseDataPathForTestingCallback callback) override;
  void GetFilePathForTesting(const blink::StorageKey& storage_key,
                             GetFilePathForTestingCallback callback) override;
  void ResetCachesForTesting(base::OnceClosure callback) override;
  void ForceSchemaDowngradeForTesting(
      const blink::StorageKey& storage_key,
      ForceSchemaDowngradeForTestingCallback callback) override;
  void HasV2SchemaCorruptionForTesting(
      const blink::StorageKey& storage_key,
      HasV2SchemaCorruptionForTestingCallback callback) override;
  void WriteToIndexedDBForTesting(const blink::StorageKey& storage_key,
                                  const std::string& key,
                                  const std::string& value,
                                  base::OnceClosure callback) override;
  void GetBlobCountForTesting(const blink::StorageKey& storage_key,
                              GetBlobCountForTestingCallback callback) override;
  void GetNextBlobNumberForTesting(
      const blink::StorageKey& storage_key,
      int64_t database_id,
      GetNextBlobNumberForTestingCallback callback) override;
  void GetPathForBlobForTesting(
      const blink::StorageKey& storage_key,
      int64_t database_id,
      int64_t blob_number,
      GetPathForBlobForTestingCallback callback) override;
  void CompactBackingStoreForTesting(const blink::StorageKey& storage_key,
                                     base::OnceClosure callback) override;
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver)
      override;
  void GetDatabaseKeysForTesting(
      GetDatabaseKeysForTestingCallback callback) override;

  // TODO(enne): fix internal indexeddb callers to use ForceClose async instead.
  void ForceCloseSync(const blink::StorageKey& storage_key,
                      storage::mojom::ForceCloseReason reason);

  IndexedDBFactoryImpl* GetIDBFactory();

  // Called by StoragePartitionImpl to clear session-only data.
  // *not* called on the IDBTaskRunner.
  void Shutdown();

  int64_t GetStorageKeyDiskUsage(const blink::StorageKey& storage_key);

  // This getter is thread-safe.
  base::SequencedTaskRunner* IDBTaskRunner() { return idb_task_runner_.get(); }

  // Methods called by IndexedDBFactoryImpl or IndexedDBDispatcherHost for
  // quota support.
  void FactoryOpened(const blink::StorageKey& storage_key);
  void ConnectionOpened(const blink::StorageKey& storage_key,
                        IndexedDBConnection* db);
  void ConnectionClosed(const blink::StorageKey& storage_key,
                        IndexedDBConnection* db);
  void TransactionComplete(const blink::StorageKey& storage_key);
  void DatabaseDeleted(const blink::StorageKey& storage_key);

  // Called when blob files have been cleaned (an aggregated delayed task).
  void BlobFilesCleaned(const blink::StorageKey& storage_key);

  // Will be null in unit tests.
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  // Returns a list of all storage_keys with backing stores.
  std::vector<blink::StorageKey> GetAllStorageKeys();
  bool HasStorageKey(const blink::StorageKey& storage_key);

  // Used by IndexedDBInternalsUI to populate internals page.
  base::Value* GetAllStorageKeysDetails();

  // GetStoragePaths returns all paths owned by this database, in arbitrary
  // order.
  std::vector<base::FilePath> GetStoragePaths(
      const blink::StorageKey& storage_key) const;

  const base::FilePath& data_path() const { return data_path_; }
  bool IsInMemoryContext() const { return data_path_.empty(); }
  size_t GetConnectionCountSync(const blink::StorageKey& storage_key);
  int GetStorageKeyBlobFileCount(const blink::StorageKey& storage_key);

  bool is_incognito() const { return data_path_.empty(); }

  storage::mojom::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_ ? blob_storage_context_.get() : nullptr;
  }
  storage::mojom::FileSystemAccessContext* file_system_access_context() const {
    return file_system_access_context_ ? file_system_access_context_.get()
                                       : nullptr;
  }

  void NotifyIndexedDBListChanged(const blink::StorageKey& storage_key);
  void NotifyIndexedDBContentChanged(const blink::StorageKey& storage_key,
                                     const std::u16string& database_name,
                                     const std::u16string& object_store_name);

 private:
  friend class base::RefCountedThreadSafe<IndexedDBContextImpl>;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearLocalState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearSessionOnlyDatabases);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, SetForceKeepSessionState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ForceCloseOpenDatabasesOnDelete);
  friend class IndexedDBQuotaClientTest;

  class IndexedDBGetUsageAndQuotaCallback;

  ~IndexedDBContextImpl() override;

  // Binds receiver on bucket retrieval to ensure that a bucket always exists
  // for a storage key.
  void BindIndexedDBWithBucket(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> result);

  void ShutdownOnIDBSequence();

  base::FilePath GetBlobStorePath(const blink::StorageKey& storage_key) const;
  base::FilePath GetLevelDBPath(const blink::StorageKey& storage_key) const;

  int64_t ReadUsageFromDisk(const blink::StorageKey& storage_key) const;
  void EnsureDiskUsageCacheInitialized(const blink::StorageKey& storage_key);
  // Compares the disk usage stored in `storage_key_size_map_` with disk. If
  // there is a difference, it updates `storage_key_size_map_` and notifies the
  // quota system.
  void QueryDiskAndUpdateQuotaUsage(const blink::StorageKey& storage_key);
  base::Time GetStorageKeyLastModified(const blink::StorageKey& storage_key);

  // Returns `storage_key_set_` (this context's in-memory cache of storage_keys
  // with backing stores); the cache will be primed as needed by checking disk.
  std::set<blink::StorageKey>* GetStorageKeySet();

  const scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;
  IndexedDBDispatcherHost dispatcher_host_;

  // Bound and accessed on the |idb_task_runner_|.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  mojo::Remote<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;
  std::unique_ptr<IndexedDBFactoryImpl> indexeddb_factory_;

  // If |data_path_| is empty then this is an incognito session and the backing
  // store will be held in-memory rather than on-disk.
  const base::FilePath data_path_;

  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<std::set<blink::StorageKey>> storage_key_set_;
  std::map<blink::StorageKey, int64_t> storage_key_size_map_;
  // The set of storage_keys whose storage should be cleared on shutdown.
  std::set<blink::StorageKey> storage_keys_to_purge_on_shutdown_;
  const raw_ptr<base::Clock> clock_;

  const std::unique_ptr<IndexedDBQuotaClient> quota_client_;
  const std::unique_ptr<storage::QuotaClientCallbackWrapper>
      quota_client_wrapper_;

  mojo::ReceiverSet<storage::mojom::IndexedDBControl> receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBControlTest> test_receivers_;
  absl::optional<mojo::Receiver<storage::mojom::MockFailureInjector>>
      mock_failure_injector_;
  mojo::RemoteSet<storage::mojom::IndexedDBObserver> observers_;
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_;
  const std::unique_ptr<storage::FilesystemProxy> filesystem_proxy_;

  base::WeakPtrFactory<IndexedDBContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

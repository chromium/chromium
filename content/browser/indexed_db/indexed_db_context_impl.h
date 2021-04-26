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

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/origin.h"

namespace base {
class Clock;
class ListValue;
class FilePath;
class SequencedTaskRunner;
}

namespace storage {
class QuotaClientCallbackWrapper;
}

namespace url {
class Origin;
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

  void Bind(mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForOrigin(const url::Origin& origin,
                       DeleteForOriginCallback callback) override;
  void ForceClose(const url::Origin& origin,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(const url::Origin& origin,
                          GetConnectionCountCallback callback) override;
  void DownloadOriginData(const url::Origin& origin,
                          DownloadOriginDataCallback callback) override;
  void GetAllOriginsDetails(GetAllOriginsDetailsCallback callback) override;
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
  void GetFilePathForTesting(const url::Origin& origin,
                             GetFilePathForTestingCallback callback) override;
  void ResetCachesForTesting(base::OnceClosure callback) override;
  void ForceSchemaDowngradeForTesting(
      const url::Origin& origin,
      ForceSchemaDowngradeForTestingCallback callback) override;
  void HasV2SchemaCorruptionForTesting(
      const url::Origin& origin,
      HasV2SchemaCorruptionForTestingCallback callback) override;
  void WriteToIndexedDBForTesting(const url::Origin& origin,
                                  const std::string& key,
                                  const std::string& value,
                                  base::OnceClosure callback) override;
  void GetBlobCountForTesting(const url::Origin& origin,
                              GetBlobCountForTestingCallback callback) override;
  void GetNextBlobNumberForTesting(
      const url::Origin& origin,
      int64_t database_id,
      GetNextBlobNumberForTestingCallback callback) override;
  void GetPathForBlobForTesting(
      const url::Origin& origin,
      int64_t database_id,
      int64_t blob_number,
      GetPathForBlobForTestingCallback callback) override;
  void CompactBackingStoreForTesting(const url::Origin& origin,
                                     base::OnceClosure callback) override;
  void BindMockFailureSingletonForTesting(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver)
      override;
  void GetDatabaseKeysForTesting(
      GetDatabaseKeysForTestingCallback callback) override;

  // TODO(enne): fix internal indexeddb callers to use ForceClose async instead.
  void ForceCloseSync(const url::Origin& origin,
                      storage::mojom::ForceCloseReason reason);

  IndexedDBFactoryImpl* GetIDBFactory();

  // Called by StoragePartitionImpl to clear session-only data.
  // *not* called on the IDBTaskRunner.
  void Shutdown();

  int64_t GetOriginDiskUsage(const url::Origin& origin);

  // This getter is thread-safe.
  base::SequencedTaskRunner* IDBTaskRunner() { return idb_task_runner_.get(); }

  // Methods called by IndexedDBFactoryImpl or IndexedDBDispatcherHost for
  // quota support.
  void FactoryOpened(const url::Origin& origin);
  void ConnectionOpened(const url::Origin& origin, IndexedDBConnection* db);
  void ConnectionClosed(const url::Origin& origin, IndexedDBConnection* db);
  void TransactionComplete(const url::Origin& origin);
  void DatabaseDeleted(const url::Origin& origin);

  // Called when blob files have been cleaned (an aggregated delayed task).
  void BlobFilesCleaned(const url::Origin& origin);

  // Will be null in unit tests.
  storage::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Returns a list of all origins with backing stores.
  std::vector<url::Origin> GetAllOrigins();
  bool HasOrigin(const url::Origin& origin);

  // Used by IndexedDBInternalsUI to populate internals page.
  base::ListValue* GetAllOriginsDetails();

  // GetStoragePaths returns all paths owned by this database, in arbitrary
  // order.
  std::vector<base::FilePath> GetStoragePaths(const url::Origin& origin) const;

  const base::FilePath& data_path() const { return data_path_; }
  bool IsInMemoryContext() const { return data_path_.empty(); }
  size_t GetConnectionCountSync(const url::Origin& origin);
  int GetOriginBlobFileCount(const url::Origin& origin);

  bool is_incognito() const { return data_path_.empty(); }

  storage::mojom::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_ ? blob_storage_context_.get() : nullptr;
  }
  storage::mojom::FileSystemAccessContext* file_system_access_context() const {
    return file_system_access_context_ ? file_system_access_context_.get()
                                       : nullptr;
  }

  void NotifyIndexedDBListChanged(const url::Origin& origin);
  void NotifyIndexedDBContentChanged(const url::Origin& origin,
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

  void ShutdownOnIDBSequence();

  base::FilePath GetBlobStorePath(const url::Origin& origin) const;
  base::FilePath GetLevelDBPath(const url::Origin& origin) const;

  int64_t ReadUsageFromDisk(const url::Origin& origin) const;
  void EnsureDiskUsageCacheInitialized(const url::Origin& origin);
  // Compares the disk usage stored in |origin_size_map_| with disk. If there
  // is a difference, it updates |origin_size_map_| and notifies the quota
  // system.
  void QueryDiskAndUpdateQuotaUsage(const url::Origin& origin);
  base::Time GetOriginLastModified(const url::Origin& origin);

  // Returns |origin_set_| (this context's in-memory cache of origins with
  // backing stores); the cache will be primed as needed by checking disk.
  std::set<url::Origin>* GetOriginSet();

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
  std::unique_ptr<std::set<url::Origin>> origin_set_;
  std::map<url::Origin, int64_t> origin_size_map_;
  // The set of origins whose storage should be cleared on shutdown.
  std::set<url::Origin> origins_to_purge_on_shutdown_;
  base::Clock* const clock_;

  const std::unique_ptr<IndexedDBQuotaClient> quota_client_;
  const std::unique_ptr<storage::QuotaClientCallbackWrapper>
      quota_client_wrapper_;

  mojo::ReceiverSet<storage::mojom::IndexedDBControl> receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBControlTest> test_receivers_;
  base::Optional<mojo::Receiver<storage::mojom::MockFailureInjector>>
      mock_failure_injector_;
  mojo::RemoteSet<storage::mojom::IndexedDBObserver> observers_;
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_;
  const std::unique_ptr<storage::FilesystemProxy> filesystem_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

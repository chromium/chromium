// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class IndexedDBBucketContextHandle;
class IndexedDBClientStateCheckerWrapper;
class IndexedDBContextImpl;
class IndexedDBDatabase;
class TransactionalLevelDBDatabase;

// This class has a 1:1 relationship with `IndexedDBContextImpl`.
// TODO(crbug.com/1474996): merge with `IndexedDBContextImpl`.
class CONTENT_EXPORT IndexedDBFactory
    : public blink::mojom::IDBFactory,
      public base::trace_event::MemoryDumpProvider {
 public:
  explicit IndexedDBFactory(IndexedDBContextImpl* context);

  IndexedDBFactory(const IndexedDBFactory&) = delete;
  IndexedDBFactory& operator=(const IndexedDBFactory&) = delete;

  ~IndexedDBFactory() override;

  void AddReceiver(
      absl::optional<storage::BucketInfo> bucket,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(GetDatabaseInfoCallback callback) override;
  void Open(mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
                factory_client,
            mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
                database_callbacks_remote,
            const std::u16string& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
                transaction_receiver,
            int64_t transaction_id) override;
  void DeleteDatabase(mojo::PendingAssociatedRemote<
                          blink::mojom::IDBFactoryClient> factory_client,
                      const std::u16string& name,
                      bool force_close) override;

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  std::vector<IndexedDBDatabase*> GetOpenDatabasesForBucket(
      const storage::BucketLocator& bucket_locator) const;

  // Close all connections to all databases within the bucket. If
  // `will_be_deleted` is true, references to in-memory databases will be
  // dropped thereby allowing their deletion (otherwise they are retained for
  // the lifetime of the factory).
  //
  // TODO(dmurph): This eventually needs to be async, to support scopes
  // multithreading.
  void ForceClose(storage::BucketId bucket_id, bool will_be_deleted);

  void ForceSchemaDowngrade(const storage::BucketLocator& bucket_locator);
  V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const storage::BucketLocator& bucket_locator);

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed();

  // Used for chrome://indexeddb-internals.
  size_t GetConnectionCount(storage::BucketId bucket_id) const;

  // For usage reporting.
  int64_t GetInMemoryDBSize(const storage::BucketLocator& bucket_locator) const;

  std::vector<storage::BucketId> GetOpenBucketIdsForTesting() const;

  IndexedDBBucketContext* GetBucketContextForTesting(
      const storage::BucketId& id) const;

  std::tuple<IndexedDBBucketContextHandle,
             leveldb::Status,
             IndexedDBDatabaseError,
             IndexedDBDataLossInfo,
             /*was_cold_open=*/bool>
  GetOrCreateBucketContext(const storage::BucketInfo& bucket,
                           const base::FilePath& data_directory,
                           bool create_if_missing);

 protected:
  // Used by unittests to allow subclassing of IndexedDBBackingStore.
  virtual std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
      IndexedDBBackingStore::ReportOutstandingBlobsCallback
          report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner);

 private:
  // The data structure that stores everything bound to the receiver. This will
  // be stored together with the receiver in the `mojo::ReceiverSet`.
  struct ReceiverContext {
    ReceiverContext(
        absl::optional<storage::BucketInfo> bucket,
        mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
            client_state_checker_remote);

    ~ReceiverContext();

    ReceiverContext(const ReceiverContext&) = delete;
    ReceiverContext(ReceiverContext&&) noexcept;
    ReceiverContext& operator=(const ReceiverContext&) = delete;
    ReceiverContext& operator=(ReceiverContext&&) = delete;

    // The `bucket` might be null if `QuotaDatabase::GetDatabase()` fails
    // during the IndexedDB binding.
    absl::optional<storage::BucketInfo> bucket;
    // This is needed when the checker needs to be copied to another holder,
    // e.g. `IndexedDBConnection`s that are opened through this dispatcher.
    scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker;
  };

  // `path_base` is the directory that will contain the database directory, the
  // blob directory, and any data loss info. `database_path` is the directory
  // for the leveldb database, and `blob_path` is the directory to store blob
  // files. If `path_base` is empty, then an in-memory database is opened.
  std::tuple<std::unique_ptr<IndexedDBBackingStore>,
             leveldb::Status,
             IndexedDBDataLossInfo,
             bool /* is_disk_full */>
  OpenAndVerifyIndexedDBBackingStore(
      const storage::BucketLocator& bucket_locator,
      base::FilePath data_directory,
      base::FilePath database_path,
      base::FilePath blob_path,
      PartitionedLockManager* lock_manager,
      bool is_first_attempt,
      bool create_if_missing);

  void HandleBackingStoreFailure(const storage::BucketLocator& bucket_locator);
  void HandleBackingStoreCorruption(storage::BucketLocator bucket_locator,
                                    const IndexedDBDatabaseError& error);

  //////////////////////////////////////////////////////
  // Callbacks passed to bucket-sequence classes.

  // Applies the given `callback` to all bucket contexts.
  void ForEachBucketContext(IndexedDBBucketContext::InstanceClosure callback);

  // Used to report fatal database errors.
  void OnDatabaseError(const storage::BucketLocator& bucket_locator,
                       leveldb::Status s,
                       const char* message);

  void OnDatabaseDeleted(const storage::BucketLocator& bucket_locator);

  // Passed to IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const storage::BucketLocator& bucket_locator);

  // Furnished to the IndexedDBActiveBlobRegistry as a callback.
  void ReportOutstandingBlobs(const storage::BucketLocator& bucket_locator,
                              bool blobs_outstanding);

  SEQUENCE_CHECKER(sequence_checker_);

  // This will be set to null after `ContextDestroyed` is called.
  raw_ptr<IndexedDBContextImpl> context_;

  IndexedDBBucketContext::InstanceClosure for_each_bucket_context_;

  // TODO(crbug.com/1474996): these bucket contexts need to be `SequenceBound`.
  std::map<storage::BucketId, std::unique_ptr<IndexedDBBucketContext>>
      bucket_contexts_;

  std::set<storage::BucketLocator> backends_opened_since_startup_;

  mojo::ReceiverSet<blink::mojom::IDBFactory, ReceiverContext> receivers_;

  // Weak pointers from this factory are invalidated when `context_` is
  // destroyed.
  base::WeakPtrFactory<IndexedDBFactory> idb_context_destruction_weak_factory_{
      this};
  base::WeakPtrFactory<IndexedDBFactory> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

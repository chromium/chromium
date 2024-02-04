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
}  // namespace base

namespace content {
class IndexedDBContextImpl;

// This class has a 1:1 relationship with `IndexedDBContextImpl`.
// TODO(crbug.com/1474996): merge with `IndexedDBContextImpl`.
class CONTENT_EXPORT IndexedDBFactory : public blink::mojom::IDBFactory {
 public:
  explicit IndexedDBFactory(IndexedDBContextImpl* context);

  IndexedDBFactory(const IndexedDBFactory&) = delete;
  IndexedDBFactory& operator=(const IndexedDBFactory&) = delete;

  ~IndexedDBFactory() override;

  void AddReceiver(
      std::optional<storage::BucketInfo> bucket,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>,
      base::UnguessableToken client_token,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  // blink::mojom::IDBFactory implementation:
  //
  // The `IndexedDBFactory` is only an IDBFactory implementation for the case of
  // a missing bucket, i.e. where the Quota system failed to retrieve a bucket.
  // Hence, these implementations only return errors. In the normal case,
  // the IndexedDBBucketContext is the IDBFactory endpoint.
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

  // For usage reporting.
  int64_t GetInMemoryDBSize(const storage::BucketLocator& bucket_locator) const;

  std::vector<storage::BucketId> GetOpenBucketIdsForTesting() const;

  IndexedDBBucketContext* GetBucketContextForTesting(
      const storage::BucketId& id) const;

  // Finishes filling in `info` with data relevant to idb-internals and passes
  // the result back via `result`. The bucket is described by
  // `info->bucket_locator`.
  void FillInBucketMetadata(
      storage::mojom::IdbBucketMetadataPtr info,
      base::OnceCallback<void(storage::mojom::IdbBucketMetadataPtr)> result);

  void CompactBackingStoreForTesting(
      const storage::BucketLocator& bucket_locator);

 private:
  friend class IndexedDBFactoryTest;
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTestWithStoragePartitioning,
                           BasicFactoryCreationAndTearDown);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, TooLongOrigin);

  IndexedDBBucketContext& GetOrCreateBucketContext(
      const storage::BucketInfo& bucket,
      const base::FilePath& data_directory);

  void HandleBackingStoreFailure(const storage::BucketLocator& bucket_locator);

  //////////////////////////////////////////////////////
  // Callbacks passed to bucket-sequence classes.

  // Applies the given `callback` to all bucket contexts.
  void ForEachBucketContext(IndexedDBBucketContext::InstanceClosure callback);

  // Used to report fatal database errors.
  void OnDatabaseError(const storage::BucketLocator& bucket_locator,
                       leveldb::Status s,
                       const std::string& message);

  void HandleBackingStoreCorruption(storage::BucketLocator bucket_locator,
                                    const IndexedDBDatabaseError& error);

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

  // See comment above IDBFactory overrides.
  mojo::ReceiverSet<blink::mojom::IDBFactory> receivers_;

  // Weak pointers from this factory are invalidated when `context_` is
  // destroyed.
  base::WeakPtrFactory<IndexedDBFactory> idb_context_destruction_weak_factory_{
      this};
  base::WeakPtrFactory<IndexedDBFactory> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

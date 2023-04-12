// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/sequence_checker.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
class TaskRunner;
}  // namespace base

namespace content {
class IndexedDBContextImpl;
class IndexedDBCursor;
class IndexedDBDataItemReader;
class IndexedDBTransaction;

// All calls but the constructor (including destruction) must
// happen on the IDB sequenced task runner.
class CONTENT_EXPORT IndexedDBDispatcherHost : public blink::mojom::IDBFactory {
 public:
  // The data structure that stores everything bound to the receiver. This will
  // be stored together with the receiver in the `mojo::ReceiverSet`.
  struct ReceiverContext {
    ReceiverContext();
    ReceiverContext(absl::optional<storage::BucketInfo> bucket,
                    mojo::PendingAssociatedRemote<
                        storage::mojom::IndexedDBClientStateChecker>
                        client_state_checker_remote);

    ~ReceiverContext();

    ReceiverContext(const ReceiverContext&) = delete;
    ReceiverContext(ReceiverContext&&) noexcept;
    ReceiverContext& operator=(const ReceiverContext&) = delete;
    ReceiverContext& operator=(ReceiverContext&&) = delete;

    // The `bucket` might be null if `QuotaDatabase::GetDatabase()` fails
    // during the IndexedDB binding.
    absl::optional<storage::BucketInfo> bucket;
    // This is needed when the checker needs to be copied to other holder, e.g.
    // `IndexedDBConnection`s that are opened through this dispatcher.
    scoped_refptr<IndexedDBClientStateCheckerWrapper> client_state_checker;
  };

  explicit IndexedDBDispatcherHost(
      IndexedDBContextImpl* indexed_db_context,
      scoped_refptr<base::TaskRunner> io_task_runner);

  IndexedDBDispatcherHost(const IndexedDBDispatcherHost&) = delete;
  IndexedDBDispatcherHost& operator=(const IndexedDBDispatcherHost&) = delete;

  ~IndexedDBDispatcherHost() override;

  void AddReceiver(
      ReceiverContext context,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  void AddDatabaseBinding(
      std::unique_ptr<blink::mojom::IDBDatabase> database,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabase>
          pending_receiver);

  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> CreateCursorBinding(
      const storage::BucketLocator& bucket_locator,
      std::unique_ptr<IndexedDBCursor> cursor);
  void RemoveCursorBinding(mojo::ReceiverId receiver_id);

  void AddTransactionBinding(
      std::unique_ptr<blink::mojom::IDBTransaction> transaction,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction> receiver);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_; }

  // Must be called on the IDB sequence.
  base::WeakPtr<IndexedDBDispatcherHost> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void CreateAndBindTransactionImpl(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          transaction_receiver,
      const storage::BucketLocator& bucket_locator,
      base::WeakPtr<IndexedDBTransaction> transaction);

  // Bind this receiver to read from this given file.
  void BindFileReader(
      const base::FilePath& path,
      base::Time expected_modification_time,
      base::RepeatingClosure release_callback,
      mojo::PendingReceiver<storage::mojom::BlobDataItemReader> receiver);
  // Removes all readers for this file path.
  void RemoveBoundReaders(const base::FilePath& path);

  // Create external objects from |objects| and store the results in
  // |mojo_objects|.  |mojo_objects| must be the same length as |objects|.
  void CreateAllExternalObjects(
      const storage::BucketLocator& bucket_locator,
      const std::vector<IndexedDBExternalObject>& objects,
      std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects);

 private:
  friend class IndexedDBDispatcherHostTest;

  storage::mojom::BlobStorageContext* mojo_blob_storage_context();
  storage::mojom::FileSystemAccessContext* file_system_access_context();

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(GetDatabaseInfoCallback callback) override;
  void Open(mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                pending_callbacks,
            mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
                database_callbacks_remote,
            const std::u16string& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
                transaction_receiver,
            int64_t transaction_id) override;
  void DeleteDatabase(mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                          pending_callbacks,
                      const std::u16string& name,
                      bool force_close) override;

  base::SequencedTaskRunner* IDBTaskRunner() const;

  // IndexedDBDispatcherHost is owned by IndexedDBContextImpl.
  // This field is not a raw_ptr<> because templates made it difficult for the
  // rewriter to see that |.get()| needs to be appended.
  RAW_PTR_EXCLUSION IndexedDBContextImpl* indexed_db_context_;

  // Shared task runner used for async I/O while reading blob files.
  const scoped_refptr<base::TaskRunner> io_task_runner_;
  // Shared task runner used to read blob files on.
  const scoped_refptr<base::TaskRunner> file_task_runner_;

  mojo::ReceiverSet<blink::mojom::IDBFactory,
                    IndexedDBDispatcherHost::ReceiverContext>
      receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBDatabase>
      database_receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBCursor> cursor_receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBTransaction>
      transaction_receivers_;

  std::map<base::FilePath, std::unique_ptr<IndexedDBDataItemReader>>
      file_reader_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

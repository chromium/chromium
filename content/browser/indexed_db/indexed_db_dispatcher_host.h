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
#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class IndexedDBContextImpl;

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

  explicit IndexedDBDispatcherHost(IndexedDBContextImpl* indexed_db_context);

  IndexedDBDispatcherHost(const IndexedDBDispatcherHost&) = delete;
  IndexedDBDispatcherHost& operator=(const IndexedDBDispatcherHost&) = delete;

  ~IndexedDBDispatcherHost() override;

  void AddReceiver(
      ReceiverContext context,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_; }

  // Must be called on the IDB sequence.
  base::WeakPtr<IndexedDBDispatcherHost> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class IndexedDBDispatcherHostTest;

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

  base::SequencedTaskRunner* IDBTaskRunner() const;

  // IndexedDBDispatcherHost is owned by IndexedDBContextImpl.
  // This field is not a raw_ptr<> because templates made it difficult for the
  // rewriter to see that |.get()| needs to be appended.
  RAW_PTR_EXCLUSION IndexedDBContextImpl* indexed_db_context_;

  mojo::ReceiverSet<blink::mojom::IDBFactory,
                    IndexedDBDispatcherHost::ReceiverContext>
      receivers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

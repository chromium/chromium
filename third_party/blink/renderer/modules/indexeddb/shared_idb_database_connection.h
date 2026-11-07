// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_SHARED_IDB_DATABASE_CONNECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_SHARED_IDB_DATABASE_CONNECTION_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class ExecutionContext;
class IDBDatabase;

// Represents a 1:1 Mojo connection wrapper that can be shared among multiple
// IDBDatabase objects (frontends) in the renderer process.
//
// When connection deduplication is enabled, instead of opening a new Mojo
// connection to the browser for every `IDBFactory.open()` call, we reuse an
// existing `SharedIDBDatabaseConnection` if the version matches.
//
// This class implements `mojom::blink::IDBDatabaseCallbacks` to receive
// database-level events from the browser (e.g. ForcedClose, VersionChange)
// and multiplexes them to all active `IDBDatabase` frontends.
class MODULES_EXPORT SharedIDBDatabaseConnection final
    : public GarbageCollected<SharedIDBDatabaseConnection>,
      public mojom::blink::IDBDatabaseCallbacks {
 public:
  SharedIDBDatabaseConnection(
      ExecutionContext*,
      mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
          callbacks_receiver,
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      const IDBDatabaseMetadata& metadata,
      int64_t open_request_transaction_id);
  ~SharedIDBDatabaseConnection() override;

  // Registers an IDBDatabase frontend that is sharing this connection.
  void RegisterFrontend(IDBDatabase* frontend);

  // Unregisters an IDBDatabase frontend. If no frontends remain and there are
  // no pending sharing requests, the Mojo connection to the browser is closed.
  void UnregisterFrontend(IDBDatabase* frontend);

  void RegisterTransaction(int64_t transaction_id, IDBDatabase* frontend);
  void UnregisterTransaction(int64_t transaction_id);

  // Increments/Decrements the count of in-flight open requests that intend to
  // share this connection. This acts as a lock preventing the connection from
  // closing while a new frontend is in the process of sharing it.
  void IncrementPendingSharingCount() { pending_sharing_count_++; }
  void DecrementPendingSharingCount();

  void SetDisconnectCallback(base::OnceClosure callback) {
    disconnect_callback_ = std::move(callback);
  }

  // Returns the Mojo remote to the database backend.
  mojom::blink::IDBDatabase* GetDatabaseRemote() {
    return database_remote_.get();
  }

  bool is_bound() const { return database_remote_.is_bound(); }
  int pending_sharing_count() const { return pending_sharing_count_; }

  const IDBDatabaseMetadata& metadata() const { return metadata_; }

  // The `transaction_id` of the open request that created this connection.
  // Open requests that share this connection pass it to the browser as
  // `shared_connection_id`. See `mojom::blink::IDBFactory::Open()`.
  int64_t open_request_transaction_id() const {
    return open_request_transaction_id_;
  }

  void ForcedClose() override;
  void VersionChange(int64_t old_version, int64_t new_version) override;
  void Abort(int64_t transaction_id,
             mojom::blink::IDBException code,
             const String& message) override;
  void Complete(int64_t transaction_id) override;

  void Trace(Visitor* visitor) const;

 private:
  void ClearExternalMemory();
  void OnDisconnect();
  void MaybeClose();

  HeapMojoAssociatedRemote<mojom::blink::IDBDatabase> database_remote_;
  HeapMojoAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks,
                             SharedIDBDatabaseConnection>
      callbacks_receiver_;

  HeapLinkedHashSet<WeakMember<IDBDatabase>> frontends_;
  HeapHashMap<int64_t, Member<IDBDatabase>> transaction_map_;
  V8ExternalMemoryAccounter external_memory_accounter_;
  IDBDatabaseMetadata metadata_;
  const int64_t open_request_transaction_id_;
  base::OnceClosure disconnect_callback_;
  int pending_sharing_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_SHARED_IDB_DATABASE_CONNECTION_H_

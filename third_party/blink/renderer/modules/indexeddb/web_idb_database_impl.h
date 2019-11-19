// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_IMPL_H_

#include <stdint.h>

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class WebIDBCallbacks;

class MODULES_EXPORT WebIDBDatabaseImpl : public WebIDBDatabase {
 public:
  WebIDBDatabaseImpl(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebIDBDatabaseImpl() override;

  // WebIDBDatabase
  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const String& new_name) override;
  void CreateTransaction(mojo::PendingAssociatedReceiver<
                             mojom::blink::IDBTransaction> transaction_receiver,
                         int64_t transaction_id,
                         const Vector<int64_t>& scope,
                         mojom::IDBTransactionMode mode,
                         mojom::IDBTransactionDurability durability) override;

  void Close() override;
  void VersionChangeIgnored() override;

  void AddObserver(
      int64_t transaction_id,
      int32_t observer_id,
      bool include_transaction,
      bool no_records,
      bool values,
      std::bitset<kIDBOperationTypeCount> operation_types) override;
  void RemoveObservers(const Vector<int32_t>& observer_ids) override;

  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           const IDBKeyRange*,
           bool key_only,
           WebIDBCallbacks*) override;
  void GetCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                   mojom::blink::IDBDatabaseGetResultPtr result);
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              const IDBKeyRange*,
              int64_t max_count,
              bool key_only,
              WebIDBCallbacks*) override;
  void GetAllCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                      mojom::blink::IDBDatabaseGetAllResultPtr result);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    std::unique_ptr<IDBKey> primary_key,
                    Vector<IDBIndexKeys>) override;
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const Vector<int64_t>& index_ids) override;
  void OpenCursor(int64_t transaction_id,
                  int64_t object_store_id,
                  int64_t index_id,
                  const IDBKeyRange*,
                  mojom::IDBCursorDirection direction,
                  bool key_only,
                  mojom::IDBTaskType,
                  WebIDBCallbacks*) override;
  void OpenCursorCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                          mojom::blink::IDBDatabaseOpenCursorResultPtr result);
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             const IDBKeyRange*,
             WebIDBCallbacks*) override;
  void Delete(int64_t transaction_id,
              int64_t object_store_id,
              const IDBKey* primary_key,
              WebIDBCallbacks*) override;
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   const IDBKeyRange*,
                   WebIDBCallbacks*) override;
  void GetKeyGeneratorCurrentNumber(int64_t transaction_id,
                                    int64_t object_store_id,
                                    WebIDBCallbacks*) override;
  void Clear(int64_t transaction_id,
             int64_t object_store_id,
             WebIDBCallbacks*) override;
  void CreateIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const String& name,
                   const IDBKeyPath&,
                   bool unique,
                   bool multi_entry) override;
  void DeleteIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id) override;
  void RenameIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const String& new_name) override;
  void Abort(int64_t transaction_id) override;

 private:
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> GetCallbacksProxy(
      std::unique_ptr<WebIDBCallbacks> callbacks);

  mojo::AssociatedRemote<mojom::blink::IDBDatabase> database_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_IMPL_H_

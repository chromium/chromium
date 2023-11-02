// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_

#include <stdint.h>
#include <cstdint>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class WebIDBCallbacks;

class MODULES_EXPORT WebIDBDatabase final {
 public:
  WebIDBDatabase(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebIDBDatabase();

  static const int64_t kMinimumIndexId = 30;

  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const String& new_name);
  void CreateTransaction(mojo::PendingAssociatedReceiver<
                             mojom::blink::IDBTransaction> transaction_receiver,
                         int64_t transaction_id,
                         const Vector<int64_t>& scope,
                         mojom::blink::IDBTransactionMode mode,
                         mojom::blink::IDBTransactionDurability durability);

  void Close();
  void VersionChangeIgnored();

  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           const IDBKeyRange*,
           bool key_only,
           WebIDBCallbacks*);
  void GetCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                   mojom::blink::IDBDatabaseGetResultPtr result);
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              const IDBKeyRange*,
              int64_t max_count,
              bool key_only,
              WebIDBCallbacks*);
  void GetAllCallback(
      std::unique_ptr<WebIDBCallbacks> callbacks,
      bool key_only,
      mojo::PendingReceiver<mojom::blink::IDBDatabaseGetAllResultSink>
          receiver);
  void BatchGetAll(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   Vector<mojom::blink::IDBKeyRangePtr> key_ranges,
                   uint32_t max_count,
                   WebIDBCallbacks*);
  void BatchGetAllCallback(
      std::unique_ptr<WebIDBCallbacks> callbacks,
      mojom::blink::IDBDatabaseBatchGetAllResultPtr result);
  void SetIndexKeys(int64_t transaction_id,
                    int64_t object_store_id,
                    std::unique_ptr<IDBKey> primary_key,
                    Vector<IDBIndexKeys>);
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const Vector<int64_t>& index_ids);
  void OpenCursor(int64_t transaction_id,
                  int64_t object_store_id,
                  int64_t index_id,
                  const IDBKeyRange*,
                  mojom::blink::IDBCursorDirection direction,
                  bool key_only,
                  mojom::blink::IDBTaskType,
                  WebIDBCallbacks*);
  void OpenCursorCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                          mojom::blink::IDBDatabaseOpenCursorResultPtr result);
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             const IDBKeyRange*,
             WebIDBCallbacks*);
  void Delete(int64_t transaction_id,
              int64_t object_store_id,
              const IDBKey* primary_key,
              WebIDBCallbacks*);
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   const IDBKeyRange*,
                   WebIDBCallbacks*);
  void GetKeyGeneratorCurrentNumber(int64_t transaction_id,
                                    int64_t object_store_id,
                                    WebIDBCallbacks*);
  void Clear(int64_t transaction_id, int64_t object_store_id, WebIDBCallbacks*);
  void CreateIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const String& name,
                   const IDBKeyPath&,
                   bool unique,
                   bool multi_entry);
  void DeleteIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id);
  void RenameIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const String& new_name);
  void Abort(int64_t transaction_id);

 private:
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> GetCallbacksProxy(
      std::unique_ptr<WebIDBCallbacks> callbacks);

  mojo::AssociatedRemote<mojom::blink::IDBDatabase> database_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_

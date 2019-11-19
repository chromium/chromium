/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_

#include <bitset>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class IDBKeyRange;
class WebIDBCallbacks;

class MODULES_EXPORT WebIDBDatabase {
 public:
  virtual ~WebIDBDatabase() = default;

  virtual void RenameObjectStore(int64_t transaction_id,
                                 int64_t object_store_id,
                                 const String& name) = 0;
  virtual void CreateTransaction(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
          transaction_receiver,
      int64_t id,
      const Vector<int64_t>& scope,
      mojom::IDBTransactionMode,
      mojom::IDBTransactionDurability) = 0;
  virtual void Close() = 0;
  virtual void VersionChangeIgnored() = 0;

  virtual void Abort(int64_t transaction_id) = 0;

  virtual void CreateIndex(int64_t transaction_id,
                           int64_t object_store_id,
                           int64_t index_id,
                           const String& name,
                           const IDBKeyPath&,
                           bool unique,
                           bool multi_entry) = 0;
  virtual void DeleteIndex(int64_t transaction_id,
                           int64_t object_store_id,
                           int64_t index_id) = 0;
  virtual void RenameIndex(int64_t transaction_id,
                           int64_t object_store_id,
                           int64_t index_id,
                           const String& new_name) = 0;

  static const int64_t kMinimumIndexId = 30;

  virtual void AddObserver(
      int64_t transaction_id,
      int32_t observer_id,
      bool include_transaction,
      bool no_records,
      bool values,
      std::bitset<blink::kIDBOperationTypeCount> operation_types) = 0;
  virtual void RemoveObservers(
      const Vector<int32_t>& observer_ids_to_remove) = 0;
  virtual void Get(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const IDBKeyRange*,
                   bool key_only,
                   WebIDBCallbacks*) = 0;
  virtual void GetAll(int64_t transaction_id,
                      int64_t object_store_id,
                      int64_t index_id,
                      const IDBKeyRange*,
                      int64_t max_count,
                      bool key_only,
                      WebIDBCallbacks*) = 0;
  virtual void SetIndexKeys(int64_t transaction_id,
                            int64_t object_store_id,
                            std::unique_ptr<IDBKey> primary_key,
                            Vector<IDBIndexKeys>) = 0;
  virtual void SetIndexesReady(int64_t transaction_id,
                               int64_t object_store_id,
                               const Vector<int64_t>& index_ids) = 0;
  virtual void OpenCursor(int64_t transaction_id,
                          int64_t object_store_id,
                          int64_t index_id,
                          const IDBKeyRange*,
                          mojom::IDBCursorDirection,
                          bool key_only,
                          mojom::IDBTaskType,
                          WebIDBCallbacks*) = 0;
  virtual void Count(int64_t transaction_id,
                     int64_t object_store_id,
                     int64_t index_id,
                     const IDBKeyRange*,
                     WebIDBCallbacks*) = 0;
  virtual void Delete(int64_t transaction_id,
                      int64_t object_store_id,
                      const IDBKey* primary_key,
                      WebIDBCallbacks*) = 0;
  virtual void DeleteRange(int64_t transaction_id,
                           int64_t object_store_id,
                           const IDBKeyRange*,
                           WebIDBCallbacks*) = 0;
  virtual void GetKeyGeneratorCurrentNumber(int64_t transaction_id,
                                            int64_t object_store_id,
                                            WebIDBCallbacks*) = 0;
  virtual void Clear(int64_t transaction_id,
                     int64_t object_store_id,
                     WebIDBCallbacks*) = 0;

 protected:
  WebIDBDatabase() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_

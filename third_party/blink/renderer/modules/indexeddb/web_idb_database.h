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

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class WebData;
class WebIDBCallbacks;
class WebIDBKeyPath;
class WebIDBKeyRange;

class MODULES_EXPORT WebIDBDatabase {
 public:
  virtual ~WebIDBDatabase() = default;

  virtual void CreateObjectStore(long long transaction_id,
                                 long long object_store_id,
                                 const String& name,
                                 const WebIDBKeyPath&,
                                 bool auto_increment) = 0;
  virtual void DeleteObjectStore(long long transaction_id,
                                 long long object_store_id) = 0;
  virtual void RenameObjectStore(long long transaction_id,
                                 long long object_store_id,
                                 const String& name) = 0;
  virtual void CreateTransaction(long long id,
                                 const Vector<int64_t>& scope,
                                 WebIDBTransactionMode) = 0;
  virtual void Close() = 0;
  virtual void VersionChangeIgnored() = 0;

  virtual void Abort(long long transaction_id) = 0;
  virtual void Commit(long long transaction_id) = 0;

  virtual void CreateIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id,
                           const String& name,
                           const WebIDBKeyPath&,
                           bool unique,
                           bool multi_entry) = 0;
  virtual void DeleteIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id) = 0;
  virtual void RenameIndex(long long transaction_id,
                           long long object_store_id,
                           long long index_id,
                           const String& new_name) = 0;

  static const long long kMinimumIndexId = 30;

  virtual void AddObserver(
      long long transaction_id,
      int32_t observer_id,
      bool include_transaction,
      bool no_records,
      bool values,
      const std::bitset<kWebIDBOperationTypeCount>& operation_types) = 0;
  virtual void RemoveObservers(
      const Vector<int32_t>& observer_ids_to_remove) = 0;
  virtual void Get(long long transaction_id,
                   long long object_store_id,
                   long long index_id,
                   const WebIDBKeyRange&,
                   bool key_only,
                   WebIDBCallbacks*) = 0;
  virtual void GetAll(long long transaction_id,
                      long long object_store_id,
                      long long index_id,
                      const WebIDBKeyRange&,
                      long long max_count,
                      bool key_only,
                      WebIDBCallbacks*) = 0;
  virtual void Put(long long transaction_id,
                   long long object_store_id,
                   const WebData& value,
                   const Vector<WebBlobInfo>&,
                   WebIDBKeyView primary_key,
                   WebIDBPutMode,
                   WebIDBCallbacks*,
                   const Vector<WebIDBIndexKeys>&) = 0;
  virtual void SetIndexKeys(long long transaction_id,
                            long long object_store_id,
                            WebIDBKeyView primary_key,
                            const Vector<WebIDBIndexKeys>&) = 0;
  virtual void SetIndexesReady(long long transaction_id,
                               long long object_store_id,
                               const Vector<int64_t>& index_ids) = 0;
  virtual void OpenCursor(long long transaction_id,
                          long long object_store_id,
                          long long index_id,
                          const WebIDBKeyRange&,
                          WebIDBCursorDirection,
                          bool key_only,
                          WebIDBTaskType,
                          WebIDBCallbacks*) = 0;
  virtual void Count(long long transaction_id,
                     long long object_store_id,
                     long long index_id,
                     const WebIDBKeyRange&,
                     WebIDBCallbacks*) = 0;
  virtual void Delete(long long transaction_id,
                      long long object_store_id,
                      WebIDBKeyView primary_key,
                      WebIDBCallbacks*) = 0;
  virtual void DeleteRange(long long transaction_id,
                           long long object_store_id,
                           const WebIDBKeyRange&,
                           WebIDBCallbacks*) = 0;
  virtual void Clear(long long transaction_id,
                     long long object_store_id,
                     WebIDBCallbacks*) = 0;

 protected:
  WebIDBDatabase() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_DATABASE_H_

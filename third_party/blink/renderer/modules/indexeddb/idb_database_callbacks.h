/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_CALLBACKS_H_

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DOMException;
class IDBDatabase;
class IDBObservation;

class MODULES_EXPORT IDBDatabaseCallbacks
    : public GarbageCollected<IDBDatabaseCallbacks> {
 public:
  IDBDatabaseCallbacks();
  virtual ~IDBDatabaseCallbacks();
  void Trace(blink::Visitor*);

  // IDBDatabaseCallbacks
  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(int64_t transaction_id, DOMException*);
  virtual void OnComplete(int64_t transaction_id);
  virtual void OnChanges(
      const WebIDBDatabaseCallbacks::ObservationIndexMap&,
      Vector<Persistent<IDBObservation>> observations,
      const WebIDBDatabaseCallbacks::TransactionMap& transactions);

  void Connect(IDBDatabase*);

  // Returns a new WebIDBDatabaseCallbacks for this object. Must only be
  // called once.
  std::unique_ptr<WebIDBDatabaseCallbacks> CreateWebCallbacks();
  void DetachWebCallbacks();
  void WebCallbacksDestroyed();

 private:
  // The initial IDBOpenDBRequest, final IDBDatabase, and/or
  // WebIDBDatabaseCallbacks have strong references to an IDBDatabaseCallbacks
  // object.
  // Oilpan: We'd like to delete an IDBDatabase object by a
  // GC. WebIDBDatabaseCallbacks can survive the GC, and IDBDatabaseCallbacks
  // can survive too. |database_| should be a weak reference to avoid that an
  // IDBDatabase survives the GC with the IDBDatabaseCallbacks.
  WeakMember<IDBDatabase> database_;

  // Pointer back to the WebIDBDatabaseCallbacks that holds a persistent
  // reference to this object.
  WebIDBDatabaseCallbacks* web_callbacks_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_CALLBACKS_H_

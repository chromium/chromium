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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_string_sequence.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store_parameters.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction_options.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

class DOMException;
class ExceptionState;
class ExecutionContext;
class IDBObservation;
class IDBObserver;

class MODULES_EXPORT IDBDatabase final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<IDBDatabase>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(IDBDatabase);
  DEFINE_WRAPPERTYPEINFO();

 public:
  IDBDatabase(ExecutionContext*,
              std::unique_ptr<WebIDBDatabase>,
              IDBDatabaseCallbacks*,
              v8::Isolate*);
  ~IDBDatabase() override;

  void Trace(blink::Visitor*) override;

  // Overwrites the database metadata, including object store and index
  // metadata. Used to pass metadata to the database when it is opened.
  void SetMetadata(const IDBDatabaseMetadata&);
  // Overwrites the database's own metadata, but does not change object store
  // and index metadata. Used to revert the database's metadata when a
  // versionchage transaction is aborted.
  void SetDatabaseMetadata(const IDBDatabaseMetadata&);
  void TransactionCreated(IDBTransaction*);
  void TransactionFinished(const IDBTransaction*);
  const String& GetObjectStoreName(int64_t object_store_id) const;
  int32_t AddObserver(IDBObserver*,
                      int64_t transaction_id,
                      bool include_transaction,
                      bool no_records,
                      bool values,
                      std::bitset<kIDBOperationTypeCount> operation_types);
  void RemoveObservers(const Vector<int32_t>& observer_ids);

  // Implement the IDL
  const String& name() const { return metadata_.name; }
  uint64_t version() const { return metadata_.version; }
  DOMStringList* objectStoreNames() const;

  IDBObjectStore* createObjectStore(const String& name,
                                    const IDBObjectStoreParameters* options,
                                    ExceptionState& exception_state) {
    return createObjectStore(name, IDBKeyPath(options->keyPath()),
                             options->autoIncrement(), exception_state);
  }
  IDBTransaction* transaction(ScriptState*,
                              const StringOrStringSequence& store_names,
                              const String& mode,
                              ExceptionState&);
  IDBTransaction* transaction(ScriptState*,
                              const StringOrStringSequence& store_names,
                              const String& mode,
                              const IDBTransactionOptions* options,
                              ExceptionState&);
  void deleteObjectStore(const String& name, ExceptionState&);
  void close();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(versionchange, kVersionchange)

  // IDBDatabaseCallbacks
  void OnVersionChange(int64_t old_version, int64_t new_version);
  void OnAbort(int64_t, DOMException*);
  void OnComplete(int64_t);
  void OnChanges(const WebIDBDatabaseCallbacks::ObservationIndexMap&,
                 Vector<Persistent<IDBObservation>> observations,
                 const WebIDBDatabaseCallbacks::TransactionMap& transactions);

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  bool IsClosePending() const { return close_pending_; }
  void ForceClose();
  const IDBDatabaseMetadata& Metadata() const { return metadata_; }
  void EnqueueEvent(Event*);

  int64_t FindObjectStoreId(const String& name) const;
  bool ContainsObjectStore(const String& name) const {
    return FindObjectStoreId(name) != IDBObjectStoreMetadata::kInvalidId;
  }
  void RenameObjectStore(int64_t store_id, const String& new_name);
  void RevertObjectStoreCreation(int64_t object_store_id);
  void RevertObjectStoreMetadata(
      scoped_refptr<IDBObjectStoreMetadata> old_metadata);

  // Will return nullptr if this database is stopped.
  WebIDBDatabase* Backend() const { return backend_.get(); }

  static int64_t NextTransactionId();
  static int32_t NextObserverId();

  static const char kCannotObserveVersionChangeTransaction[];
  static const char kIndexDeletedErrorMessage[];
  static const char kIndexNameTakenErrorMessage[];
  static const char kIsKeyCursorErrorMessage[];
  static const char kNoKeyOrKeyRangeErrorMessage[];
  static const char kNoSuchIndexErrorMessage[];
  static const char kNoSuchObjectStoreErrorMessage[];
  static const char kNoValueErrorMessage[];
  static const char kNotValidKeyErrorMessage[];
  static const char kNotVersionChangeTransactionErrorMessage[];
  static const char kObjectStoreDeletedErrorMessage[];
  static const char kObjectStoreNameTakenErrorMessage[];
  static const char kRequestNotFinishedErrorMessage[];
  static const char kSourceDeletedErrorMessage[];
  static const char kTransactionFinishedErrorMessage[];
  static const char kTransactionInactiveErrorMessage[];
  static const char kTransactionReadOnlyErrorMessage[];
  static const char kDatabaseClosedErrorMessage[];

 protected:
  // EventTarget
  DispatchEventResult DispatchEventInternal(Event&) override;

 private:
  IDBObjectStore* createObjectStore(const String& name,
                                    const IDBKeyPath&,
                                    bool auto_increment,
                                    ExceptionState&);
  void CloseConnection();

  IDBDatabaseMetadata metadata_;
  std::unique_ptr<WebIDBDatabase> backend_;
  Member<IDBTransaction> version_change_transaction_;
  HeapHashMap<int64_t, Member<IDBTransaction>> transactions_;
  HeapHashMap<int32_t, Member<IDBObserver>> observers_;

  bool close_pending_ = false;

  Member<EventQueue> event_queue_;

  Member<IDBDatabaseCallbacks> database_callbacks_;
  // Maintain the isolate so that all externally allocated memory can be
  // registered against it.
  v8::Isolate* isolate_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_DATABASE_H_

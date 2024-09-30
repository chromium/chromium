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

#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"

#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IDBIndex::IDBIndex(scoped_refptr<IDBIndexMetadata> metadata,
                   IDBObjectStore* object_store,
                   IDBTransaction* transaction)
    : metadata_(std::move(metadata)),
      object_store_(object_store),
      transaction_(transaction) {
  DCHECK(object_store_);
  DCHECK(transaction_);
  DCHECK(metadata_.get());
  DCHECK_NE(Id(), IDBIndexMetadata::kInvalidId);
}

IDBIndex::~IDBIndex() = default;

void IDBIndex::Trace(Visitor* visitor) const {
  visitor->Trace(object_store_);
  visitor->Trace(transaction_);
  ScriptWrappable::Trace(visitor);
}

void IDBIndex::setName(const String& name, ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBIndex::setName");
  if (!transaction_->IsVersionChange()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }

  if (this->name() == name)
    return;
  if (object_store_->ContainsIndex(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      IDBDatabase::kIndexNameTakenErrorMessage);
    return;
  }
  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  object_store_->RenameIndex(Id(), name);
}

ScriptValue IDBIndex::keyPath(ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     Metadata().key_path.ToV8(script_state));
}

void IDBIndex::RevertMetadata(scoped_refptr<IDBIndexMetadata> old_metadata) {
  metadata_ = std::move(old_metadata);

  // An index's metadata will only get reverted if the index was in the
  // database when the versionchange transaction started.
  deleted_ = false;
}

IDBRequest* IDBIndex::openCursor(ScriptState* script_state,
                                 const ScriptValue& range,
                                 const V8IDBCursorDirection& v8_direction,
                                 ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::openCursorRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kIndexOpenCursor);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  mojom::blink::IDBCursorDirection direction =
      IDBCursor::V8EnumToDirection(v8_direction.AsEnum());
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  return openCursor(script_state, key_range, direction, std::move(metrics));
}

IDBRequest* IDBIndex::openCursor(ScriptState* script_state,
                                 IDBKeyRange* key_range,
                                 mojom::blink::IDBCursorDirection direction,
                                 IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  request->SetCursorDetails(indexed_db::kCursorKeyAndValue, direction);
  db().OpenCursor(object_store_->Id(), Id(), key_range, direction, false,
                  mojom::blink::IDBTaskType::Normal, request);
  return request;
}

IDBRequest* IDBIndex::count(ScriptState* script_state,
                            const ScriptValue& range,
                            ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::countRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(IDBRequest::TypeForMetrics::kIndexCount);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  db().Count(transaction_->Id(), object_store_->Id(), Id(), key_range,
             WTF::BindOnce(&IDBRequest::OnCount, WrapWeakPersistent(request)));
  return request;
}

IDBRequest* IDBIndex::openKeyCursor(ScriptState* script_state,
                                    const ScriptValue& range,
                                    const V8IDBCursorDirection& v8_direction,
                                    ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::openKeyCursorRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kIndexOpenKeyCursor);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  mojom::blink::IDBCursorDirection direction =
      IDBCursor::V8EnumToDirection(v8_direction.AsEnum());
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  request->SetCursorDetails(indexed_db::kCursorKeyOnly, direction);
  db().OpenCursor(object_store_->Id(), Id(), key_range, direction, true,
                  mojom::blink::IDBTaskType::Normal, request);
  return request;
}

IDBRequest* IDBIndex::get(ScriptState* script_state,
                          const ScriptValue& key,
                          ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::getRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(IDBRequest::TypeForMetrics::kIndexGet);
  return GetInternal(script_state, key, exception_state, false,
                     std::move(metrics));
}

IDBRequest* IDBIndex::getAll(ScriptState* script_state,
                             const ScriptValue& range,
                             ExceptionState& exception_state) {
  return getAll(script_state, range, std::numeric_limits<uint32_t>::max(),
                exception_state);
}

IDBRequest* IDBIndex::getAll(ScriptState* script_state,
                             const ScriptValue& range,
                             uint32_t max_count,
                             ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::getAllRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(IDBRequest::TypeForMetrics::kIndexGetAll);
  return GetAllInternal(script_state, range, max_count, exception_state, false,
                        std::move(metrics));
}

IDBRequest* IDBIndex::getAllKeys(ScriptState* script_state,
                                 const ScriptValue& range,
                                 ExceptionState& exception_state) {
  return getAllKeys(script_state, range, std::numeric_limits<uint32_t>::max(),
                    exception_state);
}

IDBRequest* IDBIndex::getAllKeys(ScriptState* script_state,
                                 const ScriptValue& range,
                                 uint32_t max_count,
                                 ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::getAllKeysRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kIndexGetAllKeys);
  return GetAllInternal(script_state, range, max_count, exception_state,
                        /*key_only=*/true, std::move(metrics));
}

IDBRequest* IDBIndex::getKey(ScriptState* script_state,
                             const ScriptValue& key,
                             ExceptionState& exception_state) {
  TRACE_EVENT1("IndexedDB", "IDBIndex::getKeyRequestSetup", "index_name",
               metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics(IDBRequest::TypeForMetrics::kIndexGetKey);
  return GetInternal(script_state, key, exception_state, true,
                     std::move(metrics));
}

IDBRequest* IDBIndex::GetInternal(ScriptState* script_state,
                                  const ScriptValue& key,
                                  ExceptionState& exception_state,
                                  bool key_only,
                                  IDBRequest::AsyncTraceState metrics) {
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key_range) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        IDBDatabase::kNoKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }
  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  db().Get(transaction_->Id(), object_store_->Id(), Id(), key_range, key_only,
           WTF::BindOnce(&IDBRequest::OnGet, WrapPersistent(request)));
  return request;
}

IDBRequest* IDBIndex::GetAllInternal(ScriptState* script_state,
                                     const ScriptValue& range,
                                     uint32_t max_count,
                                     ExceptionState& exception_state,
                                     bool key_only,
                                     IDBRequest::AsyncTraceState metrics) {
  if (!max_count)
    max_count = std::numeric_limits<uint32_t>::max();

  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!db().IsConnectionOpen()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  db().GetAll(transaction_->Id(), object_store_->Id(), Id(), key_range,
              max_count, key_only, request);
  return request;
}

IDBDatabase& IDBIndex::db() {
  return transaction_->db();
}

}  // namespace blink

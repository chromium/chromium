// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_observation.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

IDBObservation::~IDBObservation() = default;

ScriptValue IDBObservation::key(ScriptState* script_state) {
  if (!key_range_)
    return ScriptValue::From(script_state,
                             v8::Undefined(script_state->GetIsolate()));

  return ScriptValue::From(script_state, key_range_);
}

ScriptValue IDBObservation::value(ScriptState* script_state) {
  return ScriptValue::From(script_state, value_);
}

mojom::IDBOperationType IDBObservation::StringToOperationType(
    const String& type) {
  if (type == indexed_db_names::kAdd)
    return mojom::IDBOperationType::Add;
  if (type == indexed_db_names::kPut)
    return mojom::IDBOperationType::Put;
  if (type == indexed_db_names::kDelete)
    return mojom::IDBOperationType::Delete;
  if (type == indexed_db_names::kClear)
    return mojom::IDBOperationType::Clear;

  NOTREACHED();
  return mojom::IDBOperationType::Add;
}

const String& IDBObservation::type() const {
  switch (operation_type_) {
    case mojom::IDBOperationType::Add:
      return indexed_db_names::kAdd;

    case mojom::IDBOperationType::Put:
      return indexed_db_names::kPut;

    case mojom::IDBOperationType::Delete:
      return indexed_db_names::kDelete;

    case mojom::IDBOperationType::Clear:
      return indexed_db_names::kClear;

    default:
      NOTREACHED();
      return indexed_db_names::kAdd;
  }
}

IDBObservation::IDBObservation(int64_t object_store_id,
                               mojom::IDBOperationType type,
                               IDBKeyRange* key_range,
                               std::unique_ptr<IDBValue> value)
    : object_store_id_(object_store_id),
      operation_type_(type),
      key_range_(key_range) {
  value_ = IDBAny::Create(std::move(value));
}

void IDBObservation::SetIsolate(v8::Isolate* isolate) {
  DCHECK(value_ && value_->Value());
  value_->Value()->SetIsolate(isolate);
}

void IDBObservation::Trace(blink::Visitor* visitor) {
  visitor->Trace(key_range_);
  visitor->Trace(value_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

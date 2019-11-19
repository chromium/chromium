// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_observer_changes.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_observation.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptValue IDBObserverChanges::records(ScriptState* script_state) {
  v8::Local<v8::Context> context(script_state->GetContext());
  v8::Isolate* isolate(script_state->GetIsolate());
  v8::Local<v8::Map> map = v8::Map::New(isolate);
  for (const auto& it : records_) {
    v8::Local<v8::String> key =
        V8String(isolate, database_->GetObjectStoreName(it.key));
    v8::Local<v8::Value> value = ToV8(it.value, context->Global(), isolate);
    map->Set(context, key, value).ToLocalChecked();
  }
  return ScriptValue::From(script_state, map);
}

IDBObserverChanges::IDBObserverChanges(
    IDBDatabase* database,
    IDBTransaction* transaction,
    const Vector<Persistent<IDBObservation>>& observations,
    const Vector<int32_t>& observation_indices)
    : database_(database), transaction_(transaction) {
  ExtractChanges(observations, observation_indices);
}

void IDBObserverChanges::ExtractChanges(
    const Vector<Persistent<IDBObservation>>& observations,
    const Vector<int32_t>& observation_indices) {
  // TODO(dmurph): Avoid getting and setting repeated times.
  for (const auto& idx : observation_indices) {
    records_
        .insert(observations[idx]->object_store_id(),
                HeapVector<Member<IDBObservation>>())
        .stored_value->value.emplace_back(observations[idx]);
  }
}

void IDBObserverChanges::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_);
  visitor->Trace(transaction_);
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

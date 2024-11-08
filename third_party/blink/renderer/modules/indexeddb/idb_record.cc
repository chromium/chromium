// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_record.h"

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "v8/include/v8.h"

namespace blink {

IDBRecord::IDBRecord(std::unique_ptr<IDBKey> primary_key,
                     std::unique_ptr<IDBValue> value,
                     std::unique_ptr<IDBKey> index_key)
    : primary_key_(std::move(primary_key)),
      value_(std::move(value)),
      index_key_(std::move(index_key)) {}

IDBRecord::~IDBRecord() = default;

ScriptValue IDBRecord::key(ScriptState* script_state) {
  key_dirty_ = false;
  IDBKey* key = index_key_ ? index_key_.get() : primary_key_.get();
  return ScriptValue(script_state->GetIsolate(), key->ToV8(script_state));
}

ScriptValue IDBRecord::primaryKey(ScriptState* script_state) {
  primary_key_dirty_ = false;
  return ScriptValue(script_state->GetIsolate(),
                     primary_key_->ToV8(script_state));
}

ScriptValue IDBRecord::value(ScriptState* script_state) {
  value_dirty_ = false;
  return ScriptValue(script_state->GetIsolate(),
                     DeserializeIDBValue(script_state, value_.get()));
}

bool IDBRecord::isKeyDirty() const {
  return key_dirty_;
}

bool IDBRecord::isPrimaryKeyDirty() const {
  return primary_key_dirty_;
}

bool IDBRecord::isValueDirty() const {
  return value_dirty_;
}

}  // namespace blink

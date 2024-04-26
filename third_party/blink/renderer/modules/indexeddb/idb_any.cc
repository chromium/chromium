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

#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"

namespace blink {

IDBAny::IDBAny(Type type) : type_(type) {
  DCHECK(type == kUndefinedType || type == kNullType);
}

IDBAny::~IDBAny() = default;

void IDBAny::ContextWillBeDestroyed() {
  if (idb_cursor_)
    idb_cursor_->ContextWillBeDestroyed();
}

IDBCursor* IDBAny::IdbCursor() const {
  DCHECK_EQ(type_, kIDBCursorType);
  SECURITY_DCHECK(idb_cursor_->IsKeyCursor());
  return idb_cursor_.Get();
}

IDBCursorWithValue* IDBAny::IdbCursorWithValue() const {
  DCHECK_EQ(type_, kIDBCursorWithValueType);
  SECURITY_DCHECK(IsA<IDBCursorWithValue>(idb_cursor_.Get()));
  return To<IDBCursorWithValue>(idb_cursor_.Get());
}

IDBDatabase* IDBAny::IdbDatabase() const {
  DCHECK_EQ(type_, kIDBDatabaseType);
  return idb_database_.Get();
}

const IDBKey* IDBAny::Key() const {
  // If type is IDBValueType then instead use value()->primaryKey().
  DCHECK_EQ(type_, kKeyType);
  return idb_key_.get();
}

IDBValue* IDBAny::Value() const {
  DCHECK_EQ(type_, kIDBValueType);
  return idb_value_.get();
}

const Vector<std::unique_ptr<IDBValue>>& IDBAny::Values() const {
  DCHECK_EQ(type_, kIDBValueArrayType);
  return idb_values_;
}

int64_t IDBAny::Integer() const {
  DCHECK_EQ(type_, kIntegerType);
  return integer_;
}

v8::Local<v8::Value> IDBAny::ToV8(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  switch (type_) {
    case IDBAny::kUndefinedType:
      return v8::Undefined(isolate);
    case IDBAny::kNullType:
      return v8::Null(isolate);
    case IDBAny::kIDBCursorType:
      return ToV8Traits<IDBCursor>::ToV8(script_state, IdbCursor());
    case IDBAny::kIDBCursorWithValueType:
      return ToV8Traits<IDBCursorWithValue>::ToV8(script_state,
                                                  IdbCursorWithValue());
    case IDBAny::kIDBDatabaseType:
      return ToV8Traits<IDBDatabase>::ToV8(script_state, IdbDatabase());
    case IDBAny::kIDBValueType:
      return DeserializeIDBValue(script_state, Value());
    case IDBAny::kIDBValueArrayType:
      return DeserializeIDBValueArray(script_state, Values());
    case IDBAny::kIntegerType:
      return v8::Number::New(isolate, Integer());
    case IDBAny::kKeyType:
      return Key()->ToV8(script_state);
  }
}

IDBAny::IDBAny(IDBCursor* value)
    : type_(IsA<IDBCursorWithValue>(value) ? kIDBCursorWithValueType
                                           : kIDBCursorType),
      idb_cursor_(value) {}

IDBAny::IDBAny(IDBDatabase* value)
    : type_(kIDBDatabaseType), idb_database_(value) {}

IDBAny::IDBAny(Vector<std::unique_ptr<IDBValue>> values)
    : type_(kIDBValueArrayType), idb_values_(std::move(values)) {}

IDBAny::IDBAny(std::unique_ptr<IDBValue> value)
    : type_(kIDBValueType), idb_value_(std::move(value)) {}

IDBAny::IDBAny(std::unique_ptr<IDBKey> key)
    : type_(kKeyType), idb_key_(std::move(key)) {}

IDBAny::IDBAny(int64_t value) : type_(kIntegerType), integer_(value) {}

void IDBAny::Trace(Visitor* visitor) const {
  visitor->Trace(idb_cursor_);
  visitor->Trace(idb_database_);
}

}  // namespace blink

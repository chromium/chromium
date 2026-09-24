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
#include <variant>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"

namespace blink {

IDBAny::IDBAny() = default;

IDBAny::~IDBAny() = default;

void IDBAny::ContextWillBeDestroyed() {
  if (const auto* cursor = std::get_if<Member<IDBCursor>>(&value_);
      cursor && *cursor) {
    (*cursor)->ContextWillBeDestroyed();
  }
}

IDBAny::Type IDBAny::GetType() const {
  return std::visit(
      absl::Overload{
          [](std::monostate) { return kUndefinedType; },
          [](const Member<IDBCursor>& cursor) {
            return cursor && cursor->IsCursorWithValue()
                       ? kIDBCursorWithValueType
                       : kIDBCursorType;
          },
          [](const Member<IDBDatabase>&) { return kIDBDatabaseType; },
          [](const std::unique_ptr<IDBKey>&) { return kKeyType; },
          [](const std::unique_ptr<IDBValue>&) { return kIDBValueType; },
          [](const Vector<std::unique_ptr<IDBValue>>&) {
            return kIDBValueArrayType;
          },
          [](const std::unique_ptr<IDBRecordArray>&) {
            return kIDBRecordArrayType;
          },
          [](int64_t) { return kIntegerType; },
      },
      value_);
}

IDBCursor* IDBAny::IdbCursor() const {
  const auto& cursor = std::get<Member<IDBCursor>>(value_);
  SECURITY_DCHECK(cursor->IsKeyCursor());
  return cursor.Get();
}

IDBCursorWithValue* IDBAny::IdbCursorWithValue() const {
  const auto& cursor = std::get<Member<IDBCursor>>(value_);
  SECURITY_DCHECK(IsA<IDBCursorWithValue>(cursor.Get()));
  return To<IDBCursorWithValue>(cursor.Get());
}

IDBDatabase* IDBAny::IdbDatabase() const {
  return std::get<Member<IDBDatabase>>(value_).Get();
}

const IDBKey* IDBAny::Key() const {
  // If type is IDBValueType then instead use value()->primaryKey().
  return std::get<std::unique_ptr<IDBKey>>(value_).get();
}

IDBValue* IDBAny::Value() const {
  return std::get<std::unique_ptr<IDBValue>>(value_).get();
}

const Vector<std::unique_ptr<IDBValue>>& IDBAny::Values() const {
  return std::get<Vector<std::unique_ptr<IDBValue>>>(value_);
}

const IDBRecordArray& IDBAny::Records() const {
  const auto& records = std::get<std::unique_ptr<IDBRecordArray>>(value_);
  CHECK(records);
  return *records;
}

int64_t IDBAny::Integer() const {
  return std::get<int64_t>(value_);
}

v8::Local<v8::Value> IDBAny::ToV8(ScriptState* script_state) {
  return std::visit(
      absl::Overload{
          [script_state](std::monostate) -> v8::Local<v8::Value> {
            return v8::Undefined(script_state->GetIsolate());
          },
          [script_state](
              const Member<IDBCursor>& cursor) -> v8::Local<v8::Value> {
            if (cursor->IsCursorWithValue()) {
              return ToV8Traits<IDBCursorWithValue>::ToV8(
                  script_state, To<IDBCursorWithValue>(cursor.Get()));
            }
            return ToV8Traits<IDBCursor>::ToV8(script_state, cursor.Get());
          },
          [script_state](
              const Member<IDBDatabase>& database) -> v8::Local<v8::Value> {
            return ToV8Traits<IDBDatabase>::ToV8(script_state, database.Get());
          },
          [script_state](const std::unique_ptr<IDBKey>& key)
              -> v8::Local<v8::Value> { return key->ToV8(script_state); },
          [script_state](
              const std::unique_ptr<IDBValue>& value) -> v8::Local<v8::Value> {
            return DeserializeIDBValue(script_state, value.get());
          },
          [script_state](const Vector<std::unique_ptr<IDBValue>>& values)
              -> v8::Local<v8::Value> {
            return DeserializeIDBValueArray(script_state, values);
          },
          [script_state](int64_t integer) -> v8::Local<v8::Value> {
            return v8::Number::New(script_state->GetIsolate(), integer);
          },
          [script_state](std::unique_ptr<IDBRecordArray>& records)
              -> v8::Local<v8::Value> {
            // `IDBAny` must not convert `records` multiple times. `ToV8()`
            // consumes `records`.
            CHECK(records);

            v8::Local<v8::Value> v8_value =
                IDBRecordArray::ToV8(script_state, std::move(*records));
            records.reset();
            return v8_value;
          },
      },
      value_);
}

IDBAny::IDBAny(IDBCursor* value) : value_(Member<IDBCursor>(value)) {}

IDBAny::IDBAny(IDBDatabase* value) : value_(Member<IDBDatabase>(value)) {}

IDBAny::IDBAny(Vector<std::unique_ptr<IDBValue>> values)
    : value_(std::move(values)) {}

IDBAny::IDBAny(std::unique_ptr<IDBValue> value) : value_(std::move(value)) {}

IDBAny::IDBAny(std::unique_ptr<IDBKey> key) : value_(std::move(key)) {}

IDBAny::IDBAny(int64_t value) : value_(value) {}

IDBAny::IDBAny(IDBRecordArray idb_records)
    : value_(std::make_unique<IDBRecordArray>(std::move(idb_records))) {}

void IDBAny::Trace(Visitor* visitor) const {
  std::visit(absl::Overload{
                 [visitor](const Member<IDBCursor>& cursor) {
                   visitor->Trace(cursor);
                 },
                 [visitor](const Member<IDBDatabase>& database) {
                   visitor->Trace(database);
                 },
                 [](const auto&) {},
             },
             value_);
}

}  // namespace blink

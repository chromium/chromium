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

#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_string_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_cursor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_cursor_with_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_database.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_index.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_key_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_object_store.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static v8::Local<v8::Value> DeserializeIDBValueData(v8::Isolate*,
                                                    const IDBValue*);
static v8::Local<v8::Value> DeserializeIDBValueArray(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context,
    const Vector<std::unique_ptr<IDBValue>>&);
static v8::Local<v8::Value> DeserializeIDBValueArrayArray(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const Vector<Vector<std::unique_ptr<IDBValue>>>&);

v8::Local<v8::Value> ToV8(const IDBKeyPath& value,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate* isolate) {
  switch (value.GetType()) {
    case mojom::IDBKeyPathType::Null:
      return v8::Null(isolate);
    case mojom::IDBKeyPathType::String:
      return V8String(isolate, value.GetString());
    case mojom::IDBKeyPathType::Array:
      return ToV8(value.Array(), creation_context, isolate);
  }
  NOTREACHED();
  return v8::Undefined(isolate);
}

v8::Local<v8::Value> ToV8(const IDBKey* key,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate* isolate) {
  if (!key) {
    // The IndexedDB spec requires that absent keys appear as attribute
    // values as undefined, rather than the more typical (for DOM) null.
    // This appears on the |upper| and |lower| attributes of IDBKeyRange.
    // Spec: http://www.w3.org/TR/IndexedDB/#idl-def-IDBKeyRange
    return v8::Local<v8::Value>();
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  switch (key->GetType()) {
    case mojom::IDBKeyType::Invalid:
    case mojom::IDBKeyType::Min:
      NOTREACHED();
      return v8::Local<v8::Value>();
    case mojom::IDBKeyType::None:
      return v8::Null(isolate);
    case mojom::IDBKeyType::Number:
      return v8::Number::New(isolate, key->Number());
    case mojom::IDBKeyType::String:
      return V8String(isolate, key->GetString());
    case mojom::IDBKeyType::Binary:
      // https://w3c.github.io/IndexedDB/#convert-a-value-to-a-key
      return ToV8(DOMArrayBuffer::Create(key->Binary()), creation_context,
                  isolate);
    case mojom::IDBKeyType::Date:
      return v8::Date::New(context, key->Date()).ToLocalChecked();
    case mojom::IDBKeyType::Array: {
      v8::Local<v8::Array> array = v8::Array::New(isolate, key->Array().size());
      for (wtf_size_t i = 0; i < key->Array().size(); ++i) {
        v8::Local<v8::Value> value =
            ToV8(key->Array()[i].get(), creation_context, isolate);
        if (value.IsEmpty())
          value = v8::Undefined(isolate);
        bool created_property;
        if (!array->CreateDataProperty(context, i, value)
                 .To(&created_property) ||
            !created_property)
          return v8::Local<v8::Value>();
      }
      return array;
    }
  }

  NOTREACHED();
  return v8::Local<v8::Value>();
}

// IDBAny is a variant type used to hold the values produced by the |result|
// attribute of IDBRequest and (as a convenience) the |source| attribute of
// IDBRequest and IDBCursor.
// TODO(jsbell): Replace the use of IDBAny for |source| attributes (which are
// ScriptWrappable types) using unions per IDL.
v8::Local<v8::Value> ToV8(const IDBAny* impl,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate* isolate) {
  if (!impl)
    return v8::Null(isolate);

  switch (impl->GetType()) {
    case IDBAny::kUndefinedType:
      return v8::Undefined(isolate);
    case IDBAny::kNullType:
      return v8::Null(isolate);
    case IDBAny::kIDBCursorType:
      return ToV8(impl->IdbCursor(), creation_context, isolate);
    case IDBAny::kIDBCursorWithValueType:
      return ToV8(impl->IdbCursorWithValue(), creation_context, isolate);
    case IDBAny::kIDBDatabaseType:
      return ToV8(impl->IdbDatabase(), creation_context, isolate);
    case IDBAny::kIDBValueType:
      return DeserializeIDBValue(isolate, creation_context, impl->Value());
    case IDBAny::kIDBValueArrayType:
      return DeserializeIDBValueArray(isolate, creation_context,
                                      impl->Values());
    case IDBAny::kIntegerType:
      return v8::Number::New(isolate, impl->Integer());
    case IDBAny::kKeyType:
      return ToV8(impl->Key(), creation_context, isolate);
    case IDBAny::kIDBValueArrayArrayType:
      return DeserializeIDBValueArrayArray(isolate, creation_context,
                                           impl->ValuesArray());
  }

  NOTREACHED();
  return v8::Undefined(isolate);
}

// Convert a simple (non-Array) script value to an Indexed DB key. If the
// conversion fails due to a detached buffer, an exception is thrown. If
// the value can't be converted into a key, an 'Invalid' key is returned. This
// is used to implement part of the following spec algorithm:
// https://w3c.github.io/IndexedDB/#convert-value-to-key
// A V8 exception may be thrown on bad data or by script's getters; if so,
// callers should not make further V8 calls.
static std::unique_ptr<IDBKey> CreateIDBKeyFromSimpleValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DCHECK(!value->IsArray());

  if (value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value()))
    return IDBKey::CreateNumber(value.As<v8::Number>()->Value());

  if (value->IsString())
    return IDBKey::CreateString(ToCoreString(value.As<v8::String>()));

  if (value->IsDate() && !std::isnan(value.As<v8::Date>()->ValueOf()))
    return IDBKey::CreateDate(value.As<v8::Date>()->ValueOf());

  if (value->IsArrayBuffer()) {
    DOMArrayBuffer* buffer = NativeValueTraits<DOMArrayBuffer>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return IDBKey::CreateInvalid();
    if (buffer->IsDetached()) {
      exception_state.ThrowTypeError("The ArrayBuffer is detached.");
      return IDBKey::CreateInvalid();
    }
    const char* start = static_cast<const char*>(buffer->Data());
    size_t length = buffer->ByteLength();
    return IDBKey::CreateBinary(SharedBuffer::Create(start, length));
  }

  if (value->IsArrayBufferView()) {
    DOMArrayBufferView* view =
        NativeValueTraits<MaybeShared<DOMArrayBufferView>>::NativeValue(
            isolate, value, exception_state)
            .Get();
    if (exception_state.HadException())
      return IDBKey::CreateInvalid();
    if (view->buffer()->IsDetached()) {
      exception_state.ThrowTypeError("The viewed ArrayBuffer is detached.");
      return IDBKey::CreateInvalid();
    }
    const char* start = static_cast<const char*>(view->BaseAddress());
    size_t length = view->byteLength();
    return IDBKey::CreateBinary(SharedBuffer::Create(start, length));
  }

  return IDBKey::CreateInvalid();
}

// Convert a script value to an Indexed DB key. If the result cannot be
// converted, an 'Invalid' key is returned. If an array is being
// converted, and a potential subkey does not exist, then the array is
// returned but with an 'Invalid' entry; this is used for "multi-entry"
// indexes where an array with invalid members is permitted will later be
// sanitized. This is used to implement both of the following spec algorithms:
// https://w3c.github.io/IndexedDB/#convert-value-to-key
// https://w3c.github.io/IndexedDB/#convert-a-value-to-a-multientry-key
// A V8 exception may be thrown on bad data or by script's getters; if so,
// callers should not make further V8 calls.
static std::unique_ptr<IDBKey> CreateIDBKeyFromValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  // Simple case:
  if (!value->IsArray())
    return CreateIDBKeyFromSimpleValue(isolate, value, exception_state);

  // Recursion is done on the heap rather than the stack.
  struct Record {
    Record(v8::Local<v8::Array> array)
        : array(std::move(array)), length(array->Length()) {
      subkeys.ReserveInitialCapacity(length);
    }
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    // Array being converted.
    v8::Local<v8::Array> array;
    // Length of |array|. Snapshotted (per spec), since getters may alter it.
    uint32_t length;
    // Converted sub-keys.
    IDBKey::KeyArray subkeys;
  };

  // Recursion stack.
  Vector<std::unique_ptr<Record>> stack;

  // Tracks seen arrays, to detect circular references and abort (per spec).
  Vector<v8::Local<v8::Array>> seen;

  // Initial state.
  {
    v8::Local<v8::Array> array = value.As<v8::Array>();
    if (array->Length() > IndexedDBKey::kMaximumArraySize)
      return IDBKey::CreateInvalid();

    stack.push_back(std::make_unique<Record>(array));
    seen.push_back(array);
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::TryCatch try_block(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  // Process stack - will return when complete.
  while (true) {
    DCHECK(!stack.empty());
    Record* top = stack.back().get();
    const wtf_size_t item_index = top->subkeys.size();

    // Done this array?
    if (item_index == top->length) {
      std::unique_ptr<IDBKey> key =
          IDBKey::CreateArray(std::move(top->subkeys));
      seen.pop_back();
      stack.pop_back();

      if (stack.empty())
        return key;
      top = stack.back().get();
      top->subkeys.push_back(std::move(key));
      continue;
    }

    // Process next item from current array.
    bool has_own_property;
    if (!top->array->HasOwnProperty(context, item_index)
             .To(&has_own_property)) {
      exception_state.RethrowV8Exception(try_block.Exception());
      return IDBKey::CreateInvalid();
    }
    if (!has_own_property)
      return IDBKey::CreateInvalid();
    v8::Local<v8::Value> item;
    if (!top->array->Get(context, item_index).ToLocal(&item)) {
      exception_state.RethrowV8Exception(try_block.Exception());
      return IDBKey::CreateInvalid();
    }

    if (!item->IsArray()) {
      // A non-array: convert it directly.
      auto key = CreateIDBKeyFromSimpleValue(isolate, item, exception_state);
      if (exception_state.HadException()) {
        DCHECK(!try_block.HasCaught());
        return IDBKey::CreateInvalid();
      }
      top->subkeys.push_back(std::move(key));
    } else {
      // A sub-array; push onto the stack and start processing it.
      v8::Local<v8::Array> array = item.As<v8::Array>();
      if (seen.Contains(array) || stack.size() >= IndexedDBKey::kMaximumDepth ||
          array->Length() > IndexedDBKey::kMaximumArraySize) {
        return IDBKey::CreateInvalid();
      }

      stack.push_back(std::make_unique<Record>(array));
      seen.push_back(array);
    }
  }
}

// Indexed DB key paths should apply to explicitly copied properties (that
// will be "own" properties when deserialized) as well as the following.
// http://www.w3.org/TR/IndexedDB/#key-path-construct
static bool IsImplicitProperty(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               const String& name) {
  if (value->IsString() && name == "length")
    return true;
  if (value->IsArray() && name == "length")
    return true;
  if (V8Blob::HasInstance(isolate, value)) {
    return name == "size" || name == "type";
  }
  if (V8File::HasInstance(isolate, value)) {
    return name == "name" || name == "lastModified" ||
           name == "lastModifiedDate";
  }
  return false;
}

// Assumes a valid key path.
static Vector<String> ParseKeyPath(const String& key_path) {
  Vector<String> elements;
  IDBKeyPathParseError error;
  IDBParseKeyPath(key_path, elements, error);
  DCHECK_EQ(error, kIDBKeyPathParseErrorNone);
  return elements;
}

// Evaluate a key path string against a value and return a key. It must be
// called repeatedly for array-type key paths. Failure in the evaluation steps
// (per spec) is represented by returning nullptr. Failure to convert the result
// to a key is representing by returning an 'Invalid' key. (An array with
// invalid members will be returned if needed.)
// https://w3c.github.io/IndexedDB/#evaluate-a-key-path-on-a-value
// A V8 exception may be thrown on bad data or by script's getters; if so,
// callers should not make further V8 calls.
static std::unique_ptr<IDBKey> CreateIDBKeyFromValueAndKeyPath(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    const String& key_path,
    ExceptionState& exception_state) {
  Vector<String> key_path_elements = ParseKeyPath(key_path);
  DCHECK(isolate->InContext());

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::TryCatch block(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  for (wtf_size_t i = 0; i < key_path_elements.size(); ++i) {
    const String& element = key_path_elements[i];

    // Special cases from https://w3c.github.io/IndexedDB/#key-path-construct
    // These access special or non-own properties directly, to avoid side
    // effects.

    if (v8_value->IsString() && element == "length") {
      int32_t length = v8_value.As<v8::String>()->Length();
      v8_value = v8::Number::New(isolate, length);
      continue;
    }

    if (v8_value->IsArray() && element == "length") {
      int32_t length = v8_value.As<v8::Array>()->Length();
      v8_value = v8::Number::New(isolate, length);
      continue;
    }

    if (!v8_value->IsObject())
      return nullptr;
    v8::Local<v8::Object> object = v8_value.As<v8::Object>();

    if (Blob* blob = V8Blob::ToWrappable(isolate, object)) {
      if (element == "size") {
        v8_value = v8::Number::New(isolate, blob->size());
        continue;
      }
      if (element == "type") {
        v8_value = V8String(isolate, blob->type());
        continue;
      }
      // Fall through.
    }

    if (File* file = V8File::ToWrappable(isolate, object)) {
      if (element == "name") {
        v8_value = V8String(isolate, file->name());
        continue;
      }
      if (element == "lastModified") {
        v8_value = v8::Number::New(isolate, file->lastModified());
        continue;
      }
      if (element == "lastModifiedDate") {
        v8_value = file->lastModifiedDate(ScriptState::From(context)).V8Value();
        continue;
      }
      // Fall through.
    }

    v8::Local<v8::String> key = V8String(isolate, element);
    bool has_own_property;
    if (!object->HasOwnProperty(context, key).To(&has_own_property)) {
      exception_state.RethrowV8Exception(block.Exception());
      return nullptr;
    }
    if (!has_own_property)
      return nullptr;
    if (!object->Get(context, key).ToLocal(&v8_value)) {
      exception_state.RethrowV8Exception(block.Exception());
      return nullptr;
    }
  }
  return CreateIDBKeyFromValue(isolate, v8_value, exception_state);
}

// Evaluate a key path against a value and return a key. For string-type
// paths, nullptr is returned if evaluation of the path fails, and an
// 'Invalid' key if evaluation succeeds but conversion fails. For array-type
// paths, nullptr is returned if evaluation of any sub-path fails, otherwise
// an array key is returned (with potentially 'Invalid' members).
// https://w3c.github.io/IndexedDB/#evaluate-a-key-path-on-a-value
// A V8 exception may be thrown on bad data or by script's getters; if so,
// callers should not make further V8 calls.
static std::unique_ptr<IDBKey> CreateIDBKeyFromValueAndKeyPath(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const IDBKeyPath& key_path,
    ExceptionState& exception_state) {
  DCHECK(!key_path.IsNull());
  v8::HandleScope handle_scope(isolate);
  if (key_path.GetType() == mojom::IDBKeyPathType::Array) {
    const Vector<String>& array = key_path.Array();
    IDBKey::KeyArray result;
    result.ReserveInitialCapacity(array.size());
    for (const String& path : array) {
      auto key = CreateIDBKeyFromValueAndKeyPath(isolate, value, path,
                                                 exception_state);
      if (exception_state.HadException())
        return nullptr;
      // Evaluation of path failed - overall failure.
      if (!key)
        return nullptr;
      result.emplace_back(std::move(key));
    }
    // An array key is always returned, even if members may be invalid.
    return IDBKey::CreateArray(std::move(result));
  }

  DCHECK_EQ(key_path.GetType(), mojom::IDBKeyPathType::String);
  return CreateIDBKeyFromValueAndKeyPath(isolate, value, key_path.GetString(),
                                         exception_state);
}

// Evaluate an index's key path against a value and return a key. This
// handles the special case for indexes where a compound key path
// may result in "holes", depending on the store's properties.
// Otherwise, nullptr is returned.
// https://w3c.github.io/IndexedDB/#evaluate-a-key-path-on-a-value
// A V8 exception may be thrown on bad data or by script's getters; if so,
// callers should not make further V8 calls.
static std::unique_ptr<IDBKey> CreateIDBKeyFromValueAndKeyPaths(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const IDBKeyPath& store_key_path,
    const IDBKeyPath& index_key_path,
    ExceptionState& exception_state) {
  DCHECK(!index_key_path.IsNull());
  v8::HandleScope handle_scope(isolate);
  if (index_key_path.GetType() == mojom::IDBKeyPathType::Array) {
    const Vector<String>& array = index_key_path.Array();
    const bool uses_inline_keys =
        store_key_path.GetType() == mojom::IDBKeyPathType::String;
    IDBKey::KeyArray result;
    result.ReserveInitialCapacity(array.size());
    for (const String& path : array) {
      auto key = CreateIDBKeyFromValueAndKeyPath(isolate, value, path,
                                                 exception_state);
      if (exception_state.HadException())
        return nullptr;
      if (!key && uses_inline_keys && store_key_path.GetString() == path) {
        // Compound keys that include the store's inline primary key which
        // will be generated lazily are represented as "holes".
        key = IDBKey::CreateNone();
      } else if (!key) {
        // Key path evaluation failed.
        return nullptr;
      } else if (!key->IsValid()) {
        // An Invalid key is returned if not valid in this case (but not the
        // other CreateIDBKeyFromValueAndKeyPath function) because:
        // * Invalid members are only allowed for multi-entry arrays.
        // * Array key paths can't be multi-entry.
        return IDBKey::CreateInvalid();
      }

      result.emplace_back(std::move(key));
    }
    return IDBKey::CreateArray(std::move(result));
  }

  DCHECK_EQ(index_key_path.GetType(), mojom::IDBKeyPathType::String);
  return CreateIDBKeyFromValueAndKeyPath(
      isolate, value, index_key_path.GetString(), exception_state);
}

// Deserialize just the value data & blobInfo from the given IDBValue.
//
// Primary key injection is performed in deserializeIDBValue() below.
static v8::Local<v8::Value> DeserializeIDBValueData(v8::Isolate* isolate,
                                                    const IDBValue* value) {
  DCHECK(isolate->InContext());
  if (!value || value->IsNull())
    return v8::Null(isolate);

  scoped_refptr<SerializedScriptValue> serialized_value =
      value->CreateSerializedValue();

  serialized_value->FileSystemAccessTokens() =
      std::move(const_cast<IDBValue*>(value)->FileSystemAccessTokens());

  SerializedScriptValue::DeserializeOptions options;
  options.blob_info = &value->BlobInfo();

  // deserialize() returns null when serialization fails.  This is sub-optimal
  // because IndexedDB values can be null, so an application cannot distinguish
  // between a de-serialization failure and a legitimately stored null value.
  //
  // TODO(crbug.com/703704): Ideally, SerializedScriptValue should return an
  // empty handle on serialization errors, which should be handled by higher
  // layers. For example, IndexedDB could throw an exception, abort the
  // transaction, or close the database connection.
  return serialized_value->Deserialize(isolate, options);
}

// Deserialize the entire IDBValue.
//
// On top of deserializeIDBValueData(), this handles the special case of having
// to inject a key into the de-serialized value. See injectV8KeyIntoV8Value()
// for details.
v8::Local<v8::Value> DeserializeIDBValue(v8::Isolate* isolate,
                                         v8::Local<v8::Object> creation_context,
                                         const IDBValue* value) {
  DCHECK(isolate->InContext());
  if (!value || value->IsNull())
    return v8::Null(isolate);

  v8::Local<v8::Value> v8_value = DeserializeIDBValueData(isolate, value);
  if (value->PrimaryKey()) {
    v8::Local<v8::Value> key =
        ToV8(value->PrimaryKey(), creation_context, isolate);
    if (key.IsEmpty())
      return v8::Local<v8::Value>();

    InjectV8KeyIntoV8Value(isolate, key, v8_value, value->KeyPath());

    // TODO(crbug.com/703704): Throw an error here or at a higher layer if
    // injectV8KeyIntoV8Value() returns false, which means that the serialized
    // value got corrupted while on disk.
  }

  return v8_value;
}

static v8::Local<v8::Value> DeserializeIDBValueArray(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const Vector<std::unique_ptr<IDBValue>>& values) {
  DCHECK(isolate->InContext());

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> array = v8::Array::New(isolate, values.size());
  for (wtf_size_t i = 0; i < values.size(); ++i) {
    v8::Local<v8::Value> v8_value =
        DeserializeIDBValue(isolate, creation_context, values[i].get());
    if (v8_value.IsEmpty())
      v8_value = v8::Undefined(isolate);
    bool created_property;
    if (!array->CreateDataProperty(context, i, v8_value)
             .To(&created_property) ||
        !created_property)
      return v8::Local<v8::Value>();
  }

  return array;
}

static v8::Local<v8::Value> DeserializeIDBValueArrayArray(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const Vector<Vector<std::unique_ptr<IDBValue>>>& all_values) {
  DCHECK(isolate->InContext());

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> array = v8::Array::New(isolate, all_values.size());
  for (wtf_size_t i = 0; i < all_values.size(); ++i) {
    v8::Local<v8::Value> v8_values =
        DeserializeIDBValueArray(isolate, creation_context, all_values[i]);
    if (v8_values.IsEmpty())
      v8_values = v8::Undefined(isolate);
    bool created_property;
    if (!array->CreateDataProperty(context, i, v8_values)
             .To(&created_property) ||
        !created_property)
      return v8::Local<v8::Value>();
  }

  return array;
}

// Injects a primary key into a deserialized V8 value.
//
// In general, the value stored in IndexedDB is the serialized version of a
// value passed to the API. However, the specification has a special case of
// object stores that specify a key path and have a key generator. In this case,
// the conceptual description in the spec states that the key produced by the
// key generator is injected into the value before it is written to IndexedDB.
//
// We cannot implement the spec's conceptual description. We need to assign
// primary keys in the browser process, to ensure that multiple renderer
// processes talking to the same database receive sequential keys. At the same
// time, we want the value serialization code to live in the renderer process,
// because this type of code is a likely victim to security exploits.
//
// We handle this special case by serializing and writing values without the
// corresponding keys. At read time, we obtain the keys and the values
// separately, and we inject the keys into values.
bool InjectV8KeyIntoV8Value(v8::Isolate* isolate,
                            v8::Local<v8::Value> key,
                            v8::Local<v8::Value> value,
                            const IDBKeyPath& key_path) {
  TRACE_EVENT0("IndexedDB", "injectIDBV8KeyIntoV8Value");
  DCHECK(isolate->InContext());

  DCHECK_EQ(key_path.GetType(), mojom::IDBKeyPathType::String);
  Vector<String> key_path_elements = ParseKeyPath(key_path.GetString());

  // The conbination of a key generator and an empty key path is forbidden by
  // spec.
  if (!key_path_elements.size()) {
    NOTREACHED();
    return false;
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // For an object o = {} which should have keypath 'a.b.c' and key k, this
  // populates o to be {a:{b:{}}}. This is only applied to deserialized
  // values, so we can assume that there are no getters/setters on the
  // object itself (though there might be on the prototype chain).
  //
  // Previous versions of this code assumed that the deserialized value meets
  // the constraints checked by the serialization validation code. For example,
  // given a keypath of a.b.c, the code assumed that the de-serialized value
  // cannot possibly be {a:{b:42}}. This is not a safe assumption.
  //
  // IndexedDB's backing store (LevelDB) does use CRC32C to protect against disk
  // errors. However, this does not prevent corruption caused by bugs in the
  // higher level code writing invalid values. The following cases are
  // interesting here.
  //
  // (1) Deserialization failures, which are currently handled by returning
  // null. Disk errors aside, deserialization errors can also occur when a user
  // switches channels and receives an older build which does not support the
  // serialization format used by the previous (more recent) build that the user
  // had.
  //
  // (2) Bugs that write a value which is incompatible with the primary key
  // injection required by the object store. The simplest example is writing
  // numbers or booleans to an object store with an auto-incrementing primary
  // keys.
  for (wtf_size_t i = 0; i < key_path_elements.size() - 1; ++i) {
    if (!value->IsObject())
      return false;

    const String& key_path_element = key_path_elements[i];
    DCHECK(!IsImplicitProperty(isolate, value, key_path_element));
    v8::Local<v8::Object> object = value.As<v8::Object>();
    v8::Local<v8::String> property = V8String(isolate, key_path_element);
    bool has_own_property;
    if (!object->HasOwnProperty(context, property).To(&has_own_property))
      return false;
    if (has_own_property) {
      if (!object->Get(context, property).ToLocal(&value))
        return false;
    } else {
      value = v8::Object::New(isolate);
      bool created_property;
      if (!object->CreateDataProperty(context, property, value)
               .To(&created_property) ||
          !created_property)
        return false;
    }
  }

  // Implicit properties don't need to be set. The caller is not required to
  // be aware of this, so this is an expected no-op. The caller can verify
  // that the value is correct via assertPrimaryKeyValidOrInjectable.
  if (IsImplicitProperty(isolate, value, key_path_elements.back()))
    return true;

  // If the key path does not point to an implicit property, the value must be
  // an object. Previous code versions DCHECKed this, which is unsafe in the
  // event of database corruption or version skew in the serialization format.
  if (!value->IsObject())
    return false;

  v8::Local<v8::Object> object = value.As<v8::Object>();
  v8::Local<v8::String> property = V8String(isolate, key_path_elements.back());

  bool created_property;
  if (!object->CreateDataProperty(context, property, key).To(&created_property))
    return false;
  return created_property;
}

// Verify that an value can have an generated key inserted at the location
// specified by the key path (by injectV8KeyIntoV8Value) when the object is
// later deserialized.
bool CanInjectIDBKeyIntoScriptValue(v8::Isolate* isolate,
                                    const ScriptValue& script_value,
                                    const IDBKeyPath& key_path) {
  TRACE_EVENT0("IndexedDB", "canInjectIDBKeyIntoScriptValue");
  DCHECK_EQ(key_path.GetType(), mojom::IDBKeyPathType::String);
  Vector<String> key_path_elements = ParseKeyPath(key_path.GetString());

  if (!key_path_elements.size())
    return false;

  v8::Local<v8::Value> current(script_value.V8Value());
  if (!current->IsObject())
    return false;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (wtf_size_t i = 0; i < key_path_elements.size(); ++i) {
    const String& key_path_element = key_path_elements[i];
    // Can't overwrite properties like array or string length.
    if (IsImplicitProperty(isolate, current, key_path_element))
      return false;
    // Can't set properties on non-objects.
    if (!current->IsObject())
      return false;
    v8::Local<v8::Object> object = current.As<v8::Object>();
    v8::Local<v8::String> property = V8String(isolate, key_path_element);
    // If the value lacks an "own" property, it can be added - either as
    // an intermediate object or as the final value.
    bool has_own_property;
    if (!object->HasOwnProperty(context, property).To(&has_own_property))
      return false;
    if (!has_own_property)
      return true;
    // Otherwise, get it and keep traversing.
    if (!object->Get(context, property).ToLocal(&current))
      return false;
  }
  return true;
}

ScriptValue DeserializeScriptValue(ScriptState* script_state,
                                   SerializedScriptValue* serialized_value,
                                   const Vector<WebBlobInfo>* blob_info) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  if (!serialized_value)
    return ScriptValue::CreateNull(script_state->GetIsolate());

  SerializedScriptValue::DeserializeOptions options;
  options.blob_info = blob_info;
  return ScriptValue(isolate, serialized_value->Deserialize(isolate, options));
}

SQLValue NativeValueTraits<SQLValue>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (value.IsEmpty() || value->IsNull())
    return SQLValue();
  if (value->IsNumber())
    return SQLValue(value.As<v8::Number>()->Value());
  V8StringResource<> string_value(value);
  if (!string_value.Prepare(isolate, exception_state))
    return SQLValue();
  return SQLValue(string_value);
}

std::unique_ptr<IDBKey> NativeValueTraits<std::unique_ptr<IDBKey>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return CreateIDBKeyFromValue(isolate, value, exception_state);
}

std::unique_ptr<IDBKey> NativeValueTraits<std::unique_ptr<IDBKey>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state,
    const IDBKeyPath& key_path) {
  TRACE_EVENT0("IndexedDB", "createIDBKeyFromValueAndKeyPath");
  return CreateIDBKeyFromValueAndKeyPath(isolate, value, key_path,
                                         exception_state);
}

std::unique_ptr<IDBKey> NativeValueTraits<std::unique_ptr<IDBKey>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state,
    const IDBKeyPath& store_key_path,
    const IDBKeyPath& index_key_path) {
  TRACE_EVENT0("IndexedDB", "createIDBKeyFromValueAndKeyPaths");
  return CreateIDBKeyFromValueAndKeyPaths(isolate, value, store_key_path,
                                          index_key_path, exception_state);
}

IDBKeyRange* NativeValueTraits<IDBKeyRange*>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return V8IDBKeyRange::ToWrappable(isolate, value);
}

#if DCHECK_IS_ON()
// This assertion is used when a value has been retrieved from an object store
// with implicit keys (i.e. a key path). It verifies that either the value
// contains an implicit key matching the primary key (so it was correctly
// extracted when stored) or that the key can be inserted as an own property.
void AssertPrimaryKeyValidOrInjectable(ScriptState* script_state,
                                       const IDBValue* value) {
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptValue key_value = ScriptValue::From(script_state, value->PrimaryKey());
  ScriptValue script_value(isolate, DeserializeIDBValueData(isolate, value));

  DummyExceptionStateForTesting exception_state;
  std::unique_ptr<IDBKey> expected_key = CreateIDBKeyFromValueAndKeyPath(
      isolate, script_value.V8Value(), value->KeyPath(), exception_state);
  DCHECK(!exception_state.HadException());
  if (expected_key && expected_key->IsEqual(value->PrimaryKey()))
    return;

  bool injected = InjectV8KeyIntoV8Value(
      isolate, key_value.V8Value(), script_value.V8Value(), value->KeyPath());
  DCHECK(injected);
}
#endif  // DCHECK_IS_ON()

}  // namespace blink

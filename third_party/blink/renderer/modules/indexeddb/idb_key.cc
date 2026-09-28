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

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"

#include <algorithm>
#include <memory>
#include <variant>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kIDBKeyOverheadSize = 16;

size_t CalculateIDBKeyArraySize(const IDBKey::KeyArray& keys) {
  size_t size(0);
  for (const auto& key : keys)
    size += key.get()->SizeEstimate();
  return size;
}

}  // namespace

// static
std::unique_ptr<IDBKey> IDBKey::Clone(const IDBKey* rkey) {
  if (!rkey) {
    return CreateNone();
  }

  return std::visit(
      absl::Overload{
          [](const InvalidKey&) { return CreateInvalid(); },
          [](const NoneKey&) { return CreateNone(); },
          [](double number) { return CreateNumber(number); },
          [](const DateKey& date) { return CreateDate(date.value); },
          [](const String& string) { return CreateString(string); },
          [](const BinaryKey& binary) { return CreateBinary(binary); },
          [](const KeyArray& array) {
            KeyArray key_array;
            key_array.ReserveInitialCapacity(array.size());
            for (const auto& item : array) {
              key_array.push_back(Clone(item));
            }
            return CreateArray(std::move(key_array));
          },
      },
      rkey->value_);
}

IDBKey::IDBKey() = default;

IDBKey::IDBKey(NoneKey) : value_(NoneKey{}) {}

IDBKey::IDBKey(double number) : value_(number) {}

IDBKey::IDBKey(DateKey date) : value_(date) {}

IDBKey::IDBKey(const String& value) : value_(value) {}

IDBKey::IDBKey(BinaryKey value) : value_(std::move(value)) {}

IDBKey::IDBKey(KeyArray key_array) : value_(std::move(key_array)) {}

IDBKey::~IDBKey() = default;

mojom::IDBKeyType IDBKey::GetType() const {
  return std::visit(
      absl::Overload{
          [](const InvalidKey&) { return mojom::IDBKeyType::Invalid; },
          [](const NoneKey&) { return mojom::IDBKeyType::None; },
          [](double) { return mojom::IDBKeyType::Number; },
          [](const DateKey&) { return mojom::IDBKeyType::Date; },
          [](const String&) { return mojom::IDBKeyType::String; },
          [](const BinaryKey&) { return mojom::IDBKeyType::Binary; },
          [](const KeyArray&) { return mojom::IDBKeyType::Array; },
      },
      value_);
}

const IDBKey::KeyArray& IDBKey::Array() const {
  return std::get<KeyArray>(value_);
}

scoped_refptr<base::RefCountedData<Vector<char>>> IDBKey::Binary() const {
  return std::get<BinaryKey>(value_);
}

const String& IDBKey::GetString() const {
  return std::get<String>(value_);
}

double IDBKey::Date() const {
  return std::get<DateKey>(value_).value;
}

double IDBKey::Number() const {
  return std::get<double>(value_);
}

bool IDBKey::IsValid() const {
  return std::visit(absl::Overload{
                        [](const InvalidKey&) { return false; },
                        [](const KeyArray& array) {
                          return std::ranges::all_of(array, &IDBKey::IsValid);
                        },
                        [](const auto&) { return true; },
                    },
                    value_);
}

namespace {

template <typename T>
int GenericCompare(const T& a, const T& b) {
  auto cmp = a <=> b;
  return cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
}

}  // namespace

int IDBKey::Compare(const IDBKey* other) const {
  DCHECK(other);
  if (auto type = GetType(), other_type = other->GetType();
      type != other_type) {
    return type > other_type ? -1 : 1;
  }

  return std::visit(
      absl::Overload{
          [&](double number) {
            return GenericCompare(number, other->Number());
          },
          [&](const DateKey& date) {
            return GenericCompare(date.value, other->Date());
          },
          [&](const String& string) {
            return CodeUnitCompare(string, other->GetString());
          },
          [&](const BinaryKey& binary) {
            return GenericCompare(base::as_byte_span(binary->data),
                                  base::as_byte_span(other->Binary()->data));
          },
          [&](const KeyArray& array) {
            const auto& other_array = other->Array();
            for (wtf_size_t i = 0; i < array.size() && i < other_array.size();
                 ++i) {
              if (int result = array[i]->Compare(other_array[i].get())) {
                return result;
              }
            }
            return GenericCompare(array.size(), other_array.size());
          },
          [](const auto&) -> int { NOTREACHED(); },
      },
      value_);
}

v8::Local<v8::Value> IDBKey::ToV8(ScriptState* script_state) const {
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Isolate* isolate = script_state->GetIsolate();
  return std::visit(
      absl::Overload{
          [](const InvalidKey&) -> v8::Local<v8::Value> { NOTREACHED(); },
          [&](const NoneKey&) -> v8::Local<v8::Value> {
            return v8::Null(isolate);
          },
          [&](double number) -> v8::Local<v8::Value> {
            return v8::Number::New(isolate, number);
          },
          [&](const DateKey& date) -> v8::Local<v8::Value> {
            return v8::Date::New(context, date.value).ToLocalChecked();
          },
          [&](const String& string) -> v8::Local<v8::Value> {
            return V8String(isolate, string);
          },
          [&](const BinaryKey& binary) -> v8::Local<v8::Value> {
            // https://w3c.github.io/IndexedDB/#convert-a-value-to-a-key
            return ToV8Traits<DOMArrayBuffer>::ToV8(
                script_state,
                DOMArrayBuffer::Create(base::as_byte_span(binary->data)));
          },
          [&](const KeyArray& key_array) -> v8::Local<v8::Value> {
            v8::Local<v8::Array> array =
                v8::Array::New(isolate, key_array.size());
            for (wtf_size_t i = 0; i < key_array.size(); ++i) {
              v8::Local<v8::Value> value = key_array[i]->ToV8(script_state);
              if (value.IsEmpty()) {
                value = v8::Undefined(isolate);
              }
              bool created_property;
              if (!array->CreateDataProperty(context, i, value)
                       .To(&created_property) ||
                  !created_property) {
                return v8::Local<v8::Value>();
              }
            }
            return array;
          },
      },
      value_);
}

bool IDBKey::IsLessThan(const IDBKey* other) const {
  DCHECK(other);
  return Compare(other) == -1;
}

bool IDBKey::IsEqual(const IDBKey* other) const {
  if (!other)
    return false;

  return !Compare(other);
}

size_t IDBKey::SizeEstimate() const {
  return kIDBKeyOverheadSize +
         std::visit(absl::Overload{
                        [](const KeyArray& array) -> size_t {
                          return CalculateIDBKeyArraySize(array);
                        },
                        [](const BinaryKey& binary) -> size_t {
                          return binary->data.size();
                        },
                        [](const String& string) -> size_t {
                          return string.length() * sizeof(UChar);
                        },
                        [](double) -> size_t { return sizeof(double); },
                        [](const DateKey&) -> size_t { return sizeof(double); },
                        [](const auto&) -> size_t { return 0; },
                    },
                    value_);
}

// static
Vector<std::unique_ptr<IDBKey>> IDBKey::ToMultiEntryArray(
    std::unique_ptr<IDBKey> array_key) {
  auto& array = std::get<KeyArray>(array_key->value_);
  Vector<std::unique_ptr<IDBKey>> result;
  result.ReserveInitialCapacity(array.size());
  for (std::unique_ptr<IDBKey>& key : array) {
    if (key->IsValid())
      result.emplace_back(std::move(key));
  }

  // Remove duplicates using std::sort/std::unique rather than a hashtable to
  // avoid the complexity of implementing HashTraits<IDBKey>.
  std::sort(
      result.begin(), result.end(),
      [](const std::unique_ptr<IDBKey>& a, const std::unique_ptr<IDBKey>& b) {
        return a->IsLessThan(b.get());
      });
  auto end = std::unique(
      result.begin(), result.end(),
      [](const std::unique_ptr<IDBKey>& a, const std::unique_ptr<IDBKey>& b) {
        return a->IsEqual(b.get());
      });
  result.erase(end, result.end());

  return result;
}

}  // namespace blink

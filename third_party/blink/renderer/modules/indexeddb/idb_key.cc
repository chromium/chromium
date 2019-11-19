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

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

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
  if (!rkey)
    return IDBKey::CreateNone();

  switch (rkey->GetType()) {
    case mojom::IDBKeyType::Invalid:
      return IDBKey::CreateInvalid();
    case mojom::IDBKeyType::None:
      return IDBKey::CreateNone();
    case mojom::IDBKeyType::Array: {
      IDBKey::KeyArray lkey_array;
      const auto& rkey_array = rkey->Array();
      for (const auto& rkey_item : rkey_array)
        lkey_array.push_back(IDBKey::Clone(rkey_item));
      return IDBKey::CreateArray(std::move(lkey_array));
    }
    case mojom::IDBKeyType::Binary:
      return IDBKey::CreateBinary(rkey->Binary());
    case mojom::IDBKeyType::String:
      return IDBKey::CreateString(rkey->GetString());
    case mojom::IDBKeyType::Date:
      return IDBKey::CreateDate(rkey->Date());
    case mojom::IDBKeyType::Number:
      return IDBKey::CreateNumber(rkey->Number());

    case mojom::IDBKeyType::Min:
      break;  // Not used, NOTREACHED.
  }
  NOTREACHED();
  return nullptr;
}

IDBKey::IDBKey()
    : type_(mojom::IDBKeyType::Invalid), size_estimate_(kIDBKeyOverheadSize) {}

// Must be Invalid or None.
IDBKey::IDBKey(mojom::IDBKeyType type)
    : type_(type), size_estimate_(kIDBKeyOverheadSize) {
  DCHECK(type_ == mojom::IDBKeyType::Invalid ||
         type_ == mojom::IDBKeyType::None);
}

// Must be Number or Date.
IDBKey::IDBKey(mojom::IDBKeyType type, double number)
    : type_(type),
      number_(number),
      size_estimate_(kIDBKeyOverheadSize + sizeof(number_)) {
  DCHECK(type_ == mojom::IDBKeyType::Number ||
         type_ == mojom::IDBKeyType::Date);
}

IDBKey::IDBKey(const String& value)
    : type_(mojom::IDBKeyType::String),
      string_(value),
      size_estimate_(kIDBKeyOverheadSize + (string_.length() * sizeof(UChar))) {
}

IDBKey::IDBKey(scoped_refptr<SharedBuffer> value)
    : type_(mojom::IDBKeyType::Binary),
      binary_(std::move(value)),
      size_estimate_(kIDBKeyOverheadSize + binary_.get()->size()) {}

IDBKey::IDBKey(KeyArray key_array)
    : type_(mojom::IDBKeyType::Array),
      array_(std::move(key_array)),
      size_estimate_(kIDBKeyOverheadSize + CalculateIDBKeyArraySize(array_)) {}

IDBKey::~IDBKey() = default;

bool IDBKey::IsValid() const {
  if (type_ == mojom::IDBKeyType::Invalid)
    return false;

  if (type_ == mojom::IDBKeyType::Array) {
    for (const auto& element : array_) {
      if (!element->IsValid())
        return false;
    }
  }

  return true;
}

// Safely compare numbers (signed/unsigned ints/floats/doubles).
template <typename T>
static int CompareNumbers(const T& a, const T& b) {
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}

int IDBKey::Compare(const IDBKey* other) const {
  DCHECK(other);
  if (type_ != other->type_)
    return type_ > other->type_ ? -1 : 1;

  switch (type_) {
    case mojom::IDBKeyType::Array:
      for (wtf_size_t i = 0; i < array_.size() && i < other->array_.size();
           ++i) {
        if (int result = array_[i]->Compare(other->array_[i].get()))
          return result;
      }
      return CompareNumbers(array_.size(), other->array_.size());
    case mojom::IDBKeyType::Binary:
      if (int result =
              memcmp(binary_->Data(), other->binary_->Data(),
                     std::min(binary_->size(), other->binary_->size())))
        return result < 0 ? -1 : 1;
      return CompareNumbers(binary_->size(), other->binary_->size());
    case mojom::IDBKeyType::String:
      return CodeUnitCompare(string_, other->string_);
    case mojom::IDBKeyType::Date:
    case mojom::IDBKeyType::Number:
      return CompareNumbers(number_, other->number_);

    // These values cannot be compared to each other.
    case mojom::IDBKeyType::Invalid:
    case mojom::IDBKeyType::None:
    case mojom::IDBKeyType::Min:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
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

// static
Vector<std::unique_ptr<IDBKey>> IDBKey::ToMultiEntryArray(
    std::unique_ptr<IDBKey> array_key) {
  DCHECK_EQ(array_key->type_, mojom::IDBKeyType::Array);
  Vector<std::unique_ptr<IDBKey>> result;
  result.ReserveInitialCapacity(array_key->array_.size());
  for (std::unique_ptr<IDBKey>& key : array_key->array_) {
    if (key->IsValid())
      result.emplace_back(std::move(key));
  }

  // Remove duplicates using std::sort/std::unique rather than a hashtable to
  // avoid the complexity of implementing DefaultHash<IDBKey>.
  std::sort(
      result.begin(), result.end(),
      [](const std::unique_ptr<IDBKey>& a, const std::unique_ptr<IDBKey>& b) {
        return (a)->IsLessThan(b.get());
      });
  std::unique_ptr<IDBKey>* end = std::unique(result.begin(), result.end());
  DCHECK_LE(static_cast<wtf_size_t>(end - result.begin()), result.size());
  result.resize(static_cast<wtf_size_t>(end - result.begin()));

  return result;
}

}  // namespace blink

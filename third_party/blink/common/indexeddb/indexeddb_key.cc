// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"

namespace blink {

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kOverheadSize = 16;

size_t CalculateArraySize(const IndexedDBKey::KeyArray& keys) {
  size_t size(0);
  for (const auto& key : keys)
    size += key.size_estimate();
  return size;
}

template <typename T>
int Compare(const T& a, const T& b) {
  // Using '<' for both comparisons here is as generic as possible (for e.g.
  // objects which only define operator<() and not operator>() or operator==())
  // and also allows e.g. floating point NaNs to compare equal.
  if (a < b)
    return -1;
  return (b < a) ? 1 : 0;
}

}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(mojom::IDBKeyType::None), size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(mojom::IDBKeyType type)
    : type_(type), size_estimate_(kOverheadSize) {
  DCHECK(type == mojom::IDBKeyType::None ||
         type == mojom::IDBKeyType::Invalid || type == mojom::IDBKeyType::Min);
}

IndexedDBKey::IndexedDBKey(double number, mojom::IDBKeyType type)
    : type_(type),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == mojom::IDBKeyType::Number || type == mojom::IDBKeyType::Date);
}

IndexedDBKey::IndexedDBKey(KeyArray array)
    : type_(mojom::IDBKeyType::Array),
      array_(std::move(array)),
      size_estimate_(kOverheadSize + CalculateArraySize(array_)) {}

IndexedDBKey::IndexedDBKey(std::string binary)
    : type_(mojom::IDBKeyType::Binary),
      binary_(std::move(binary)),
      size_estimate_(kOverheadSize +
                     (binary_.length() * sizeof(std::string::value_type))) {}

IndexedDBKey::IndexedDBKey(std::u16string string)
    : type_(mojom::IDBKeyType::String),
      string_(std::move(string)),
      size_estimate_(kOverheadSize +
                     (string_.length() * sizeof(std::u16string::value_type))) {}

IndexedDBKey::IndexedDBKey(const IndexedDBKey& other) = default;
IndexedDBKey::IndexedDBKey(IndexedDBKey&& other) = default;
IndexedDBKey::~IndexedDBKey() = default;
IndexedDBKey& IndexedDBKey::operator=(const IndexedDBKey& other) = default;

bool IndexedDBKey::IsValid() const {
  switch (type_) {
    case mojom::IDBKeyType::Array:
      return base::ranges::all_of(array_, &IndexedDBKey::IsValid);
    case mojom::IDBKeyType::Binary:
    case mojom::IDBKeyType::String:
    case mojom::IDBKeyType::Date:
    case mojom::IDBKeyType::Number:
      return true;
    case mojom::IDBKeyType::Invalid:
    case mojom::IDBKeyType::None:
    case mojom::IDBKeyType::Min:
      return false;
  }
}

bool IndexedDBKey::IsLessThan(const IndexedDBKey& other) const {
  return CompareTo(other) < 0;
}

bool IndexedDBKey::Equals(const IndexedDBKey& other) const {
  return !CompareTo(other);
}

bool IndexedDBKey::HasHoles() const {
  if (type_ != mojom::IDBKeyType::Array)
    return false;

  for (const auto& subkey : array_) {
    if (subkey.type() == mojom::IDBKeyType::None)
      return true;
  }
  return false;
}

IndexedDBKey IndexedDBKey::FillHoles(const IndexedDBKey& primary_key) const {
  if (type_ != mojom::IDBKeyType::Array)
    return IndexedDBKey(*this);

  std::vector<IndexedDBKey> subkeys;
  subkeys.reserve(array_.size());
  for (const auto& subkey : array_) {
    if (subkey.type() == mojom::IDBKeyType::None) {
      subkeys.push_back(primary_key);
    } else {
      // "Holes" can only exist at the top level of an array key, as (1) they
      // are produced by an index's array keypath when a member matches the
      // store's keypath, and (2) array keypaths are flat (no
      // arrays-of-arrays).
      DCHECK(!subkey.HasHoles());
      subkeys.push_back(subkey);
    }
  }
  return IndexedDBKey(subkeys);
}

std::string IndexedDBKey::DebugString() const {
  std::stringstream result;
  result << "IDBKey{";
  switch (type_) {
    case mojom::IDBKeyType::Array: {
      result << "array: [";
      for (size_t i = 0; i < array_.size(); ++i) {
        result << array_[i].DebugString();
        if (i != array_.size() - 1)
          result << ", ";
      }
      result << "]";
      break;
    }
    case mojom::IDBKeyType::Binary:
      result << "binary: 0x" << base::HexEncode(binary_);
      break;
    case mojom::IDBKeyType::String:
      result << "string: " << string_;
      break;
    case mojom::IDBKeyType::Date:
      result << "date: " << number_;
      break;
    case mojom::IDBKeyType::Number:
      result << "number: " << number_;
      break;
    case mojom::IDBKeyType::Invalid:
      result << "Invalid";
      break;
    case mojom::IDBKeyType::None:
      result << "None";
      break;
    case mojom::IDBKeyType::Min:
      result << "Min";
      break;
    default:
      result << "InvalidKey";
  }
  result << "}";
  return result.str();
}

int IndexedDBKey::CompareTo(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case mojom::IDBKeyType::Array:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        int result = array_[i].CompareTo(other.array_[i]);
        if (result != 0)
          return result;
      }
      return Compare(array_.size(), other.array_.size());
    case mojom::IDBKeyType::Binary:
      return binary_.compare(other.binary_);
    case mojom::IDBKeyType::String:
      return string_.compare(other.string_);
    case mojom::IDBKeyType::Date:
    case mojom::IDBKeyType::Number:
      return Compare(number_, other.number_);
    case mojom::IDBKeyType::Invalid:
    case mojom::IDBKeyType::None:
    case mojom::IDBKeyType::Min:
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace blink

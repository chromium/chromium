// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"

namespace blink {

IndexedDBKeyPath::IndexedDBKeyPath() : type_(mojom::IDBKeyPathType::Null) {}

IndexedDBKeyPath::IndexedDBKeyPath(const std::u16string& string)
    : type_(mojom::IDBKeyPathType::String), string_(string) {}

IndexedDBKeyPath::IndexedDBKeyPath(const std::vector<std::u16string>& array)
    : type_(mojom::IDBKeyPathType::Array), array_(array) {}

IndexedDBKeyPath::IndexedDBKeyPath(const IndexedDBKeyPath& other) = default;
IndexedDBKeyPath::IndexedDBKeyPath(IndexedDBKeyPath&& other) = default;
IndexedDBKeyPath::~IndexedDBKeyPath() = default;
IndexedDBKeyPath& IndexedDBKeyPath::operator=(const IndexedDBKeyPath& other) =
    default;
IndexedDBKeyPath& IndexedDBKeyPath::operator=(IndexedDBKeyPath&& other) =
    default;

const std::vector<std::u16string>& IndexedDBKeyPath::array() const {
  DCHECK(type_ == blink::mojom::IDBKeyPathType::Array);
  return array_;
}

const std::u16string& IndexedDBKeyPath::string() const {
  DCHECK(type_ == blink::mojom::IDBKeyPathType::String);
  return string_;
}

bool IndexedDBKeyPath::operator==(const IndexedDBKeyPath& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case mojom::IDBKeyPathType::Null:
      return true;
    case mojom::IDBKeyPathType::String:
      return string_ == other.string_;
    case mojom::IDBKeyPathType::Array:
      return array_ == other.array_;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace blink

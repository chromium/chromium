// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_range.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"

namespace blink {

namespace {

IndexedDBKey::KeyArray CopyKeyArray(WebIDBKeyArrayView array) {
  IndexedDBKey::KeyArray result;
  const size_t array_size = array.size();
  result.reserve(array_size);
  for (size_t i = 0; i < array_size; ++i)
    result.emplace_back(IndexedDBKeyBuilder::Build(array[i]));
  return result;
}

std::vector<base::string16> CopyArray(const WebVector<WebString>& array) {
  std::vector<base::string16> result;
  result.reserve(array.size());
  for (const WebString& element : array)
    result.emplace_back(element.Utf16());
  return result;
}

}  // anonymous namespace

// static
IndexedDBKey IndexedDBKeyBuilder::Build(WebIDBKeyView key) {
  switch (key.KeyType()) {
    case kWebIDBKeyTypeArray:
      return IndexedDBKey(CopyKeyArray(key.ArrayView()));
    case kWebIDBKeyTypeBinary: {
      const WebData data = key.Binary();
      std::string key_string;
      key_string.reserve(data.size());

      data.ForEachSegment([&key_string](const char* segment,
                                        size_t segment_size,
                                        size_t segment_offset) {
        key_string.append(segment, segment_size);
        return true;
      });
      return IndexedDBKey(key_string);
    }
    case kWebIDBKeyTypeString:
      return IndexedDBKey(key.String().Utf16());
    case kWebIDBKeyTypeDate:
      return IndexedDBKey(key.Date(), kWebIDBKeyTypeDate);
    case kWebIDBKeyTypeNumber:
      return IndexedDBKey(key.Number(), kWebIDBKeyTypeNumber);
    case kWebIDBKeyTypeNull:
    case kWebIDBKeyTypeInvalid:
      return IndexedDBKey(key.KeyType());
    case kWebIDBKeyTypeMin:
      NOTREACHED();
      return IndexedDBKey();
  }
}

// static
WebIDBKey WebIDBKeyBuilder::Build(const WebIDBKeyView& key) {
  switch (key.KeyType()) {
    case kWebIDBKeyTypeArray: {
      const WebIDBKeyArrayView& array = key.ArrayView();
      WebVector<WebIDBKey> web_idb_keys;
      const size_t array_size = array.size();
      web_idb_keys.reserve(array_size);
      for (size_t i = 0; i < array_size; ++i)
        web_idb_keys.emplace_back(Build(array[i]));
      return WebIDBKey::CreateArray(std::move(web_idb_keys));
    }
    case kWebIDBKeyTypeBinary: {
      const WebData data = key.Binary();
      return WebIDBKey::CreateBinary(data);
    }
    case kWebIDBKeyTypeString:
      return WebIDBKey::CreateString(key.String());
    case kWebIDBKeyTypeDate:
      return WebIDBKey::CreateDate(key.Date());
    case kWebIDBKeyTypeNumber:
      return WebIDBKey::CreateNumber(key.Number());
    case kWebIDBKeyTypeInvalid:
      return WebIDBKey::CreateInvalid();
    case kWebIDBKeyTypeNull:
      return WebIDBKey::CreateNull();
    case kWebIDBKeyTypeMin:
      NOTREACHED();
      return WebIDBKey::CreateInvalid();
  }
}

// static
WebIDBKey WebIDBKeyBuilder::Build(const IndexedDBKey& key) {
  switch (key.type()) {
    case kWebIDBKeyTypeArray: {
      const IndexedDBKey::KeyArray& array = key.array();
      WebVector<WebIDBKey> web_idb_keys;
      web_idb_keys.reserve(array.size());
      for (const IndexedDBKey& array_element : array)
        web_idb_keys.emplace_back(Build(array_element));
      return WebIDBKey::CreateArray(std::move(web_idb_keys));
    }
    case kWebIDBKeyTypeBinary: {
      const std::string& str = key.binary();
      const WebData& data = WebData(str.c_str(), str.length());
      return WebIDBKey::CreateBinary(data);
    }
    case kWebIDBKeyTypeString:
      return WebIDBKey::CreateString(WebString::FromUTF16(key.string()));
    case kWebIDBKeyTypeDate:
      return WebIDBKey::CreateDate(key.date());
    case kWebIDBKeyTypeNumber:
      return WebIDBKey::CreateNumber(key.number());
    case kWebIDBKeyTypeInvalid:
      return WebIDBKey::CreateInvalid();
    case kWebIDBKeyTypeNull:
      return WebIDBKey::CreateNull();
    case kWebIDBKeyTypeMin:
      NOTREACHED();
      return WebIDBKey::CreateInvalid();
  }
}

// static
IndexedDBKeyRange IndexedDBKeyRangeBuilder::Build(
    const WebIDBKeyRange& key_range) {
  return IndexedDBKeyRange(IndexedDBKeyBuilder::Build(key_range.Lower()),
                           IndexedDBKeyBuilder::Build(key_range.Upper()),
                           key_range.LowerOpen(), key_range.UpperOpen());
}

// static
IndexedDBKeyRange IndexedDBKeyRangeBuilder::Build(WebIDBKeyView key) {
  return IndexedDBKeyRange(IndexedDBKeyBuilder::Build(key),
                           IndexedDBKeyBuilder::Build(key),
                           false /* lower_open */, false /* upper_open */);
}

// static
WebIDBKeyRange WebIDBKeyRangeBuilder::Build(WebIDBKeyView key) {
  return WebIDBKeyRange(WebIDBKeyBuilder::Build(key),
                        WebIDBKeyBuilder::Build(key), false /* lower_open */,
                        false /* upper_open */);
}

// static
WebIDBKeyRange WebIDBKeyRangeBuilder::Build(
    const IndexedDBKeyRange& key_range) {
  return WebIDBKeyRange(WebIDBKeyBuilder::Build(key_range.lower()),
                        WebIDBKeyBuilder::Build(key_range.upper()),
                        key_range.lower_open(), key_range.upper_open());
}

// static
IndexedDBKeyPath IndexedDBKeyPathBuilder::Build(const WebIDBKeyPath& key_path) {
  switch (key_path.KeyPathType()) {
    case kWebIDBKeyPathTypeString:
      return IndexedDBKeyPath(key_path.String().Utf16());
    case kWebIDBKeyPathTypeArray:
      return IndexedDBKeyPath(CopyArray(key_path.Array()));
    case kWebIDBKeyPathTypeNull:
      return IndexedDBKeyPath();
  }
}

// static
WebIDBKeyPath WebIDBKeyPathBuilder::Build(const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case kWebIDBKeyPathTypeString:
      return WebIDBKeyPath::Create(WebString::FromUTF16(key_path.string()));
    case kWebIDBKeyPathTypeArray: {
      WebVector<WebString> key_path_vector(key_path.array().size());
      for (const auto& item : key_path.array())
        key_path_vector.emplace_back(WebString::FromUTF16(item));
      return WebIDBKeyPath::Create(key_path_vector);
    }
    case kWebIDBKeyPathTypeNull:
      return WebIDBKeyPath::CreateNull();
  }
}

}  // namespace blink

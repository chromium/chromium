// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexed_db_default_mojom_traits.h"

#include <utility>

#include "base/stl_util.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace mojo {

using blink::mojom::IDBOperationType;

// static
bool StructTraits<blink::mojom::IDBDatabaseMetadataDataView,
                  blink::IndexedDBDatabaseMetadata>::
    Read(blink::mojom::IDBDatabaseMetadataDataView data,
         blink::IndexedDBDatabaseMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  MapDataView<int64_t, blink::mojom::IDBObjectStoreMetadataDataView>
      object_stores;
  data.GetObjectStoresDataView(&object_stores);
  for (size_t i = 0; i < object_stores.size(); ++i) {
    const int64_t key = object_stores.keys()[i];
    blink::IndexedDBObjectStoreMetadata object_store;
    if (!object_stores.values().Read(i, &object_store))
      return false;
    DCHECK_EQ(out->object_stores.count(key), 0UL);
    out->object_stores[key] = object_store;
  }
  out->was_cold_open = data.was_cold_open();
  return true;
}

// static
bool StructTraits<
    blink::mojom::IDBIndexKeysDataView,
    blink::IndexedDBIndexKeys>::Read(blink::mojom::IDBIndexKeysDataView data,
                                     blink::IndexedDBIndexKeys* out) {
  out->id = data.index_id();
  return data.ReadIndexKeys(&out->keys);
}

// static
bool StructTraits<blink::mojom::IDBIndexMetadataDataView,
                  blink::IndexedDBIndexMetadata>::
    Read(blink::mojom::IDBIndexMetadataDataView data,
         blink::IndexedDBIndexMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->unique = data.unique();
  out->multi_entry = data.multi_entry();
  return true;
}

// static
blink::mojom::IDBKeyDataDataView::Tag
UnionTraits<blink::mojom::IDBKeyDataDataView, blink::IndexedDBKey>::GetTag(
    const blink::IndexedDBKey& key) {
  switch (key.type()) {
    case blink::mojom::IDBKeyType::Array:
      return blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY;
    case blink::mojom::IDBKeyType::Binary:
      return blink::mojom::IDBKeyDataDataView::Tag::BINARY;
    case blink::mojom::IDBKeyType::String:
      return blink::mojom::IDBKeyDataDataView::Tag::STRING;
    case blink::mojom::IDBKeyType::Date:
      return blink::mojom::IDBKeyDataDataView::Tag::DATE;
    case blink::mojom::IDBKeyType::Number:
      return blink::mojom::IDBKeyDataDataView::Tag::NUMBER;
    case blink::mojom::IDBKeyType::None:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER_NONE;

    // Not used, fall through to NOTREACHED.
    case blink::mojom::IDBKeyType::Invalid:  // Only used in blink.
    case blink::mojom::IDBKeyType::Min:;     // Only used in the browser.
  }
  NOTREACHED();
  return blink::mojom::IDBKeyDataDataView::Tag::OTHER_NONE;
}

// static
bool UnionTraits<blink::mojom::IDBKeyDataDataView, blink::IndexedDBKey>::Read(
    blink::mojom::IDBKeyDataDataView data,
    blink::IndexedDBKey* out) {
  switch (data.tag()) {
    case blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY: {
      std::vector<blink::IndexedDBKey> array;
      if (!data.ReadKeyArray(&array))
        return false;
      *out = blink::IndexedDBKey(std::move(array));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::BINARY: {
      ArrayDataView<uint8_t> bytes;
      data.GetBinaryDataView(&bytes);
      std::string binary(bytes.data(), bytes.data() + bytes.size());
      *out = blink::IndexedDBKey(std::move(binary));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data.ReadString(&string))
        return false;
      *out = blink::IndexedDBKey(std::move(string));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::DATE:
      *out = blink::IndexedDBKey(data.date(), blink::mojom::IDBKeyType::Date);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::NUMBER:
      *out =
          blink::IndexedDBKey(data.number(), blink::mojom::IDBKeyType::Number);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER_NONE:
      *out = blink::IndexedDBKey(blink::mojom::IDBKeyType::None);
      return true;
  }

  return false;
}

// static
const blink::IndexedDBKey&
StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey>::data(
    const blink::IndexedDBKey& key) {
  return key;
}

// static
bool StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey>::Read(
    blink::mojom::IDBKeyDataView data,
    blink::IndexedDBKey* out) {
  return data.ReadData(out);
}

// static
blink::mojom::IDBKeyPathDataPtr
StructTraits<blink::mojom::IDBKeyPathDataView, blink::IndexedDBKeyPath>::data(
    const blink::IndexedDBKeyPath& key_path) {
  if (key_path.IsNull())
    return nullptr;

  auto data = blink::mojom::IDBKeyPathData::New();
  switch (key_path.type()) {
    case blink::mojom::IDBKeyPathType::String:
      data->set_string(key_path.string());
      return data;
    case blink::mojom::IDBKeyPathType::Array:
      data->set_string_array(key_path.array());
      return data;

    // The following key path types are not used.
    case blink::mojom::IDBKeyPathType::Null:;  // No-op, fall out of switch
                                               // block to NOTREACHED().
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<blink::mojom::IDBKeyPathDataView, blink::IndexedDBKeyPath>::
    Read(blink::mojom::IDBKeyPathDataView data, blink::IndexedDBKeyPath* out) {
  blink::mojom::IDBKeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = blink::IndexedDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::IndexedDBKeyPath(string);
      return true;
    }
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING_ARRAY: {
      std::vector<base::string16> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = blink::IndexedDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
bool StructTraits<blink::mojom::IDBKeyRangeDataView, blink::IndexedDBKeyRange>::
    Read(blink::mojom::IDBKeyRangeDataView data,
         blink::IndexedDBKeyRange* out) {
  blink::IndexedDBKey lower;
  blink::IndexedDBKey upper;
  if (!data.ReadLower(&lower) || !data.ReadUpper(&upper))
    return false;

  *out = blink::IndexedDBKeyRange(lower, upper, data.lower_open(),
                                  data.upper_open());
  return true;
}

// static
bool StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                  blink::IndexedDBObjectStoreMetadata>::
    Read(blink::mojom::IDBObjectStoreMetadataDataView data,
         blink::IndexedDBObjectStoreMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->auto_increment = data.auto_increment();
  out->max_index_id = data.max_index_id();
  MapDataView<int64_t, blink::mojom::IDBIndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  for (size_t i = 0; i < indexes.size(); ++i) {
    const int64_t key = indexes.keys()[i];
    blink::IndexedDBIndexMetadata index;
    if (!indexes.values().Read(i, &index))
      return false;
    DCHECK_EQ(out->indexes.count(key), 0UL);
    out->indexes[key] = index;
  }
  return true;
}

}  // namespace mojo

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_mojom_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/array_traits_web_vector.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using blink::mojom::IDBCursorDirection;
using blink::mojom::IDBDataLoss;
using blink::mojom::IDBOperationType;

namespace mojo {

// static
bool StructTraits<
    blink::mojom::IDBDatabaseMetadataDataView,
    blink::WebIDBMetadata>::Read(blink::mojom::IDBDatabaseMetadataDataView data,
                                 blink::WebIDBMetadata* out) {
  out->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  out->name = name;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  if (!data.ReadObjectStores(&out->object_stores))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexKeysDataView, blink::WebIDBIndexKeys>::
    Read(blink::mojom::IDBIndexKeysDataView data, blink::WebIDBIndexKeys* out) {
  out->first = data.index_id();
  if (!data.ReadIndexKeys(&out->second))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexMetadataDataView,
                  blink::WebIDBMetadata::Index>::
    Read(blink::mojom::IDBIndexMetadataDataView data,
         blink::WebIDBMetadata::Index* out) {
  out->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  out->name = name;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->unique = data.unique();
  out->multi_entry = data.multi_entry();
  return true;
}

// static
blink::mojom::IDBKeyDataDataView::Tag
UnionTraits<blink::mojom::IDBKeyDataDataView, blink::WebIDBKey>::GetTag(
    const blink::WebIDBKey& key) {
  switch (key.View().KeyType()) {
    case blink::kWebIDBKeyTypeInvalid:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER;
    case blink::kWebIDBKeyTypeArray:
      return blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY;
    case blink::kWebIDBKeyTypeBinary:
      return blink::mojom::IDBKeyDataDataView::Tag::BINARY;
    case blink::kWebIDBKeyTypeString:
      return blink::mojom::IDBKeyDataDataView::Tag::STRING;
    case blink::kWebIDBKeyTypeDate:
      return blink::mojom::IDBKeyDataDataView::Tag::DATE;
    case blink::kWebIDBKeyTypeNumber:
      return blink::mojom::IDBKeyDataDataView::Tag::NUMBER;
    case blink::kWebIDBKeyTypeNull:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER;
    case blink::kWebIDBKeyTypeMin:
      break;
  }
  NOTREACHED();
  return blink::mojom::IDBKeyDataDataView::Tag::OTHER;
}

// static
bool UnionTraits<blink::mojom::IDBKeyDataDataView, blink::WebIDBKey>::Read(
    blink::mojom::IDBKeyDataDataView data,
    blink::WebIDBKey* out) {
  switch (data.tag()) {
    case blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY: {
      Vector<blink::WebIDBKey> array;
      if (!data.ReadKeyArray(&array))
        return false;
      blink::WebVector<blink::WebIDBKey> webvector_array;
      for (const auto& item : array) {
        webvector_array.emplace_back(
            blink::WebIDBKeyBuilder::Build(item.View()));
      }
      *out = blink::WebIDBKey::CreateArray(std::move(webvector_array));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::BINARY: {
      Vector<uint8_t> binary_vector;
      if (!data.ReadBinary(&binary_vector))
        return false;
      std::string binary_string = std::string(
          binary_vector.data(), binary_vector.data() + binary_vector.size());
      *out = blink::WebIDBKey::CreateBinary(
          blink::WebData(binary_string.c_str(), binary_string.length()));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::STRING: {
      String string;
      if (!data.ReadString(&string))
        return false;
      *out = blink::WebIDBKey::CreateString(blink::WebString(string));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::DATE:
      *out = blink::WebIDBKey::CreateDate(data.date());
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::NUMBER:
      *out = blink::WebIDBKey::CreateNumber(data.number());
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER:
      switch (data.other()) {
        case blink::mojom::IDBDatalessKeyType::Invalid:
          *out = blink::WebIDBKey::CreateInvalid();
          return true;
        case blink::mojom::IDBDatalessKeyType::Null:
          *out = blink::WebIDBKey::CreateNull();
          return true;
      }
  }

  return false;
}

// static
const blink::WebVector<blink::WebIDBKey>
UnionTraits<blink::mojom::IDBKeyDataDataView, blink::WebIDBKey>::key_array(
    const blink::WebIDBKey& key) {
  const auto& array_view = key.View().ArrayView();
  const size_t array_size = array_view.size();
  Vector<blink::WebIDBKey> result;
  result.ReserveInitialCapacity(array_view.size());
  for (size_t i = 0; i < array_size; ++i)
    result.emplace_back(blink::WebIDBKeyBuilder::Build(array_view[i]));
  return result;
}

// static
const Vector<uint8_t>
UnionTraits<blink::mojom::IDBKeyDataDataView, blink::WebIDBKey>::binary(
    const blink::WebIDBKey& key) {
  const auto& data_view = key.View().Binary().Copy();
  const size_t data_size = data_view.size();
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(data_size);
  for (const auto& item : data_view)
    result.push_back(item);
  return result;
}

// static
const blink::WebIDBKey&
StructTraits<blink::mojom::IDBKeyDataView, blink::WebIDBKey>::data(
    const blink::WebIDBKey& key) {
  return key;
}

// static
bool StructTraits<blink::mojom::IDBKeyDataView, blink::WebIDBKey>::Read(
    blink::mojom::IDBKeyDataView data,
    blink::WebIDBKey* out) {
  return data.ReadData(out);
}

// static
blink::mojom::blink::IDBKeyPathDataPtr
StructTraits<blink::mojom::IDBKeyPathDataView, blink::WebIDBKeyPath>::data(
    const blink::WebIDBKeyPath& key_path) {
  if (key_path.KeyPathType() == blink::kWebIDBKeyPathTypeNull)
    return nullptr;

  auto data = blink::mojom::blink::IDBKeyPathData::New();
  switch (key_path.KeyPathType()) {
    case blink::kWebIDBKeyPathTypeString: {
      String key_path_string = key_path.String();
      if (key_path_string.IsNull())
        key_path_string = "";
      data->set_string(key_path_string);
      return data;
    }
    case blink::kWebIDBKeyPathTypeArray: {
      const auto& array = key_path.Array();
      const size_t array_size = array.size();
      Vector<String> result;
      result.ReserveInitialCapacity(array_size);
      for (const auto& item : array)
        result.push_back(item);
      data->set_string_array(result);
      return data;
    }

    case blink::kWebIDBKeyPathTypeNull:
      break;  // Not used, NOTREACHED.
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<blink::mojom::IDBKeyPathDataView, blink::WebIDBKeyPath>::Read(
    blink::mojom::IDBKeyPathDataView data,
    blink::WebIDBKeyPath* out) {
  blink::mojom::IDBKeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = blink::WebIDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING: {
      String string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::WebIDBKeyPath(blink::WebString(string));
      return true;
    }
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING_ARRAY: {
      Vector<String> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = blink::WebIDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
bool StructTraits<blink::mojom::IDBKeyRangeDataView, blink::WebIDBKeyRange>::
    Read(blink::mojom::IDBKeyRangeDataView data, blink::WebIDBKeyRange* out) {
  // TODO(cmp): Use WebIDBKey and WebIDBKeyRange directly.
  blink::IndexedDBKey lower;
  blink::IndexedDBKey upper;
  if (!data.ReadLower(&lower) || !data.ReadUpper(&upper))
    return false;

  blink::IndexedDBKeyRange temp(lower, upper, data.lower_open(),
                                data.upper_open());
  *out = blink::WebIDBKeyRangeBuilder::Build(temp);
  return true;
}

// static
bool StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                  blink::WebIDBMetadata::ObjectStore>::
    Read(blink::mojom::IDBObjectStoreMetadataDataView data,
         blink::WebIDBMetadata::ObjectStore* out) {
  out->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  out->name = name;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->auto_increment = data.auto_increment();
  out->max_index_id = data.max_index_id();
  if (!data.ReadIndexes(&out->indexes))
    return false;
  return true;
}

}  // namespace mojo

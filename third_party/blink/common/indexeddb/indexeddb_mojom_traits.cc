// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_mojom_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

namespace mojo {

using blink::mojom::IDBCursorDirection;
using blink::mojom::IDBDataLoss;
using blink::mojom::IDBOperationType;

// static
IDBCursorDirection
EnumTraits<IDBCursorDirection, blink::WebIDBCursorDirection>::ToMojom(
    blink::WebIDBCursorDirection input) {
  switch (input) {
    case blink::kWebIDBCursorDirectionNext:
      return IDBCursorDirection::Next;
    case blink::kWebIDBCursorDirectionNextNoDuplicate:
      return IDBCursorDirection::NextNoDuplicate;
    case blink::kWebIDBCursorDirectionPrev:
      return IDBCursorDirection::Prev;
    case blink::kWebIDBCursorDirectionPrevNoDuplicate:
      return IDBCursorDirection::PrevNoDuplicate;
  }
  NOTREACHED();
  return IDBCursorDirection::Next;
}

// static
bool EnumTraits<IDBCursorDirection, blink::WebIDBCursorDirection>::FromMojom(
    IDBCursorDirection input,
    blink::WebIDBCursorDirection* output) {
  switch (input) {
    case IDBCursorDirection::Next:
      *output = blink::kWebIDBCursorDirectionNext;
      return true;
    case IDBCursorDirection::NextNoDuplicate:
      *output = blink::kWebIDBCursorDirectionNextNoDuplicate;
      return true;
    case IDBCursorDirection::Prev:
      *output = blink::kWebIDBCursorDirectionPrev;
      return true;
    case IDBCursorDirection::PrevNoDuplicate:
      *output = blink::kWebIDBCursorDirectionPrevNoDuplicate;
      return true;
  }
  return false;
}

// static
IDBDataLoss EnumTraits<IDBDataLoss, blink::WebIDBDataLoss>::ToMojom(
    blink::WebIDBDataLoss input) {
  switch (input) {
    case blink::kWebIDBDataLossNone:
      return IDBDataLoss::None;
    case blink::kWebIDBDataLossTotal:
      return IDBDataLoss::Total;
  }
  NOTREACHED();
  return IDBDataLoss::None;
}

// static
bool EnumTraits<IDBDataLoss, blink::WebIDBDataLoss>::FromMojom(
    IDBDataLoss input,
    blink::WebIDBDataLoss* output) {
  switch (input) {
    case IDBDataLoss::None:
      *output = blink::kWebIDBDataLossNone;
      return true;
    case IDBDataLoss::Total:
      *output = blink::kWebIDBDataLossTotal;
      return true;
  }
  return false;
}

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
  ArrayDataView<blink::mojom::IDBObjectStoreMetadataDataView> object_stores;
  data.GetObjectStoresDataView(&object_stores);
  for (size_t i = 0; i < object_stores.size(); ++i) {
    blink::mojom::IDBObjectStoreMetadataDataView object_store;
    object_stores.GetDataView(i, &object_store);
    DCHECK(!base::ContainsKey(out->object_stores, object_store.id()));
    if (!StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                      blink::IndexedDBObjectStoreMetadata>::
            Read(object_store, &out->object_stores[object_store.id()]))
      return false;
  }
  return true;
}

// static
bool StructTraits<
    blink::mojom::IDBIndexKeysDataView,
    blink::IndexedDBIndexKeys>::Read(blink::mojom::IDBIndexKeysDataView data,
                                     blink::IndexedDBIndexKeys* out) {
  out->first = data.index_id();
  return data.ReadIndexKeys(&out->second);
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
    case blink::kWebIDBKeyTypeInvalid:
    case blink::kWebIDBKeyTypeNull:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER;

    // Not used, fall through to NOTREACHED.
    case blink::kWebIDBKeyTypeMin:;
  }
  NOTREACHED();
  return blink::mojom::IDBKeyDataDataView::Tag::OTHER;
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
      *out = blink::IndexedDBKey(array);
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::BINARY: {
      std::vector<uint8_t> binary;
      if (!data.ReadBinary(&binary))
        return false;
      *out = blink::IndexedDBKey(
          std::string(binary.data(), binary.data() + binary.size()));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data.ReadString(&string))
        return false;
      *out = blink::IndexedDBKey(string);
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::DATE:
      *out = blink::IndexedDBKey(data.date(), blink::kWebIDBKeyTypeDate);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::NUMBER:
      *out = blink::IndexedDBKey(data.number(), blink::kWebIDBKeyTypeNumber);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER:
      switch (data.other()) {
        case blink::mojom::IDBDatalessKeyType::Invalid:
          *out = blink::IndexedDBKey(blink::kWebIDBKeyTypeInvalid);
          return true;
        case blink::mojom::IDBDatalessKeyType::Null:
          *out = blink::IndexedDBKey(blink::kWebIDBKeyTypeNull);
          return true;
      }
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
    case blink::kWebIDBKeyPathTypeString:
      data->set_string(key_path.string());
      return data;
    case blink::kWebIDBKeyPathTypeArray:
      data->set_string_array(key_path.array());
      return data;

    // The following key path types are not used.
    case blink::kWebIDBKeyPathTypeNull:;  // No-op, fall out of switch block to
                                          // NOTREACHED().
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
  ArrayDataView<blink::mojom::IDBIndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  for (size_t i = 0; i < indexes.size(); ++i) {
    blink::mojom::IDBIndexMetadataDataView index;
    indexes.GetDataView(i, &index);
    DCHECK(!base::ContainsKey(out->indexes, index.id()));
    if (!StructTraits<
            blink::mojom::IDBIndexMetadataDataView,
            blink::IndexedDBIndexMetadata>::Read(index,
                                                 &out->indexes[index.id()]))
      return false;
  }
  return true;
}

// static
IDBOperationType
EnumTraits<IDBOperationType, blink::WebIDBOperationType>::ToMojom(
    blink::WebIDBOperationType input) {
  switch (input) {
    case blink::kWebIDBAdd:
      return IDBOperationType::Add;
    case blink::kWebIDBPut:
      return IDBOperationType::Put;
    case blink::kWebIDBDelete:
      return IDBOperationType::Delete;
    case blink::kWebIDBClear:
      return IDBOperationType::Clear;
    case blink::kWebIDBOperationTypeCount:
      // WebIDBOperationTypeCount is not a valid option.
      break;
  }
  NOTREACHED();
  return IDBOperationType::Add;
}

// static
bool EnumTraits<IDBOperationType, blink::WebIDBOperationType>::FromMojom(
    IDBOperationType input,
    blink::WebIDBOperationType* output) {
  switch (input) {
    case IDBOperationType::Add:
      *output = blink::kWebIDBAdd;
      return true;
    case IDBOperationType::Put:
      *output = blink::kWebIDBPut;
      return true;
    case IDBOperationType::Delete:
      *output = blink::kWebIDBDelete;
      return true;
    case IDBOperationType::Clear:
      *output = blink::kWebIDBClear;
      return true;
  }
  return false;
}

// static
blink::mojom::IDBPutMode
EnumTraits<blink::mojom::IDBPutMode, blink::WebIDBPutMode>::ToMojom(
    blink::WebIDBPutMode input) {
  switch (input) {
    case blink::kWebIDBPutModeAddOrUpdate:
      return blink::mojom::IDBPutMode::AddOrUpdate;
    case blink::kWebIDBPutModeAddOnly:
      return blink::mojom::IDBPutMode::AddOnly;
    case blink::kWebIDBPutModeCursorUpdate:
      return blink::mojom::IDBPutMode::CursorUpdate;
  }
  NOTREACHED();
  return blink::mojom::IDBPutMode::AddOrUpdate;
}

// static
bool EnumTraits<blink::mojom::IDBPutMode, blink::WebIDBPutMode>::FromMojom(
    blink::mojom::IDBPutMode input,
    blink::WebIDBPutMode* output) {
  switch (input) {
    case blink::mojom::IDBPutMode::AddOrUpdate:
      *output = blink::kWebIDBPutModeAddOrUpdate;
      return true;
    case blink::mojom::IDBPutMode::AddOnly:
      *output = blink::kWebIDBPutModeAddOnly;
      return true;
    case blink::mojom::IDBPutMode::CursorUpdate:
      *output = blink::kWebIDBPutModeCursorUpdate;
      return true;
  }
  return false;
}

// static
blink::mojom::IDBTaskType
EnumTraits<blink::mojom::IDBTaskType, blink::WebIDBTaskType>::ToMojom(
    blink::WebIDBTaskType input) {
  switch (input) {
    case blink::kWebIDBTaskTypeNormal:
      return blink::mojom::IDBTaskType::Normal;
    case blink::kWebIDBTaskTypePreemptive:
      return blink::mojom::IDBTaskType::Preemptive;
  }
  NOTREACHED();
  return blink::mojom::IDBTaskType::Normal;
}

// static
bool EnumTraits<blink::mojom::IDBTaskType, blink::WebIDBTaskType>::FromMojom(
    blink::mojom::IDBTaskType input,
    blink::WebIDBTaskType* output) {
  switch (input) {
    case blink::mojom::IDBTaskType::Normal:
      *output = blink::kWebIDBTaskTypeNormal;
      return true;
    case blink::mojom::IDBTaskType::Preemptive:
      *output = blink::kWebIDBTaskTypePreemptive;
      return true;
  }
  return false;
}

// static
blink::mojom::IDBTransactionMode EnumTraits<
    blink::mojom::IDBTransactionMode,
    blink::WebIDBTransactionMode>::ToMojom(blink::WebIDBTransactionMode input) {
  switch (input) {
    case blink::kWebIDBTransactionModeReadOnly:
      return blink::mojom::IDBTransactionMode::ReadOnly;
    case blink::kWebIDBTransactionModeReadWrite:
      return blink::mojom::IDBTransactionMode::ReadWrite;
    case blink::kWebIDBTransactionModeVersionChange:
      return blink::mojom::IDBTransactionMode::VersionChange;
  }
  NOTREACHED();
  return blink::mojom::IDBTransactionMode::ReadOnly;
}

// static
bool EnumTraits<blink::mojom::IDBTransactionMode,
                blink::WebIDBTransactionMode>::
    FromMojom(blink::mojom::IDBTransactionMode input,
              blink::WebIDBTransactionMode* output) {
  switch (input) {
    case blink::mojom::IDBTransactionMode::ReadOnly:
      *output = blink::kWebIDBTransactionModeReadOnly;
      return true;
    case blink::mojom::IDBTransactionMode::ReadWrite:
      *output = blink::kWebIDBTransactionModeReadWrite;
      return true;
    case blink::mojom::IDBTransactionMode::VersionChange:
      *output = blink::kWebIDBTransactionModeVersionChange;
      return true;
  }
  return false;
}

}  // namespace mojo

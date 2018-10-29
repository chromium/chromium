// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "third_party/blink/common/common_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBCursorDirection, blink::WebIDBCursorDirection> {
  static blink::mojom::IDBCursorDirection ToMojom(
      blink::WebIDBCursorDirection input);
  static bool FromMojom(blink::mojom::IDBCursorDirection input,
                        blink::WebIDBCursorDirection* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBDataLoss, blink::WebIDBDataLoss> {
  static blink::mojom::IDBDataLoss ToMojom(blink::WebIDBDataLoss input);
  static bool FromMojom(blink::mojom::IDBDataLoss input,
                        blink::WebIDBDataLoss* output);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBDatabaseMetadataDataView,
                 blink::IndexedDBDatabaseMetadata> {
  static int64_t id(const blink::IndexedDBDatabaseMetadata& metadata) {
    return metadata.id;
  }
  static const base::string16& name(
      const blink::IndexedDBDatabaseMetadata& metadata) {
    return metadata.name;
  }
  static int64_t version(const blink::IndexedDBDatabaseMetadata& metadata) {
    return metadata.version;
  }
  static int64_t max_object_store_id(
      const blink::IndexedDBDatabaseMetadata& metadata) {
    return metadata.max_object_store_id;
  }
  static MapValuesArrayView<int64_t, blink::IndexedDBObjectStoreMetadata>
  object_stores(const blink::IndexedDBDatabaseMetadata& metadata) {
    return MapValuesToArray(metadata.object_stores);
  }
  static bool Read(blink::mojom::IDBDatabaseMetadataDataView data,
                   blink::IndexedDBDatabaseMetadata* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::IDBIndexKeysDataView,
                                        blink::IndexedDBIndexKeys> {
  static int64_t index_id(const blink::IndexedDBIndexKeys& index_keys) {
    return index_keys.first;
  }
  static const std::vector<blink::IndexedDBKey>& index_keys(
      const blink::IndexedDBIndexKeys& index_keys) {
    return index_keys.second;
  }
  static bool Read(blink::mojom::IDBIndexKeysDataView data,
                   blink::IndexedDBIndexKeys* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::IDBIndexMetadataDataView,
                                        blink::IndexedDBIndexMetadata> {
  static int64_t id(const blink::IndexedDBIndexMetadata& metadata) {
    return metadata.id;
  }
  static const base::string16& name(
      const blink::IndexedDBIndexMetadata& metadata) {
    return metadata.name;
  }
  static const blink::IndexedDBKeyPath& key_path(
      const blink::IndexedDBIndexMetadata& metadata) {
    return metadata.key_path;
  }
  static bool unique(const blink::IndexedDBIndexMetadata& metadata) {
    return metadata.unique;
  }
  static bool multi_entry(const blink::IndexedDBIndexMetadata& metadata) {
    return metadata.multi_entry;
  }
  static bool Read(blink::mojom::IDBIndexMetadataDataView data,
                   blink::IndexedDBIndexMetadata* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::IDBKeyDataDataView, blink::IndexedDBKey> {
  static blink::mojom::IDBKeyDataDataView::Tag GetTag(
      const blink::IndexedDBKey& key);
  static bool Read(blink::mojom::IDBKeyDataDataView data,
                   blink::IndexedDBKey* out);
  static const std::vector<blink::IndexedDBKey>& key_array(
      const blink::IndexedDBKey& key) {
    return key.array();
  }
  static base::span<const uint8_t> binary(const blink::IndexedDBKey& key) {
    return base::make_span(
        reinterpret_cast<const uint8_t*>(key.binary().data()),
        key.binary().size());
  }
  static const base::string16& string(const blink::IndexedDBKey& key) {
    return key.string();
  }
  static double date(const blink::IndexedDBKey& key) { return key.date(); }
  static double number(const blink::IndexedDBKey& key) { return key.number(); }
  static blink::mojom::IDBDatalessKeyType other(
      const blink::IndexedDBKey& key) {
    switch (key.type()) {
      case blink::kWebIDBKeyTypeInvalid:
        return blink::mojom::IDBDatalessKeyType::Invalid;
      case blink::kWebIDBKeyTypeNull:
        return blink::mojom::IDBDatalessKeyType::Null;
      default:
        NOTREACHED();
        return blink::mojom::IDBDatalessKeyType::Invalid;
    }
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey> {
  static const blink::IndexedDBKey& data(const blink::IndexedDBKey& key);
  static bool Read(blink::mojom::IDBKeyDataView data, blink::IndexedDBKey* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBKeyPathDataView, blink::IndexedDBKeyPath> {
  static blink::mojom::IDBKeyPathDataPtr data(
      const blink::IndexedDBKeyPath& key_path);
  static bool Read(blink::mojom::IDBKeyPathDataView data,
                   blink::IndexedDBKeyPath* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBKeyRangeDataView, blink::IndexedDBKeyRange> {
  static const blink::IndexedDBKey& lower(
      const blink::IndexedDBKeyRange& key_range) {
    return key_range.lower();
  }
  static const blink::IndexedDBKey& upper(
      const blink::IndexedDBKeyRange& key_range) {
    return key_range.upper();
  }
  static bool lower_open(const blink::IndexedDBKeyRange& key_range) {
    return key_range.lower_open();
  }
  static bool upper_open(const blink::IndexedDBKeyRange& key_range) {
    return key_range.upper_open();
  }
  static bool Read(blink::mojom::IDBKeyRangeDataView data,
                   blink::IndexedDBKeyRange* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                 blink::IndexedDBObjectStoreMetadata> {
  static int64_t id(const blink::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.id;
  }
  static const base::string16& name(
      const blink::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.name;
  }
  static const blink::IndexedDBKeyPath& key_path(
      const blink::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.key_path;
  }
  static bool auto_increment(
      const blink::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.auto_increment;
  }
  static int64_t max_index_id(
      const blink::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.max_index_id;
  }
  static MapValuesArrayView<int64_t, blink::IndexedDBIndexMetadata> indexes(
      const blink::IndexedDBObjectStoreMetadata& metadata) {
    return MapValuesToArray(metadata.indexes);
  }
  static bool Read(blink::mojom::IDBObjectStoreMetadataDataView data,
                   blink::IndexedDBObjectStoreMetadata* out);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBOperationType, blink::WebIDBOperationType> {
  static blink::mojom::IDBOperationType ToMojom(
      blink::WebIDBOperationType input);
  static bool FromMojom(blink::mojom::IDBOperationType input,
                        blink::WebIDBOperationType* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBPutMode, blink::WebIDBPutMode> {
  static blink::mojom::IDBPutMode ToMojom(blink::WebIDBPutMode input);
  static bool FromMojom(blink::mojom::IDBPutMode input,
                        blink::WebIDBPutMode* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBTaskType, blink::WebIDBTaskType> {
  static blink::mojom::IDBTaskType ToMojom(blink::WebIDBTaskType input);
  static bool FromMojom(blink::mojom::IDBTaskType input,
                        blink::WebIDBTaskType* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBTransactionMode, blink::WebIDBTransactionMode> {
  static blink::mojom::IDBTransactionMode ToMojom(
      blink::WebIDBTransactionMode input);
  static bool FromMojom(blink::mojom::IDBTransactionMode input,
                        blink::WebIDBTransactionMode* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_MOJOM_TRAITS_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

using blink::mojom::IDBCursorDirection;
using blink::mojom::IDBDataLoss;
using blink::mojom::IDBOperationType;

namespace mojo {

// static
bool StructTraits<blink::mojom::IDBDatabaseMetadataDataView,
                  blink::IDBDatabaseMetadata>::
    Read(blink::mojom::IDBDatabaseMetadataDataView data,
         blink::IDBDatabaseMetadata* out) {
  out->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  out->name = name;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  MapDataView<int64_t, blink::mojom::IDBObjectStoreMetadataDataView>
      object_stores;
  data.GetObjectStoresDataView(&object_stores);
  out->object_stores.ReserveCapacityForSize(
      base::checked_cast<wtf_size_t>(object_stores.size()));
  for (size_t i = 0; i < object_stores.size(); ++i) {
    const int64_t key = object_stores.keys()[i];
    scoped_refptr<blink::IDBObjectStoreMetadata> object_store;
    if (!object_stores.values().Read(i, &object_store)) {
      return false;
    }
    DCHECK(!out->object_stores.Contains(key));
    out->object_stores.insert(key, object_store);
  }
  out->was_cold_open = data.was_cold_open();
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexKeysDataView, blink::IDBIndexKeys>::
    Read(blink::mojom::IDBIndexKeysDataView data, blink::IDBIndexKeys* out) {
  out->id = data.index_id();
  if (!data.ReadIndexKeys(&out->keys))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexMetadataDataView,
                  scoped_refptr<blink::IDBIndexMetadata>>::
    Read(blink::mojom::IDBIndexMetadataDataView data,
         scoped_refptr<blink::IDBIndexMetadata>* out) {
  scoped_refptr<blink::IDBIndexMetadata> value =
      blink::IDBIndexMetadata::Create();
  value->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  value->name = name;
  if (!data.ReadKeyPath(&value->key_path))
    return false;
  value->unique = data.unique();
  value->multi_entry = data.multi_entry();
  *out = std::move(value);
  return true;
}

// static
blink::mojom::IDBKeyDataView::Tag
UnionTraits<blink::mojom::IDBKeyDataView, std::unique_ptr<blink::IDBKey>>::
    GetTag(const std::unique_ptr<blink::IDBKey>& key) {
  DCHECK(key.get());
  switch (key->GetType()) {
    case blink::mojom::IDBKeyType::Array:
      return blink::mojom::IDBKeyDataView::Tag::kKeyArray;
    case blink::mojom::IDBKeyType::Binary:
      return blink::mojom::IDBKeyDataView::Tag::kBinary;
    case blink::mojom::IDBKeyType::String:
      return blink::mojom::IDBKeyDataView::Tag::kString;
    case blink::mojom::IDBKeyType::Date:
      return blink::mojom::IDBKeyDataView::Tag::kDate;
    case blink::mojom::IDBKeyType::Number:
      return blink::mojom::IDBKeyDataView::Tag::kNumber;
    case blink::mojom::IDBKeyType::None:
      return blink::mojom::IDBKeyDataView::Tag::kOtherNone;

    // Not used, fall through to NOTREACHED.
    case blink::mojom::IDBKeyType::Invalid:  // Only used in blink.
    case blink::mojom::IDBKeyType::Min:      // Only used in the browser.
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::IDBKeyDataView::Tag::kOtherNone;
}

// static
bool UnionTraits<blink::mojom::IDBKeyDataView, std::unique_ptr<blink::IDBKey>>::
    Read(blink::mojom::IDBKeyDataView data,
         std::unique_ptr<blink::IDBKey>* out) {
  switch (data.tag()) {
    case blink::mojom::IDBKeyDataView::Tag::kKeyArray: {
      Vector<std::unique_ptr<blink::IDBKey>> array;
      if (!data.ReadKeyArray(&array))
        return false;
      *out = blink::IDBKey::CreateArray(std::move(array));
      return true;
    }
    case blink::mojom::IDBKeyDataView::Tag::kBinary: {
      ArrayDataView<uint8_t> bytes;
      data.GetBinaryDataView(&bytes);
      *out = blink::IDBKey::CreateBinary(
          base::MakeRefCounted<base::RefCountedData<Vector<char>>>(
              Vector<char>(base::span(
                  reinterpret_cast<const char*>(bytes.data()), bytes.size()))));
      return true;
    }
    case blink::mojom::IDBKeyDataView::Tag::kString: {
      String string;
      if (!data.ReadString(&string))
        return false;
      *out = blink::IDBKey::CreateString(String(string));
      return true;
    }
    case blink::mojom::IDBKeyDataView::Tag::kDate:
      *out = blink::IDBKey::CreateDate(data.date());
      return true;
    case blink::mojom::IDBKeyDataView::Tag::kNumber:
      *out = blink::IDBKey::CreateNumber(data.number());
      return true;
    case blink::mojom::IDBKeyDataView::Tag::kOtherNone:
      *out = blink::IDBKey::CreateNone();
      return true;
  }

  return false;
}

// static
const Vector<std::unique_ptr<blink::IDBKey>>&
UnionTraits<blink::mojom::IDBKeyDataView, std::unique_ptr<blink::IDBKey>>::
    key_array(const std::unique_ptr<blink::IDBKey>& key) {
  return key->Array();
}

// static
base::span<const uint8_t>
UnionTraits<blink::mojom::IDBKeyDataView, std::unique_ptr<blink::IDBKey>>::
    binary(const std::unique_ptr<blink::IDBKey>& key) {
  return base::as_byte_span(key->Binary()->data);
}

// static
base::span<const uint8_t>
StructTraits<blink::mojom::IDBValueDataView, std::unique_ptr<blink::IDBValue>>::
    bits(const std::unique_ptr<blink::IDBValue>& input) {
  return base::as_byte_span(input->Data());
}

// static
Vector<blink::mojom::blink::IDBExternalObjectPtr>
StructTraits<blink::mojom::IDBValueDataView, std::unique_ptr<blink::IDBValue>>::
    external_objects(const std::unique_ptr<blink::IDBValue>& input) {
  Vector<blink::mojom::blink::IDBExternalObjectPtr> external_objects;
  external_objects.ReserveInitialCapacity(
      input->BlobInfo().size() + input->FileSystemAccessTokens().size());
  for (const blink::WebBlobInfo& info : input->BlobInfo()) {
    auto blob_info = blink::mojom::blink::IDBBlobInfo::New();
    if (info.IsFile()) {
      blob_info->file = blink::mojom::blink::IDBFileInfo::New();
      String name = info.FileName();
      if (name.IsNull())
        name = g_empty_string;
      blob_info->file->name = name;
      blob_info->file->last_modified =
          info.LastModified().value_or(base::Time());
    }
    blob_info->size = info.size();
    String mime_type = info.GetType();
    if (mime_type.IsNull())
      mime_type = g_empty_string;
    blob_info->mime_type = mime_type;
    blob_info->blob = info.CloneBlobRemote();
    external_objects.push_back(
        blink::mojom::blink::IDBExternalObject::NewBlobOrFile(
            std::move(blob_info)));
  }
  for (auto& token : input->FileSystemAccessTokens()) {
    external_objects.push_back(
        blink::mojom::blink::IDBExternalObject::NewFileSystemAccessToken(
            std::move(token)));
  }
  return external_objects;
}

// static
bool StructTraits<blink::mojom::IDBValueDataView,
                  std::unique_ptr<blink::IDBValue>>::
    Read(blink::mojom::IDBValueDataView data,
         std::unique_ptr<blink::IDBValue>* out) {
  Vector<char> value_bits;
  if (!data.ReadBits(reinterpret_cast<Vector<uint8_t>*>(&value_bits))) {
    return false;
  }

  if (value_bits.empty()) {
    *out = std::make_unique<blink::IDBValue>(std::move(value_bits),
                                             Vector<blink::WebBlobInfo>());
    return true;
  }

  Vector<blink::mojom::blink::IDBExternalObjectPtr> external_objects;
  if (!data.ReadExternalObjects(&external_objects))
    return false;

  Vector<blink::WebBlobInfo> value_blob_info;
  Vector<
      mojo::PendingRemote<blink::mojom::blink::FileSystemAccessTransferToken>>
      file_system_access_tokens;

  for (const auto& object : external_objects) {
    switch (object->which()) {
      case blink::mojom::blink::IDBExternalObject::Tag::kBlobOrFile: {
        auto& info = object->get_blob_or_file();
        // The UUID is used as an implementation detail of V8 serialization
        // code, but it is no longer relevant to or related to the blob storage
        // context UUID, so we can make one up here.
        // TODO(crbug.com/40529364): remove the UUID parameter from WebBlobInfo.
        if (info->file) {
          value_blob_info.emplace_back(
              WTF::CreateCanonicalUUIDString(), info->file->name,
              info->mime_type,
              blink::NullableTimeToOptionalTime(info->file->last_modified),
              info->size, std::move(info->blob));
        } else {
          value_blob_info.emplace_back(WTF::CreateCanonicalUUIDString(),
                                       info->mime_type, info->size,
                                       std::move(info->blob));
        }
        break;
      }
      case blink::mojom::blink::IDBExternalObject::Tag::kFileSystemAccessToken:
        file_system_access_tokens.push_back(
            std::move(object->get_file_system_access_token()));
        break;
    }
  }

  *out = std::make_unique<blink::IDBValue>(
      std::move(value_bits), std::move(value_blob_info),
      std::move(file_system_access_tokens));
  return true;
}

// static
blink::mojom::blink::IDBKeyPathDataPtr
StructTraits<blink::mojom::IDBKeyPathDataView, blink::IDBKeyPath>::data(
    const blink::IDBKeyPath& key_path) {
  if (key_path.GetType() == blink::mojom::IDBKeyPathType::Null)
    return nullptr;

  switch (key_path.GetType()) {
    case blink::mojom::IDBKeyPathType::String: {
      String key_path_string = key_path.GetString();
      if (key_path_string.IsNull())
        key_path_string = g_empty_string;
      return blink::mojom::blink::IDBKeyPathData::NewString(key_path_string);
    }
    case blink::mojom::IDBKeyPathType::Array: {
      const auto& array = key_path.Array();
      Vector<String> result;
      result.ReserveInitialCapacity(
          base::checked_cast<wtf_size_t>(array.size()));
      for (const auto& item : array)
        result.push_back(item);
      return blink::mojom::blink::IDBKeyPathData::NewStringArray(
          std::move(result));
    }

    case blink::mojom::IDBKeyPathType::Null:
      break;  // Not used, NOTREACHED.
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
bool StructTraits<blink::mojom::IDBKeyPathDataView, blink::IDBKeyPath>::Read(
    blink::mojom::IDBKeyPathDataView data,
    blink::IDBKeyPath* out) {
  blink::mojom::IDBKeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = blink::IDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyPathDataDataView::Tag::kString: {
      String string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::IDBKeyPath(string);
      return true;
    }
    case blink::mojom::IDBKeyPathDataDataView::Tag::kStringArray: {
      Vector<String> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = blink::IDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
bool StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                  scoped_refptr<blink::IDBObjectStoreMetadata>>::
    Read(blink::mojom::IDBObjectStoreMetadataDataView data,
         scoped_refptr<blink::IDBObjectStoreMetadata>* out) {
  scoped_refptr<blink::IDBObjectStoreMetadata> value =
      blink::IDBObjectStoreMetadata::Create();
  value->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  value->name = name;
  if (!data.ReadKeyPath(&value->key_path))
    return false;
  value->auto_increment = data.auto_increment();
  value->max_index_id = data.max_index_id();
  MapDataView<int64_t, blink::mojom::IDBIndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  value->indexes.ReserveCapacityForSize(
      base::checked_cast<wtf_size_t>(indexes.size()));
  for (size_t i = 0; i < indexes.size(); ++i) {
    const int64_t key = indexes.keys()[i];
    scoped_refptr<blink::IDBIndexMetadata> index;
    if (!indexes.values().Read(i, &index))
      return false;
    DCHECK(!value->indexes.Contains(key));
    value->indexes.insert(key, index);
  }
  *out = std::move(value);
  return true;
}

// static
blink::mojom::blink::IDBKeyRangePtr TypeConverter<
    blink::mojom::blink::IDBKeyRangePtr,
    const blink::IDBKeyRange*>::Convert(const blink::IDBKeyRange* input) {
  if (!input) {
    std::unique_ptr<blink::IDBKey> lower = blink::IDBKey::CreateNone();
    std::unique_ptr<blink::IDBKey> upper = blink::IDBKey::CreateNone();
    return blink::mojom::blink::IDBKeyRange::New(
        std::move(lower), std::move(upper), false /* lower_open */,
        false /* upper_open */);
  }

  return blink::mojom::blink::IDBKeyRange::New(
      blink::IDBKey::Clone(input->Lower()),
      blink::IDBKey::Clone(input->Upper()), input->lowerOpen(),
      input->upperOpen());
}

// static
blink::mojom::blink::IDBKeyRangePtr
TypeConverter<blink::mojom::blink::IDBKeyRangePtr,
              blink::IDBKeyRange*>::Convert(blink::IDBKeyRange* input) {
  if (!input) {
    std::unique_ptr<blink::IDBKey> lower = blink::IDBKey::CreateNone();
    std::unique_ptr<blink::IDBKey> upper = blink::IDBKey::CreateNone();
    return blink::mojom::blink::IDBKeyRange::New(
        std::move(lower), std::move(upper), false /* lower_open */,
        false /* upper_open */);
  }

  return blink::mojom::blink::IDBKeyRange::New(
      blink::IDBKey::Clone(input->Lower()),
      blink::IDBKey::Clone(input->Upper()), input->lowerOpen(),
      input->upperOpen());
}

// static
blink::IDBKeyRange*
TypeConverter<blink::IDBKeyRange*, blink::mojom::blink::IDBKeyRangePtr>::
    Convert(const blink::mojom::blink::IDBKeyRangePtr& input) {
  if (!input)
    return nullptr;

  blink::IDBKeyRange::LowerBoundType lower_type =
      blink::IDBKeyRange::kLowerBoundClosed;
  if (input->lower_open)
    lower_type = blink::IDBKeyRange::kLowerBoundOpen;

  blink::IDBKeyRange::UpperBoundType upper_type =
      blink::IDBKeyRange::kUpperBoundClosed;
  if (input->upper_open)
    upper_type = blink::IDBKeyRange::kUpperBoundOpen;

  return blink::IDBKeyRange::Create(
      std::move(input->lower), std::move(input->upper), lower_type, upper_type);
}

}  // namespace mojo

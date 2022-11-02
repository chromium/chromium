// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/drag_mojom_traits.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

// static
WTF::String StructTraits<
    blink::mojom::DragItemStringDataView,
    blink::WebDragData::Item>::string_type(blink::WebDragData::Item item) {
  return item.string_type;
}

// static
WTF::String
StructTraits<blink::mojom::DragItemStringDataView, blink::WebDragData::Item>::
    string_data(const blink::WebDragData::Item& item) {
  return item.string_data;
}

// static
WTF::String StructTraits<
    blink::mojom::DragItemStringDataView,
    blink::WebDragData::Item>::title(const blink::WebDragData::Item& item) {
  return item.title;
}

// static
absl::optional<blink::KURL> StructTraits<
    blink::mojom::DragItemStringDataView,
    blink::WebDragData::Item>::base_url(const blink::WebDragData::Item& item) {
  if (item.base_url.IsNull())
    return absl::nullopt;
  return item.base_url;
}

// static
bool StructTraits<
    blink::mojom::DragItemStringDataView,
    blink::WebDragData::Item>::Read(blink::mojom::DragItemStringDataView data,
                                    blink::WebDragData::Item* out) {
  blink::WebDragData::Item item;
  WTF::String string_type, string_data, title;
  absl::optional<blink::KURL> url;
  if (!data.ReadStringType(&string_type) ||
      !data.ReadStringData(&string_data) || !data.ReadTitle(&title) ||
      !data.ReadBaseUrl(&url))
    return false;

  item.storage_type = blink::WebDragData::Item::kStorageTypeString;
  item.string_type = string_type;
  item.string_data = string_data;
  item.title = title;
  item.base_url = url.value_or(blink::KURL());
  *out = std::move(item);
  return true;
}

// static
base::FilePath StructTraits<
    blink::mojom::DataTransferFileDataView,
    blink::WebDragData::Item>::path(const blink::WebDragData::Item& item) {
  return WebStringToFilePath(item.filename_data);
}

// static
base::FilePath
StructTraits<blink::mojom::DataTransferFileDataView, blink::WebDragData::Item>::
    display_name(const blink::WebDragData::Item& item) {
  return WebStringToFilePath(item.display_name_data);
}

// static
bool StructTraits<
    blink::mojom::DataTransferFileDataView,
    blink::WebDragData::Item>::Read(blink::mojom::DataTransferFileDataView data,
                                    blink::WebDragData::Item* out) {
  blink::WebDragData::Item item;
  base::FilePath filename_data, display_name_data;
  if (!data.ReadPath(&filename_data) ||
      !data.ReadDisplayName(&display_name_data))
    return false;

  item.storage_type = blink::WebDragData::Item::kStorageTypeFilename;
  item.filename_data = blink::FilePathToWebString(filename_data);
  item.display_name_data = blink::FilePathToWebString(display_name_data);
  mojo::PendingRemote<::blink::mojom::blink::FileSystemAccessDataTransferToken>
      file_system_access_token(
          data.TakeFileSystemAccessToken<mojo::PendingRemote<
              ::blink::mojom::blink::FileSystemAccessDataTransferToken>>());
  item.file_system_access_entry =
      base::MakeRefCounted<::blink::FileSystemAccessDropData>(
          std::move(file_system_access_token));

  *out = std::move(item);
  return true;
}

// static
mojo_base::BigBuffer StructTraits<
    blink::mojom::DragItemBinaryDataView,
    blink::WebDragData::Item>::data(const blink::WebDragData::Item& item) {
  mojo_base::BigBuffer buffer(item.binary_data.size());
  item.binary_data.ForEachSegment([&buffer](const char* segment,
                                            size_t segment_size,
                                            size_t segment_offset) {
    std::copy(segment, segment + segment_size, buffer.data() + segment_offset);
    return true;
  });
  return buffer;
}

// static
bool StructTraits<blink::mojom::DragItemBinaryDataView,
                  blink::WebDragData::Item>::
    is_image_accessible(const blink::WebDragData::Item& item) {
  return item.binary_data_image_accessible;
}

// static
blink::KURL
StructTraits<blink::mojom::DragItemBinaryDataView, blink::WebDragData::Item>::
    source_url(const blink::WebDragData::Item& item) {
  return item.binary_data_source_url;
}

// static
base::FilePath
StructTraits<blink::mojom::DragItemBinaryDataView, blink::WebDragData::Item>::
    filename_extension(const blink::WebDragData::Item& item) {
  return WebStringToFilePath(item.binary_data_filename_extension);
}

// static
WTF::String
StructTraits<blink::mojom::DragItemBinaryDataView, blink::WebDragData::Item>::
    content_disposition(const blink::WebDragData::Item& item) {
  return item.binary_data_content_disposition;
}

// static
bool StructTraits<
    blink::mojom::DragItemBinaryDataView,
    blink::WebDragData::Item>::Read(blink::mojom::DragItemBinaryDataView data,
                                    blink::WebDragData::Item* out) {
  mojo_base::BigBufferView file_contents;
  blink::KURL source_url;
  base::FilePath filename_extension;
  String content_disposition;
  if (!data.ReadData(&file_contents) || !data.ReadSourceUrl(&source_url) ||
      !data.ReadFilenameExtension(&filename_extension) ||
      !data.ReadContentDisposition(&content_disposition)) {
    return false;
  }
  blink::WebDragData::Item item;
  item.storage_type = blink::WebDragData::Item::kStorageTypeBinaryData;
  item.binary_data =
      blink::WebData(reinterpret_cast<const char*>(file_contents.data().data()),
                     file_contents.data().size());
  item.binary_data_image_accessible = data.is_image_accessible();
  item.binary_data_source_url = source_url;
  item.binary_data_filename_extension =
      blink::FilePathToWebString(filename_extension);
  item.binary_data_content_disposition = content_disposition;

  *out = std::move(item);
  return true;
}

//  static
blink::KURL StructTraits<
    blink::mojom::DragItemFileSystemFileDataView,
    blink::WebDragData::Item>::url(const blink::WebDragData::Item& item) {
  return item.file_system_url;
}

//  static
int64_t StructTraits<
    blink::mojom::DragItemFileSystemFileDataView,
    blink::WebDragData::Item>::size(const blink::WebDragData::Item& item) {
  return item.file_system_file_size;
}

//  static
WTF::String StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                         blink::WebDragData::Item>::
    file_system_id(const blink::WebDragData::Item& item) {
  DCHECK(item.file_system_id.IsNull());
  return item.file_system_id;
}

//  static
scoped_refptr<blink::BlobDataHandle> StructTraits<
    blink::mojom::DragItemFileSystemFileDataView,
    blink::WebDragData::Item>::serialized_blob(const blink::WebDragData::Item&
                                                   item) {
  return item.file_system_blob_info.GetBlobHandle();
}

mojo::PendingRemote<blink::mojom::blink::FileSystemAccessDataTransferToken>
StructTraits<blink::mojom::DataTransferFileDataView, blink::WebDragData::Item>::
    file_system_access_token(const blink::WebDragData::Item& item) {
  // Should never have to send a transfer token information from the renderer
  // to the browser.
  DCHECK(!item.file_system_access_entry);
  return mojo::NullRemote();
}

// static
bool StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                  blink::WebDragData::Item>::
    Read(blink::mojom::DragItemFileSystemFileDataView data,
         blink::WebDragData::Item* out) {
  blink::KURL file_system_url;
  WTF::String file_system_id;

  if (!data.ReadUrl(&file_system_url) ||
      !data.ReadFileSystemId(&file_system_id))
    return false;

  scoped_refptr<blink::BlobDataHandle> blob_data_handle;

  if (!data.ReadSerializedBlob(&blob_data_handle))
    return false;

  blink::WebDragData::Item item;
  item.storage_type = blink::WebDragData::Item::kStorageTypeFileSystemFile;
  item.file_system_url = file_system_url;
  item.file_system_file_size = data.size();
  item.file_system_id = file_system_id;
  if (blob_data_handle) {
    item.file_system_blob_info =
        blink::WebBlobInfo(std::move(blob_data_handle));
  }
  *out = std::move(item);
  return true;
}

// static
bool UnionTraits<blink::mojom::DragItemDataView, blink::WebDragData::Item>::
    Read(blink::mojom::DragItemDataView data, blink::WebDragData::Item* out) {
  blink::WebDragData::Item item;
  switch (data.tag()) {
    case blink::mojom::DragItemDataView::Tag::kString:
      return data.ReadString(out);
    case blink::mojom::DragItemDataView::Tag::kFile:
      return data.ReadFile(out);
    case blink::mojom::DragItemDataView::Tag::kBinary:
      return data.ReadBinary(out);
    case blink::mojom::DragItemDataView::Tag::kFileSystemFile:
      return data.ReadFileSystemFile(out);
  }
  NOTREACHED();
  return false;
}

// static
blink::mojom::DragItemDataView::Tag
UnionTraits<blink::mojom::DragItemDataView, blink::WebDragData::Item>::GetTag(
    const blink::WebDragData::Item& item) {
  switch (item.storage_type) {
    case blink::WebDragData::Item::kStorageTypeString:
      return blink::mojom::DragItemDataView::Tag::kString;
    case blink::WebDragData::Item::kStorageTypeFilename:
      return blink::mojom::DragItemDataView::Tag::kFile;
    case blink::WebDragData::Item::kStorageTypeBinaryData:
      return blink::mojom::DragItemDataView::Tag::kBinary;
    case blink::WebDragData::Item::kStorageTypeFileSystemFile:
      return blink::mojom::DragItemDataView::Tag::kFileSystemFile;
  }
  NOTREACHED();
  return blink::mojom::DragItemDataView::Tag::kString;
}

// static
blink::WebVector<blink::WebDragData::Item>
StructTraits<blink::mojom::DragDataDataView, blink::WebDragData>::items(
    const blink::WebDragData& drag_data) {
  return drag_data.Items();
}

// static
WTF::String StructTraits<blink::mojom::DragDataDataView, blink::WebDragData>::
    file_system_id(const blink::WebDragData& drag_data) {
  // Only used when dragging into Blink.
  DCHECK(drag_data.FilesystemId().IsNull());
  return drag_data.FilesystemId();
}

// static
network::mojom::ReferrerPolicy StructTraits<
    blink::mojom::DragDataDataView,
    blink::WebDragData>::referrer_policy(const blink::WebDragData& drag_data) {
  return drag_data.ReferrerPolicy();
}

// static
bool StructTraits<blink::mojom::DragDataDataView, blink::WebDragData>::Read(
    blink::mojom::DragDataDataView data,
    blink::WebDragData* out) {
  blink::WebVector<blink::WebDragData::Item> items;
  WTF::String file_system_id;
  network::mojom::ReferrerPolicy referrer_policy;
  if (!data.ReadItems(&items) || !data.ReadFileSystemId(&file_system_id) ||
      !data.ReadReferrerPolicy(&referrer_policy))
    return false;

  blink::WebDragData drag_data;
  drag_data.SetItems(std::move(items));
  drag_data.SetFilesystemId(file_system_id);
  drag_data.SetReferrerPolicy(referrer_policy);
  *out = std::move(drag_data);
  return true;
}

}  // namespace mojo

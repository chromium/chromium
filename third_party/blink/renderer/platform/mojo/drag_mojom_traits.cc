// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/mojo/drag_mojom_traits.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

// static
WTF::String StructTraits<blink::mojom::DragItemStringDataView,
                         blink::WebDragData::StringItem>::
    string_type(const blink::WebDragData::StringItem& item) {
  return item.type;
}

// static
WTF::String StructTraits<blink::mojom::DragItemStringDataView,
                         blink::WebDragData::StringItem>::
    string_data(const blink::WebDragData::StringItem& item) {
  return item.data;
}

// static
WTF::String StructTraits<blink::mojom::DragItemStringDataView,
                         blink::WebDragData::StringItem>::
    title(const blink::WebDragData::StringItem& item) {
  return item.title;
}

// static
std::optional<blink::KURL> StructTraits<blink::mojom::DragItemStringDataView,
                                        blink::WebDragData::StringItem>::
    base_url(const blink::WebDragData::StringItem& item) {
  if (item.base_url.IsNull())
    return std::nullopt;
  return item.base_url;
}

// static
bool StructTraits<blink::mojom::DragItemStringDataView,
                  blink::WebDragData::StringItem>::
    Read(blink::mojom::DragItemStringDataView data,
         blink::WebDragData::StringItem* out) {
  WTF::String string_type, string_data, title;
  std::optional<blink::KURL> url;
  if (!data.ReadStringType(&string_type) ||
      !data.ReadStringData(&string_data) || !data.ReadTitle(&title) ||
      !data.ReadBaseUrl(&url))
    return false;

  out->type = string_type;
  out->data = string_data;
  out->title = title;
  out->base_url = url.value_or(blink::KURL());
  return true;
}

// static
base::FilePath StructTraits<blink::mojom::DataTransferFileDataView,
                            blink::WebDragData::FilenameItem>::
    path(const blink::WebDragData::FilenameItem& item) {
  return WebStringToFilePath(item.filename);
}

// static
base::FilePath StructTraits<blink::mojom::DataTransferFileDataView,
                            blink::WebDragData::FilenameItem>::
    display_name(const blink::WebDragData::FilenameItem& item) {
  return WebStringToFilePath(item.display_name);
}

// static
mojo::PendingRemote<blink::mojom::blink::FileSystemAccessDataTransferToken>
StructTraits<blink::mojom::DataTransferFileDataView,
             blink::WebDragData::FilenameItem>::
    file_system_access_token(const blink::WebDragData::FilenameItem& item) {
  // Should never have to send a transfer token information from the renderer
  // to the browser.
  DCHECK(!item.file_system_access_entry);
  return mojo::NullRemote();
}

// static
bool StructTraits<blink::mojom::DataTransferFileDataView,
                  blink::WebDragData::FilenameItem>::
    Read(blink::mojom::DataTransferFileDataView data,
         blink::WebDragData::FilenameItem* out) {
  base::FilePath filename_data, display_name_data;
  if (!data.ReadPath(&filename_data) ||
      !data.ReadDisplayName(&display_name_data))
    return false;

  out->filename = blink::FilePathToWebString(filename_data);
  out->display_name = blink::FilePathToWebString(display_name_data);
  mojo::PendingRemote<::blink::mojom::blink::FileSystemAccessDataTransferToken>
      file_system_access_token(
          data.TakeFileSystemAccessToken<mojo::PendingRemote<
              ::blink::mojom::blink::FileSystemAccessDataTransferToken>>());
  out->file_system_access_entry =
      base::MakeRefCounted<::blink::FileSystemAccessDropData>(
          std::move(file_system_access_token));

  return true;
}

// static
mojo_base::BigBuffer StructTraits<blink::mojom::DragItemBinaryDataView,
                                  blink::WebDragData::BinaryDataItem>::
    data(const blink::WebDragData::BinaryDataItem& item) {
  mojo_base::BigBuffer buffer(item.data.size());
  item.data.ForEachSegment([&buffer](const char* segment, size_t segment_size,
                                     size_t segment_offset) {
    std::copy(segment, segment + segment_size, buffer.data() + segment_offset);
    return true;
  });
  return buffer;
}

// static
bool StructTraits<blink::mojom::DragItemBinaryDataView,
                  blink::WebDragData::BinaryDataItem>::
    is_image_accessible(const blink::WebDragData::BinaryDataItem& item) {
  return item.image_accessible;
}

// static
blink::KURL StructTraits<blink::mojom::DragItemBinaryDataView,
                         blink::WebDragData::BinaryDataItem>::
    source_url(const blink::WebDragData::BinaryDataItem& item) {
  return item.source_url;
}

// static
base::FilePath StructTraits<blink::mojom::DragItemBinaryDataView,
                            blink::WebDragData::BinaryDataItem>::
    filename_extension(const blink::WebDragData::BinaryDataItem& item) {
  return WebStringToFilePath(item.filename_extension);
}

// static
WTF::String StructTraits<blink::mojom::DragItemBinaryDataView,
                         blink::WebDragData::BinaryDataItem>::
    content_disposition(const blink::WebDragData::BinaryDataItem& item) {
  return item.content_disposition;
}

// static
bool StructTraits<blink::mojom::DragItemBinaryDataView,
                  blink::WebDragData::BinaryDataItem>::
    Read(blink::mojom::DragItemBinaryDataView data,
         blink::WebDragData::BinaryDataItem* out) {
  mojo_base::BigBufferView file_contents;
  blink::KURL source_url;
  base::FilePath filename_extension;
  String content_disposition;
  if (!data.ReadData(&file_contents) || !data.ReadSourceUrl(&source_url) ||
      !data.ReadFilenameExtension(&filename_extension) ||
      !data.ReadContentDisposition(&content_disposition)) {
    return false;
  }
  out->data =
      blink::WebData(reinterpret_cast<const char*>(file_contents.data().data()),
                     file_contents.data().size());
  out->image_accessible = data.is_image_accessible();
  out->source_url = source_url;
  out->filename_extension = blink::FilePathToWebString(filename_extension);
  out->content_disposition = content_disposition;

  return true;
}

//  static
blink::KURL StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                         blink::WebDragData::FileSystemFileItem>::
    url(const blink::WebDragData::FileSystemFileItem& item) {
  return item.url;
}

//  static
int64_t StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                     blink::WebDragData::FileSystemFileItem>::
    size(const blink::WebDragData::FileSystemFileItem& item) {
  return item.size;
}

//  static
WTF::String StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                         blink::WebDragData::FileSystemFileItem>::
    file_system_id(const blink::WebDragData::FileSystemFileItem& item) {
  DCHECK(item.file_system_id.IsNull());
  return item.file_system_id;
}

//  static
scoped_refptr<blink::BlobDataHandle>
StructTraits<blink::mojom::DragItemFileSystemFileDataView,
             blink::WebDragData::FileSystemFileItem>::
    serialized_blob(const blink::WebDragData::FileSystemFileItem& item) {
  return item.blob_info.GetBlobHandle();
}

// static
bool StructTraits<blink::mojom::DragItemFileSystemFileDataView,
                  blink::WebDragData::FileSystemFileItem>::
    Read(blink::mojom::DragItemFileSystemFileDataView data,
         blink::WebDragData::FileSystemFileItem* out) {
  blink::KURL file_system_url;
  WTF::String file_system_id;

  if (!data.ReadUrl(&file_system_url) ||
      !data.ReadFileSystemId(&file_system_id))
    return false;

  scoped_refptr<blink::BlobDataHandle> blob_data_handle;

  if (!data.ReadSerializedBlob(&blob_data_handle))
    return false;

  out->url = file_system_url;
  out->size = data.size();
  out->file_system_id = file_system_id;
  if (blob_data_handle) {
    out->blob_info = blink::WebBlobInfo(std::move(blob_data_handle));
  }
  return true;
}

// static
bool UnionTraits<blink::mojom::DragItemDataView, blink::WebDragData::Item>::
    Read(blink::mojom::DragItemDataView data, blink::WebDragData::Item* out) {
  switch (data.tag()) {
    case blink::mojom::DragItemDataView::Tag::kString:
      return data.ReadString(&out->emplace<blink::WebDragData::StringItem>());
    case blink::mojom::DragItemDataView::Tag::kFile:
      return data.ReadFile(&out->emplace<blink::WebDragData::FilenameItem>());
    case blink::mojom::DragItemDataView::Tag::kBinary:
      return data.ReadBinary(
          &out->emplace<blink::WebDragData::BinaryDataItem>());
    case blink::mojom::DragItemDataView::Tag::kFileSystemFile:
      return data.ReadFileSystemFile(
          &out->emplace<blink::WebDragData::FileSystemFileItem>());
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::DragItemDataView::Tag
UnionTraits<blink::mojom::DragItemDataView, blink::WebDragData::Item>::GetTag(
    const blink::WebDragData::Item& item) {
  return absl::visit(
      base::Overloaded{
          [](const blink::WebDragData::StringItem&) {
            return blink::mojom::DragItemDataView::Tag::kString;
          },
          [](const blink::WebDragData::FilenameItem&) {
            return blink::mojom::DragItemDataView::Tag::kFile;
          },
          [](const blink::WebDragData::BinaryDataItem&) {
            return blink::mojom::DragItemDataView::Tag::kBinary;
          },
          [](const blink::WebDragData::FileSystemFileItem&) {
            return blink::mojom::DragItemDataView::Tag::kFileSystemFile;
          }},
      item);
}

// static
const blink::WebVector<blink::WebDragData::Item>&
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
bool StructTraits<blink::mojom::DragDataDataView, blink::WebDragData>::
    force_default_action(const blink::WebDragData& drag_data) {
  return drag_data.ForceDefaultAction();
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
  drag_data.SetForceDefaultAction(data.force_default_action());
  drag_data.SetReferrerPolicy(referrer_policy);
  *out = std::move(drag_data);
  return true;
}

}  // namespace mojo

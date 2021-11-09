// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/data_transfer_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "url/gurl.h"

namespace content {

namespace {

// On Chrome OS paths that exist on an external mount point need to be treated
// differently to make sure the File System Access code accesses these paths via
// the correct file system backend. This method checks if this is the case, and
// updates `entry_path` to the path that should be used by the File System
// Access implementation.
content::FileSystemAccessEntryFactory::PathType MaybeRemapPath(
    base::FilePath* entry_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath virtual_path;
  auto* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (external_mount_points->GetVirtualPath(*entry_path, &virtual_path)) {
    *entry_path = std::move(virtual_path);
    return content::FileSystemAccessEntryFactory::PathType::kExternal;
  }
#endif
  return content::FileSystemAccessEntryFactory::PathType::kLocal;
}

}  // namespace

std::vector<blink::mojom::DataTransferFilePtr> FileInfosToDataTransferFiles(
    const std::vector<ui::FileInfo>& filenames,
    FileSystemAccessManagerImpl* file_system_access_manager,
    int child_id) {
  std::vector<blink::mojom::DataTransferFilePtr> result;
  for (const ui::FileInfo& file_info : filenames) {
    blink::mojom::DataTransferFilePtr file =
        blink::mojom::DataTransferFile::New();
    file->path = file_info.path;
    file->display_name = file_info.display_name;
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
        pending_token;
    base::FilePath entry_path = file_info.path;
    FileSystemAccessManagerImpl::PathType path_type =
        MaybeRemapPath(&entry_path);
    file_system_access_manager->CreateFileSystemAccessDataTransferToken(
        path_type, entry_path, child_id,
        pending_token.InitWithNewPipeAndPassReceiver());
    file->file_system_access_token = std::move(pending_token);
    result.push_back(std::move(file));
  }
  return result;
}

blink::mojom::DragDataPtr DropDataToDragData(
    const DropData& drop_data,
    FileSystemAccessManagerImpl* file_system_access_manager,
    int child_id) {
  // These fields are currently unused when dragging into Blink.
  DCHECK(drop_data.download_metadata.empty());
  DCHECK(drop_data.file_contents_content_disposition.empty());

  std::vector<blink::mojom::DragItemPtr> items;
  if (drop_data.text) {
    blink::mojom::DragItemStringPtr item = blink::mojom::DragItemString::New();
    item->string_type = ui::kMimeTypeText;
    item->string_data = *drop_data.text;
    items.push_back(blink::mojom::DragItem::NewString(std::move(item)));
  }
  if (!drop_data.url.is_empty()) {
    blink::mojom::DragItemStringPtr item = blink::mojom::DragItemString::New();
    item->string_type = ui::kMimeTypeURIList;
    item->string_data = base::UTF8ToUTF16(drop_data.url.spec());
    item->title = drop_data.url_title;
    items.push_back(blink::mojom::DragItem::NewString(std::move(item)));
  }
  if (drop_data.html) {
    blink::mojom::DragItemStringPtr item = blink::mojom::DragItemString::New();
    item->string_type = ui::kMimeTypeHTML;
    item->string_data = *drop_data.html;
    item->base_url = drop_data.html_base_url;
    items.push_back(blink::mojom::DragItem::NewString(std::move(item)));
  }
  std::vector<blink::mojom::DataTransferFilePtr> files =
      FileInfosToDataTransferFiles(drop_data.filenames,
                                   file_system_access_manager, child_id);
  for (size_t i = 0; i < files.size(); ++i) {
    items.push_back(blink::mojom::DragItem::NewFile(std::move(files[i])));
  }
  for (const content::DropData::FileSystemFileInfo& file_system_file :
       drop_data.file_system_files) {
    blink::mojom::DragItemFileSystemFilePtr item =
        blink::mojom::DragItemFileSystemFile::New();
    item->url = file_system_file.url;
    item->size = file_system_file.size;
    item->file_system_id = file_system_file.filesystem_id;
    items.push_back(blink::mojom::DragItem::NewFileSystemFile(std::move(item)));
  }
  if (drop_data.file_contents_source_url.is_valid()) {
    blink::mojom::DragItemBinaryPtr item = blink::mojom::DragItemBinary::New();
    item->data = mojo_base::BigBuffer(
        base::as_bytes(base::make_span(drop_data.file_contents)));
    item->source_url = drop_data.file_contents_source_url;
    item->filename_extension =
        base::FilePath(drop_data.file_contents_filename_extension);
    items.push_back(blink::mojom::DragItem::NewBinary(std::move(item)));
  }
  for (const std::pair<const std::u16string, std::u16string>& data :
       drop_data.custom_data) {
    blink::mojom::DragItemStringPtr item = blink::mojom::DragItemString::New();
    item->string_type = base::UTF16ToUTF8(data.first);
    item->string_data = data.second;
    items.push_back(blink::mojom::DragItem::NewString(std::move(item)));
  }

  return blink::mojom::DragData::New(
      std::move(items),
      // While this shouldn't be a problem in production code, as the
      // real file_system_id should never be empty if used in browser to
      // renderer messages, some tests use this function to test renderer to
      // browser messages, in which case the field is unused and this will hit
      // a DCHECK.
      drop_data.filesystem_id.empty()
          ? absl::nullopt
          : absl::optional<std::string>(
                base::UTF16ToUTF8(drop_data.filesystem_id)),
      drop_data.referrer_policy);
}

blink::mojom::DragDataPtr DropMetaDataToDragData(
    const std::vector<DropData::Metadata>& drop_meta_data) {
  std::vector<blink::mojom::DragItemPtr> items;

  for (const auto& meta_data_item : drop_meta_data) {
    if (meta_data_item.kind == DropData::Kind::STRING) {
      blink::mojom::DragItemStringPtr item =
          blink::mojom::DragItemString::New();
      item->string_type = base::UTF16ToUTF8(meta_data_item.mime_type);
      // Have to pass a dummy URL here instead of an empty URL because the
      // DropData received by browser_plugins goes through a round trip:
      // DropData::MetaData --> WebDragData-->DropData. In the end, DropData
      // will contain an empty URL (which means no URL is dragged) if the URL in
      // WebDragData is empty.
      if (base::EqualsASCII(meta_data_item.mime_type, ui::kMimeTypeURIList)) {
        item->string_data = u"about:dragdrop-placeholder";
      }
      items.push_back(blink::mojom::DragItem::NewString(std::move(item)));
      continue;
    }

    // TODO(hush): crbug.com/584789. Blink needs to support creating a file with
    // just the mimetype. This is needed to drag files to WebView on Android
    // platform.
    if ((meta_data_item.kind == DropData::Kind::FILENAME) &&
        !meta_data_item.filename.empty()) {
      blink::mojom::DataTransferFilePtr item =
          blink::mojom::DataTransferFile::New();
      item->path = meta_data_item.filename;
      items.push_back(blink::mojom::DragItem::NewFile(std::move(item)));
      continue;
    }

    if (meta_data_item.kind == DropData::Kind::FILESYSTEMFILE) {
      blink::mojom::DragItemFileSystemFilePtr item =
          blink::mojom::DragItemFileSystemFile::New();
      item->url = meta_data_item.file_system_url;
      items.push_back(
          blink::mojom::DragItem::NewFileSystemFile(std::move(item)));
      continue;
    }

    if (meta_data_item.kind == DropData::Kind::BINARY) {
      blink::mojom::DragItemBinaryPtr item =
          blink::mojom::DragItemBinary::New();
      item->source_url = meta_data_item.file_contents_url;
      items.push_back(blink::mojom::DragItem::NewBinary(std::move(item)));
      continue;
    }
  }
  return blink::mojom::DragData::New(std::move(items), absl::nullopt,
                                     network::mojom::ReferrerPolicy::kDefault);
}

DropData DragDataToDropData(const blink::mojom::DragData& drag_data) {
  // This field should be empty when dragging from the renderer.
  DCHECK(!drag_data.file_system_id);

  DropData result;
  for (const blink::mojom::DragItemPtr& item : drag_data.items) {
    switch (item->which()) {
      case blink::mojom::DragItemDataView::Tag::STRING: {
        const blink::mojom::DragItemStringPtr& string_item = item->get_string();
        std::string str_type = string_item->string_type;
        if (str_type == ui::kMimeTypeText) {
          result.text = string_item->string_data;
        } else if (str_type == ui::kMimeTypeURIList) {
          result.url = GURL(string_item->string_data);
          if (string_item->title)
            result.url_title = *string_item->title;
        } else if (str_type == ui::kMimeTypeDownloadURL) {
          result.download_metadata = string_item->string_data;
          result.referrer_policy = drag_data.referrer_policy;
        } else if (str_type == ui::kMimeTypeHTML) {
          result.html = string_item->string_data;
          if (string_item->base_url)
            result.html_base_url = *string_item->base_url;
        } else {
          result.custom_data.emplace(
              base::UTF8ToUTF16(string_item->string_type),
              string_item->string_data);
        }
        break;
      }
      case blink::mojom::DragItemDataView::Tag::BINARY: {
        DCHECK(result.file_contents.empty());

        const blink::mojom::DragItemBinaryPtr& binary_item = item->get_binary();
        base::span<const uint8_t> contents = base::make_span(binary_item->data);
        result.file_contents.assign(contents.begin(), contents.end());
        result.file_contents_source_url = binary_item->source_url;
        result.file_contents_filename_extension =
            binary_item->filename_extension.value();
        if (binary_item->content_disposition) {
          result.file_contents_content_disposition =
              *binary_item->content_disposition;
        }
        break;
      }
      case blink::mojom::DragItemDataView::Tag::FILE: {
        const blink::mojom::DataTransferFilePtr& file_item = item->get_file();
        // TODO(varunjain): This only works on chromeos. Support win/mac/gtk.
        result.filenames.emplace_back(file_item->path, file_item->display_name);
        break;
      }
      case blink::mojom::DragItemDataView::Tag::FILE_SYSTEM_FILE: {
        const blink::mojom::DragItemFileSystemFilePtr& file_system_file_item =
            item->get_file_system_file();
        // This field should be empty when dragging from the renderer.
        DCHECK(!file_system_file_item->file_system_id);

        DropData::FileSystemFileInfo info;
        info.url = file_system_file_item->url;
        info.size = file_system_file_item->size;
        info.filesystem_id = std::string();
        result.file_system_files.push_back(std::move(info));
        break;
      }
    }
  }
  return result;
}

}  // namespace content

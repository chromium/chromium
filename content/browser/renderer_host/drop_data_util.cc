// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/drop_data_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/public/common/drop_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom.h"
#include "third_party/blink/public/mojom/page/drag.mojom.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/file_info/file_info.h"
#include "url/gurl.h"

namespace content {

blink::mojom::DragDataPtr DropDataToDragData(
    const DropData& drop_data,
    NativeFileSystemManagerImpl* native_file_system_manager,
    int child_id) {
  // These fields are currently unused when dragging into Blink.
  DCHECK(drop_data.download_metadata.empty());
  DCHECK(drop_data.file_contents.empty());
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
  for (const ui::FileInfo& file : drop_data.filenames) {
    blink::mojom::DragItemFilePtr item = blink::mojom::DragItemFile::New();
    item->path = file.path;
    item->display_name = file.display_name;
    mojo::PendingRemote<blink::mojom::NativeFileSystemDragDropToken>
        pending_token;
    native_file_system_manager->CreateNativeFileSystemDragDropToken(
        file.path, child_id, pending_token.InitWithNewPipeAndPassReceiver());
    item->native_file_system_token = std::move(pending_token);
    items.push_back(blink::mojom::DragItem::NewFile(std::move(item)));
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
  for (const std::pair<base::string16, base::string16> data :
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
          ? base::nullopt
          : base::Optional<std::string>(
                base::UTF16ToUTF8(drop_data.filesystem_id)),
      drop_data.referrer_policy);
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
        std::vector<uint8_t> contents = binary_item->data;
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
        const blink::mojom::DragItemFilePtr& file_item = item->get_file();
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

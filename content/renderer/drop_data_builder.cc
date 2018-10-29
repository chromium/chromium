// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/drop_data_builder.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "ui/base/clipboard/clipboard.h"

using blink::WebDragData;
using blink::WebString;
using blink::WebVector;

namespace content {

// static
DropData DropDataBuilder::Build(const WebDragData& drag_data) {
  DropData result;
  result.key_modifiers = drag_data.ModifierKeyState();
  result.referrer_policy = network::mojom::ReferrerPolicy::kDefault;

  const WebVector<WebDragData::Item>& item_list = drag_data.Items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    switch (item.storage_type) {
      case WebDragData::Item::kStorageTypeString: {
        base::string16 str_type(item.string_type.Utf16());
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeText)) {
          result.text = WebString::ToNullableString16(item.string_data);
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeURIList)) {
          result.url = blink::WebStringToGURL(item.string_data);
          result.url_title = item.title.Utf16();
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeDownloadURL)) {
          result.download_metadata = item.string_data.Utf16();
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeHTML)) {
          result.html = WebString::ToNullableString16(item.string_data);
          result.html_base_url = item.base_url;
          break;
        }
        result.custom_data.insert(
            std::make_pair(item.string_type.Utf16(), item.string_data.Utf16()));
        break;
      }
      case WebDragData::Item::kStorageTypeBinaryData:
        DCHECK(result.file_contents.empty());
        result.file_contents.reserve(item.binary_data.size());
        item.binary_data.ForEachSegment([&result](const char* segment,
                                                  size_t segment_size,
                                                  size_t segment_offset) {
          result.file_contents.append(segment, segment_size);
          return true;
        });
        result.file_contents_source_url = item.binary_data_source_url;
#if defined(OS_WIN)
        result.file_contents_filename_extension =
            item.binary_data_filename_extension.Utf16();
#else
        result.file_contents_filename_extension =
            item.binary_data_filename_extension.Utf8();
#endif
        result.file_contents_content_disposition =
            item.binary_data_content_disposition.Utf8();
        break;
      case WebDragData::Item::kStorageTypeFilename:
        // TODO(varunjain): This only works on chromeos. Support win/mac/gtk.
        result.filenames.push_back(
            ui::FileInfo(blink::WebStringToFilePath(item.filename_data),
                         blink::WebStringToFilePath(item.display_name_data)));
        break;
      case WebDragData::Item::kStorageTypeFileSystemFile: {
        DropData::FileSystemFileInfo info;
        info.url = item.file_system_url;
        info.size = item.file_system_file_size;
        info.filesystem_id = item.file_system_id.Ascii();
        result.file_system_files.push_back(info);
        break;
      }
    }
  }

  return result;
}

}  // namespace content

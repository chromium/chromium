// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DATA_TRANSFER_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_DATA_TRANSFER_UTIL_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/mojom/data_transfer/data_transfer.mojom-forward.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-forward.h"

namespace content {
class FileSystemAccessManagerImpl;
class ChromeBlobStorageContext;

// Convert ui::FileInfos to mojo DataTransferFiles. Creates
// DataTransferAccessTokens and remaps paths if needed.
CONTENT_EXPORT
std::vector<blink::mojom::DataTransferFilePtr> FileInfosToDataTransferFiles(
    const std::vector<ui::FileInfo>& filenames,
    FileSystemAccessManagerImpl* file_system_access_manager,
    int child_id);

CONTENT_EXPORT
blink::mojom::DragDataPtr DropDataToDragData(
    const DropData& drop_data,
    FileSystemAccessManagerImpl* file_system_access_manager,
    int child_id,
    scoped_refptr<ChromeBlobStorageContext> chrome_blob_storage_context);

CONTENT_EXPORT
blink::mojom::DragDataPtr DropMetaDataToDragData(
    const std::vector<DropData::Metadata>& drop_meta_data);

CONTENT_EXPORT DropData
DragDataToDropData(const blink::mojom::DragData& drag_data);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DATA_TRANSFER_UTIL_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_util.h"

#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

blink::mojom::StorageType FileSystemTypeToQuotaStorageType(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      return blink::mojom::StorageType::kTemporary;
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return blink::mojom::StorageType::kSyncable;
    default:
      return blink::mojom::StorageType::kUnknown;
  }
}

}  // namespace storage

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_util.h"

#include "base/feature_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

blink::mojom::StorageType FileSystemTypeToQuotaStorageType(
    FileSystemType type) {
  if (base::FeatureList::IsEnabled(
          blink::features::kPersistentQuotaIsTemporaryQuota) &&
      (type == kFileSystemTypeTemporary || type == kFileSystemTypePersistent)) {
    return blink::mojom::StorageType::kTemporary;
  }
  switch (type) {
    case kFileSystemTypeTemporary:
      return blink::mojom::StorageType::kTemporary;
    case kFileSystemTypePersistent:
      return blink::mojom::StorageType::kPersistent;
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return blink::mojom::StorageType::kSyncable;
    case kFileSystemTypePluginPrivate:
      return blink::mojom::StorageType::kQuotaNotManaged;
    default:
      return blink::mojom::StorageType::kUnknown;
  }
}

}  // namespace storage

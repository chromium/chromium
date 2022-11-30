// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_UTIL_H_

#include "base/component_export.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

// Maps a FileSystemType to the quota type that backs it.
COMPONENT_EXPORT(STORAGE_BROWSER)
blink::mojom::StorageType FileSystemTypeToQuotaStorageType(FileSystemType type);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_UTIL_H_

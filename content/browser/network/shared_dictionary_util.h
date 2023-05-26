// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SHARED_DICTIONARY_UTIL_H_
#define CONTENT_BROWSER_NETWORK_SHARED_DICTIONARY_UTIL_H_

#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {

class StoragePartition;

// Caliculates the cache max size for Shared Dictionary Cache on a different
// thread, and sends the caliculated value to NetworkContext of
// `storage_partition`.
// When `path` is empty, caliculates the max size for in memory cache using
// base::SysInfo::AmountOfPhysicalMemory. Otherwise, caliculates the max size
// for on disk cache using base::SysInfo::AmountOfFreeDiskSpace with the `path`.
void CalculateAndSetSharedDictionaryCacheMaxSize(
    base::WeakPtr<StoragePartition> storage_partition,
    const base::FilePath& path);

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SHARED_DICTIONARY_UTIL_H_

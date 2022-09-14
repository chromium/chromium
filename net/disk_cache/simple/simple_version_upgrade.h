// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_VERSION_UPGRADE_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_VERSION_UPGRADE_H_

// Defines functionality to upgrade the file structure of the Simple Cache
// Backend on disk. Assumes no backend operations are running simultaneously.
// Hence must be run at cache initialization step.

#include <stdint.h>

#include "net/base/cache_type.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
}

namespace disk_cache {

class BackendFileOperations;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SimpleCacheConsistencyResult {
  kOK = 0,
  kCreateDirectoryFailed = 1,
  kBadFakeIndexFile = 2,
  kBadInitialMagicNumber = 3,
  kVersionTooOld = 4,
  kVersionFromTheFuture = 5,
  kBadZeroCheck = 6,
  kUpgradeIndexV5V6Failed = 7,
  kWriteFakeIndexFileFailed = 8,
  kReplaceFileFailed = 9,
  kBadFakeIndexReadSize = 10,
  kMaxValue = kBadFakeIndexReadSize,
};

// Performs all necessary disk IO to upgrade the cache structure if it is
// needed.
//
// Returns true iff no errors were found during consistency checks and all
// necessary transitions succeeded. If this function fails, there is nothing
// left to do other than dropping the whole cache directory.
NET_EXPORT_PRIVATE SimpleCacheConsistencyResult
UpgradeSimpleCacheOnDisk(BackendFileOperations* file_operations,
                         const base::FilePath& path);

// Check if the cache structure at the given path is empty except for index
// files.  If so, then delete the index files.  Returns true if any files
// were deleted.
NET_EXPORT_PRIVATE bool DeleteIndexFilesIfCacheIsEmpty(
    const base::FilePath& path);

struct NET_EXPORT_PRIVATE FakeIndexData {
  FakeIndexData();

  // Must be equal to simplecache_v4::kSimpleInitialMagicNumber.
  uint64_t initial_magic_number;

  // Must be equal kSimpleVersion when the cache backend is instantiated.
  uint32_t version;

  // These must be zero. The first was used for experiment type (With a max
  // valid value of 2), and the second was used for an experiment parameter.
  uint32_t zero;
  uint32_t zero2;
};

// Exposed for testing.
NET_EXPORT_PRIVATE bool UpgradeIndexV5V6(BackendFileOperations* file_operations,
                                         const base::FilePath& cache_directory);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_VERSION_UPGRADE_H_

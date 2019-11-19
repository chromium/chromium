// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONSTANTS_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "build/build_config.h"

namespace storage {

constexpr size_t kDefaultIPCMemorySize = 250u * 1024;
constexpr size_t kDefaultSharedMemorySize = 10u * 1024 * 1024;
constexpr size_t kDefaultMaxBlobInMemorySpace = 500u * 1024 * 1024;
constexpr uint64_t kDefaultMaxBlobDiskSpace = 0ull;
constexpr uint64_t kDefaultMaxPageFileSize = 100ull * 1024 * 1024;

#if defined(OS_ANDROID)
// On minimal Android maximum in-memory space can be as low as 5MB.
constexpr uint64_t kDefaultMinPageFileSize = 5ull * 1024 * 1024 / 2;
const float kDefaultMaxBlobInMemorySpaceUnderPressureRatio = 0.02f;
#else
constexpr uint64_t kDefaultMinPageFileSize = 5ull * 1024 * 1024;
const float kDefaultMaxBlobInMemorySpaceUnderPressureRatio = 0.002f;
#endif

// Specifies the size at which blob data will be transported by file instead of
// memory. Overrides internal logic and allows perf tests to use the file path.
constexpr const char kBlobFileTransportByFileTriggerSwitch[] =
    "blob-transport-by-file-trigger";

// All sizes are in bytes.
struct COMPONENT_EXPORT(STORAGE_BROWSER) BlobStorageLimits {
  BlobStorageLimits();
  ~BlobStorageLimits();
  BlobStorageLimits(const BlobStorageLimits&);
  BlobStorageLimits& operator=(const BlobStorageLimits&);

  // Returns if the current configuration is valid.
  bool IsValid() const;

  size_t memory_limit_before_paging() const {
    return max_blob_in_memory_space - min_page_file_size;
  }

  // If disk space goes less than this we stop allocating more disk quota.
  uint64_t min_available_external_disk_space() const {
    return 2ull * memory_limit_before_paging();
  }

  bool IsDiskSpaceConstrained() const {
    return desired_max_disk_space != effective_max_disk_space;
  }

  // This is the maximum amount of memory we can send in an IPC.
  size_t max_ipc_memory_size = kDefaultIPCMemorySize;
  // This is the maximum size of a shared memory handle. This can be overriden
  // using the "blob-transport-shared-memory-max-size" switch (see
  // BlobMemoryController).
  size_t max_shared_memory_size = kDefaultSharedMemorySize;

  // This is the maximum size of a bytes BlobDataItem. Only used for mojo
  // based blob transportation, as the old IPC/shared memory based
  // implementation doesn't support different values for this and
  // max_shared_memory_size.
  size_t max_bytes_data_item_size = kDefaultSharedMemorySize;

  // This is the maximum amount of memory we can use to store blobs.
  size_t max_blob_in_memory_space = kDefaultMaxBlobInMemorySpace;
  // The ratio applied to |max_blob_in_memory_space| to reduce memory usage
  // under memory pressure. Note: Under pressure we modify the
  // |min_page_file_size| to ensure we can evict items until we get below the
  // reduced memory limit.
  float max_blob_in_memory_space_under_pressure_ratio =
      kDefaultMaxBlobInMemorySpaceUnderPressureRatio;

  // This is the maximum amount of disk space we can use.
  uint64_t desired_max_disk_space = kDefaultMaxBlobDiskSpace;
  // This value will change based on the amount of free space on the device.
  uint64_t effective_max_disk_space = kDefaultMaxBlobDiskSpace;

  // This is the minimum file size we can use when paging blob items to disk.
  // We combine items until we reach at least this size.
  uint64_t min_page_file_size = kDefaultMinPageFileSize;
  // This is the maximum file size we can create.
  uint64_t max_file_size = kDefaultMaxPageFileSize;
  // This overrides the minimum size for transporting a blob using the file
  // strategy. This allows perf tests to force file transportation. This is
  // usually set using the "blob-transport-by-file-min-size" switch (see
  // BlobMemoryController).
  uint64_t override_file_transport_min_size = 0ull;
};

enum class IPCBlobItemRequestStrategy {
  UNKNOWN = 0,
  IPC,
  SHARED_MEMORY,
  FILE,
  LAST = FILE
};

// This is the enum to rule them all in the blob system.
// These values are used in UMA metrics, so they should not be changed. Please
// update LAST_ERROR if you add an error condition and LAST if you add new
// state.
enum class BlobStatus {
  // Error case section:
  // The construction arguments are invalid. This is considered a bad ipc.
  ERR_INVALID_CONSTRUCTION_ARGUMENTS = 0,
  // We don't have enough memory for the blob.
  ERR_OUT_OF_MEMORY = 1,
  // We couldn't create or write to a file. File system error, like a full disk.
  ERR_FILE_WRITE_FAILED = 2,
  // The renderer was destroyed while data was in transit.
  ERR_SOURCE_DIED_IN_TRANSIT = 3,
  // The renderer destructed the blob before it was done transferring, and there
  // were no outstanding references (no one is waiting to read) to keep the
  // blob alive.
  ERR_BLOB_DEREFERENCED_WHILE_BUILDING = 4,
  // A blob that we referenced during construction is broken, or a browser-side
  // builder tries to build a blob with a blob reference that isn't finished
  // constructing.
  ERR_REFERENCED_BLOB_BROKEN = 5,
  // A file that we referenced during construction is not accessible to the
  // renderer trying to create the blob.
  ERR_REFERENCED_FILE_UNAVAILABLE = 6,
  LAST_ERROR = ERR_REFERENCED_FILE_UNAVAILABLE,

  // Blob state section:
  // The blob has finished.
  DONE = 200,
  // Waiting for memory or file quota for the to-be transported data.
  PENDING_QUOTA = 201,
  // Waiting for data to be transported (quota has been granted).
  PENDING_TRANSPORT = 202,
  // Waiting for any operations involving dependent blobs after transport data
  // has been populated. See BlobEntry::BuildingState for more info.
  PENDING_REFERENCED_BLOBS = 203,
  // Waiting for construction to begin.
  PENDING_CONSTRUCTION = 204,
  LAST_PENDING = PENDING_CONSTRUCTION,
  LAST = LAST_PENDING
};

using BlobStatusCallback = base::OnceCallback<void(BlobStatus)>;

// Returns if the status is an error code.
COMPONENT_EXPORT(STORAGE_BROWSER) bool BlobStatusIsError(BlobStatus status);

COMPONENT_EXPORT(STORAGE_BROWSER) bool BlobStatusIsPending(BlobStatus status);

// Returns if the status is a bad enough error to flag the IPC as bad. This is
// only INVALID_CONSTRUCTION_ARGUMENTS.
COMPONENT_EXPORT(STORAGE_BROWSER) bool BlobStatusIsBadIPC(BlobStatus status);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONSTANTS_H_

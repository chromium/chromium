// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_STAGING_WRITER_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_STAGING_WRITER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/common/content_export.h"
#include "crypto/hash.h"
#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom.h"

namespace content {

// Streams ingested bytes into a quarantine ("staging") file while computing
// their digest in flight for sequential writes, so that multi-gigabyte
// resources are neither buffered in memory nor re-read from disk in the common
// append-only case. If the caller seeks (`offset != size_`) or truncates mid-
// stream, `needs_rehash_` is latched and `Finish()` re-hashes the on-disk file
// before closing it.
//
// This class must be used on a sequence that allows blocking. It is intended
// to be owned by a base::SequenceBound<>.
class CONTENT_EXPORT CrossOriginStorageStagingWriter {
 public:
  struct CONTENT_EXPORT Result {
    Result();
    Result(const Result&);
    Result& operator=(const Result&);
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();

    // Status of the staging write (`kSuccess`, `kQuotaExceeded`, or
    // `kOperationFailed`). The digest is meaningless unless this is `kSuccess`.
    CrossOriginStorageStatus status =
        CrossOriginStorageStatus::kOperationFailed;

    // Number of bytes written to the staging file.
    int64_t size = 0;

    // Path of the closed staging file to pass to `Commit()`.
    base::FilePath path;

    std::vector<uint8_t> digest;
  };

  // `file` must already be open for reading and writing; it is created by the
  // registry on its own sequence so that it cannot collide with the staging
  // sweep that runs at initialization. `path` is retained only so the file can
  // be deleted again on failure.
  CrossOriginStorageStagingWriter(
      blink::mojom::CrossOriginStorageAlgorithm algorithm,
      base::FilePath path,
      base::File file,
      int64_t max_size_bytes = cos_constants::kMaxResourceSizeBytes);
  CrossOriginStorageStagingWriter(const CrossOriginStorageStagingWriter&) =
      delete;
  CrossOriginStorageStagingWriter& operator=(
      const CrossOriginStorageStagingWriter&) = delete;
  ~CrossOriginStorageStagingWriter();

  // Appends `data` at the current end of the staging file and folds it into
  // the running digest. Returns `kSuccess`, `kQuotaExceeded` if the maximum
  // resource size is exceeded, or `kOperationFailed` on write failure; failures
  // are also latched for Finish().
  CrossOriginStorageStatus Append(std::vector<uint8_t> data);

  // Writes `data` at `offset`. If `offset` equals the current file size and no
  // prior seek/truncate occurred, updates the streaming digest in flight;
  // otherwise sets `needs_rehash_` so `Finish()` re-hashes the on-disk file.
  CrossOriginStorageStatus WriteAtOffset(uint64_t offset,
                                         std::vector<uint8_t> data);

  // Resizes the staging file to `length` bytes and marks the file for rehashing
  // in `Finish()`.
  CrossOriginStorageStatus Truncate(uint64_t length);

  // Closes the staging file and returns the total size and digest of the final
  // on-disk contents. Must be called at most once.
  Result Finish();

  // Closes and deletes the staging file without producing a digest. Calling
  // `Discard()` after `Finish()` is a no-op and will not delete the finished
  // file since ownership of the staging file was transferred to `Result`.
  void Discard();

  void SetMaxSizeBytesForTesting(int64_t max_size_bytes) {
    max_size_bytes_ = max_size_bytes;
  }

 private:
  bool RehashFromDisk(base::span<uint8_t> out);

  blink::mojom::CrossOriginStorageAlgorithm algorithm_;
  base::FilePath path_;
  base::File file_;
  crypto::hash::Hasher hasher_;
  int64_t size_ = 0;
  int64_t max_size_bytes_ = cos_constants::kMaxResourceSizeBytes;
  bool needs_rehash_ = false;
  CrossOriginStorageStatus status_ = CrossOriginStorageStatus::kSuccess;
  bool finished_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_STAGING_WRITER_H_

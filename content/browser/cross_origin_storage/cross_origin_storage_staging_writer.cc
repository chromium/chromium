// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"

namespace content {

namespace {

constexpr size_t kRehashBufferSize = 64 * 1024;

crypto::hash::HashKind HashKindForAlgorithm(
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      return crypto::hash::kSha256;
  }
}

size_t DigestSizeForAlgorithm(
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      return crypto::hash::kSha256Size;
  }
}

}  // namespace

CrossOriginStorageStagingWriter::Result::Result() = default;
CrossOriginStorageStagingWriter::Result::Result(const Result&) = default;
CrossOriginStorageStagingWriter::Result&
CrossOriginStorageStagingWriter::Result::operator=(const Result&) = default;
CrossOriginStorageStagingWriter::Result::Result(Result&&) = default;
CrossOriginStorageStagingWriter::Result&
CrossOriginStorageStagingWriter::Result::operator=(Result&&) = default;
CrossOriginStorageStagingWriter::Result::~Result() = default;

CrossOriginStorageStagingWriter::CrossOriginStorageStagingWriter(
    blink::mojom::CrossOriginStorageAlgorithm algorithm,
    base::FilePath path,
    base::File file,
    int64_t max_size_bytes)
    : algorithm_(algorithm),
      path_(std::move(path)),
      file_(std::move(file)),
      hasher_(HashKindForAlgorithm(algorithm)),
      max_size_bytes_(max_size_bytes) {
  if (!file_.IsValid()) {
    DLOG(ERROR) << "Invalid COS staging file: " << path_.value();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    path_.clear();
  }
}

CrossOriginStorageStagingWriter::~CrossOriginStorageStagingWriter() {
  // If the job was torn down without an explicit Finish()/Discard() (for
  // example because the renderer disconnected mid-upload), make sure the
  // partial file does not linger in the staging directory.
  if (!finished_) {
    Discard();
  }
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::Append(
    std::vector<uint8_t> data) {
  return WriteAtOffset(static_cast<uint64_t>(size_), std::move(data));
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::WriteAtOffset(
    uint64_t offset,
    std::vector<uint8_t> data) {
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    return status_;
  }
  if (finished_) {
    return CrossOriginStorageStatus::kOperationFailed;
  }

  int64_t end_offset = 0;
  if (!base::CheckAdd(offset, data.size()).AssignIfValid(&end_offset) ||
      end_offset < 0 || end_offset > max_size_bytes_) {
    DLOG(ERROR) << "COS staging write exceeds maximum resource size: "
                << path_.value();
    status_ = CrossOriginStorageStatus::kQuotaExceeded;
    Discard();
    return status_;
  }

  if (data.empty()) {
    return CrossOriginStorageStatus::kSuccess;
  }

  if (offset != static_cast<uint64_t>(size_)) {
    needs_rehash_ = true;
  }

  base::span<const uint8_t> remaining(data);
  int64_t write_offset = static_cast<int64_t>(offset);
  constexpr size_t kMaxWriteChunkSize =
      static_cast<size_t>(std::numeric_limits<int>::max());
  while (!remaining.empty()) {
    const size_t chunk_len = std::min(remaining.size(), kMaxWriteChunkSize);
    base::span<const uint8_t> chunk = remaining.first(chunk_len);
    if (!file_.WriteAndCheck(write_offset, chunk)) {
      DLOG(ERROR) << "Failed to write to COS staging file: " << path_.value();
      status_ = CrossOriginStorageStatus::kOperationFailed;
      Discard();
      return status_;
    }
    write_offset += static_cast<int64_t>(chunk_len);
    remaining = remaining.subspan(chunk_len);
  }

  if (!needs_rehash_) {
    hasher_.Update(data);
  }
  size_ = std::max(size_, end_offset);
  return CrossOriginStorageStatus::kSuccess;
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::Truncate(
    uint64_t length) {
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    return status_;
  }
  if (finished_) {
    return CrossOriginStorageStatus::kOperationFailed;
  }
  int64_t new_size = 0;
  if (!base::CheckedNumeric<uint64_t>(length).AssignIfValid(&new_size) ||
      new_size < 0 || new_size > max_size_bytes_) {
    status_ = CrossOriginStorageStatus::kQuotaExceeded;
    Discard();
    return status_;
  }
  if (new_size == size_) {
    return CrossOriginStorageStatus::kSuccess;
  }
  if (!file_.SetLength(new_size)) {
    DLOG(ERROR) << "Failed to truncate COS staging file: " << path_.value();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    Discard();
    return status_;
  }
  if (new_size == 0) {
    hasher_ = crypto::hash::Hasher(HashKindForAlgorithm(algorithm_));
    needs_rehash_ = false;
  } else {
    needs_rehash_ = true;
  }
  size_ = new_size;
  return CrossOriginStorageStatus::kSuccess;
}

CrossOriginStorageStagingWriter::Result
CrossOriginStorageStagingWriter::Finish() {
  Result result;
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    // Do not leave a partially written file behind.
    Discard();
    result.status = status_;
    return result;
  }
  if (finished_) {
    result.status = CrossOriginStorageStatus::kOperationFailed;
    return result;
  }

  result.digest.resize(DigestSizeForAlgorithm(algorithm_));
  if (needs_rehash_) {
    if (!RehashFromDisk(result.digest)) {
      result.digest.clear();
      status_ = CrossOriginStorageStatus::kOperationFailed;
      Discard();
      result.status = status_;
      return result;
    }
  } else {
    hasher_.Finish(result.digest);
  }

  if (!file_.Flush()) {
    DLOG(ERROR) << "Failed to flush COS staging file: " << path_.value();
    result.digest.clear();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    Discard();
    result.status = status_;
    return result;
  }

  finished_ = true;
  file_.Close();
  // `finished_ = true` prevents the destructor from deleting the staging file
  // now that ownership transfers to `CrossOriginStorageRegistry::Commit()`.
  result.path = std::move(path_);
  path_.clear();
  result.size = size_;
  result.status = CrossOriginStorageStatus::kSuccess;
  return result;
}

bool CrossOriginStorageStagingWriter::RehashFromDisk(base::span<uint8_t> out) {
  crypto::hash::Hasher disk_hasher(HashKindForAlgorithm(algorithm_));
  std::vector<uint8_t> buffer(kRehashBufferSize);
  int64_t offset = 0;
  while (offset < size_) {
    const size_t to_read = static_cast<size_t>(
        std::min<int64_t>(size_ - offset, kRehashBufferSize));
    std::optional<size_t> bytes_read =
        file_.Read(offset, base::span(buffer).first(to_read));
    if (!bytes_read.has_value() || *bytes_read == 0) {
      DLOG(ERROR) << "Failed to re-read COS staging file for hashing: "
                  << path_.value();
      return false;
    }
    disk_hasher.Update(base::span(buffer).first(*bytes_read));
    offset += static_cast<int64_t>(*bytes_read);
  }
  disk_hasher.Finish(out);
  return true;
}

void CrossOriginStorageStagingWriter::Discard() {
  if (status_ == CrossOriginStorageStatus::kSuccess) {
    status_ = CrossOriginStorageStatus::kOperationFailed;
  }
  finished_ = true;
  if (file_.IsValid()) {
    file_.Close();
  }
  if (!path_.empty()) {
    if (!base::DeleteFile(path_)) {
      DLOG(WARNING) << "Failed to delete COS staging file: " << path_.value();
    }
    path_.clear();
  }
}

}  // namespace content

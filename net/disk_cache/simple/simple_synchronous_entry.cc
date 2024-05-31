// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/simple/simple_synchronous_entry.h"

#include <cstring>
#include <functional>
#include <limits>
#include <optional>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/zlib/zlib.h"

using base::FilePath;
using base::Time;

namespace disk_cache {

namespace {

void RecordSyncOpenResult(net::CacheType cache_type, OpenEntryResult result) {
  DCHECK_LT(result, OPEN_ENTRY_MAX);
  SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncOpenResult", cache_type, result,
                     OPEN_ENTRY_MAX);
}

void RecordWriteResult(net::CacheType cache_type, SyncWriteResult result) {
  SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncWriteResult", cache_type, result,
                     SYNC_WRITE_RESULT_MAX);
}

void RecordCheckEOFResult(net::CacheType cache_type, CheckEOFResult result) {
  SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncCheckEOFResult", cache_type, result,
                     CHECK_EOF_RESULT_MAX);
}

void RecordCloseResult(net::CacheType cache_type, CloseResult result) {
  SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncCloseResult", cache_type, result,
                     CLOSE_RESULT_MAX);
}

void RecordOpenPrefetchMode(net::CacheType cache_type, OpenPrefetchMode mode) {
  SIMPLE_CACHE_UMA(ENUMERATION, "SyncOpenPrefetchMode", cache_type, mode,
                   OPEN_PREFETCH_MAX);
}

void RecordDiskCreateLatency(net::CacheType cache_type, base::TimeDelta delay) {
  SIMPLE_CACHE_LOCAL(TIMES, "DiskCreateLatency", cache_type, delay);
}

bool CanOmitEmptyFile(int file_index) {
  DCHECK_GE(file_index, 0);
  DCHECK_LT(file_index, kSimpleEntryNormalFileCount);
  return file_index == simple_util::GetFileIndexFromStreamIndex(2);
}

bool TruncatePath(const FilePath& filename_to_truncate,
                  BackendFileOperations* file_operations) {
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  base::File file_to_truncate =
      file_operations->OpenFile(filename_to_truncate, flags);
  if (!file_to_truncate.IsValid())
    return false;
  if (!file_to_truncate.SetLength(0))
    return false;
  return true;
}

void CalculateSHA256OfKey(const std::string& key,
                          net::SHA256HashValue* out_hash_value) {
  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(key.data(), key.size());
  hash->Finish(out_hash_value, sizeof(*out_hash_value));
}

SimpleFileTracker::SubFile SubFileForFileIndex(int file_index) {
  DCHECK_GT(kSimpleEntryNormalFileCount, file_index);
  return file_index == 0 ? SimpleFileTracker::SubFile::FILE_0
                         : SimpleFileTracker::SubFile::FILE_1;
}

int FileIndexForSubFile(SimpleFileTracker::SubFile sub_file) {
  DCHECK_NE(SimpleFileTracker::SubFile::FILE_SPARSE, sub_file);
  return sub_file == SimpleFileTracker::SubFile::FILE_0 ? 0 : 1;
}

}  // namespace

// Helper class to track a range of data prefetched from a file.
class SimpleSynchronousEntry::PrefetchData final {
 public:
  explicit PrefetchData(size_t file_size)
      : file_size_(file_size), earliest_requested_offset_(file_size) {}

  // Returns true if the specified range within the file has been completely
  // prefetched.  Returns false if any part of the range has not been
  // prefetched.
  bool HasData(size_t offset, size_t length) {
    size_t end = 0;
    if (!base::CheckAdd(offset, length).AssignIfValid(&end))
      return false;
    UpdateEarliestOffset(offset);
    return offset >= offset_in_file_ &&
           end <= (offset_in_file_ + buffer_.size());
  }

  // Read the given range out of the prefetch buffer into the target
  // destination buffer.  If the range is not wholely contained within
  // the prefetch buffer than no data will be written to the target
  // buffer.  Returns true if the range has been copied.
  bool ReadData(size_t offset, size_t length, char* dest) {
    DCHECK(dest);
    if (!length)
      return true;
    if (!HasData(offset, length))
      return false;
    DCHECK(offset >= offset_in_file_);
    size_t buffer_offset = offset - offset_in_file_;
    memcpy(dest, buffer_.data() + buffer_offset, length);
    return true;
  }

  // Populate the prefetch buffer from the given file and range.  Returns
  // true if the data is successfully read.
  bool PrefetchFromFile(SimpleFileTracker::FileHandle* file,
                        size_t offset,
                        size_t length) {
    DCHECK(file);
    if (!buffer_.empty()) {
      return false;
    }
    buffer_.resize(length);
    if (file->get()->Read(offset, buffer_.data(), length) !=
        static_cast<int>(length)) {
      buffer_.resize(0);
      return false;
    }
    offset_in_file_ = offset;
    return true;
  }

  // Return how much trailing data has been requested via HasData() or
  // ReadData().  The intent is that this value can be used to tune
  // future prefetching behavior.
  size_t GetDesiredTrailerPrefetchSize() const {
    return file_size_ - earliest_requested_offset_;
  }

 private:
  // Track the earliest offset requested in order to return an optimal trailer
  // prefetch amount in GetDesiredTrailerPrefetchSize().
  void UpdateEarliestOffset(size_t offset) {
    DCHECK_LE(earliest_requested_offset_, file_size_);
    earliest_requested_offset_ = std::min(earliest_requested_offset_, offset);
  }

  const size_t file_size_;

  // Prefer to read the prefetch data into a stack buffer to minimize
  // memory pressure on the OS disk cache.
  absl::InlinedVector<char, 1024> buffer_;
  size_t offset_in_file_ = 0;

  size_t earliest_requested_offset_;
};

class SimpleSynchronousEntry::ScopedFileOperationsBinding final {
 public:
  ScopedFileOperationsBinding(SimpleSynchronousEntry* owner,
                              BackendFileOperations** file_operations)
      : owner_(owner),
        file_operations_(owner->unbound_file_operations_->Bind(
            base::SequencedTaskRunner::GetCurrentDefault())) {
    *file_operations = file_operations_.get();
  }
  ~ScopedFileOperationsBinding() {
    owner_->unbound_file_operations_ = file_operations_->Unbind();
  }

 private:
  const raw_ptr<SimpleSynchronousEntry> owner_;
  std::unique_ptr<BackendFileOperations> file_operations_;
};

using simple_util::GetEntryHashKey;
using simple_util::GetFilenameFromEntryFileKeyAndFileIndex;
using simple_util::GetSparseFilenameFromEntryFileKey;
using simple_util::GetHeaderSize;
using simple_util::GetDataSizeFromFileSize;
using simple_util::GetFileSizeFromDataSize;
using simple_util::GetFileIndexFromStreamIndex;

BASE_FEATURE(kSimpleCachePrefetchExperiment,
             "SimpleCachePrefetchExperiment2",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSimpleCacheFullPrefetchBytesParam[] = "FullPrefetchBytes";
constexpr base::FeatureParam<int> kSimpleCacheFullPrefetchSize{
    &kSimpleCachePrefetchExperiment, kSimpleCacheFullPrefetchBytesParam, 0};

const char kSimpleCacheTrailerPrefetchSpeculativeBytesParam[] =
    "TrailerPrefetchSpeculativeBytes";
constexpr base::FeatureParam<int> kSimpleCacheTrailerPrefetchSpeculativeBytes{
    &kSimpleCachePrefetchExperiment,
    kSimpleCacheTrailerPrefetchSpeculativeBytesParam, 0};

int GetSimpleCacheFullPrefetchSize() {
  return kSimpleCacheFullPrefetchSize.Get();
}

int GetSimpleCacheTrailerPrefetchSize(int hint_size) {
  if (hint_size > 0)
    return hint_size;
  return kSimpleCacheTrailerPrefetchSpeculativeBytes.Get();
}

SimpleEntryStat::SimpleEntryStat(base::Time last_used,
                                 base::Time last_modified,
                                 const int32_t data_size[],
                                 const int32_t sparse_data_size)
    : last_used_(last_used),
      last_modified_(last_modified),
      sparse_data_size_(sparse_data_size) {
  memcpy(data_size_, data_size, sizeof(data_size_));
}

// These size methods all assume the presence of the SHA256 on stream zero,
// since this version of the cache always writes it. In the read case, it may
// not be present and these methods can't be relied upon.

int SimpleEntryStat::GetOffsetInFile(size_t key_length,
                                     int offset,
                                     int stream_index) const {
  const size_t headers_size = sizeof(SimpleFileHeader) + key_length;
  const size_t additional_offset =
      stream_index == 0 ? data_size_[1] + sizeof(SimpleFileEOF) : 0;
  return headers_size + offset + additional_offset;
}

int SimpleEntryStat::GetEOFOffsetInFile(size_t key_length,
                                        int stream_index) const {
  size_t additional_offset;
  if (stream_index != 0)
    additional_offset = 0;
  else
    additional_offset = sizeof(net::SHA256HashValue);
  return additional_offset +
         GetOffsetInFile(key_length, data_size_[stream_index], stream_index);
}

int SimpleEntryStat::GetLastEOFOffsetInFile(size_t key_length,
                                            int stream_index) const {
  if (stream_index == 1)
    return GetEOFOffsetInFile(key_length, 0);
  return GetEOFOffsetInFile(key_length, stream_index);
}

int64_t SimpleEntryStat::GetFileSize(size_t key_length, int file_index) const {
  int32_t total_data_size;
  if (file_index == 0) {
    total_data_size = data_size_[0] + data_size_[1] +
                      sizeof(net::SHA256HashValue) + sizeof(SimpleFileEOF);
  } else {
    total_data_size = data_size_[2];
  }
  return GetFileSizeFromDataSize(key_length, total_data_size);
}

SimpleStreamPrefetchData::SimpleStreamPrefetchData()
    : stream_crc32(crc32(0, Z_NULL, 0)) {}

SimpleStreamPrefetchData::~SimpleStreamPrefetchData() = default;

SimpleEntryCreationResults::SimpleEntryCreationResults(
    SimpleEntryStat entry_stat)
    : sync_entry(nullptr), entry_stat(entry_stat) {}

SimpleEntryCreationResults::~SimpleEntryCreationResults() = default;

SimpleSynchronousEntry::CRCRecord::CRCRecord() : index(-1),
                                                 has_crc32(false),
                                                 data_crc32(0) {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord(int index_p,
                                             bool has_crc32_p,
                                             uint32_t data_crc32_p)
    : index(index_p), has_crc32(has_crc32_p), data_crc32(data_crc32_p) {}

SimpleSynchronousEntry::ReadRequest::ReadRequest(int index_p,
                                                 int offset_p,
                                                 int buf_len_p)
    : index(index_p), offset(offset_p), buf_len(buf_len_p) {}

SimpleSynchronousEntry::WriteRequest::WriteRequest(int index_p,
                                                   int offset_p,
                                                   int buf_len_p,
                                                   uint32_t previous_crc32_p,
                                                   bool truncate_p,
                                                   bool doomed_p,
                                                   bool request_update_crc_p)
    : index(index_p),
      offset(offset_p),
      buf_len(buf_len_p),
      previous_crc32(previous_crc32_p),
      truncate(truncate_p),
      doomed(doomed_p),
      request_update_crc(request_update_crc_p) {}

SimpleSynchronousEntry::SparseRequest::SparseRequest(int64_t sparse_offset_p,
                                                     int buf_len_p)
    : sparse_offset(sparse_offset_p), buf_len(buf_len_p) {}

// static
void SimpleSynchronousEntry::OpenEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const std::optional<std::string>& key,
    const uint64_t entry_hash,
    SimpleFileTracker* file_tracker,
    std::unique_ptr<UnboundBackendFileOperations> file_operations,
    int32_t trailer_prefetch_size,
    SimpleEntryCreationResults* out_results) {
  base::TimeTicks start_sync_open_entry = base::TimeTicks::Now();

  auto sync_entry = std::make_unique<SimpleSynchronousEntry>(
      cache_type, path, key, entry_hash, file_tracker,
      std::move(file_operations), trailer_prefetch_size);
  {
    BackendFileOperations* bound_file_operations = nullptr;
    ScopedFileOperationsBinding binding(sync_entry.get(),
                                        &bound_file_operations);
    out_results->result = sync_entry->InitializeForOpen(
        bound_file_operations, &out_results->entry_stat,
        out_results->stream_prefetch_data);
  }
  if (out_results->result != net::OK) {
    sync_entry->Doom();
    sync_entry->CloseFiles();
    out_results->sync_entry = nullptr;
    out_results->unbound_file_operations =
        std::move(sync_entry->unbound_file_operations_);
    out_results->stream_prefetch_data[0].data = nullptr;
    out_results->stream_prefetch_data[1].data = nullptr;
    return;
  }
  SIMPLE_CACHE_UMA(TIMES, "DiskOpenLatency", cache_type,
                   base::TimeTicks::Now() - start_sync_open_entry);
  out_results->sync_entry = sync_entry.release();
  out_results->computed_trailer_prefetch_size =
      out_results->sync_entry->computed_trailer_prefetch_size();
}

// static
void SimpleSynchronousEntry::CreateEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const std::string& key,
    const uint64_t entry_hash,
    SimpleFileTracker* file_tracker,
    std::unique_ptr<UnboundBackendFileOperations> file_operations,
    SimpleEntryCreationResults* out_results) {
  DCHECK_EQ(entry_hash, GetEntryHashKey(key));
  base::TimeTicks start_sync_create_entry = base::TimeTicks::Now();

  auto sync_entry = std::make_unique<SimpleSynchronousEntry>(
      cache_type, path, key, entry_hash, file_tracker,
      std::move(file_operations), -1);
  {
    BackendFileOperations* bound_file_operations = nullptr;
    ScopedFileOperationsBinding binding(sync_entry.get(),
                                        &bound_file_operations);
    out_results->result = sync_entry->InitializeForCreate(
        bound_file_operations, &out_results->entry_stat);
  }
  if (out_results->result != net::OK) {
    if (out_results->result != net::ERR_FILE_EXISTS)
      sync_entry->Doom();
    sync_entry->CloseFiles();
    out_results->unbound_file_operations =
        std::move(sync_entry->unbound_file_operations_);
    out_results->sync_entry = nullptr;
    return;
  }
  out_results->sync_entry = sync_entry.release();
  out_results->created = true;
  RecordDiskCreateLatency(cache_type,
                          base::TimeTicks::Now() - start_sync_create_entry);
}

// static
void SimpleSynchronousEntry::OpenOrCreateEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const std::string& key,
    const uint64_t entry_hash,
    OpenEntryIndexEnum index_state,
    bool optimistic_create,
    SimpleFileTracker* file_tracker,
    std::unique_ptr<UnboundBackendFileOperations> file_operations,
    int32_t trailer_prefetch_size,
    SimpleEntryCreationResults* out_results) {
  base::TimeTicks start = base::TimeTicks::Now();
  if (index_state == INDEX_MISS) {
    // Try to just create.
    auto sync_entry = std::make_unique<SimpleSynchronousEntry>(
        cache_type, path, key, entry_hash, file_tracker,
        std::move(file_operations), trailer_prefetch_size);
    {
      BackendFileOperations* bound_file_operations = nullptr;
      ScopedFileOperationsBinding binding(sync_entry.get(),
                                          &bound_file_operations);
      out_results->result = sync_entry->InitializeForCreate(
          bound_file_operations, &out_results->entry_stat);
    }
    switch (out_results->result) {
      case net::OK:
        out_results->sync_entry = sync_entry.release();
        out_results->created = true;
        RecordDiskCreateLatency(cache_type, base::TimeTicks::Now() - start);
        return;
      case net::ERR_FILE_EXISTS:
        // Our index was messed up.
        if (optimistic_create) {
          // In this case, ::OpenOrCreateEntry already returned claiming it made
          // a new entry. Try extra-hard to make that the actual case.
          sync_entry->Doom();
          sync_entry->CloseFiles();
          file_operations = std::move(sync_entry->unbound_file_operations_);
          sync_entry = nullptr;
          CreateEntry(cache_type, path, key, entry_hash, file_tracker,
                      std::move(file_operations), out_results);
          return;
        }
        // Otherwise can just try opening.
        break;
      default:
        // Trouble. Fail this time.
        sync_entry->Doom();
        sync_entry->CloseFiles();
        out_results->unbound_file_operations =
            std::move(sync_entry->unbound_file_operations_);
        return;
    }
    file_operations = std::move(sync_entry->unbound_file_operations_);
  }

  DCHECK(file_operations);
  // Try open, then if that fails create.
  OpenEntry(cache_type, path, key, entry_hash, file_tracker,
            std::move(file_operations), trailer_prefetch_size, out_results);
  if (out_results->sync_entry)
    return;
  file_operations = std::move(out_results->unbound_file_operations);
  DCHECK(file_operations);
  CreateEntry(cache_type, path, key, entry_hash, file_tracker,
              std::move(file_operations), out_results);
}

// static
int SimpleSynchronousEntry::DeleteEntryFiles(
    const FilePath& path,
    net::CacheType cache_type,
    uint64_t entry_hash,
    std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations) {
  auto file_operations = unbound_file_operations->Bind(
      base::SequencedTaskRunner::GetCurrentDefault());
  return DeleteEntryFilesInternal(path, cache_type, entry_hash,
                                  file_operations.get());
}

// static
int SimpleSynchronousEntry::DeleteEntryFilesInternal(
    const FilePath& path,
    net::CacheType cache_type,
    uint64_t entry_hash,
    BackendFileOperations* file_operations) {
  base::TimeTicks start = base::TimeTicks::Now();
  const bool deleted_well =
      DeleteFilesForEntryHash(path, entry_hash, file_operations);
  SIMPLE_CACHE_UMA(TIMES, "DiskDoomLatency", cache_type,
                   base::TimeTicks::Now() - start);
  return deleted_well ? net::OK : net::ERR_FAILED;
}

int SimpleSynchronousEntry::Doom() {
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  return DoomInternal(file_operations);
}

int SimpleSynchronousEntry::DoomInternal(
    BackendFileOperations* file_operations) {
  if (entry_file_key_.doom_generation != 0u) {
    // Already doomed.
    return true;
  }

  if (have_open_files_) {
    base::TimeTicks start = base::TimeTicks::Now();
    bool ok = true;
    SimpleFileTracker::EntryFileKey orig_key = entry_file_key_;
    file_tracker_->Doom(this, &entry_file_key_);

    for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
      if (!empty_file_omitted_[i]) {
        base::File::Error out_error;
        FilePath old_name = path_.AppendASCII(
            GetFilenameFromEntryFileKeyAndFileIndex(orig_key, i));
        FilePath new_name = path_.AppendASCII(
            GetFilenameFromEntryFileKeyAndFileIndex(entry_file_key_, i));
        ok = file_operations->ReplaceFile(old_name, new_name, &out_error) && ok;
      }
    }

    if (sparse_file_open()) {
      base::File::Error out_error;
      FilePath old_name =
          path_.AppendASCII(GetSparseFilenameFromEntryFileKey(orig_key));
      FilePath new_name =
          path_.AppendASCII(GetSparseFilenameFromEntryFileKey(entry_file_key_));
      ok = file_operations->ReplaceFile(old_name, new_name, &out_error) && ok;
    }

    SIMPLE_CACHE_UMA(TIMES, "DiskDoomLatency", cache_type_,
                     base::TimeTicks::Now() - start);

    return ok ? net::OK : net::ERR_FAILED;
  } else {
    // No one has ever called Create or Open on us, so we don't have to worry
    // about being accessible to other ops after doom.
    return DeleteEntryFilesInternal(
        path_, cache_type_, entry_file_key_.entry_hash, file_operations);
  }
}

// static
int SimpleSynchronousEntry::TruncateEntryFiles(
    const base::FilePath& path,
    uint64_t entry_hash,
    std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations) {
  auto file_operations = unbound_file_operations->Bind(
      base::SequencedTaskRunner::GetCurrentDefault());
  const bool deleted_well =
      TruncateFilesForEntryHash(path, entry_hash, file_operations.get());
  return deleted_well ? net::OK : net::ERR_FAILED;
}

// static
int SimpleSynchronousEntry::DeleteEntrySetFiles(
    const std::vector<uint64_t>* key_hashes,
    const FilePath& path,
    std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations) {
  auto file_operations = unbound_file_operations->Bind(
      base::SequencedTaskRunner::GetCurrentDefault());
  const size_t did_delete_count = base::ranges::count_if(
      *key_hashes, [&path, &file_operations](const uint64_t& key_hash) {
        return SimpleSynchronousEntry::DeleteFilesForEntryHash(
            path, key_hash, file_operations.get());
      });
  return (did_delete_count == key_hashes->size()) ? net::OK : net::ERR_FAILED;
}

void SimpleSynchronousEntry::ReadData(const ReadRequest& in_entry_op,
                                      SimpleEntryStat* entry_stat,
                                      net::IOBuffer* out_buf,
                                      ReadResult* out_result) {
  DCHECK(initialized_);
  DCHECK_NE(0, in_entry_op.index);
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  int file_index = GetFileIndexFromStreamIndex(in_entry_op.index);
  SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
      file_operations, this, SubFileForFileIndex(file_index));

  out_result->crc_updated = false;
  if (!file.IsOK() || (header_and_key_check_needed_[file_index] &&
                       !CheckHeaderAndKey(file.get(), file_index))) {
    out_result->result = net::ERR_FAILED;
    DoomInternal(file_operations);
    return;
  }
  const int64_t file_offset = entry_stat->GetOffsetInFile(
      key_->size(), in_entry_op.offset, in_entry_op.index);
  // Zero-length reads and reads to the empty streams of omitted files should
  // be handled in the SimpleEntryImpl.
  DCHECK_GT(in_entry_op.buf_len, 0);
  DCHECK(!empty_file_omitted_[file_index]);
  int bytes_read =
      file->Read(file_offset, out_buf->data(), in_entry_op.buf_len);
  if (bytes_read > 0) {
    entry_stat->set_last_used(Time::Now());
    if (in_entry_op.request_update_crc) {
      out_result->updated_crc32 = simple_util::IncrementalCrc32(
          in_entry_op.previous_crc32, out_buf->data(), bytes_read);
      out_result->crc_updated = true;
      // Verify checksum after last read, if we've been asked to.
      if (in_entry_op.request_verify_crc &&
          in_entry_op.offset + bytes_read ==
              entry_stat->data_size(in_entry_op.index)) {
        int checksum_result =
            CheckEOFRecord(file_operations, file.get(), in_entry_op.index,
                           *entry_stat, out_result->updated_crc32);
        if (checksum_result < 0) {
          out_result->result = checksum_result;
          return;
        }
      }
    }
  }
  if (bytes_read >= 0) {
    out_result->result = bytes_read;
  } else {
    out_result->result = net::ERR_CACHE_READ_FAILURE;
    DoomInternal(file_operations);
  }
}

void SimpleSynchronousEntry::WriteData(const WriteRequest& in_entry_op,
                                       net::IOBuffer* in_buf,
                                       SimpleEntryStat* out_entry_stat,
                                       WriteResult* out_write_result) {
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  base::ElapsedTimer write_time;
  DCHECK(initialized_);
  DCHECK_NE(0, in_entry_op.index);
  int index = in_entry_op.index;
  int file_index = GetFileIndexFromStreamIndex(index);
  if (header_and_key_check_needed_[file_index] &&
      !empty_file_omitted_[file_index]) {
    SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
        file_operations, this, SubFileForFileIndex(file_index));
    if (!file.IsOK() || !CheckHeaderAndKey(file.get(), file_index)) {
      out_write_result->result = net::ERR_FAILED;
      DoomInternal(file_operations);
      return;
    }
  }
  int offset = in_entry_op.offset;
  int buf_len = in_entry_op.buf_len;
  bool truncate = in_entry_op.truncate;
  bool doomed = in_entry_op.doomed;
  size_t key_size = key_->size();
  const int64_t file_offset = out_entry_stat->GetOffsetInFile(
      key_size, in_entry_op.offset, in_entry_op.index);
  bool extending_by_write = offset + buf_len > out_entry_stat->data_size(index);

  if (empty_file_omitted_[file_index]) {
    // Don't create a new file if the entry has been doomed, to avoid it being
    // mixed up with a newly-created entry with the same key.
    if (doomed) {
      DLOG(WARNING) << "Rejecting write to lazily omitted stream "
                    << in_entry_op.index << " of doomed cache entry.";
      RecordWriteResult(cache_type_,
                        SYNC_WRITE_RESULT_LAZY_STREAM_ENTRY_DOOMED);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    base::File::Error error;
    if (!MaybeCreateFile(file_operations, file_index, FILE_REQUIRED, &error)) {
      RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_LAZY_CREATE_FAILURE);
      DoomInternal(file_operations);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    if (!InitializeCreatedFile(file_operations, file_index)) {
      RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_LAZY_INITIALIZE_FAILURE);
      DoomInternal(file_operations);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  DCHECK(!empty_file_omitted_[file_index]);

  // This needs to be grabbed after the above block, since that's what may
  // create the file (for stream 2/file 1).
  SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
      file_operations, this, SubFileForFileIndex(file_index));
  if (!file.IsOK()) {
    out_write_result->result = net::ERR_FAILED;
    DoomInternal(file_operations);
    return;
  }

  if (extending_by_write) {
    // The EOF record and the eventual stream afterward need to be zeroed out.
    const int64_t file_eof_offset =
        out_entry_stat->GetEOFOffsetInFile(key_size, index);
    if (!file->SetLength(file_eof_offset)) {
      RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_PRETRUNCATE_FAILURE);
      DoomInternal(file_operations);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  if (buf_len > 0) {
    if (file->Write(file_offset, in_buf->data(), buf_len) != buf_len) {
      RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_WRITE_FAILURE);
      DoomInternal(file_operations);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  if (!truncate && (buf_len > 0 || !extending_by_write)) {
    out_entry_stat->set_data_size(
        index, std::max(out_entry_stat->data_size(index), offset + buf_len));
  } else {
    out_entry_stat->set_data_size(index, offset + buf_len);
    int file_eof_offset =
        out_entry_stat->GetLastEOFOffsetInFile(key_size, index);
    if (!file->SetLength(file_eof_offset)) {
      RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_TRUNCATE_FAILURE);
      DoomInternal(file_operations);
      out_write_result->result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }

  if (in_entry_op.request_update_crc && buf_len > 0) {
    out_write_result->updated_crc32 = simple_util::IncrementalCrc32(
        in_entry_op.previous_crc32, in_buf->data(), buf_len);
    out_write_result->crc_updated = true;
  }

  SIMPLE_CACHE_UMA(TIMES, "DiskWriteLatency", cache_type_,
                   write_time.Elapsed());
  RecordWriteResult(cache_type_, SYNC_WRITE_RESULT_SUCCESS);
  base::Time modification_time = Time::Now();
  out_entry_stat->set_last_used(modification_time);
  out_entry_stat->set_last_modified(modification_time);
  out_write_result->result = buf_len;
}

void SimpleSynchronousEntry::ReadSparseData(const SparseRequest& in_entry_op,
                                            net::IOBuffer* out_buf,
                                            base::Time* out_last_used,
                                            int* out_result) {
  DCHECK(initialized_);
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  int64_t offset = in_entry_op.sparse_offset;
  int buf_len = in_entry_op.buf_len;

  char* buf = out_buf->data();
  int read_so_far = 0;

  if (!sparse_file_open()) {
    *out_result = 0;
    return;
  }

  SimpleFileTracker::FileHandle sparse_file = file_tracker_->Acquire(
      file_operations, this, SimpleFileTracker::SubFile::FILE_SPARSE);
  if (!sparse_file.IsOK()) {
    DoomInternal(file_operations);
    *out_result = net::ERR_CACHE_READ_FAILURE;
    return;
  }

  // Find the first sparse range at or after the requested offset.
  auto it = sparse_ranges_.lower_bound(offset);

  if (it != sparse_ranges_.begin()) {
    // Hop back one range and read the one overlapping with the start.
    --it;
    SparseRange* found_range = &it->second;
    DCHECK_EQ(it->first, found_range->offset);
    if (found_range->offset + found_range->length > offset) {
      DCHECK_GE(found_range->length, 0);
      DCHECK_LE(found_range->length, std::numeric_limits<int32_t>::max());
      DCHECK_GE(offset - found_range->offset, 0);
      DCHECK_LE(offset - found_range->offset,
                std::numeric_limits<int32_t>::max());
      int net_offset = static_cast<int>(offset - found_range->offset);
      int range_len_after_offset =
          static_cast<int>(found_range->length - net_offset);
      DCHECK_GE(range_len_after_offset, 0);

      int len_to_read = std::min(buf_len, range_len_after_offset);
      if (!ReadSparseRange(sparse_file.get(), found_range, net_offset,
                           len_to_read, buf)) {
        DoomInternal(file_operations);
        *out_result = net::ERR_CACHE_READ_FAILURE;
        return;
      }
      read_so_far += len_to_read;
    }
    ++it;
  }

  // Keep reading until the buffer is full or there is not another contiguous
  // range.
  while (read_so_far < buf_len &&
         it != sparse_ranges_.end() &&
         it->second.offset == offset + read_so_far) {
    SparseRange* found_range = &it->second;
    DCHECK_EQ(it->first, found_range->offset);
    int range_len = base::saturated_cast<int>(found_range->length);
    int len_to_read = std::min(buf_len - read_so_far, range_len);
    if (!ReadSparseRange(sparse_file.get(), found_range, 0, len_to_read,
                         buf + read_so_far)) {
      DoomInternal(file_operations);
      *out_result = net::ERR_CACHE_READ_FAILURE;
      return;
    }
    read_so_far += len_to_read;
    ++it;
  }

  *out_result = read_so_far;
}

void SimpleSynchronousEntry::WriteSparseData(const SparseRequest& in_entry_op,
                                             net::IOBuffer* in_buf,
                                             uint64_t max_sparse_data_size,
                                             SimpleEntryStat* out_entry_stat,
                                             int* out_result) {
  DCHECK(initialized_);
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  int64_t offset = in_entry_op.sparse_offset;
  int buf_len = in_entry_op.buf_len;

  const char* buf = in_buf->data();
  int written_so_far = 0;
  int appended_so_far = 0;

  if (!sparse_file_open() && !CreateSparseFile(file_operations)) {
    DoomInternal(file_operations);
    *out_result = net::ERR_CACHE_WRITE_FAILURE;
    return;
  }
  SimpleFileTracker::FileHandle sparse_file = file_tracker_->Acquire(
      file_operations, this, SimpleFileTracker::SubFile::FILE_SPARSE);
  if (!sparse_file.IsOK()) {
    DoomInternal(file_operations);
    *out_result = net::ERR_CACHE_WRITE_FAILURE;
    return;
  }

  int32_t sparse_data_size = out_entry_stat->sparse_data_size();
  int32_t future_sparse_data_size;
  if (!base::CheckAdd(sparse_data_size, buf_len)
           .AssignIfValid(&future_sparse_data_size) ||
      future_sparse_data_size < 0) {
    DoomInternal(file_operations);
    *out_result = net::ERR_CACHE_WRITE_FAILURE;
    return;
  }
  // This is a pessimistic estimate; it assumes the entire buffer is going to
  // be appended as a new range, not written over existing ranges.
  if (static_cast<uint64_t>(future_sparse_data_size) > max_sparse_data_size) {
    DVLOG(1) << "Truncating sparse data file (" << sparse_data_size << " + "
             << buf_len << " > " << max_sparse_data_size << ")";
    TruncateSparseFile(sparse_file.get());
    out_entry_stat->set_sparse_data_size(0);
  }

  auto it = sparse_ranges_.lower_bound(offset);

  if (it != sparse_ranges_.begin()) {
    --it;
    SparseRange* found_range = &it->second;
    if (found_range->offset + found_range->length > offset) {
      DCHECK_GE(found_range->length, 0);
      DCHECK_LE(found_range->length, std::numeric_limits<int32_t>::max());
      DCHECK_GE(offset - found_range->offset, 0);
      DCHECK_LE(offset - found_range->offset,
                std::numeric_limits<int32_t>::max());
      int net_offset = static_cast<int>(offset - found_range->offset);
      int range_len_after_offset =
          static_cast<int>(found_range->length - net_offset);
      DCHECK_GE(range_len_after_offset, 0);

      int len_to_write = std::min(buf_len, range_len_after_offset);
      if (!WriteSparseRange(sparse_file.get(), found_range, net_offset,
                            len_to_write, buf)) {
        DoomInternal(file_operations);
        *out_result = net::ERR_CACHE_WRITE_FAILURE;
        return;
      }
      written_so_far += len_to_write;
    }
    ++it;
  }

  while (written_so_far < buf_len &&
         it != sparse_ranges_.end() &&
         it->second.offset < offset + buf_len) {
    SparseRange* found_range = &it->second;
    if (offset + written_so_far < found_range->offset) {
      int len_to_append =
          static_cast<int>(found_range->offset - (offset + written_so_far));
      if (!AppendSparseRange(sparse_file.get(), offset + written_so_far,
                             len_to_append, buf + written_so_far)) {
        DoomInternal(file_operations);
        *out_result = net::ERR_CACHE_WRITE_FAILURE;
        return;
      }
      written_so_far += len_to_append;
      appended_so_far += len_to_append;
    }
    int range_len = base::saturated_cast<int>(found_range->length);
    int len_to_write = std::min(buf_len - written_so_far, range_len);
    if (!WriteSparseRange(sparse_file.get(), found_range, 0, len_to_write,
                          buf + written_so_far)) {
      DoomInternal(file_operations);
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    written_so_far += len_to_write;
    ++it;
  }

  if (written_so_far < buf_len) {
    int len_to_append = buf_len - written_so_far;
    if (!AppendSparseRange(sparse_file.get(), offset + written_so_far,
                           len_to_append, buf + written_so_far)) {
      DoomInternal(file_operations);
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    written_so_far += len_to_append;
    appended_so_far += len_to_append;
  }

  DCHECK_EQ(buf_len, written_so_far);

  base::Time modification_time = Time::Now();
  out_entry_stat->set_last_used(modification_time);
  out_entry_stat->set_last_modified(modification_time);
  int32_t old_sparse_data_size = out_entry_stat->sparse_data_size();
  out_entry_stat->set_sparse_data_size(old_sparse_data_size + appended_so_far);
  *out_result = written_so_far;
}

void SimpleSynchronousEntry::GetAvailableRange(const SparseRequest& in_entry_op,
                                               RangeResult* out_result) {
  DCHECK(initialized_);
  int64_t offset = in_entry_op.sparse_offset;
  int len = in_entry_op.buf_len;

  auto it = sparse_ranges_.lower_bound(offset);

  int64_t start = offset;
  int64_t avail_so_far = 0;

  if (it != sparse_ranges_.end() && it->second.offset < offset + len)
    start = it->second.offset;

  if ((it == sparse_ranges_.end() || it->second.offset > offset) &&
      it != sparse_ranges_.begin()) {
    --it;
    if (it->second.offset + it->second.length > offset) {
      start = offset;
      avail_so_far = (it->second.offset + it->second.length) - offset;
    }
    ++it;
  }

  while (start + avail_so_far < offset + len &&
         it != sparse_ranges_.end() &&
         it->second.offset == start + avail_so_far) {
    avail_so_far += it->second.length;
    ++it;
  }

  int64_t len_from_start = len - (start - offset);
  *out_result = RangeResult(
      start, static_cast<int>(std::min(avail_so_far, len_from_start)));
}

int SimpleSynchronousEntry::CheckEOFRecord(
    BackendFileOperations* file_operations,
    base::File* file,
    int stream_index,
    const SimpleEntryStat& entry_stat,
    uint32_t expected_crc32) {
  DCHECK(initialized_);
  SimpleFileEOF eof_record;
  int file_offset = entry_stat.GetEOFOffsetInFile(key_->size(), stream_index);
  int file_index = GetFileIndexFromStreamIndex(stream_index);
  int rv =
      GetEOFRecordData(file, nullptr, file_index, file_offset, &eof_record);

  if (rv != net::OK) {
    DoomInternal(file_operations);
    return rv;
  }
  if ((eof_record.flags & SimpleFileEOF::FLAG_HAS_CRC32) &&
      eof_record.data_crc32 != expected_crc32) {
    DVLOG(1) << "EOF record had bad crc.";
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_CRC_MISMATCH);
    DoomInternal(file_operations);
    return net::ERR_CACHE_CHECKSUM_MISMATCH;
  }
  RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_SUCCESS);
  return net::OK;
}

int SimpleSynchronousEntry::PreReadStreamPayload(
    base::File* file,
    PrefetchData* prefetch_data,
    int stream_index,
    int extra_size,
    const SimpleEntryStat& entry_stat,
    const SimpleFileEOF& eof_record,
    SimpleStreamPrefetchData* out) {
  DCHECK(stream_index == 0 || stream_index == 1);

  int stream_size = entry_stat.data_size(stream_index);
  int read_size = stream_size + extra_size;
  out->data = base::MakeRefCounted<net::GrowableIOBuffer>();
  out->data->SetCapacity(read_size);
  int file_offset = entry_stat.GetOffsetInFile(key_->size(), 0, stream_index);
  if (!ReadFromFileOrPrefetched(file, prefetch_data, 0, file_offset, read_size,
                                out->data->data()))
    return net::ERR_FAILED;

  // Check the CRC32.
  uint32_t expected_crc32 = simple_util::Crc32(out->data->data(), stream_size);
  if ((eof_record.flags & SimpleFileEOF::FLAG_HAS_CRC32) &&
      eof_record.data_crc32 != expected_crc32) {
    DVLOG(1) << "EOF record had bad crc.";
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_CRC_MISMATCH);
    return net::ERR_CACHE_CHECKSUM_MISMATCH;
  }
  out->stream_crc32 = expected_crc32;
  RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_SUCCESS);
  return net::OK;
}

void SimpleSynchronousEntry::Close(
    const SimpleEntryStat& entry_stat,
    std::unique_ptr<std::vector<CRCRecord>> crc32s_to_write,
    net::GrowableIOBuffer* stream_0_data,
    SimpleEntryCloseResults* out_results) {
  // As we delete `this`, we cannot use ScopedFileOperationsBinding here.
  std::unique_ptr<BackendFileOperations> file_operations =
      unbound_file_operations_->Bind(
          base::SequencedTaskRunner::GetCurrentDefault());
  unbound_file_operations_ = nullptr;
  base::ElapsedTimer close_time;
  DCHECK(stream_0_data);
  const std::string& key = *key_;

  for (auto& crc_record : *crc32s_to_write) {
    const int stream_index = crc_record.index;
    const int file_index = GetFileIndexFromStreamIndex(stream_index);
    if (empty_file_omitted_[file_index])
      continue;

    SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
        file_operations.get(), this, SubFileForFileIndex(file_index));
    if (!file.IsOK()) {
      RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
      DoomInternal(file_operations.get());
      break;
    }

    if (stream_index == 0) {
      // Write stream 0 data.
      int stream_0_offset = entry_stat.GetOffsetInFile(key.size(), 0, 0);
      if (file->Write(stream_0_offset, stream_0_data->data(),
                      entry_stat.data_size(0)) != entry_stat.data_size(0)) {
        RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
        DVLOG(1) << "Could not write stream 0 data.";
        DoomInternal(file_operations.get());
      }
      net::SHA256HashValue hash_value;
      CalculateSHA256OfKey(key, &hash_value);
      if (file->Write(stream_0_offset + entry_stat.data_size(0),
                      reinterpret_cast<char*>(hash_value.data),
                      sizeof(hash_value)) != sizeof(hash_value)) {
        RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
        DVLOG(1) << "Could not write stream 0 data.";
        DoomInternal(file_operations.get());
      }

      // Re-compute stream 0 CRC if the data got changed (we may be here even
      // if it didn't change if stream 0's position on disk got changed due to
      // stream 1 write).
      if (!crc_record.has_crc32) {
        crc_record.data_crc32 =
            simple_util::Crc32(stream_0_data->data(), entry_stat.data_size(0));
        crc_record.has_crc32 = true;
      }

      out_results->estimated_trailer_prefetch_size =
          entry_stat.data_size(0) + sizeof(hash_value) + sizeof(SimpleFileEOF);
    }

    SimpleFileEOF eof_record;
    eof_record.stream_size = entry_stat.data_size(stream_index);
    eof_record.final_magic_number = kSimpleFinalMagicNumber;
    eof_record.flags = 0;
    if (crc_record.has_crc32)
      eof_record.flags |= SimpleFileEOF::FLAG_HAS_CRC32;
    if (stream_index == 0)
      eof_record.flags |= SimpleFileEOF::FLAG_HAS_KEY_SHA256;
    eof_record.data_crc32 = crc_record.data_crc32;
    int eof_offset = entry_stat.GetEOFOffsetInFile(key.size(), stream_index);
    // If stream 0 changed size, the file needs to be resized, otherwise the
    // next open will yield wrong stream sizes. On stream 1 and stream 2 proper
    // resizing of the file is handled in SimpleSynchronousEntry::WriteData().
    if (stream_index == 0 && !file->SetLength(eof_offset)) {
      RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
      DVLOG(1) << "Could not truncate stream 0 file.";
      DoomInternal(file_operations.get());
      break;
    }
    if (file->Write(eof_offset, reinterpret_cast<const char*>(&eof_record),
                    sizeof(eof_record)) != sizeof(eof_record)) {
      RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
      DVLOG(1) << "Could not write eof record.";
      DoomInternal(file_operations.get());
      break;
    }
  }
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    if (header_and_key_check_needed_[i]) {
      SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
          file_operations.get(), this, SubFileForFileIndex(i));
      if (!file.IsOK() || !CheckHeaderAndKey(file.get(), i))
        DoomInternal(file_operations.get());
    }
    CloseFile(file_operations.get(), i);
  }

  if (sparse_file_open()) {
    CloseSparseFile(file_operations.get());
  }

  SIMPLE_CACHE_UMA(TIMES, "DiskCloseLatency", cache_type_,
                   close_time.Elapsed());
  RecordCloseResult(cache_type_, CLOSE_RESULT_SUCCESS);
  have_open_files_ = false;
  delete this;
}

SimpleSynchronousEntry::SimpleSynchronousEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const std::optional<std::string>& key,
    const uint64_t entry_hash,
    SimpleFileTracker* file_tracker,
    std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations,
    int32_t trailer_prefetch_size)
    : cache_type_(cache_type),
      path_(path),
      entry_file_key_(entry_hash),
      key_(key),
      file_tracker_(file_tracker),
      unbound_file_operations_(std::move(unbound_file_operations)),
      trailer_prefetch_size_(trailer_prefetch_size) {
  for (bool& empty_file_omitted : empty_file_omitted_) {
    empty_file_omitted = false;
  }
}

SimpleSynchronousEntry::~SimpleSynchronousEntry() {
  DCHECK(!have_open_files_);
}

bool SimpleSynchronousEntry::MaybeOpenFile(
    BackendFileOperations* file_operations,
    int file_index,
    base::File::Error* out_error) {
  DCHECK(out_error);

  FilePath filename = GetFilenameFromFileIndex(file_index);
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  auto file = std::make_unique<base::File>();
  *file = file_operations->OpenFile(filename, flags);
  *out_error = file->error_details();

  if (CanOmitEmptyFile(file_index) && !file->IsValid() &&
      *out_error == base::File::FILE_ERROR_NOT_FOUND) {
    empty_file_omitted_[file_index] = true;
    return true;
  }

  if (file->IsValid()) {
    file_tracker_->Register(this, SubFileForFileIndex(file_index),
                            std::move(file));
    return true;
  }
  return false;
}

bool SimpleSynchronousEntry::MaybeCreateFile(
    BackendFileOperations* file_operations,
    int file_index,
    FileRequired file_required,
    base::File::Error* out_error) {
  DCHECK(out_error);

  if (CanOmitEmptyFile(file_index) && file_required == FILE_NOT_REQUIRED) {
    empty_file_omitted_[file_index] = true;
    return true;
  }

  FilePath filename = GetFilenameFromFileIndex(file_index);
  int flags = base::File::FLAG_CREATE | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  auto file =
      std::make_unique<base::File>(file_operations->OpenFile(filename, flags));

  // It's possible that the creation failed because someone deleted the
  // directory (e.g. because someone pressed "clear cache" on Android).
  // If so, we would keep failing for a while until periodic index snapshot
  // re-creates the cache dir, so try to recover from it quickly here.
  //
  // This previously also checked whether the directory was missing, but that
  // races against other entry creations attempting the same recovery.
  if (!file->IsValid() &&
      file->error_details() == base::File::FILE_ERROR_NOT_FOUND) {
    file_operations->CreateDirectory(path_);
    *file = file_operations->OpenFile(filename, flags);
  }

  *out_error = file->error_details();
  if (file->IsValid()) {
    file_tracker_->Register(this, SubFileForFileIndex(file_index),
                            std::move(file));
    empty_file_omitted_[file_index] = false;
    return true;
  }
  return false;
}

bool SimpleSynchronousEntry::OpenFiles(BackendFileOperations* file_operations,
                                       SimpleEntryStat* out_entry_stat) {
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    base::File::Error error;

    if (!MaybeOpenFile(file_operations, i, &error)) {
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_PLATFORM_FILE_ERROR);
      SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncOpenPlatformFileError", cache_type_,
                         -error, -base::File::FILE_ERROR_MAX);
      while (--i >= 0)
        CloseFile(file_operations, i);
      return false;
    }
  }

  have_open_files_ = true;

  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    if (empty_file_omitted_[i]) {
      out_entry_stat->set_data_size(i + 1, 0);
      continue;
    }

    base::File::Info file_info;
    SimpleFileTracker::FileHandle file =
        file_tracker_->Acquire(file_operations, this, SubFileForFileIndex(i));
    bool success = file.IsOK() && file->GetInfo(&file_info);
    if (!success) {
      DLOG(WARNING) << "Could not get platform file info.";
      continue;
    }
    out_entry_stat->set_last_used(file_info.last_accessed);
    out_entry_stat->set_last_modified(file_info.last_modified);

    // Two things prevent from knowing the right values for |data_size|:
    // 1) The key might not be known, hence its length might be unknown.
    // 2) Stream 0 and stream 1 are in the same file, and the exact size for
    // each will only be known when reading the EOF record for stream 0.
    //
    // The size for file 0 and 1 is temporarily kept in
    // |data_size(1)| and |data_size(2)| respectively. Reading the key in
    // InitializeForOpen yields the data size for each file. In the case of
    // file hash_1, this is the total size of stream 2, and is assigned to
    // data_size(2). In the case of file 0, it is the combined size of stream
    // 0, stream 1 and one EOF record. The exact distribution of sizes between
    // stream 1 and stream 0 is only determined after reading the EOF record
    // for stream 0 in ReadAndValidateStream0AndMaybe1.
    if (!base::IsValueInRangeForNumericType<int>(file_info.size)) {
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_INVALID_FILE_LENGTH);
      return false;
    }
    out_entry_stat->set_data_size(i + 1, static_cast<int>(file_info.size));
  }

  return true;
}

bool SimpleSynchronousEntry::CreateFiles(BackendFileOperations* file_operations,
                                         SimpleEntryStat* out_entry_stat) {
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    base::File::Error error;
    if (!MaybeCreateFile(file_operations, i, FILE_NOT_REQUIRED, &error)) {
      SIMPLE_CACHE_LOCAL(ENUMERATION, "SyncCreatePlatformFileError",
                         cache_type_, -error, -base::File::FILE_ERROR_MAX);
      while (--i >= 0)
        CloseFile(file_operations, i);
      return false;
    }
  }

  have_open_files_ = true;

  base::Time creation_time = Time::Now();
  out_entry_stat->set_last_modified(creation_time);
  out_entry_stat->set_last_used(creation_time);
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i)
    out_entry_stat->set_data_size(i, 0);

  return true;
}

void SimpleSynchronousEntry::CloseFile(BackendFileOperations* file_operations,
                                       int index) {
  if (empty_file_omitted_[index]) {
    empty_file_omitted_[index] = false;
  } else {
    // We want to delete files that were renamed for doom here; and we should do
    // this before calling SimpleFileTracker::Close, since that would make the
    // name available to other threads.
    if (entry_file_key_.doom_generation != 0u) {
      file_operations->DeleteFile(path_.AppendASCII(
          GetFilenameFromEntryFileKeyAndFileIndex(entry_file_key_, index)));
    }
    file_tracker_->Close(this, SubFileForFileIndex(index));
  }
}

void SimpleSynchronousEntry::CloseFiles() {
  if (!have_open_files_) {
    return;
  }
  BackendFileOperations* file_operations = nullptr;
  ScopedFileOperationsBinding binding(this, &file_operations);
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i)
    CloseFile(file_operations, i);
  if (sparse_file_open())
    CloseSparseFile(file_operations);
  have_open_files_ = false;
}

bool SimpleSynchronousEntry::CheckHeaderAndKey(base::File* file,
                                               int file_index) {
  std::vector<char> header_data(
      !key_.has_value() ? kInitialHeaderRead : GetHeaderSize(key_->size()));
  int bytes_read = file->Read(0, header_data.data(), header_data.size());
  const SimpleFileHeader* header =
      reinterpret_cast<const SimpleFileHeader*>(header_data.data());

  if (bytes_read == -1 || static_cast<size_t>(bytes_read) < sizeof(*header)) {
    RecordSyncOpenResult(cache_type_, OPEN_ENTRY_CANT_READ_HEADER);
    return false;
  }
  // This resize will not invalidate iterators since it does not enlarge the
  // header_data.
  DCHECK_LE(static_cast<size_t>(bytes_read), header_data.size());
  header_data.resize(bytes_read);

  if (header->initial_magic_number != kSimpleInitialMagicNumber) {
    RecordSyncOpenResult(cache_type_, OPEN_ENTRY_BAD_MAGIC_NUMBER);
    return false;
  }

  if (header->version != kSimpleEntryVersionOnDisk) {
    RecordSyncOpenResult(cache_type_, OPEN_ENTRY_BAD_VERSION);
    return false;
  }

  size_t expected_header_size = GetHeaderSize(header->key_length);
  if (header_data.size() < expected_header_size) {
    size_t old_size = header_data.size();
    int bytes_to_read = expected_header_size - old_size;
    // This resize will invalidate iterators, since it is enlarging header_data.
    header_data.resize(expected_header_size);
    int read_result =
        file->Read(old_size, header_data.data() + old_size, bytes_to_read);
    if (read_result != bytes_to_read) {
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_CANT_READ_KEY);
      return false;
    }
    header = reinterpret_cast<const SimpleFileHeader*>(header_data.data());
  }

  const char* key_data = header_data.data() + sizeof(*header);
  base::span<const char> key_span =
      base::make_span(key_data, header->key_length);
  if (base::PersistentHash(base::as_bytes(key_span)) != header->key_hash) {
    RecordSyncOpenResult(cache_type_, OPEN_ENTRY_KEY_HASH_MISMATCH);
    return false;
  }

  std::string key_from_header(key_data, header->key_length);
  if (!key_.has_value()) {
    key_.emplace(std::move(key_from_header));
  } else {
    if (*key_ != key_from_header) {
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_KEY_MISMATCH);
      return false;
    }
  }

  header_and_key_check_needed_[file_index] = false;
  return true;
}

int SimpleSynchronousEntry::InitializeForOpen(
    BackendFileOperations* file_operations,
    SimpleEntryStat* out_entry_stat,
    SimpleStreamPrefetchData stream_prefetch_data[2]) {
  DCHECK(!initialized_);
  if (!OpenFiles(file_operations, out_entry_stat)) {
    DLOG(WARNING) << "Could not open platform files for entry.";
    return net::ERR_FAILED;
  }
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    if (!key_.has_value()) {
      SimpleFileTracker::FileHandle file =
          file_tracker_->Acquire(file_operations, this, SubFileForFileIndex(i));
      // If |key_| is empty, we were opened via the iterator interface, without
      // knowing what our key is. We must therefore read the header immediately
      // to discover it, so SimpleEntryImpl can make it available to
      // disk_cache::Entry::GetKey().
      if (!file.IsOK() || !CheckHeaderAndKey(file.get(), i))
        return net::ERR_FAILED;
    } else {
      // If we do know which key were are looking for, we still need to
      // check that the file actually has it (rather than just being a hash
      // collision or some sort of file system accident), but that can be put
      // off until opportune time: either the read of the footer, or when we
      // start reading in the data, depending on stream # and format revision.
      header_and_key_check_needed_[i] = true;
    }
    size_t key_size = key_->size();

    if (i == 0) {
      // File size for stream 0 has been stored temporarily in data_size[1].
      int ret_value_stream_0 = ReadAndValidateStream0AndMaybe1(
          file_operations, out_entry_stat->data_size(1), out_entry_stat,
          stream_prefetch_data);
      if (ret_value_stream_0 != net::OK)
        return ret_value_stream_0;
    } else {
      out_entry_stat->set_data_size(
          2, GetDataSizeFromFileSize(key_size, out_entry_stat->data_size(2)));
      const int32_t data_size_2 = out_entry_stat->data_size(2);
      int ret_value_stream_2 = net::OK;
      if (data_size_2 < 0) {
        DLOG(WARNING) << "Stream 2 file is too small.";
        ret_value_stream_2 = net::ERR_FAILED;
      } else if (data_size_2 > 0) {
        // Validate non empty stream 2.
        SimpleFileEOF eof_record;
        SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
            file_operations, this, SubFileForFileIndex(i));
        int file_offset =
            out_entry_stat->GetEOFOffsetInFile(key_size, 2 /*stream index*/);
        ret_value_stream_2 =
            GetEOFRecordData(file.get(), nullptr, i, file_offset, &eof_record);
      }

      if (ret_value_stream_2 != net::OK) {
        DCHECK_EQ(i, GetFileIndexFromStreamIndex(2));
        DCHECK(CanOmitEmptyFile(GetFileIndexFromStreamIndex(2)));
        // Stream 2 is broken, set its size to zero to have it automatically
        // deleted further down in this function. For V8 this preserves the
        // cached source when only the code cache was corrupted.
        out_entry_stat->set_data_size(2, 0);
      }
    }
  }

  int32_t sparse_data_size = 0;
  if (!OpenSparseFileIfExists(file_operations, &sparse_data_size)) {
    RecordSyncOpenResult(cache_type_, OPEN_ENTRY_SPARSE_OPEN_FAILED);
    return net::ERR_FAILED;
  }
  out_entry_stat->set_sparse_data_size(sparse_data_size);

  const int stream2_file_index = GetFileIndexFromStreamIndex(2);
  DCHECK(CanOmitEmptyFile(stream2_file_index));
  if (!empty_file_omitted_[stream2_file_index] &&
      out_entry_stat->data_size(2) == 0) {
    CloseFile(file_operations, stream2_file_index);
    DeleteFileForEntryHash(path_, entry_file_key_.entry_hash,
                           stream2_file_index, file_operations);
    empty_file_omitted_[stream2_file_index] = true;
  }

  RecordSyncOpenResult(cache_type_, OPEN_ENTRY_SUCCESS);
  initialized_ = true;
  return net::OK;
}

bool SimpleSynchronousEntry::InitializeCreatedFile(
    BackendFileOperations* file_operations,
    int file_index) {
  SimpleFileTracker::FileHandle file = file_tracker_->Acquire(
      file_operations, this, SubFileForFileIndex(file_index));
  if (!file.IsOK())
    return false;
  const std::string& key = *key_;

  SimpleFileHeader header;
  header.initial_magic_number = kSimpleInitialMagicNumber;
  header.version = kSimpleEntryVersionOnDisk;

  header.key_length = key.size();
  header.key_hash = base::PersistentHash(key);

  int bytes_written =
      file->Write(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (bytes_written != sizeof(header))
    return false;

  bytes_written = file->Write(sizeof(header), key.data(), key.size());
  if (bytes_written != base::checked_cast<int>(key.size())) {
    return false;
  }

  return true;
}

int SimpleSynchronousEntry::InitializeForCreate(
    BackendFileOperations* file_operations,
    SimpleEntryStat* out_entry_stat) {
  DCHECK(!initialized_);
  if (!CreateFiles(file_operations, out_entry_stat)) {
    DLOG(WARNING) << "Could not create platform files.";
    return net::ERR_FILE_EXISTS;
  }
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    if (!InitializeCreatedFile(file_operations, i))
      return net::ERR_FAILED;
  }
  initialized_ = true;
  return net::OK;
}

int SimpleSynchronousEntry::ReadAndValidateStream0AndMaybe1(
    BackendFileOperations* file_operations,
    int file_size,
    SimpleEntryStat* out_entry_stat,
    SimpleStreamPrefetchData stream_prefetch_data[2]) {
  SimpleFileTracker::FileHandle file =
      file_tracker_->Acquire(file_operations, this, SubFileForFileIndex(0));
  if (!file.IsOK())
    return net::ERR_FAILED;

  // We may prefetch data from file in a couple cases:
  //  1) If the file is small enough we may prefetch it entirely.
  //  2) We may also prefetch a block of trailer bytes from the end of
  //     the file.
  // In these cases the PrefetchData object is used to store the
  // bytes read from the file.  The PrefetchData object also keeps track
  // of what range within the file has been prefetched.  It will only
  // allow reads wholely within this range to be accessed via its
  // ReadData() method.
  PrefetchData prefetch_data(file_size);

  // Determine a threshold for fully prefetching the entire entry file.  If
  // the entry file is less than or equal to this number of bytes it will
  // be fully prefetched.
  int full_prefetch_size = GetSimpleCacheFullPrefetchSize();

  // Determine how much trailer data to prefetch.  If the full file prefetch
  // does not trigger then this is the number of bytes to read from the end
  // of the file in a single file operation.  Ideally the trailer prefetch
  // will contain at least stream 0 and its EOF record.
  int trailer_prefetch_size =
      GetSimpleCacheTrailerPrefetchSize(trailer_prefetch_size_);

  OpenPrefetchMode prefetch_mode = OPEN_PREFETCH_NONE;
  if (file_size <= full_prefetch_size || file_size <= trailer_prefetch_size) {
    // Prefetch the entire file.
    prefetch_mode = OPEN_PREFETCH_FULL;
    RecordOpenPrefetchMode(cache_type_, prefetch_mode);
    if (!prefetch_data.PrefetchFromFile(&file, 0, file_size))
      return net::ERR_FAILED;
  } else if (trailer_prefetch_size > 0) {
    // Prefetch trailer data from the end of the file.
    prefetch_mode = OPEN_PREFETCH_TRAILER;
    RecordOpenPrefetchMode(cache_type_, prefetch_mode);
    size_t length = std::min(trailer_prefetch_size, file_size);
    size_t offset = file_size - length;
    if (!prefetch_data.PrefetchFromFile(&file, offset, length))
      return net::ERR_FAILED;
  } else {
    // Do no prefetching.
    RecordOpenPrefetchMode(cache_type_, prefetch_mode);
  }

  // Read stream 0 footer first --- it has size/feature info required to figure
  // out file 0's layout.
  SimpleFileEOF stream_0_eof;
  int rv = GetEOFRecordData(
      file.get(), &prefetch_data, /* file_index = */ 0,
      /* file_offset = */ file_size - sizeof(SimpleFileEOF), &stream_0_eof);
  if (rv != net::OK)
    return rv;

  int32_t stream_0_size = stream_0_eof.stream_size;
  if (stream_0_size < 0 || stream_0_size > file_size)
    return net::ERR_FAILED;
  out_entry_stat->set_data_size(0, stream_0_size);

  // Calculate size for stream 1, now we know stream 0's.
  // See comments in simple_entry_format.h for background.
  bool has_key_sha256 =
      (stream_0_eof.flags & SimpleFileEOF::FLAG_HAS_KEY_SHA256) ==
      SimpleFileEOF::FLAG_HAS_KEY_SHA256;
  int extra_post_stream_0_read = 0;
  if (has_key_sha256)
    extra_post_stream_0_read += sizeof(net::SHA256HashValue);

  const std::string& key = *key_;
  int32_t stream1_size = file_size - 2 * sizeof(SimpleFileEOF) - stream_0_size -
                         sizeof(SimpleFileHeader) - key.size() -
                         extra_post_stream_0_read;
  if (stream1_size < 0 || stream1_size > file_size)
    return net::ERR_FAILED;

  out_entry_stat->set_data_size(1, stream1_size);

  // Put stream 0 data in memory --- plus maybe the sha256(key) footer.
  rv = PreReadStreamPayload(file.get(), &prefetch_data, /* stream_index = */ 0,
                            extra_post_stream_0_read, *out_entry_stat,
                            stream_0_eof, &stream_prefetch_data[0]);
  if (rv != net::OK)
    return rv;

  // Note the exact range needed in order to read the EOF record and stream 0.
  // In APP_CACHE mode this will be stored directly in the index so we can
  // know exactly how much to read next time.
  computed_trailer_prefetch_size_ =
      prefetch_data.GetDesiredTrailerPrefetchSize();

  // If prefetch buffer is available, and we have sha256(key) (so we don't need
  // to look at the header), extract out stream 1 info as well.
  int stream_1_offset = out_entry_stat->GetOffsetInFile(
      key.size(), /* offset= */ 0, /* stream_index = */ 1);
  int stream_1_read_size =
      sizeof(SimpleFileEOF) + out_entry_stat->data_size(/* stream_index = */ 1);
  if (has_key_sha256 &&
      prefetch_data.HasData(stream_1_offset, stream_1_read_size)) {
    SimpleFileEOF stream_1_eof;
    int stream_1_eof_offset =
        out_entry_stat->GetEOFOffsetInFile(key.size(), /* stream_index = */ 1);
    rv = GetEOFRecordData(file.get(), &prefetch_data, /* file_index = */ 0,
                          stream_1_eof_offset, &stream_1_eof);
    if (rv != net::OK)
      return rv;

    rv = PreReadStreamPayload(file.get(), &prefetch_data,
                              /* stream_index = */ 1,
                              /* extra_size = */ 0, *out_entry_stat,
                              stream_1_eof, &stream_prefetch_data[1]);
    if (rv != net::OK)
      return rv;
  }

  // If present, check the key SHA256.
  if (has_key_sha256) {
    net::SHA256HashValue hash_value;
    CalculateSHA256OfKey(key, &hash_value);
    bool matched =
        std::memcmp(&hash_value,
                    stream_prefetch_data[0].data->data() + stream_0_size,
                    sizeof(hash_value)) == 0;
    if (!matched)
      return net::ERR_FAILED;

    // Elide header check if we verified sha256(key) via footer.
    header_and_key_check_needed_[0] = false;
  }

  // Ensure the key is validated before completion.
  if (!has_key_sha256 && header_and_key_check_needed_[0])
    CheckHeaderAndKey(file.get(), 0);

  return net::OK;
}

bool SimpleSynchronousEntry::ReadFromFileOrPrefetched(
    base::File* file,
    PrefetchData* prefetch_data,
    int file_index,
    int offset,
    int size,
    char* dest) {
  if (offset < 0 || size < 0)
    return false;
  if (size == 0)
    return true;

  base::CheckedNumeric<size_t> start(offset);
  size_t start_numeric;
  if (!start.AssignIfValid(&start_numeric))
    return false;

  base::CheckedNumeric<size_t> length(size);
  size_t length_numeric;
  if (!length.AssignIfValid(&length_numeric))
    return false;

  // First try to extract the desired range from the PrefetchData.
  if (file_index == 0 && prefetch_data &&
      prefetch_data->ReadData(start_numeric, length_numeric, dest)) {
    return true;
  }

  // If we have not prefetched the range then we must read it from disk.
  return file->Read(start_numeric, dest, length_numeric) == size;
}

int SimpleSynchronousEntry::GetEOFRecordData(base::File* file,
                                             PrefetchData* prefetch_data,
                                             int file_index,
                                             int file_offset,
                                             SimpleFileEOF* eof_record) {
  if (!ReadFromFileOrPrefetched(file, prefetch_data, file_index, file_offset,
                                sizeof(SimpleFileEOF),
                                reinterpret_cast<char*>(eof_record))) {
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_READ_FAILURE);
    return net::ERR_CACHE_CHECKSUM_READ_FAILURE;
  }

  if (eof_record->final_magic_number != kSimpleFinalMagicNumber) {
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH);
    DVLOG(1) << "EOF record had bad magic number.";
    return net::ERR_CACHE_CHECKSUM_READ_FAILURE;
  }

  if (!base::IsValueInRangeForNumericType<int32_t>(eof_record->stream_size))
    return net::ERR_FAILED;
  return net::OK;
}

// static
bool SimpleSynchronousEntry::DeleteFileForEntryHash(
    const FilePath& path,
    const uint64_t entry_hash,
    const int file_index,
    BackendFileOperations* file_operations) {
  FilePath to_delete = path.AppendASCII(GetFilenameFromEntryFileKeyAndFileIndex(
      SimpleFileTracker::EntryFileKey(entry_hash), file_index));
  return file_operations->DeleteFile(to_delete);
}

// static
bool SimpleSynchronousEntry::DeleteFilesForEntryHash(
    const FilePath& path,
    const uint64_t entry_hash,
    BackendFileOperations* file_operations) {
  bool result = true;
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    if (!DeleteFileForEntryHash(path, entry_hash, i, file_operations) &&
        !CanOmitEmptyFile(i)) {
      result = false;
    }
  }
  FilePath to_delete = path.AppendASCII(GetSparseFilenameFromEntryFileKey(
      SimpleFileTracker::EntryFileKey(entry_hash)));
  file_operations->DeleteFile(
      to_delete,
      BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
  return result;
}

// static
bool SimpleSynchronousEntry::TruncateFilesForEntryHash(
    const FilePath& path,
    const uint64_t entry_hash,
    BackendFileOperations* file_operations) {
  SimpleFileTracker::EntryFileKey file_key(entry_hash);
  bool result = true;
  for (int i = 0; i < kSimpleEntryNormalFileCount; ++i) {
    FilePath filename_to_truncate =
        path.AppendASCII(GetFilenameFromEntryFileKeyAndFileIndex(file_key, i));
    if (!TruncatePath(filename_to_truncate, file_operations))
      result = false;
  }
  FilePath to_delete =
      path.AppendASCII(GetSparseFilenameFromEntryFileKey(file_key));
  TruncatePath(to_delete, file_operations);
  return result;
}

FilePath SimpleSynchronousEntry::GetFilenameFromFileIndex(
    int file_index) const {
  return path_.AppendASCII(
      GetFilenameFromEntryFileKeyAndFileIndex(entry_file_key_, file_index));
}

base::FilePath SimpleSynchronousEntry::GetFilenameForSubfile(
    SimpleFileTracker::SubFile sub_file) const {
  if (sub_file == SimpleFileTracker::SubFile::FILE_SPARSE)
    return path_.AppendASCII(
        GetSparseFilenameFromEntryFileKey(entry_file_key_));
  else
    return GetFilenameFromFileIndex(FileIndexForSubFile(sub_file));
}

bool SimpleSynchronousEntry::OpenSparseFileIfExists(
    BackendFileOperations* file_operations,
    int32_t* out_sparse_data_size) {
  DCHECK(!sparse_file_open());

  FilePath filename =
      path_.AppendASCII(GetSparseFilenameFromEntryFileKey(entry_file_key_));
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  auto sparse_file =
      std::make_unique<base::File>(file_operations->OpenFile(filename, flags));
  if (!sparse_file->IsValid()) {
    // No file -> OK, file open error -> 'trouble.
    return sparse_file->error_details() == base::File::FILE_ERROR_NOT_FOUND;
  }

  if (!ScanSparseFile(sparse_file.get(), out_sparse_data_size))
    return false;

  file_tracker_->Register(this, SimpleFileTracker::SubFile::FILE_SPARSE,
                          std::move(sparse_file));
  sparse_file_open_ = true;
  return true;
}

bool SimpleSynchronousEntry::CreateSparseFile(
    BackendFileOperations* file_operations) {
  DCHECK(!sparse_file_open());

  FilePath filename =
      path_.AppendASCII(GetSparseFilenameFromEntryFileKey(entry_file_key_));
  int flags = base::File::FLAG_CREATE | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  std::unique_ptr<base::File> sparse_file =
      std::make_unique<base::File>(file_operations->OpenFile(filename, flags));
  if (!sparse_file->IsValid())
    return false;
  if (!InitializeSparseFile(sparse_file.get()))
    return false;
  file_tracker_->Register(this, SimpleFileTracker::SubFile::FILE_SPARSE,
                          std::move(sparse_file));
  sparse_file_open_ = true;
  return true;
}

void SimpleSynchronousEntry::CloseSparseFile(
    BackendFileOperations* file_operations) {
  DCHECK(sparse_file_open());
  if (entry_file_key_.doom_generation != 0u) {
    file_operations->DeleteFile(
        path_.AppendASCII(GetSparseFilenameFromEntryFileKey(entry_file_key_)));
  }
  file_tracker_->Close(this, SimpleFileTracker::SubFile::FILE_SPARSE);
  sparse_file_open_ = false;
}

bool SimpleSynchronousEntry::TruncateSparseFile(base::File* sparse_file) {
  DCHECK(sparse_file_open());

  int64_t header_and_key_length = sizeof(SimpleFileHeader) + key_->size();
  if (!sparse_file->SetLength(header_and_key_length)) {
    DLOG(WARNING) << "Could not truncate sparse file";
    return false;
  }

  sparse_ranges_.clear();
  sparse_tail_offset_ = header_and_key_length;

  return true;
}

bool SimpleSynchronousEntry::InitializeSparseFile(base::File* sparse_file) {
  SimpleFileHeader header;
  header.initial_magic_number = kSimpleInitialMagicNumber;
  header.version = kSimpleVersion;
  const std::string& key = *key_;
  header.key_length = key.size();
  header.key_hash = base::PersistentHash(key);

  int header_write_result =
      sparse_file->Write(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (header_write_result != sizeof(header)) {
    DLOG(WARNING) << "Could not write sparse file header";
    return false;
  }

  int key_write_result =
      sparse_file->Write(sizeof(header), key.data(), key.size());
  if (key_write_result != base::checked_cast<int>(key.size())) {
    DLOG(WARNING) << "Could not write sparse file key";
    return false;
  }

  sparse_ranges_.clear();
  sparse_tail_offset_ = sizeof(header) + key.size();

  return true;
}

bool SimpleSynchronousEntry::ScanSparseFile(base::File* sparse_file,
                                            int32_t* out_sparse_data_size) {
  int64_t sparse_data_size = 0;

  SimpleFileHeader header;
  int header_read_result =
      sparse_file->Read(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (header_read_result != sizeof(header)) {
    DLOG(WARNING) << "Could not read header from sparse file.";
    return false;
  }

  if (header.initial_magic_number != kSimpleInitialMagicNumber) {
    DLOG(WARNING) << "Sparse file magic number did not match.";
    return false;
  }

  if (header.version < kLastCompatSparseVersion ||
      header.version > kSimpleVersion) {
    DLOG(WARNING) << "Sparse file unreadable version.";
    return false;
  }

  sparse_ranges_.clear();

  int64_t range_header_offset = sizeof(header) + key_->size();
  while (true) {
    SimpleFileSparseRangeHeader range_header;
    int range_header_read_result = sparse_file->Read(
        range_header_offset, reinterpret_cast<char*>(&range_header),
        sizeof(range_header));
    if (range_header_read_result == 0)
      break;
    if (range_header_read_result != sizeof(range_header)) {
      DLOG(WARNING) << "Could not read sparse range header.";
      return false;
    }

    if (range_header.sparse_range_magic_number !=
        kSimpleSparseRangeMagicNumber) {
      DLOG(WARNING) << "Invalid sparse range header magic number.";
      return false;
    }

    SparseRange range;
    range.offset = range_header.offset;
    range.length = range_header.length;
    range.data_crc32 = range_header.data_crc32;
    range.file_offset = range_header_offset + sizeof(range_header);
    sparse_ranges_.emplace(range.offset, range);

    range_header_offset += sizeof(range_header) + range.length;

    DCHECK_GE(sparse_data_size + range.length, sparse_data_size);
    sparse_data_size += range.length;
  }

  *out_sparse_data_size = static_cast<int32_t>(sparse_data_size);
  sparse_tail_offset_ = range_header_offset;

  return true;
}

bool SimpleSynchronousEntry::ReadSparseRange(base::File* sparse_file,
                                             const SparseRange* range,
                                             int offset,
                                             int len,
                                             char* buf) {
  DCHECK(range);
  DCHECK(buf);
  DCHECK_LE(offset, range->length);
  DCHECK_LE(offset + len, range->length);

  int bytes_read = sparse_file->Read(range->file_offset + offset, buf, len);
  if (bytes_read < len) {
    DLOG(WARNING) << "Could not read sparse range.";
    return false;
  }

  // If we read the whole range and we have a crc32, check it.
  if (offset == 0 && len == range->length && range->data_crc32 != 0) {
    if (simple_util::Crc32(buf, len) != range->data_crc32) {
      DLOG(WARNING) << "Sparse range crc32 mismatch.";
      return false;
    }
  }
  // TODO(morlovich): Incremental crc32 calculation?

  return true;
}

bool SimpleSynchronousEntry::WriteSparseRange(base::File* sparse_file,
                                              SparseRange* range,
                                              int offset,
                                              int len,
                                              const char* buf) {
  DCHECK(range);
  DCHECK(buf);
  DCHECK_LE(offset, range->length);
  DCHECK_LE(offset + len, range->length);

  uint32_t new_crc32 = 0;
  if (offset == 0 && len == range->length) {
    new_crc32 = simple_util::Crc32(buf, len);
  }

  if (new_crc32 != range->data_crc32) {
    range->data_crc32 = new_crc32;

    SimpleFileSparseRangeHeader header;
    header.sparse_range_magic_number = kSimpleSparseRangeMagicNumber;
    header.offset = range->offset;
    header.length = range->length;
    header.data_crc32 = range->data_crc32;

    int bytes_written =
        sparse_file->Write(range->file_offset - sizeof(header),
                           reinterpret_cast<char*>(&header), sizeof(header));
    if (bytes_written != base::checked_cast<int>(sizeof(header))) {
      DLOG(WARNING) << "Could not rewrite sparse range header.";
      return false;
    }
  }

  int bytes_written = sparse_file->Write(range->file_offset + offset, buf, len);
  if (bytes_written < len) {
    DLOG(WARNING) << "Could not write sparse range.";
    return false;
  }

  return true;
}

bool SimpleSynchronousEntry::AppendSparseRange(base::File* sparse_file,
                                               int64_t offset,
                                               int len,
                                               const char* buf) {
  DCHECK_GE(offset, 0);
  DCHECK_GT(len, 0);
  DCHECK(buf);

  uint32_t data_crc32 = simple_util::Crc32(buf, len);

  SimpleFileSparseRangeHeader header;
  header.sparse_range_magic_number = kSimpleSparseRangeMagicNumber;
  header.offset = offset;
  header.length = len;
  header.data_crc32 = data_crc32;

  int bytes_written = sparse_file->Write(
      sparse_tail_offset_, reinterpret_cast<char*>(&header), sizeof(header));
  if (bytes_written != base::checked_cast<int>(sizeof(header))) {
    DLOG(WARNING) << "Could not append sparse range header.";
    return false;
  }
  sparse_tail_offset_ += bytes_written;

  bytes_written = sparse_file->Write(sparse_tail_offset_, buf, len);
  if (bytes_written < len) {
    DLOG(WARNING) << "Could not append sparse range data.";
    return false;
  }
  int64_t data_file_offset = sparse_tail_offset_;
  sparse_tail_offset_ += bytes_written;

  SparseRange range;
  range.offset = offset;
  range.length = len;
  range.data_crc32 = data_crc32;
  range.file_offset = data_file_offset;
  sparse_ranges_.emplace(offset, range);

  return true;
}

}  // namespace disk_cache

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/simple/simple_index_file.h"

#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_file_enumerator.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"

namespace disk_cache {
namespace {

const int kEntryFilesHashLength = 16;
const int kEntryFilesSuffixLength = 2;

// Limit on how big a file we are willing to work with, to avoid crashes
// when its corrupt.
const int kMaxEntriesInIndex = 1000000;

// Here 8 comes from the key size.
const int64_t kMaxIndexFileSizeBytes =
    kMaxEntriesInIndex * (8 + EntryMetadata::kOnDiskSizeBytes);

uint32_t CalculatePickleCRC(const base::Pickle& pickle) {
  return simple_util::Crc32(pickle.payload_bytes());
}

// Used in histograms. Please only add new values at the end.
enum IndexFileState {
  INDEX_STATE_CORRUPT = 0,
  INDEX_STATE_STALE = 1,
  INDEX_STATE_FRESH = 2,
  INDEX_STATE_FRESH_CONCURRENT_UPDATES = 3,
  INDEX_STATE_MAX = 4,
};

enum StaleIndexQuality {
  STALE_INDEX_OK = 0,
  STALE_INDEX_MISSED_ENTRIES = 1,
  STALE_INDEX_EXTRA_ENTRIES = 2,
  STALE_INDEX_BOTH_MISSED_AND_EXTRA_ENTRIES = 3,
  STALE_INDEX_MAX = 4,
};

void UmaRecordIndexFileState(IndexFileState state, net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "IndexFileStateOnLoad", cache_type, state, INDEX_STATE_MAX);
}

void UmaRecordIndexInitMethod(SimpleIndex::IndexInitMethod method,
                              net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(ENUMERATION, "IndexInitializeMethod", cache_type, method,
                   SimpleIndex::INITIALIZE_METHOD_MAX);
}

void UmaRecordStaleIndexQuality(int missed_entry_count,
                                int extra_entry_count,
                                net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(CUSTOM_COUNTS, "StaleIndexMissedEntryCount", cache_type,
                   missed_entry_count, 1, 100, 5);
  SIMPLE_CACHE_UMA(CUSTOM_COUNTS, "StaleIndexExtraEntryCount", cache_type,
                   extra_entry_count, 1, 100, 5);

  StaleIndexQuality quality;
  if (missed_entry_count > 0 && extra_entry_count > 0)
    quality = STALE_INDEX_BOTH_MISSED_AND_EXTRA_ENTRIES;
  else if (missed_entry_count > 0)
    quality = STALE_INDEX_MISSED_ENTRIES;
  else if (extra_entry_count > 0)
    quality = STALE_INDEX_EXTRA_ENTRIES;
  else
    quality = STALE_INDEX_OK;
  SIMPLE_CACHE_UMA(ENUMERATION, "StaleIndexQuality", cache_type, quality,
                   STALE_INDEX_MAX);
}

struct PickleHeader : public base::Pickle::Header {
  uint32_t crc;
};

class SimpleIndexPickle : public base::Pickle {
 public:
  SimpleIndexPickle() : base::Pickle(sizeof(PickleHeader)) {}
  explicit SimpleIndexPickle(base::span<const uint8_t> data)
      : base::Pickle(base::Pickle::kUnownedData, data) {}

  bool HeaderValid() const { return header_size() == sizeof(PickleHeader); }
};

bool WritePickleFile(BackendFileOperations* file_operations,
                     base::Pickle* pickle,
                     const base::FilePath& file_name) {
  base::File file = file_operations->OpenFile(
      file_name, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                     base::File::FLAG_WIN_SHARE_DELETE);
  if (!file.IsValid())
    return false;

  int bytes_written = file.Write(0, pickle->data_as_char(), pickle->size());
  if (bytes_written != base::checked_cast<int>(pickle->size())) {
    file_operations->DeleteFile(
        file_name,
        BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
    return false;
  }
  return true;
}

// Called for each cache directory traversal iteration.
void ProcessEntryFile(BackendFileOperations* file_operations,
                      net::CacheType cache_type,
                      SimpleIndex::EntrySet* entries,
                      const base::FilePath& file_path,
                      base::Time last_accessed,
                      base::Time last_modified,
                      int64_t size) {
  static const size_t kEntryFilesLength =
      kEntryFilesHashLength + kEntryFilesSuffixLength;
  // Converting to std::string is OK since we never use UTF8 wide chars in our
  // file names.
  const base::FilePath::StringType base_name = file_path.BaseName().value();
  const std::string file_name(base_name.begin(), base_name.end());

  // Cleanup any left over doomed entries.
  if (file_name.starts_with("todelete_")) {
    file_operations->DeleteFile(file_path);
    return;
  }

  if (file_name.size() != kEntryFilesLength)
    return;
  const auto hash_string = base::MakeStringPiece(
      file_name.begin(), file_name.begin() + kEntryFilesHashLength);
  uint64_t hash_key = 0;
  if (!simple_util::GetEntryHashKeyFromHexString(hash_string, &hash_key)) {
    LOG(WARNING) << "Invalid entry hash key filename while restoring index from"
                 << " disk: " << file_name;
    return;
  }

  base::Time last_used_time;
#if BUILDFLAG(IS_POSIX)
  // For POSIX systems, a last access time is available. However, it's not
  // guaranteed to be more accurate than mtime. It is no worse though.
  last_used_time = last_accessed;
#endif
  if (last_used_time.is_null())
    last_used_time = last_modified;

  auto it = entries->find(hash_key);
  base::CheckedNumeric<uint32_t> total_entry_size = size;

  // Sometimes we see entry sizes here which are nonsense. We can't use them
  // as-is, as they simply won't fit the type. The options that come to mind
  // are:
  // 1) Ignore the file.
  // 2) Make something up.
  // 3) Delete the files for the hash.
  // ("crash the browser" isn't considered a serious alternative).
  //
  // The problem with doing (1) is that we are recovering the index here, so if
  // we don't include the info on the file here, we may completely lose track of
  // the entry and never clean the file up.
  //
  // (2) is actually mostly fine: we may trigger eviction too soon or too late,
  // but we can't really do better since we can't trust the size. If the entry
  // is never opened, it will eventually get evicted. If it is opened, we will
  // re-check the file size, and if it's nonsense delete it there, and if it's
  // fine we will fix up the index via a UpdateDataFromEntryStat to have the
  // correct size.
  //
  // (3) does the best thing except when the wrong size is some weird interim
  // thing just on directory listing (in which case it may evict an entry
  // prematurely). It's a little harder to think about since it involves
  // mutating the disk while there are other mutations going on, however,
  // while (2) is single-threaded.
  //
  // Hence this picks (2).

  const int kPlaceHolderSizeWhenInvalid = 32768;
  if (!total_entry_size.IsValid()) {
    LOG(WARNING) << "Invalid file size while restoring index from disk: "
                 << size << " on file:" << file_name;
  }

  if (it == entries->end()) {
    uint32_t size_to_use =
        total_entry_size.ValueOrDefault(kPlaceHolderSizeWhenInvalid);
    if (cache_type == net::APP_CACHE) {
      SimpleIndex::InsertInEntrySet(
          hash_key, EntryMetadata(0 /* trailer_prefetch_size */, size_to_use),
          entries);
    } else {
      SimpleIndex::InsertInEntrySet(
          hash_key, EntryMetadata(last_used_time, size_to_use), entries);
    }
  } else {
    // Summing up the total size of the entry through all the *_[0-1] files
    total_entry_size += it->second.GetEntrySize();
    it->second.SetEntrySize(
        total_entry_size.ValueOrDefault(kPlaceHolderSizeWhenInvalid));
  }
}

}  // namespace

SimpleIndexLoadResult::SimpleIndexLoadResult() = default;

SimpleIndexLoadResult::~SimpleIndexLoadResult() = default;

void SimpleIndexLoadResult::Reset() {
  did_load = false;
  index_write_reason = SimpleIndex::INDEX_WRITE_REASON_MAX;
  flush_required = false;
  entries.clear();
}

// static
const char SimpleIndexFile::kIndexFileName[] = "the-real-index";
// static
const char SimpleIndexFile::kIndexDirectory[] = "index-dir";
// static
const char SimpleIndexFile::kTempIndexFileName[] = "temp-index";

SimpleIndexFile::IndexMetadata::IndexMetadata()
    : reason_(SimpleIndex::INDEX_WRITE_REASON_MAX),
      entry_count_(0),
      cache_size_(0) {}

SimpleIndexFile::IndexMetadata::IndexMetadata(
    SimpleIndex::IndexWriteToDiskReason reason,
    uint64_t entry_count,
    uint64_t cache_size)
    : reason_(reason), entry_count_(entry_count), cache_size_(cache_size) {}

void SimpleIndexFile::IndexMetadata::Serialize(base::Pickle* pickle) const {
  DCHECK(pickle);
  pickle->WriteUInt64(magic_number_);
  pickle->WriteUInt32(version_);
  pickle->WriteUInt64(entry_count_);
  pickle->WriteUInt64(cache_size_);
  pickle->WriteUInt32(static_cast<uint32_t>(reason_));
}

// static
void SimpleIndexFile::SerializeFinalData(base::Time cache_modified,
                                         base::Pickle* pickle) {
  pickle->WriteInt64(cache_modified.ToInternalValue());
  PickleHeader* header_p = pickle->headerT<PickleHeader>();
  header_p->crc = CalculatePickleCRC(*pickle);
}

bool SimpleIndexFile::IndexMetadata::Deserialize(base::PickleIterator* it) {
  DCHECK(it);

  bool v6_format_index_read_results =
      it->ReadUInt64(&magic_number_) && it->ReadUInt32(&version_) &&
      it->ReadUInt64(&entry_count_) && it->ReadUInt64(&cache_size_);
  if (!v6_format_index_read_results)
    return false;
  if (version_ >= 7) {
    uint32_t tmp_reason;
    if (!it->ReadUInt32(&tmp_reason))
      return false;
    reason_ = static_cast<SimpleIndex::IndexWriteToDiskReason>(tmp_reason);
  }
  return true;
}

void SimpleIndexFile::SyncWriteToDisk(
    std::unique_ptr<BackendFileOperations> file_operations,
    net::CacheType cache_type,
    const base::FilePath& cache_directory,
    const base::FilePath& index_filename,
    const base::FilePath& temp_index_filename,
    std::unique_ptr<base::Pickle> pickle) {
  DCHECK_EQ(index_filename.DirName().value(),
            temp_index_filename.DirName().value());
  base::FilePath index_file_directory = temp_index_filename.DirName();
  if (!file_operations->DirectoryExists(index_file_directory) &&
      !file_operations->CreateDirectory(index_file_directory)) {
    LOG(ERROR) << "Could not create a directory to hold the index file";
    return;
  }

  // There is a chance that the index containing all the necessary data about
  // newly created entries will appear to be stale. This can happen if on-disk
  // part of a Create operation does not fit into the time budget for the index
  // flush delay. This simple approach will be reconsidered if it does not allow
  // for maintaining freshness.
  base::Time cache_dir_mtime;
  std::optional<base::File::Info> file_info =
      file_operations->GetFileInfo(cache_directory);
  if (!file_info) {
    LOG(ERROR) << "Could not obtain information about cache age";
    return;
  }
  cache_dir_mtime = file_info->last_modified;
  SerializeFinalData(cache_dir_mtime, pickle.get());
  if (!WritePickleFile(file_operations.get(), pickle.get(),
                       temp_index_filename)) {
    LOG(ERROR) << "Failed to write the temporary index file";
    return;
  }

  // Atomically rename the temporary index file to become the real one.
  if (!file_operations->ReplaceFile(temp_index_filename, index_filename,
                                    nullptr)) {
    return;
  }
}

bool SimpleIndexFile::IndexMetadata::CheckIndexMetadata() {
  if (entry_count_ > kMaxEntriesInIndex ||
      magic_number_ != kSimpleIndexMagicNumber) {
    return false;
  }

  static_assert(kSimpleVersion == 9, "index metadata reader out of date");
  // No |reason_| is saved in the version 6 file format.
  if (version_ == 6)
    return reason_ == SimpleIndex::INDEX_WRITE_REASON_MAX;
  return (version_ == 7 || version_ == 8 || version_ == 9) &&
         reason_ < SimpleIndex::INDEX_WRITE_REASON_MAX;
}

SimpleIndexFile::SimpleIndexFile(
    scoped_refptr<base::SequencedTaskRunner> cache_runner,
    scoped_refptr<BackendFileOperationsFactory> file_operations_factory,
    net::CacheType cache_type,
    const base::FilePath& cache_directory)
    : cache_runner_(std::move(cache_runner)),
      file_operations_factory_(std::move(file_operations_factory)),
      cache_type_(cache_type),
      cache_directory_(cache_directory),
      index_file_(cache_directory_.AppendASCII(kIndexDirectory)
                      .AppendASCII(kIndexFileName)),
      temp_index_file_(cache_directory_.AppendASCII(kIndexDirectory)
                           .AppendASCII(kTempIndexFileName)) {}

SimpleIndexFile::~SimpleIndexFile() = default;

void SimpleIndexFile::LoadIndexEntries(base::Time cache_last_modified,
                                       base::OnceClosure callback,
                                       SimpleIndexLoadResult* out_result) {
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      SimpleBackendImpl::kWorkerPoolTaskTraits);
  base::OnceClosure task = base::BindOnce(
      &SimpleIndexFile::SyncLoadIndexEntries,
      file_operations_factory_->Create(task_runner), cache_type_,
      cache_last_modified, cache_directory_, index_file_, out_result);
  task_runner->PostTaskAndReply(FROM_HERE, std::move(task),
                                std::move(callback));
}

void SimpleIndexFile::WriteToDisk(net::CacheType cache_type,
                                  SimpleIndex::IndexWriteToDiskReason reason,
                                  const SimpleIndex::EntrySet& entry_set,
                                  uint64_t cache_size,
                                  base::OnceClosure callback) {
  IndexMetadata index_metadata(reason, entry_set.size(), cache_size);
  std::unique_ptr<base::Pickle> pickle =
      Serialize(cache_type, index_metadata, entry_set);
  auto file_operations = file_operations_factory_->Create(cache_runner_);
  base::OnceClosure task =
      base::BindOnce(&SimpleIndexFile::SyncWriteToDisk,
                     std::move(file_operations), cache_type_, cache_directory_,
                     index_file_, temp_index_file_, std::move(pickle));
  if (callback.is_null())
    cache_runner_->PostTask(FROM_HERE, std::move(task));
  else
    cache_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                    std::move(callback));
}

// static
void SimpleIndexFile::SyncLoadIndexEntries(
    std::unique_ptr<BackendFileOperations> file_operations,
    net::CacheType cache_type,
    base::Time cache_last_modified,
    const base::FilePath& cache_directory,
    const base::FilePath& index_file_path,
    SimpleIndexLoadResult* out_result) {
  // Load the index and find its age.
  base::Time last_cache_seen_by_index;
  SyncLoadFromDisk(file_operations.get(), cache_type, index_file_path,
                   &last_cache_seen_by_index, out_result);

  // Consider the index loaded if it is fresh.
  const bool index_file_existed = file_operations->PathExists(index_file_path);
  if (!out_result->did_load) {
    if (index_file_existed)
      UmaRecordIndexFileState(INDEX_STATE_CORRUPT, cache_type);
  } else {
    if (cache_last_modified <= last_cache_seen_by_index) {
      base::Time latest_dir_mtime;
      if (auto info = file_operations->GetFileInfo(cache_directory)) {
        latest_dir_mtime = info->last_modified;
      }
      if (LegacyIsIndexFileStale(file_operations.get(), latest_dir_mtime,
                                 index_file_path)) {
        UmaRecordIndexFileState(INDEX_STATE_FRESH_CONCURRENT_UPDATES,
                                cache_type);
      } else {
        UmaRecordIndexFileState(INDEX_STATE_FRESH, cache_type);
      }
      out_result->init_method = SimpleIndex::INITIALIZE_METHOD_LOADED;
      UmaRecordIndexInitMethod(out_result->init_method, cache_type);
      return;
    }
    UmaRecordIndexFileState(INDEX_STATE_STALE, cache_type);
  }

  // Reconstruct the index by scanning the disk for entries.
  SimpleIndex::EntrySet entries_from_stale_index;
  entries_from_stale_index.swap(out_result->entries);
  const base::TimeTicks start = base::TimeTicks::Now();
  SyncRestoreFromDisk(file_operations.get(), cache_type, cache_directory,
                      index_file_path, out_result);
  SIMPLE_CACHE_UMA(MEDIUM_TIMES, "IndexRestoreTime", cache_type,
                   base::TimeTicks::Now() - start);
  if (index_file_existed) {
    out_result->init_method = SimpleIndex::INITIALIZE_METHOD_RECOVERED;

    int missed_entry_count = 0;
    for (const auto& i : out_result->entries) {
      if (entries_from_stale_index.count(i.first) == 0)
        ++missed_entry_count;
    }
    int extra_entry_count = 0;
    for (const auto& i : entries_from_stale_index) {
      if (out_result->entries.count(i.first) == 0)
        ++extra_entry_count;
    }
    UmaRecordStaleIndexQuality(missed_entry_count, extra_entry_count,
                               cache_type);
  } else {
    out_result->init_method = SimpleIndex::INITIALIZE_METHOD_NEWCACHE;
    SIMPLE_CACHE_UMA(COUNTS_1M,
                     "IndexCreatedEntryCount", cache_type,
                     out_result->entries.size());
  }
  UmaRecordIndexInitMethod(out_result->init_method, cache_type);
}

// static
void SimpleIndexFile::SyncLoadFromDisk(BackendFileOperations* file_operations,
                                       net::CacheType cache_type,
                                       const base::FilePath& index_filename,
                                       base::Time* out_last_cache_seen_by_index,
                                       SimpleIndexLoadResult* out_result) {
  out_result->Reset();

  base::File file = file_operations->OpenFile(
      index_filename, base::File::FLAG_OPEN | base::File::FLAG_READ |
                          base::File::FLAG_WIN_SHARE_DELETE |
                          base::File::FLAG_WIN_SEQUENTIAL_SCAN);
  if (!file.IsValid())
    return;

  // Sanity-check the length. We don't want to crash trying to read some corrupt
  // 10GiB file or such.
  int64_t file_length = file.GetLength();
  if (file_length < 0 || file_length > kMaxIndexFileSizeBytes) {
    file_operations->DeleteFile(
        index_filename,
        BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
    return;
  }

  // Make sure to preallocate in one chunk, so we don't induce fragmentation
  // reallocating a growing buffer.
  auto buffer = std::make_unique<char[]>(file_length);

  int read = file.Read(0, buffer.get(), file_length);
  if (read < file_length) {
    file_operations->DeleteFile(
        index_filename,
        BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
    return;
  }

  SimpleIndexFile::Deserialize(cache_type, buffer.get(), read,
                               out_last_cache_seen_by_index, out_result);

  if (!out_result->did_load) {
    file_operations->DeleteFile(
        index_filename,
        BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
  }
}

// static
std::unique_ptr<base::Pickle> SimpleIndexFile::Serialize(
    net::CacheType cache_type,
    const SimpleIndexFile::IndexMetadata& index_metadata,
    const SimpleIndex::EntrySet& entries) {
  std::unique_ptr<base::Pickle> pickle = std::make_unique<SimpleIndexPickle>();

  index_metadata.Serialize(pickle.get());
  for (const auto& entry : entries) {
    pickle->WriteUInt64(entry.first);
    entry.second.Serialize(cache_type, pickle.get());
  }
  return pickle;
}

// static
void SimpleIndexFile::Deserialize(net::CacheType cache_type,
                                  const char* data,
                                  int data_len,
                                  base::Time* out_cache_last_modified,
                                  SimpleIndexLoadResult* out_result) {
  DCHECK(data);

  out_result->Reset();
  SimpleIndex::EntrySet* entries = &out_result->entries;

  SimpleIndexPickle pickle(
      base::as_bytes(base::span(data, base::checked_cast<size_t>(data_len))));
  if (!pickle.data() || !pickle.HeaderValid()) {
    LOG(WARNING) << "Corrupt Simple Index File.";
    return;
  }

  base::PickleIterator pickle_it(pickle);
  PickleHeader* header_p = pickle.headerT<PickleHeader>();
  const uint32_t crc_read = header_p->crc;
  const uint32_t crc_calculated = CalculatePickleCRC(pickle);

  if (crc_read != crc_calculated) {
    LOG(WARNING) << "Invalid CRC in Simple Index file.";
    return;
  }

  SimpleIndexFile::IndexMetadata index_metadata;
  if (!index_metadata.Deserialize(&pickle_it)) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return;
  }

  if (!index_metadata.CheckIndexMetadata()) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return;
  }

  entries->reserve(index_metadata.entry_count() + kExtraSizeForMerge);
  while (entries->size() < index_metadata.entry_count()) {
    uint64_t hash_key;
    EntryMetadata entry_metadata;
    if (!pickle_it.ReadUInt64(&hash_key) ||
        !entry_metadata.Deserialize(
            cache_type, &pickle_it, index_metadata.has_entry_in_memory_data(),
            index_metadata.app_cache_has_trailer_prefetch_size())) {
      LOG(WARNING) << "Invalid EntryMetadata in Simple Index file.";
      entries->clear();
      return;
    }
    SimpleIndex::InsertInEntrySet(hash_key, entry_metadata, entries);
  }

  int64_t cache_last_modified;
  if (!pickle_it.ReadInt64(&cache_last_modified)) {
    entries->clear();
    return;
  }
  DCHECK(out_cache_last_modified);
  *out_cache_last_modified = base::Time::FromInternalValue(cache_last_modified);

  out_result->index_write_reason = index_metadata.reason();
  out_result->did_load = true;
}

// static
void SimpleIndexFile::SyncRestoreFromDisk(
    BackendFileOperations* file_operations,
    net::CacheType cache_type,
    const base::FilePath& cache_directory,
    const base::FilePath& index_file_path,
    SimpleIndexLoadResult* out_result) {
  VLOG(1) << "Simple Cache Index is being restored from disk.";
  file_operations->DeleteFile(
      index_file_path,
      BackendFileOperations::DeleteFileMode::kEnsureImmediateAvailability);
  out_result->Reset();
  SimpleIndex::EntrySet* entries = &out_result->entries;

  auto enumerator = file_operations->EnumerateFiles(cache_directory);
  while (std::optional<SimpleFileEnumerator::Entry> entry =
             enumerator->Next()) {
    ProcessEntryFile(file_operations, cache_type, entries, entry->path,
                     entry->last_accessed, entry->last_modified, entry->size);
  }
  if (enumerator->HasError()) {
    LOG(ERROR) << "Could not reconstruct index from disk";
    return;
  }
  out_result->did_load = true;
  // When we restore from disk we write the merged index file to disk right
  // away, this might save us from having to restore again next time.
  out_result->flush_required = true;
}

// static
bool SimpleIndexFile::LegacyIsIndexFileStale(
    BackendFileOperations* file_operations,
    base::Time cache_last_modified,
    const base::FilePath& index_file_path) {
  if (auto info = file_operations->GetFileInfo(index_file_path)) {
    return info->last_modified < cache_last_modified;
  }
  return true;
}

}  // namespace disk_cache

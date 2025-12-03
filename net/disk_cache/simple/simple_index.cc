// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/memory_entry_data_hints.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index_delegate.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#include <sys/time.h>
#endif

namespace {

// How many milliseconds we delay writing the index to disk since the last cache
// operation has happened.
constexpr int kWriteToDiskDelayMSecs = 20000;
constexpr int kWriteToDiskOnBackgroundDelayMSecs = 100;

// Divides the cache space into this amount of parts to evict when only one part
// is left.
constexpr uint32_t kEvictionMarginDivisor = 20;

constexpr uint32_t kBytesInMiB = 1024 * 1024;

// This is added to the size of each entry before using the size
// to determine which entries to evict first. It's basically an
// estimate of the filesystem overhead, but it also serves to flatten
// the curve so that 1-byte entries and 2-byte entries are basically
// treated the same.
constexpr int kEstimatedEntryOverhead = 512;

// On the disk, the entry info is filled in like following:
// (upper bits)
// 26 bits: empty
// 30 bits: `entry_size_256b_chunks_`
//  6 bits: empty
//  2 bits: `in_memory_data_`
// (lower bits)
//
// | 26 bits |         30 bits         | 6 bits  |     2 bits      |
// | (empty) | entry_size_256b_chunks_ | (empty) | in_memory_data_ |
uint64_t PackEntrySizeAndInMemoryData(uint32_t entry_size_256b_chunks,
                                      uint8_t in_memory_data) {
  return (static_cast<uint64_t>(entry_size_256b_chunks) << 8) |
         static_cast<uint64_t>(in_memory_data);
}

struct EntryMetadataParams {
  EntryMetadataParams(uint32_t entry_size_256b_chunks, uint8_t in_memory_data)
      : entry_size_256b_chunks(entry_size_256b_chunks),
        in_memory_data(in_memory_data) {}

  uint32_t entry_size_256b_chunks;
  uint8_t in_memory_data;
};

EntryMetadataParams UnpackEntrySizeAndInMemoryData(uint64_t tmp_entry_size) {
  EntryMetadataParams params(static_cast<uint32_t>(tmp_entry_size >> 8),
                             static_cast<uint8_t>(tmp_entry_size & 0x03));

  return params;
}

}  // namespace

namespace disk_cache {

EntryMetadata::EntryMetadata()
    : last_used_time_seconds_since_epoch_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {}

EntryMetadata::EntryMetadata(base::Time last_used_time,
                             base::StrictNumeric<uint64_t> entry_size)
    : last_used_time_seconds_since_epoch_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {
  CHECK(SetEntrySize(entry_size))
      << "Failed to create EntryMetadata due to too large entry_size: "
      << static_cast<uint64_t>(entry_size);

  SetLastUsedTime(last_used_time);
}

EntryMetadata::EntryMetadata(uint32_t trailer_prefetch_size,
                             base::StrictNumeric<uint64_t> entry_size)
    : trailer_prefetch_size_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {
  CHECK(SetEntrySize(entry_size))
      << "Failed to create EntryMetadata due to too large entry_size: "
      << static_cast<uint64_t>(entry_size);

  SetTrailerPrefetchSize(trailer_prefetch_size);
}

base::Time EntryMetadata::GetLastUsedTime() const {
  // Preserve nullity.
  if (last_used_time_seconds_since_epoch_ == 0)
    return base::Time();

  return base::Time::UnixEpoch() +
         base::Seconds(last_used_time_seconds_since_epoch_);
}

void EntryMetadata::SetLastUsedTime(base::Time last_used_time) {
  // Preserve nullity.
  if (last_used_time.is_null()) {
    last_used_time_seconds_since_epoch_ = 0;
    return;
  }

  last_used_time_seconds_since_epoch_ = base::saturated_cast<uint32_t>(
      (last_used_time - base::Time::UnixEpoch()).InSeconds());
  // Avoid accidental nullity.
  if (last_used_time_seconds_since_epoch_ == 0)
    last_used_time_seconds_since_epoch_ = 1;
}

uint32_t EntryMetadata::GetTrailerPrefetchSize() const {
  return trailer_prefetch_size_;
}

void EntryMetadata::SetTrailerPrefetchSize(uint32_t size) {
  if (size == 0) {
    return;
  }

  trailer_prefetch_size_ = size;
}

uint64_t EntryMetadata::GetEntrySize() const {
  return static_cast<uint64_t>(entry_size_256b_chunks_) << 8;
}

bool EntryMetadata::SetEntrySize(base::StrictNumeric<uint64_t> entry_size) {
  // This should not overflow since we limit entries to 1/8th of the cache.
  uint64_t rounded_chunk = (static_cast<uint64_t>(entry_size) + 255) >> 8;

  // `entry_size_256b_chunks_` is a 30 bits field. Cannot be over the max.
  if (rounded_chunk >> 30) {
    return false;
  }

  entry_size_256b_chunks_ = rounded_chunk;
  return true;
}

uint8_t EntryMetadata::GetInMemoryData() const {
  return in_memory_data_;
}

void EntryMetadata::SetInMemoryData(uint8_t val) {
  // Memory data should only use 2 bits.
  CHECK_LE(val, 3);

  in_memory_data_ = val;
}

void EntryMetadata::Serialize(net::CacheType cache_type,
                              base::Pickle* pickle) const {
  DCHECK(pickle);
  // If you modify the size of the size of the pickle, be sure to update
  // kOnDiskSizeBytes.

  uint64_t packed_entry_info =
      PackEntrySizeAndInMemoryData(entry_size_256b_chunks_, in_memory_data_);

  if (cache_type == net::APP_CACHE) {
    pickle->WriteInt64(trailer_prefetch_size_);
  } else {
    int64_t internal_last_used_time = GetLastUsedTime().ToInternalValue();
    pickle->WriteInt64(internal_last_used_time);
  }
  pickle->WriteUInt64(packed_entry_info);
}

bool EntryMetadata::Deserialize(net::CacheType cache_type,
                                base::PickleIterator* it,
                                bool app_cache_has_trailer_prefetch_size) {
  DCHECK(it);
  int64_t tmp_time_or_prefetch_size;
  uint64_t tmp_entry_size;

  // The entry size must fit within 38 bits.
  if (!it->ReadInt64(&tmp_time_or_prefetch_size) ||
      !it->ReadUInt64(&tmp_entry_size) || tmp_entry_size >> 38) {
    return false;
  }

  if (cache_type == net::APP_CACHE) {
    if (app_cache_has_trailer_prefetch_size) {
      uint32_t trailer_prefetch_size = 0;
      base::CheckedNumeric<uint32_t> numeric_size(tmp_time_or_prefetch_size);
      if (numeric_size.AssignIfValid(&trailer_prefetch_size)) {
        SetTrailerPrefetchSize(trailer_prefetch_size);
      }
    }
  } else {
    SetLastUsedTime(base::Time::FromInternalValue(tmp_time_or_prefetch_size));
  }

  // tmp_entry_size actually packs entry_size_256b_chunks_ and
  // in_memory_data_.
  auto params = UnpackEntrySizeAndInMemoryData(tmp_entry_size);
  entry_size_256b_chunks_ = params.entry_size_256b_chunks;
  SetInMemoryData(params.in_memory_data);

  return true;
}

SimpleIndex::SimpleIndex(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker,
    SimpleIndexDelegate* delegate,
    net::CacheType cache_type,
    std::unique_ptr<SimpleIndexFile> index_file)
    : cleanup_tracker_(std::move(cleanup_tracker)),
      delegate_(delegate),
      cache_type_(cache_type),
      index_file_(std::move(index_file)),
      task_runner_(task_runner),
      prioritized_caching_enabled_(base::FeatureList::IsEnabled(
          net::features::kSimpleCachePrioritizedCaching)),
      caching_prioritization_factor_(
          net::features::kSimpleCachePrioritizedCachingPrioritizationFactor
              .Get()),
      caching_prioritization_period_in_seconds_(static_cast<uint64_t>(
          net::features::kSimpleCachePrioritizedCachingPrioritizationPeriod
              .Get()
              .InSeconds())) {
  // Creating the callback once so it is reused every time
  // write_to_disk_timer_.Start() is called.
  write_to_disk_cb_ = base::BindRepeating(&SimpleIndex::WriteToDisk,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          INDEX_WRITE_REASON_IDLE);
}

SimpleIndex::~SimpleIndex() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Fail all callbacks waiting for the index to come up.
  for (auto& callback : to_run_when_initialized_) {
    std::move(callback).Run(net::ERR_ABORTED);
  }
}

void SimpleIndex::Initialize(base::Time cache_mtime) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  if (app_status_listener_getter_) {
    base::android::ApplicationStatusListener* listener =
        app_status_listener_getter_.Run();
    if (listener) {
      listener->SetCallback(
          base::BindRepeating(&SimpleIndex::OnApplicationStateChange,
                              weak_ptr_factory_.GetWeakPtr()));
    }
    // Not using the fallback on purpose here --- if the getter is set, we may
    // be in a process where the base::android::ApplicationStatusListener::New
    // impl is unavailable.
    // (See https://crbug.com/881572)
  } else if (base::android::IsJavaAvailable()) {
    owned_app_status_listener_ = base::android::ApplicationStatusListener::New(
        base::BindRepeating(&SimpleIndex::OnApplicationStateChange,
                            weak_ptr_factory_.GetWeakPtr()));
  }
#endif

  auto load_result = std::make_unique<SimpleIndexLoadResult>();
  auto* load_result_ptr = load_result.get();
  index_file_->LoadIndexEntries(
      cache_mtime,
      base::BindOnce(&SimpleIndex::MergeInitializingSet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(load_result)),
      load_result_ptr);
}

void SimpleIndex::SetMaxSize(uint64_t max_bytes) {
  // Zero size means use the default.
  if (max_bytes) {
    max_size_ = max_bytes;
    high_watermark_ = max_size_ - max_size_ / kEvictionMarginDivisor;
    low_watermark_ = max_size_ - 2 * (max_size_ / kEvictionMarginDivisor);
  }
}

void SimpleIndex::ExecuteWhenReady(net::CompletionOnceCallback task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_)
    task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(task), net::OK));
  else
    to_run_when_initialized_.push_back(std::move(task));
}

std::unique_ptr<SimpleIndex::HashList> SimpleIndex::GetEntriesBetween(
    base::Time initial_time,
    base::Time end_time) {
  DCHECK_EQ(true, initialized_);

  // The net::APP_CACHE mode does not track access times.  Assert that external
  // consumers are not relying on access time ranges.
  DCHECK(cache_type_ != net::APP_CACHE ||
         (initial_time.is_null() && end_time.is_null()));

  if (!initial_time.is_null())
    initial_time -= EntryMetadata::GetLowerEpsilonForTimeComparisons();
  if (end_time.is_null())
    end_time = base::Time::Max();
  else
    end_time += EntryMetadata::GetUpperEpsilonForTimeComparisons();
  DCHECK(end_time >= initial_time);

  auto ret_hashes = std::make_unique<HashList>();
  for (const auto& entry : entries_set_) {
    const EntryMetadata& metadata = entry.second;
    base::Time entry_time = metadata.GetLastUsedTime();
    if (initial_time <= entry_time && entry_time < end_time)
      ret_hashes->push_back(entry.first);
  }
  return ret_hashes;
}

std::unique_ptr<SimpleIndex::HashList> SimpleIndex::GetAllHashes() {
  return GetEntriesBetween(base::Time(), base::Time());
}

int32_t SimpleIndex::GetEntryCount() const {
  // TODO(pasko): return a meaningful initial estimate before initialized.
  return entries_set_.size();
}

uint64_t SimpleIndex::GetCacheSize() const {
  DCHECK(initialized_);
  return cache_size_;
}

uint64_t SimpleIndex::GetCacheSizeBetween(base::Time initial_time,
                                          base::Time end_time) const {
  DCHECK_EQ(true, initialized_);

  if (!initial_time.is_null())
    initial_time -= EntryMetadata::GetLowerEpsilonForTimeComparisons();
  if (end_time.is_null())
    end_time = base::Time::Max();
  else
    end_time += EntryMetadata::GetUpperEpsilonForTimeComparisons();

  DCHECK(end_time >= initial_time);
  uint64_t size = 0;
  for (const auto& entry : entries_set_) {
    const EntryMetadata& metadata = entry.second;
    base::Time entry_time = metadata.GetLastUsedTime();
    if (initial_time <= entry_time && entry_time < end_time)
      size += metadata.GetEntrySize();
  }
  return size;
}

base::Time SimpleIndex::GetLastUsedTime(uint64_t entry_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(cache_type_, net::APP_CACHE);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return base::Time();
  return it->second.GetLastUsedTime();
}

void SimpleIndex::SetLastUsedTimeForTest(uint64_t entry_hash,
                                         const base::Time last_used) {
  auto it = entries_set_.find(entry_hash);
  CHECK(it != entries_set_.end());
  it->second.SetLastUsedTime(last_used);
}

bool SimpleIndex::HasPendingWrite() const {
  return write_to_disk_timer_.IsRunning();
}

void SimpleIndex::Insert(uint64_t entry_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Upon insert we don't know yet the size of the entry.
  // It will be updated later when the SimpleEntryImpl finishes opening or
  // creating the new entry, and then UpdateEntrySize will be called.
  bool inserted = false;
  if (cache_type_ == net::APP_CACHE) {
    inserted =
        InsertInEntrySet(entry_hash, EntryMetadata(-1, 0u), &entries_set_);
  } else {
    inserted = InsertInEntrySet(
        entry_hash, EntryMetadata(base::Time::Now(), 0u), &entries_set_);
  }
  if (!initialized_)
    removed_entries_.erase(entry_hash);
  if (inserted)
    PostponeWritingToDisk();
}

void SimpleIndex::Remove(uint64_t entry_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool need_write = false;
  auto it = entries_set_.find(entry_hash);
  if (it != entries_set_.end()) {
    UpdateEntryIteratorSize(&it, 0u);
    entries_set_.erase(it);
    need_write = true;
  }

  if (!initialized_)
    removed_entries_.insert(entry_hash);

  if (need_write)
    PostponeWritingToDisk();
}

bool SimpleIndex::Has(uint64_t hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If not initialized, always return true, forcing it to go to the disk.
  return !initialized_ || entries_set_.count(hash) > 0;
}

uint8_t SimpleIndex::GetEntryInMemoryData(uint64_t entry_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return 0;
  return it->second.GetInMemoryData();
}

void SimpleIndex::SetEntryInMemoryData(uint64_t entry_hash, uint8_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return;
  return it->second.SetInMemoryData(value);
}

bool SimpleIndex::UseIfExists(uint64_t entry_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always update the last used time, even if it is during initialization.
  // It will be merged later.
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    // If not initialized, always return true, forcing it to go to the disk.
    return !initialized_;
  // We do not need to track access times in APP_CACHE mode.
  if (cache_type_ == net::APP_CACHE)
    return true;
  it->second.SetLastUsedTime(base::Time::Now());
  PostponeWritingToDisk();
  return true;
}

void SimpleIndex::StartEvictionIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (eviction_in_progress_ || cache_size_ <= high_watermark_)
    return;
  // Take all live key hashes from the index and sort them by time.
  eviction_in_progress_ = true;
  eviction_start_time_ = base::TimeTicks::Now();

  const bool use_size_heuristic =
      (cache_type_ != net::GENERATED_BYTE_CODE_CACHE &&
       cache_type_ != net::GENERATED_WEBUI_BYTE_CODE_CACHE);

  // Flatten for sorting.
  std::vector<std::pair<uint64_t, const EntrySet::value_type*>> entries;
  entries.reserve(entries_set_.size());
  uint32_t now = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  for (EntrySet::const_iterator i = entries_set_.begin();
       i != entries_set_.end(); ++i) {
    const uint64_t time_since_last_used = now - i->second.RawTimeForSorting();
    uint64_t sort_value = time_since_last_used;
    // See crbug.com/736437 for context.
    //
    // Will not overflow since we're multiplying two 32-bit values and storing
    // them in a 64-bit variable.
    if (use_size_heuristic) {
      sort_value *= i->second.GetEntrySize() + kEstimatedEntryOverhead;
      // When prioritized caching is enabled, we want to evict entries that are
      // not prioritized before entries that are prioritized. So we divide the
      // sort value by the `caching_prioritization_factor`.
      if (prioritized_caching_enabled_ &&
          time_since_last_used < caching_prioritization_period_in_seconds_ &&
          (i->second.GetInMemoryData() & HINT_HIGH_PRIORITY) ==
              HINT_HIGH_PRIORITY) {
        sort_value /= caching_prioritization_factor_;
      }
    }
    // Subtract so we don't need a custom comparator.
    entries.emplace_back(std::numeric_limits<uint64_t>::max() - sort_value,
                         &*i);
  }

  uint64_t evicted_so_far_size = 0;
  const uint64_t amount_to_evict = cache_size_ - low_watermark_;
  std::vector<uint64_t> entry_hashes;
  std::sort(entries.begin(), entries.end());
  for (const auto& score_metadata_pair : entries) {
    if (evicted_so_far_size >= amount_to_evict)
      break;
    evicted_so_far_size += score_metadata_pair.second->second.GetEntrySize();
    entry_hashes.push_back(score_metadata_pair.second->first);
  }

  SIMPLE_CACHE_UMA(COUNTS_1M,
                   "Eviction.EntryCount", cache_type_, entry_hashes.size());
  SIMPLE_CACHE_UMA(TIMES,
                   "Eviction.TimeToSelectEntries", cache_type_,
                   base::TimeTicks::Now() - eviction_start_time_);

  delegate_->DoomEntries(&entry_hashes,
                         base::BindOnce(&SimpleIndex::EvictionDone,
                                        weak_ptr_factory_.GetWeakPtr()));
}

int32_t SimpleIndex::GetTrailerPrefetchSize(uint64_t entry_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(cache_type_, net::APP_CACHE);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return -1;
  return it->second.GetTrailerPrefetchSize();
}

void SimpleIndex::SetTrailerPrefetchSize(uint64_t entry_hash, int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(cache_type_, net::APP_CACHE);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return;
  uint32_t original_size = it->second.GetTrailerPrefetchSize();
  it->second.SetTrailerPrefetchSize(size);
  if (original_size != it->second.GetTrailerPrefetchSize()) {
    PostponeWritingToDisk();
  }
}

bool SimpleIndex::UpdateEntrySize(uint64_t entry_hash,
                                  base::StrictNumeric<uint64_t> entry_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = entries_set_.find(entry_hash);
  if (it == entries_set_.end())
    return false;

  // Update the entry size.  If there was no change, then there is nothing
  // else to do here.
  if (!UpdateEntryIteratorSize(&it, entry_size))
    return true;

  PostponeWritingToDisk();
  StartEvictionIfNeeded();
  return true;
}

void SimpleIndex::EvictionDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore the result of eviction. We did our best.
  eviction_in_progress_ = false;
  SIMPLE_CACHE_UMA(TIMES,
                   "Eviction.TimeToDone", cache_type_,
                   base::TimeTicks::Now() - eviction_start_time_);
}

// static
bool SimpleIndex::InsertInEntrySet(
    uint64_t entry_hash,
    const disk_cache::EntryMetadata& entry_metadata,
    EntrySet* entry_set) {
  DCHECK(entry_set);
  auto result = entry_set->emplace(entry_hash, entry_metadata);
  return result.second;
}

void SimpleIndex::InsertEntryForTesting(uint64_t entry_hash,
                                        const EntryMetadata& entry_metadata) {
  DCHECK(entries_set_.find(entry_hash) == entries_set_.end());
  if (InsertInEntrySet(entry_hash, entry_metadata, &entries_set_))
    cache_size_ += entry_metadata.GetEntrySize();
}

void SimpleIndex::PostponeWritingToDisk() {
  if (!initialized_)
    return;
  const int delay = app_on_background_ ? kWriteToDiskOnBackgroundDelayMSecs
                                       : kWriteToDiskDelayMSecs;
  // If the timer is already active, Start() will just Reset it, postponing it.
  write_to_disk_timer_.Start(FROM_HERE, base::Milliseconds(delay),
                             write_to_disk_cb_);
}

bool SimpleIndex::UpdateEntryIteratorSize(
    EntrySet::iterator* it,
    base::StrictNumeric<uint64_t> entry_size) {
  // Update the total cache size with the new entry size.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(cache_size_, (*it)->second.GetEntrySize());
  uint64_t original_size = (*it)->second.GetEntrySize();

  // If SetEntrySize fails, we cannot update the entry iterator correctly.
  if (!(*it)->second.SetEntrySize(entry_size)) {
    LOG(ERROR) << "Could not set the given entry size as it is too large: "
               << static_cast<uint64_t>(entry_size);
    return false;
  }

  cache_size_ -= original_size;
  // We use GetEntrySize to get consistent rounding.
  cache_size_ += (*it)->second.GetEntrySize();
  // Return true if the size of the entry actually changed.  Make sure to
  // compare the rounded values provided by GetEntrySize().
  return original_size != (*it)->second.GetEntrySize();
}

void SimpleIndex::MergeInitializingSet(
    std::unique_ptr<SimpleIndexLoadResult> load_result) {
  TRACE_EVENT("disk_cache", "SimpleIndex::MergeInitializingSet", "cache_type_",
              cache_type_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EntrySet* index_file_entries = &load_result->entries;

  for (uint64_t removed_entry : removed_entries_) {
    index_file_entries->erase(removed_entry);
  }
  removed_entries_.clear();

  for (const auto& it : entries_set_) {
    const uint64_t entry_hash = it.first;
    std::pair<EntrySet::iterator, bool> insert_result =
        index_file_entries->insert(EntrySet::value_type(entry_hash,
                                                        EntryMetadata()));
    EntrySet::iterator& possibly_inserted_entry = insert_result.first;
    possibly_inserted_entry->second = it.second;
  }

  uint64_t merged_cache_size = 0;
  for (const auto& index_file_entry : *index_file_entries) {
    merged_cache_size += index_file_entry.second.GetEntrySize();
  }

  entries_set_.swap(*index_file_entries);
  cache_size_ = merged_cache_size;
  initialized_ = true;
  init_method_ = load_result->init_method;

  // The actual IO is asynchronous, so calling WriteToDisk() shouldn't slow the
  // merge down much.
  if (load_result->flush_required)
    WriteToDisk(INDEX_WRITE_REASON_STARTUP_MERGE);

  SIMPLE_CACHE_UMA(CUSTOM_COUNTS, "IndexNumEntriesOnInit2", cache_type_,
                   entries_set_.size(), 0, 1000000, 50);
  SIMPLE_CACHE_UMA(
      MEMORY_MEDIUM_MB, "CacheSizeOnInit2", cache_type_,
      static_cast<base::HistogramBase::Sample32>(cache_size_ / kBytesInMiB));
  SIMPLE_CACHE_UMA(
      MEMORY_MEDIUM_MB, "MaxCacheSizeOnInit2", cache_type_,
      static_cast<base::HistogramBase::Sample32>(max_size_ / kBytesInMiB));

  // Run all callbacks waiting for the index to come up.
  for (auto& callback : to_run_when_initialized_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), net::OK));
  }
  to_run_when_initialized_.clear();
}

#if BUILDFLAG(IS_ANDROID)
void SimpleIndex::OnApplicationStateChange(
    base::android::ApplicationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // For more info about android activities, see:
  // developer.android.com/training/basics/activity-lifecycle/pausing.html
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    app_on_background_ = false;
  } else if (state ==
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {
    app_on_background_ = true;
    WriteToDisk(INDEX_WRITE_REASON_ANDROID_STOPPED);
  }
}
#endif

void SimpleIndex::WriteToDisk(IndexWriteToDiskReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initialized_)
    return;

  // Cancel any pending writes since we are about to write to disk now.
  write_to_disk_timer_.Stop();

  base::OnceClosure after_write;
  if (cleanup_tracker_) {
    // Make anyone synchronizing with our cleanup wait for the index to be
    // written back.
    after_write = base::DoNothingWithBoundArgs(cleanup_tracker_);
  }

  index_file_->WriteToDisk(cache_type_, reason, entries_set_, cache_size_,
                           std::move(after_write));
}

}  // namespace disk_cache

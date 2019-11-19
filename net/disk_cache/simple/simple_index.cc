// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index_delegate.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#include <sys/time.h>
#endif

namespace {

// How many milliseconds we delay writing the index to disk since the last cache
// operation has happened.
const int kWriteToDiskDelayMSecs = 20000;
const int kWriteToDiskOnBackgroundDelayMSecs = 100;

// Divides the cache space into this amount of parts to evict when only one part
// is left.
const uint32_t kEvictionMarginDivisor = 20;

const uint32_t kBytesInKb = 1024;

// This is added to the size of each entry before using the size
// to determine which entries to evict first. It's basically an
// estimate of the filesystem overhead, but it also serves to flatten
// the curve so that 1-byte entries and 2-byte entries are basically
// treated the same.
static const int kEstimatedEntryOverhead = 512;

}  // namespace

namespace disk_cache {

const base::Feature kSimpleCacheEvictionWithSize = {
    "SimpleCacheEvictionWithSize", base::FEATURE_ENABLED_BY_DEFAULT};

EntryMetadata::EntryMetadata()
    : last_used_time_seconds_since_epoch_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {}

EntryMetadata::EntryMetadata(base::Time last_used_time,
                             base::StrictNumeric<uint32_t> entry_size)
    : last_used_time_seconds_since_epoch_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {
  SetEntrySize(entry_size);  // to round/pack properly.
  SetLastUsedTime(last_used_time);
}

EntryMetadata::EntryMetadata(int32_t trailer_prefetch_size,
                             base::StrictNumeric<uint32_t> entry_size)
    : trailer_prefetch_size_(0),
      entry_size_256b_chunks_(0),
      in_memory_data_(0) {
  SetEntrySize(entry_size);  // to round/pack properly
  SetTrailerPrefetchSize(trailer_prefetch_size);
}

base::Time EntryMetadata::GetLastUsedTime() const {
  // Preserve nullity.
  if (last_used_time_seconds_since_epoch_ == 0)
    return base::Time();

  return base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(last_used_time_seconds_since_epoch_);
}

void EntryMetadata::SetLastUsedTime(const base::Time& last_used_time) {
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

int32_t EntryMetadata::GetTrailerPrefetchSize() const {
  return trailer_prefetch_size_;
}

void EntryMetadata::SetTrailerPrefetchSize(int32_t size) {
  if (size <= 0)
    return;
  trailer_prefetch_size_ = size;
}

uint32_t EntryMetadata::GetEntrySize() const {
  return entry_size_256b_chunks_ << 8;
}

void EntryMetadata::SetEntrySize(base::StrictNumeric<uint32_t> entry_size) {
  // This should not overflow since we limit entries to 1/8th of the cache.
  entry_size_256b_chunks_ = (static_cast<uint32_t>(entry_size) + 255) >> 8;
}

void EntryMetadata::Serialize(net::CacheType cache_type,
                              base::Pickle* pickle) const {
  DCHECK(pickle);
  // If you modify the size of the size of the pickle, be sure to update
  // kOnDiskSizeBytes.
  uint32_t packed_entry_info = (entry_size_256b_chunks_ << 8) | in_memory_data_;
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
                                bool has_entry_in_memory_data,
                                bool app_cache_has_trailer_prefetch_size) {
  DCHECK(it);
  int64_t tmp_time_or_prefetch_size;
  uint64_t tmp_entry_size;
  if (!it->ReadInt64(&tmp_time_or_prefetch_size) ||
      !it->ReadUInt64(&tmp_entry_size) ||
      tmp_entry_size > std::numeric_limits<uint32_t>::max())
    return false;
  if (cache_type == net::APP_CACHE) {
    if (app_cache_has_trailer_prefetch_size) {
      int32_t trailer_prefetch_size = 0;
      base::CheckedNumeric<int32_t> numeric_size(tmp_time_or_prefetch_size);
      if (numeric_size.AssignIfValid(&trailer_prefetch_size)) {
        SetTrailerPrefetchSize(trailer_prefetch_size);
      }
    }
  } else {
    SetLastUsedTime(base::Time::FromInternalValue(tmp_time_or_prefetch_size));
  }
  if (has_entry_in_memory_data) {
    // tmp_entry_size actually packs entry_size_256b_chunks_ and
    // in_memory_data_.
    SetEntrySize(static_cast<uint32_t>(tmp_entry_size & 0xFFFFFF00));
    SetInMemoryData(static_cast<uint8_t>(tmp_entry_size & 0xFF));
  } else {
    SetEntrySize(static_cast<uint32_t>(tmp_entry_size));
    SetInMemoryData(0);
  }
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
      // Creating the callback once so it is reused every time
      // write_to_disk_timer_.Start() is called.
      write_to_disk_cb_(base::Bind(&SimpleIndex::WriteToDisk,
                                   AsWeakPtr(),
                                   INDEX_WRITE_REASON_IDLE)) {}

SimpleIndex::~SimpleIndex() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Fail all callbacks waiting for the index to come up.
  for (auto it = to_run_when_initialized_.begin(),
            end = to_run_when_initialized_.end();
       it != end; ++it) {
    std::move(*it).Run(net::ERR_ABORTED);
  }
}

void SimpleIndex::Initialize(base::Time cache_mtime) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if defined(OS_ANDROID)
  if (app_status_listener_) {
    app_status_listener_->SetCallback(base::BindRepeating(
        &SimpleIndex::OnApplicationStateChange, AsWeakPtr()));
  } else if (base::android::IsVMInitialized()) {
    owned_app_status_listener_ =
        base::android::ApplicationStatusListener::New(base::BindRepeating(
            &SimpleIndex::OnApplicationStateChange, AsWeakPtr()));
    app_status_listener_ = owned_app_status_listener_.get();
  }
#endif

  SimpleIndexLoadResult* load_result = new SimpleIndexLoadResult();
  std::unique_ptr<SimpleIndexLoadResult> load_result_scoped(load_result);
  base::Closure reply = base::Bind(
      &SimpleIndex::MergeInitializingSet,
      AsWeakPtr(),
      base::Passed(&load_result_scoped));
  index_file_->LoadIndexEntries(cache_mtime, reply, load_result);
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

  std::unique_ptr<HashList> ret_hashes(new HashList());
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

size_t SimpleIndex::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(entries_set_) +
         base::trace_event::EstimateMemoryUsage(removed_entries_);
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
  DCHECK(it != entries_set_.end());
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
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "Eviction.CacheSizeOnStart2", cache_type_,
      static_cast<base::HistogramBase::Sample>(cache_size_ / kBytesInKb));
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "Eviction.MaxCacheSizeOnStart2", cache_type_,
      static_cast<base::HistogramBase::Sample>(max_size_ / kBytesInKb));

  // Flatten for sorting.
  std::vector<std::pair<uint64_t, const EntrySet::value_type*>> entries;
  entries.reserve(entries_set_.size());
  uint32_t now = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  bool use_size = base::FeatureList::IsEnabled(kSimpleCacheEvictionWithSize);
  for (EntrySet::const_iterator i = entries_set_.begin();
       i != entries_set_.end(); ++i) {
    uint64_t sort_value = now - i->second.RawTimeForSorting();
    if (use_size) {
      // Will not overflow since we're multiplying two 32-bit values and storing
      // them in a 64-bit variable.
      sort_value *= i->second.GetEntrySize() + kEstimatedEntryOverhead;
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
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "Eviction.SizeOfEvicted2", cache_type_,
      static_cast<base::HistogramBase::Sample>(
          evicted_so_far_size / kBytesInKb));

  delegate_->DoomEntries(
      &entry_hashes, base::BindOnce(&SimpleIndex::EvictionDone, AsWeakPtr()));
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
  int32_t original_size = it->second.GetTrailerPrefetchSize();
  it->second.SetTrailerPrefetchSize(size);
  if (original_size != it->second.GetTrailerPrefetchSize())
    PostponeWritingToDisk();
}

bool SimpleIndex::UpdateEntrySize(uint64_t entry_hash,
                                  base::StrictNumeric<uint32_t> entry_size) {
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
  SIMPLE_CACHE_UMA(BOOLEAN, "Eviction.Result", cache_type_, result == net::OK);
  SIMPLE_CACHE_UMA(TIMES,
                   "Eviction.TimeToDone", cache_type_,
                   base::TimeTicks::Now() - eviction_start_time_);
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "Eviction.SizeWhenDone2", cache_type_,
      static_cast<base::HistogramBase::Sample>(cache_size_ / kBytesInKb));
}

// static
bool SimpleIndex::InsertInEntrySet(
    uint64_t entry_hash,
    const disk_cache::EntryMetadata& entry_metadata,
    EntrySet* entry_set) {
  DCHECK(entry_set);
  auto result = entry_set->insert(std::make_pair(entry_hash, entry_metadata));
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
  write_to_disk_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(delay), write_to_disk_cb_);
}

bool SimpleIndex::UpdateEntryIteratorSize(
    EntrySet::iterator* it,
    base::StrictNumeric<uint32_t> entry_size) {
  // Update the total cache size with the new entry size.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(cache_size_, (*it)->second.GetEntrySize());
  uint32_t original_size = (*it)->second.GetEntrySize();
  cache_size_ -= (*it)->second.GetEntrySize();
  (*it)->second.SetEntrySize(entry_size);
  // We use GetEntrySize to get consistent rounding.
  cache_size_ += (*it)->second.GetEntrySize();
  // Return true if the size of the entry actually changed.  Make sure to
  // compare the rounded values provided by GetEntrySize().
  return original_size != (*it)->second.GetEntrySize();
}

void SimpleIndex::MergeInitializingSet(
    std::unique_ptr<SimpleIndexLoadResult> load_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EntrySet* index_file_entries = &load_result->entries;

  for (auto it = removed_entries_.begin(); it != removed_entries_.end(); ++it) {
    index_file_entries->erase(*it);
  }
  removed_entries_.clear();

  for (EntrySet::const_iterator it = entries_set_.begin();
       it != entries_set_.end(); ++it) {
    const uint64_t entry_hash = it->first;
    std::pair<EntrySet::iterator, bool> insert_result =
        index_file_entries->insert(EntrySet::value_type(entry_hash,
                                                        EntryMetadata()));
    EntrySet::iterator& possibly_inserted_entry = insert_result.first;
    possibly_inserted_entry->second = it->second;
  }

  uint64_t merged_cache_size = 0;
  for (auto it = index_file_entries->begin(); it != index_file_entries->end();
       ++it) {
    merged_cache_size += it->second.GetEntrySize();
  }

  entries_set_.swap(*index_file_entries);
  cache_size_ = merged_cache_size;
  initialized_ = true;
  init_method_ = load_result->init_method;

  // The actual IO is asynchronous, so calling WriteToDisk() shouldn't slow the
  // merge down much.
  if (load_result->flush_required)
    WriteToDisk(INDEX_WRITE_REASON_STARTUP_MERGE);

  SIMPLE_CACHE_UMA(CUSTOM_COUNTS,
                   "IndexInitializationWaiters", cache_type_,
                   to_run_when_initialized_.size(), 0, 100, 20);
  SIMPLE_CACHE_UMA(CUSTOM_COUNTS, "IndexNumEntriesOnInit", cache_type_,
                   entries_set_.size(), 0, 100000, 50);
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "CacheSizeOnInit", cache_type_,
      static_cast<base::HistogramBase::Sample>(cache_size_ / kBytesInKb));
  SIMPLE_CACHE_UMA(
      MEMORY_KB, "MaxCacheSizeOnInit", cache_type_,
      static_cast<base::HistogramBase::Sample>(max_size_ / kBytesInKb));
  if (max_size_ > 0) {
    SIMPLE_CACHE_UMA(PERCENTAGE, "PercentFullOnInit", cache_type_,
                     static_cast<base::HistogramBase::Sample>(
                         (cache_size_ * 100) / max_size_));
  }

  // Run all callbacks waiting for the index to come up.
  for (auto it = to_run_when_initialized_.begin(),
            end = to_run_when_initialized_.end();
       it != end; ++it) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(*it), net::OK));
  }
  to_run_when_initialized_.clear();
}

#if defined(OS_ANDROID)
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
  write_to_disk_timer_.AbandonAndStop();

  SIMPLE_CACHE_UMA(CUSTOM_COUNTS,
                   "IndexNumEntriesOnWrite", cache_type_,
                   entries_set_.size(), 0, 100000, 50);
  const base::TimeTicks start = base::TimeTicks::Now();
  if (!last_write_to_disk_.is_null()) {
    if (app_on_background_) {
      SIMPLE_CACHE_UMA(MEDIUM_TIMES,
                       "IndexWriteInterval.Background", cache_type_,
                       start - last_write_to_disk_);
    } else {
      SIMPLE_CACHE_UMA(MEDIUM_TIMES,
                       "IndexWriteInterval.Foreground", cache_type_,
                       start - last_write_to_disk_);
    }
  }
  last_write_to_disk_ = start;

  base::Closure after_write;
  if (cleanup_tracker_) {
    // Make anyone synchronizing with our cleanup wait for the index to be
    // written back.
    after_write = base::Bind(
        base::DoNothing::Repeatedly<scoped_refptr<BackendCleanupTracker>>(),
        cleanup_tracker_);
  }

  index_file_->WriteToDisk(cache_type_, reason, entries_set_, cache_size_,
                           start, app_on_background_, after_write);
}

}  // namespace disk_cache

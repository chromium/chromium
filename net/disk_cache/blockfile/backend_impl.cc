// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/blockfile/backend_impl.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/base/tracing.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/blockfile/errors.h"
#include "net/disk_cache/blockfile/experiments.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/cache_util.h"

using base::Time;
using base::TimeTicks;

namespace {

const char kIndexName[] = "index";

// Seems like ~240 MB correspond to less than 50k entries for 99% of the people.
// Note that the actual target is to keep the index table load factor under 55%
// for most users.
const int k64kEntriesStore = 240 * 1000 * 1000;
const int kBaseTableLen = 64 * 1024;

// Avoid trimming the cache for the first 5 minutes (10 timer ticks).
const int kTrimDelay = 10;

int DesiredIndexTableLen(int32_t storage_size) {
  if (storage_size <= k64kEntriesStore)
    return kBaseTableLen;
  if (storage_size <= k64kEntriesStore * 2)
    return kBaseTableLen * 2;
  if (storage_size <= k64kEntriesStore * 4)
    return kBaseTableLen * 4;
  if (storage_size <= k64kEntriesStore * 8)
    return kBaseTableLen * 8;

  // The biggest storage_size for int32_t requires a 4 MB table.
  return kBaseTableLen * 16;
}

int MaxStorageSizeForTable(int table_len) {
  return table_len * (k64kEntriesStore / kBaseTableLen);
}

size_t GetIndexSize(int table_len) {
  size_t table_size = sizeof(disk_cache::CacheAddr) * table_len;
  return sizeof(disk_cache::IndexHeader) + table_size;
}

// ------------------------------------------------------------------------

// Sets group for the current experiment. Returns false if the files should be
// discarded.
bool InitExperiment(disk_cache::IndexHeader* header, bool cache_created) {
  if (header->experiment == disk_cache::EXPERIMENT_OLD_FILE1 ||
      header->experiment == disk_cache::EXPERIMENT_OLD_FILE2) {
    // Discard current cache.
    return false;
  }

  header->experiment = disk_cache::NO_EXPERIMENT;
  return true;
}

// A callback to perform final cleanup on the background thread.
void FinalCleanupCallback(disk_cache::BackendImpl* backend,
                          base::WaitableEvent* done) {
  backend->CleanupCache();
  done->Signal();
}

class CacheThread : public base::Thread {
 public:
  CacheThread() : base::Thread("CacheThread_BlockFile") {
    CHECK(
        StartWithOptions(base::Thread::Options(base::MessagePumpType::IO, 0)));
  }

  ~CacheThread() override {
    // We don't expect to be deleted, but call Stop() in dtor 'cause docs
    // say we should.
    Stop();
  }
};

static base::LazyInstance<CacheThread>::Leaky g_internal_cache_thread =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<base::SingleThreadTaskRunner> InternalCacheThread() {
  return g_internal_cache_thread.Get().task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner> FallbackToInternalIfNull(
    const scoped_refptr<base::SingleThreadTaskRunner>& cache_thread) {
  return cache_thread ? cache_thread : InternalCacheThread();
}

}  // namespace

// ------------------------------------------------------------------------

namespace disk_cache {

BackendImpl::BackendImpl(
    const base::FilePath& path,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& cache_thread,
    net::CacheType cache_type,
    net::NetLog* net_log)
    : Backend(cache_type),
      cleanup_tracker_(std::move(cleanup_tracker)),
      background_queue_(this, FallbackToInternalIfNull(cache_thread)),
      path_(path),
      block_files_(path),
      user_flags_(0),
      net_log_(net_log) {
  TRACE_EVENT0("disk_cache", "BackendImpl::BackendImpl");
}

BackendImpl::BackendImpl(
    const base::FilePath& path,
    uint32_t mask,
    const scoped_refptr<base::SingleThreadTaskRunner>& cache_thread,
    net::CacheType cache_type,
    net::NetLog* net_log)
    : Backend(cache_type),
      background_queue_(this, FallbackToInternalIfNull(cache_thread)),
      path_(path),
      block_files_(path),
      mask_(mask),
      user_flags_(kMask),
      net_log_(net_log) {
  TRACE_EVENT0("disk_cache", "BackendImpl::BackendImpl");
}

BackendImpl::~BackendImpl() {
  TRACE_EVENT0("disk_cache", "BackendImpl::~BackendImpl");
  if (user_flags_ & kNoRandom) {
    // This is a unit test, so we want to be strict about not leaking entries
    // and completing all the work.
    background_queue_.WaitForPendingIO();
  } else {
    // This is most likely not a test, so we want to do as little work as
    // possible at this time, at the price of leaving dirty entries behind.
    background_queue_.DropPendingIO();
  }

  if (background_queue_.BackgroundIsCurrentSequence()) {
    // Unit tests may use the same sequence for everything.
    CleanupCache();
  } else {
    // Signals the end of background work.
    base::WaitableEvent done;

    background_queue_.background_thread()->PostTask(
        FROM_HERE, base::BindOnce(&FinalCleanupCallback, base::Unretained(this),
                                  base::Unretained(&done)));
    // http://crbug.com/74623
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done.Wait();
  }
}

void BackendImpl::Init(CompletionOnceCallback callback) {
  background_queue_.Init(std::move(callback));
}

int BackendImpl::SyncInit() {
  TRACE_EVENT0("disk_cache", "BackendImpl::SyncInit");

#if defined(NET_BUILD_STRESS_CACHE)
  // Start evictions right away.
  up_ticks_ = kTrimDelay * 2;
#endif
  DCHECK(!init_);
  if (init_)
    return net::ERR_FAILED;

  bool create_files = false;
  if (!InitBackingStore(&create_files)) {
    ReportError(ERR_STORAGE_ERROR);
    return net::ERR_FAILED;
  }

  num_refs_ = num_pending_io_ = max_refs_ = 0;
  entry_count_ = byte_count_ = 0;

  bool should_create_timer = false;
  if (!restarted_) {
    buffer_bytes_ = 0;
    should_create_timer = true;
  }

  init_ = true;

  if (data_->header.experiment != NO_EXPERIMENT &&
      GetCacheType() != net::DISK_CACHE) {
    // No experiment for other caches.
    return net::ERR_FAILED;
  }

  if (!(user_flags_ & kNoRandom)) {
    // The unit test controls directly what to test.
    new_eviction_ = (GetCacheType() == net::DISK_CACHE);
  }

  if (!CheckIndex()) {
    ReportError(ERR_INIT_FAILED);
    return net::ERR_FAILED;
  }

  if (!restarted_ && (create_files || !data_->header.num_entries))
    ReportError(ERR_CACHE_CREATED);

  if (!(user_flags_ & kNoRandom) && GetCacheType() == net::DISK_CACHE &&
      !InitExperiment(&data_->header, create_files)) {
    return net::ERR_FAILED;
  }

  // We don't care if the value overflows. The only thing we care about is that
  // the id cannot be zero, because that value is used as "not dirty".
  // Increasing the value once per second gives us many years before we start
  // having collisions.
  data_->header.this_id++;
  if (!data_->header.this_id)
    data_->header.this_id++;

  bool previous_crash = (data_->header.crash != 0);
  data_->header.crash = 1;

  if (!block_files_.Init(create_files))
    return net::ERR_FAILED;

  // We want to minimize the changes to cache for an AppCache.
  if (GetCacheType() == net::APP_CACHE) {
    DCHECK(!new_eviction_);
    read_only_ = true;
  } else if (GetCacheType() == net::SHADER_CACHE) {
    DCHECK(!new_eviction_);
  }

  eviction_.Init(this);

  // stats_ and rankings_ may end up calling back to us so we better be enabled.
  disabled_ = false;
  if (!InitStats())
    return net::ERR_FAILED;

  disabled_ = !rankings_.Init(this, new_eviction_);

#if defined(STRESS_CACHE_EXTENDED_VALIDATION)
  trace_object_->EnableTracing(false);
  int sc = SelfCheck();
  if (sc < 0 && sc != ERR_NUM_ENTRIES_MISMATCH)
    NOTREACHED_IN_MIGRATION();
  trace_object_->EnableTracing(true);
#endif

  if (previous_crash) {
    ReportError(ERR_PREVIOUS_CRASH);
  } else if (!restarted_) {
    ReportError(ERR_NO_ERROR);
  }

  FlushIndex();

  if (!disabled_ && should_create_timer) {
    // Create a recurrent timer of 30 secs.
    DCHECK(background_queue_.BackgroundIsCurrentSequence());
    int timer_delay = unit_test_ ? 1000 : 30000;
    timer_ = std::make_unique<base::RepeatingTimer>();
    timer_->Start(FROM_HERE, base::Milliseconds(timer_delay), this,
                  &BackendImpl::OnStatsTimer);
  }

  return disabled_ ? net::ERR_FAILED : net::OK;
}

void BackendImpl::CleanupCache() {
  DCHECK(background_queue_.BackgroundIsCurrentSequence());
  TRACE_EVENT0("disk_cache", "BackendImpl::CleanupCache");

  eviction_.Stop();
  timer_.reset();

  if (init_) {
    StoreStats();
    if (data_)
      data_->header.crash = 0;

    if (user_flags_ & kNoRandom) {
      // This is a net_unittest, verify that we are not 'leaking' entries.
      // TODO(crbug.com/40171748): Refactor this and eliminate the
      //    WaitForPendingIOForTesting API.
      File::WaitForPendingIOForTesting(&num_pending_io_);
      DCHECK(!num_refs_);
    } else {
      File::DropPendingIO();
    }
  }
  block_files_.CloseFiles();
  FlushIndex();
  index_ = nullptr;
  ptr_factory_.InvalidateWeakPtrs();
}

// ------------------------------------------------------------------------

int BackendImpl::SyncOpenEntry(const std::string& key,
                               scoped_refptr<EntryImpl>* entry) {
  DCHECK(entry);
  *entry = OpenEntryImpl(key);
  return (*entry) ? net::OK : net::ERR_FAILED;
}

int BackendImpl::SyncCreateEntry(const std::string& key,
                                 scoped_refptr<EntryImpl>* entry) {
  DCHECK(entry);
  *entry = CreateEntryImpl(key);
  return (*entry) ? net::OK : net::ERR_FAILED;
}

int BackendImpl::SyncDoomEntry(const std::string& key) {
  if (disabled_)
    return net::ERR_FAILED;

  scoped_refptr<EntryImpl> entry = OpenEntryImpl(key);
  if (!entry)
    return net::ERR_FAILED;

  entry->DoomImpl();
  return net::OK;
}

int BackendImpl::SyncDoomAllEntries() {
  if (disabled_)
    return net::ERR_FAILED;

  // This is not really an error, but it is an interesting condition.
  ReportError(ERR_CACHE_DOOMED);
  stats_.OnEvent(Stats::DOOM_CACHE);
  if (!num_refs_) {
    RestartCache(false);
    return disabled_ ? net::ERR_FAILED : net::OK;
  } else {
    if (disabled_)
      return net::ERR_FAILED;

    eviction_.TrimCache(true);
    return net::OK;
  }
}

int BackendImpl::SyncDoomEntriesBetween(const base::Time initial_time,
                                        const base::Time end_time) {
  TRACE_EVENT0("disk_cache", "BackendImpl::SyncDoomEntriesBetween");

  DCHECK_NE(net::APP_CACHE, GetCacheType());
  if (end_time.is_null())
    return SyncDoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  if (disabled_)
    return net::ERR_FAILED;

  scoped_refptr<EntryImpl> node;
  auto iterator = std::make_unique<Rankings::Iterator>();
  scoped_refptr<EntryImpl> next = OpenNextEntryImpl(iterator.get());
  if (!next)
    return net::OK;

  while (next) {
    node = std::move(next);
    next = OpenNextEntryImpl(iterator.get());

    if (node->GetLastUsed() >= initial_time &&
        node->GetLastUsed() < end_time) {
      node->DoomImpl();
    } else if (node->GetLastUsed() < initial_time) {
      next = nullptr;
      SyncEndEnumeration(std::move(iterator));
    }
  }

  return net::OK;
}

int BackendImpl::SyncCalculateSizeOfAllEntries() {
  TRACE_EVENT0("disk_cache", "BackendImpl::SyncCalculateSizeOfAllEntries");

  DCHECK_NE(net::APP_CACHE, GetCacheType());
  if (disabled_)
    return net::ERR_FAILED;

  return data_->header.num_bytes;
}

// We use OpenNextEntryImpl to retrieve elements from the cache, until we get
// entries that are too old.
int BackendImpl::SyncDoomEntriesSince(const base::Time initial_time) {
  TRACE_EVENT0("disk_cache", "BackendImpl::SyncDoomEntriesSince");

  DCHECK_NE(net::APP_CACHE, GetCacheType());
  if (disabled_)
    return net::ERR_FAILED;

  stats_.OnEvent(Stats::DOOM_RECENT);
  for (;;) {
    auto iterator = std::make_unique<Rankings::Iterator>();
    scoped_refptr<EntryImpl> entry = OpenNextEntryImpl(iterator.get());
    if (!entry)
      return net::OK;

    if (initial_time > entry->GetLastUsed()) {
      entry = nullptr;
      SyncEndEnumeration(std::move(iterator));
      return net::OK;
    }

    entry->DoomImpl();
    entry = nullptr;
    SyncEndEnumeration(
        std::move(iterator));  // The doom invalidated the iterator.
  }
}

int BackendImpl::SyncOpenNextEntry(Rankings::Iterator* iterator,
                                   scoped_refptr<EntryImpl>* next_entry) {
  TRACE_EVENT0("disk_cache", "BackendImpl::SyncOpenNextEntry");

  *next_entry = OpenNextEntryImpl(iterator);
  return (*next_entry) ? net::OK : net::ERR_FAILED;
}

void BackendImpl::SyncEndEnumeration(
    std::unique_ptr<Rankings::Iterator> iterator) {
  iterator->Reset();
}

void BackendImpl::SyncOnExternalCacheHit(const std::string& key) {
  if (disabled_)
    return;

  uint32_t hash = base::PersistentHash(key);
  bool error;
  scoped_refptr<EntryImpl> cache_entry =
      MatchEntry(key, hash, false, Addr(), &error);
  if (cache_entry && ENTRY_NORMAL == cache_entry->entry()->Data()->state)
    UpdateRank(cache_entry.get(), GetCacheType() == net::SHADER_CACHE);
}

scoped_refptr<EntryImpl> BackendImpl::OpenEntryImpl(const std::string& key) {
  TRACE_EVENT0("disk_cache", "BackendImpl::OpenEntryImpl");

  if (disabled_)
    return nullptr;

  uint32_t hash = base::PersistentHash(key);

  bool error;
  scoped_refptr<EntryImpl> cache_entry =
      MatchEntry(key, hash, false, Addr(), &error);
  if (cache_entry && ENTRY_NORMAL != cache_entry->entry()->Data()->state) {
    // The entry was already evicted.
    cache_entry = nullptr;
  }

  if (!cache_entry) {
    stats_.OnEvent(Stats::OPEN_MISS);
    return nullptr;
  }

  eviction_.OnOpenEntry(cache_entry.get());
  entry_count_++;

  stats_.OnEvent(Stats::OPEN_HIT);
  return cache_entry;
}

scoped_refptr<EntryImpl> BackendImpl::CreateEntryImpl(const std::string& key) {
  TRACE_EVENT0("disk_cache", "BackendImpl::CreateEntryImpl");

  if (disabled_ || key.empty())
    return nullptr;

  uint32_t hash = base::PersistentHash(key);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    // We have an entry already. It could be the one we are looking for, or just
    // a hash conflict.
    bool error;
    scoped_refptr<EntryImpl> old_entry =
        MatchEntry(key, hash, false, Addr(), &error);
    if (old_entry)
      return ResurrectEntry(std::move(old_entry));

    parent = MatchEntry(key, hash, true, Addr(), &error);
    DCHECK(!error);
    if (!parent && data_->table[hash & mask_]) {
      // We should have corrected the problem.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
  }

  // The general flow is to allocate disk space and initialize the entry data,
  // followed by saving that to disk, then linking the entry though the index
  // and finally through the lists. If there is a crash in this process, we may
  // end up with:
  // a. Used, unreferenced empty blocks on disk (basically just garbage).
  // b. Used, unreferenced but meaningful data on disk (more garbage).
  // c. A fully formed entry, reachable only through the index.
  // d. A fully formed entry, also reachable through the lists, but still dirty.
  //
  // Anything after (b) can be automatically cleaned up. We may consider saving
  // the current operation (as we do while manipulating the lists) so that we
  // can detect and cleanup (a) and (b).

  int num_blocks = EntryImpl::NumBlocksForEntry(key.size());
  if (!block_files_.CreateBlock(BLOCK_256, num_blocks, &entry_address)) {
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return nullptr;
  }

  Addr node_address(0);
  if (!block_files_.CreateBlock(RANKINGS, 1, &node_address)) {
    block_files_.DeleteBlock(entry_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return nullptr;
  }

  auto cache_entry =
      base::MakeRefCounted<EntryImpl>(this, entry_address, false);
  IncreaseNumRefs();

  if (!cache_entry->CreateEntry(node_address, key, hash)) {
    block_files_.DeleteBlock(entry_address, false);
    block_files_.DeleteBlock(node_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return nullptr;
  }

  cache_entry->BeginLogging(net_log_, true);

  // We are not failing the operation; let's add this to the map.
  open_entries_[entry_address.value()] = cache_entry.get();

  // Save the entry.
  cache_entry->entry()->Store();
  cache_entry->rankings()->Store();
  IncreaseNumEntries();
  entry_count_++;

  // Link this entry through the index.
  if (parent.get()) {
    parent->SetNextAddress(entry_address);
  } else {
    data_->table[hash & mask_] = entry_address.value();
  }

  // Link this entry through the lists.
  eviction_.OnCreateEntry(cache_entry.get());

  stats_.OnEvent(Stats::CREATE_HIT);
  FlushIndex();
  return cache_entry;
}

scoped_refptr<EntryImpl> BackendImpl::OpenNextEntryImpl(
    Rankings::Iterator* iterator) {
  if (disabled_)
    return nullptr;

  const int kListsToSearch = 3;
  scoped_refptr<EntryImpl> entries[kListsToSearch];
  if (!iterator->my_rankings) {
    iterator->my_rankings = &rankings_;
    bool ret = false;

    // Get an entry from each list.
    for (int i = 0; i < kListsToSearch; i++) {
      ret |= OpenFollowingEntryFromList(static_cast<Rankings::List>(i),
                                        &iterator->nodes[i], &entries[i]);
    }
    if (!ret) {
      iterator->Reset();
      return nullptr;
    }
  } else {
    // Get the next entry from the last list, and the actual entries for the
    // elements on the other lists.
    for (int i = 0; i < kListsToSearch; i++) {
      if (iterator->list == i) {
        OpenFollowingEntryFromList(iterator->list, &iterator->nodes[i],
                                   &entries[i]);
      } else {
        entries[i] = GetEnumeratedEntry(iterator->nodes[i],
                                        static_cast<Rankings::List>(i));
      }
    }
  }

  int newest = -1;
  int oldest = -1;
  Time access_times[kListsToSearch];
  for (int i = 0; i < kListsToSearch; i++) {
    if (entries[i].get()) {
      access_times[i] = entries[i]->GetLastUsed();
      if (newest < 0) {
        DCHECK_LT(oldest, 0);
        newest = oldest = i;
        continue;
      }
      if (access_times[i] > access_times[newest])
        newest = i;
      if (access_times[i] < access_times[oldest])
        oldest = i;
    }
  }

  if (newest < 0 || oldest < 0) {
    iterator->Reset();
    return nullptr;
  }

  scoped_refptr<EntryImpl> next_entry = entries[newest];
  iterator->list = static_cast<Rankings::List>(newest);
  return next_entry;
}

bool BackendImpl::SetMaxSize(int64_t max_bytes) {
  if (max_bytes < 0 || max_bytes > std::numeric_limits<int>::max())
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  // Avoid a DCHECK later on.
  if (max_bytes >= std::numeric_limits<int32_t>::max() -
                       std::numeric_limits<int32_t>::max() / 10) {
    max_bytes = std::numeric_limits<int32_t>::max() -
                std::numeric_limits<int32_t>::max() / 10 - 1;
  }

  user_flags_ |= kMaxSize;
  max_size_ = max_bytes;
  return true;
}

base::FilePath BackendImpl::GetFileName(Addr address) const {
  if (!address.is_separate_file() || !address.is_initialized()) {
    NOTREACHED_IN_MIGRATION();
    return base::FilePath();
  }

  std::string tmp = base::StringPrintf("f_%06x", address.FileNumber());
  return path_.AppendASCII(tmp);
}

MappedFile* BackendImpl::File(Addr address) {
  if (disabled_)
    return nullptr;
  return block_files_.GetFile(address);
}

base::WeakPtr<InFlightBackendIO> BackendImpl::GetBackgroundQueue() {
  return background_queue_.GetWeakPtr();
}

bool BackendImpl::CreateExternalFile(Addr* address) {
  TRACE_EVENT0("disk_cache", "BackendImpl::CreateExternalFile");
  int file_number = data_->header.last_file + 1;
  Addr file_address(0);
  bool success = false;
  for (int i = 0; i < 0x0fffffff; i++, file_number++) {
    if (!file_address.SetFileNumber(file_number)) {
      file_number = 1;
      continue;
    }
    base::FilePath name = GetFileName(file_address);
    int flags = base::File::FLAG_READ | base::File::FLAG_WRITE |
                base::File::FLAG_CREATE | base::File::FLAG_WIN_EXCLUSIVE_WRITE;
    base::File file(name, flags);
    if (!file.IsValid()) {
      base::File::Error error = file.error_details();
      if (error != base::File::FILE_ERROR_EXISTS) {
        LOG(ERROR) << "Unable to create file: " << error;
        return false;
      }
      continue;
    }

    success = true;
    break;
  }

  DCHECK(success);
  if (!success)
    return false;

  data_->header.last_file = file_number;
  address->set_value(file_address.value());
  return true;
}

bool BackendImpl::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  return block_files_.CreateBlock(block_type, block_count, block_address);
}

void BackendImpl::DeleteBlock(Addr block_address, bool deep) {
  block_files_.DeleteBlock(block_address, deep);
}

LruData* BackendImpl::GetLruData() {
  return &data_->header.lru;
}

void BackendImpl::UpdateRank(EntryImpl* entry, bool modified) {
  if (read_only_ || (!modified && GetCacheType() == net::SHADER_CACHE))
    return;
  eviction_.UpdateRank(entry, modified);
}

void BackendImpl::RecoveredEntry(CacheRankingsBlock* rankings) {
  Addr address(rankings->Data()->contents);
  scoped_refptr<EntryImpl> cache_entry;
  if (NewEntry(address, &cache_entry)) {
    STRESS_NOTREACHED();
    return;
  }

  uint32_t hash = cache_entry->GetHash();
  cache_entry = nullptr;

  // Anything on the table means that this entry is there.
  if (data_->table[hash & mask_])
    return;

  data_->table[hash & mask_] = address.value();
  FlushIndex();
}

void BackendImpl::InternalDoomEntry(EntryImpl* entry) {
  uint32_t hash = entry->GetHash();
  std::string key = entry->GetKey();
  Addr entry_addr = entry->entry()->address();
  bool error;
  scoped_refptr<EntryImpl> parent_entry =
      MatchEntry(key, hash, true, entry_addr, &error);
  CacheAddr child(entry->GetNextAddress());

  if (!entry->doomed()) {
    // We may have doomed this entry from within MatchEntry.
    eviction_.OnDoomEntry(entry);
    entry->InternalDoom();
    if (!new_eviction_) {
      DecreaseNumEntries();
    }
    stats_.OnEvent(Stats::DOOM_ENTRY);
  }

  if (parent_entry) {
    parent_entry->SetNextAddress(Addr(child));
    parent_entry = nullptr;
  } else if (!error) {
    data_->table[hash & mask_] = child;
  }

  FlushIndex();
}

#if defined(NET_BUILD_STRESS_CACHE)

CacheAddr BackendImpl::GetNextAddr(Addr address) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end()) {
    EntryImpl* this_entry = it->second;
    return this_entry->GetNextAddress();
  }
  DCHECK(block_files_.IsValid(address));
  DCHECK(!address.is_separate_file() && address.file_type() == BLOCK_256);

  CacheEntryBlock entry(File(address), address);
  CHECK(entry.Load());
  return entry.Data()->next;
}

void BackendImpl::NotLinked(EntryImpl* entry) {
  Addr entry_addr = entry->entry()->address();
  uint32_t i = entry->GetHash() & mask_;
  Addr address(data_->table[i]);
  if (!address.is_initialized())
    return;

  for (;;) {
    DCHECK(entry_addr.value() != address.value());
    address.set_value(GetNextAddr(address));
    if (!address.is_initialized())
      break;
  }
}
#endif  // NET_BUILD_STRESS_CACHE

// An entry may be linked on the DELETED list for a while after being doomed.
// This function is called when we want to remove it.
void BackendImpl::RemoveEntry(EntryImpl* entry) {
#if defined(NET_BUILD_STRESS_CACHE)
  NotLinked(entry);
#endif
  if (!new_eviction_)
    return;

  DCHECK_NE(ENTRY_NORMAL, entry->entry()->Data()->state);

  eviction_.OnDestroyEntry(entry);
  DecreaseNumEntries();
}

void BackendImpl::OnEntryDestroyBegin(Addr address) {
  auto it = open_entries_.find(address.value());
  if (it != open_entries_.end())
    open_entries_.erase(it);
}

void BackendImpl::OnEntryDestroyEnd() {
  DecreaseNumRefs();
  consider_evicting_at_op_end_ = true;
}

void BackendImpl::OnSyncBackendOpComplete() {
  if (consider_evicting_at_op_end_) {
    if (data_->header.num_bytes > max_size_ && !read_only_ &&
        (up_ticks_ > kTrimDelay || user_flags_ & kNoRandom))
      eviction_.TrimCache(false);
    consider_evicting_at_op_end_ = false;
  }
}

EntryImpl* BackendImpl::GetOpenEntry(CacheRankingsBlock* rankings) const {
  DCHECK(rankings->HasData());
  auto it = open_entries_.find(rankings->Data()->contents);
  if (it != open_entries_.end()) {
    // We have this entry in memory.
    return it->second;
  }

  return nullptr;
}

int32_t BackendImpl::GetCurrentEntryId() const {
  return data_->header.this_id;
}

int64_t BackendImpl::MaxFileSize() const {
  return GetCacheType() == net::PNACL_CACHE ? max_size_ : max_size_ / 8;
}

void BackendImpl::ModifyStorageSize(int32_t old_size, int32_t new_size) {
  if (disabled_ || old_size == new_size)
    return;
  if (old_size > new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);

  FlushIndex();

  // Update the usage statistics.
  stats_.ModifyStorageStats(old_size, new_size);
}

void BackendImpl::TooMuchStorageRequested(int32_t size) {
  stats_.ModifyStorageStats(0, size);
}

bool BackendImpl::IsAllocAllowed(int current_size, int new_size) {
  DCHECK_GT(new_size, current_size);
  if (user_flags_ & kNoBuffering)
    return false;

  int to_add = new_size - current_size;
  if (buffer_bytes_ + to_add > MaxBuffersSize())
    return false;

  buffer_bytes_ += to_add;
  return true;
}

void BackendImpl::BufferDeleted(int size) {
  buffer_bytes_ -= size;
  DCHECK_GE(size, 0);
}

bool BackendImpl::IsLoaded() const {
  if (user_flags_ & kNoLoadProtection)
    return false;

  return (num_pending_io_ > 5 || user_load_);
}

base::WeakPtr<BackendImpl> BackendImpl::GetWeakPtr() {
  return ptr_factory_.GetWeakPtr();
}

// Previously this method was used to determine when to report histograms, so
// the logic is surprisingly convoluted.
bool BackendImpl::ShouldUpdateStats() {
  if (should_update_) {
    return should_update_ == 2;
  }

  should_update_++;
  int64_t last_report = stats_.GetCounter(Stats::LAST_REPORT);
  Time last_time = Time::FromInternalValue(last_report);
  if (!last_report || (Time::Now() - last_time).InDays() >= 7) {
    stats_.SetCounter(Stats::LAST_REPORT, Time::Now().ToInternalValue());
    should_update_++;
    return true;
  }
  return false;
}

void BackendImpl::FirstEviction() {
  DCHECK(data_->header.create_time);
  if (!GetEntryCount())
    return;  // This is just for unit tests.

  stats_.ResetRatios();
}

void BackendImpl::CriticalError(int error) {
  STRESS_NOTREACHED();
  LOG(ERROR) << "Critical error found " << error;
  if (disabled_)
    return;

  stats_.OnEvent(Stats::FATAL_ERROR);
  LogStats();
  ReportError(error);

  // Setting the index table length to an invalid value will force re-creation
  // of the cache files.
  data_->header.table_len = 1;
  disabled_ = true;

  if (!num_refs_)
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackendImpl::RestartCache, GetWeakPtr(), true));
}

void BackendImpl::ReportError(int error) {
  STRESS_DCHECK(!error || error == ERR_PREVIOUS_CRASH ||
                error == ERR_CACHE_CREATED);

  // We transmit positive numbers, instead of direct error codes.
  DCHECK_LE(error, 0);
  if (GetCacheType() == net::DISK_CACHE) {
    base::UmaHistogramExactLinear("DiskCache.0.Error", error * -1, 50);
  }
}

void BackendImpl::OnEvent(Stats::Counters an_event) {
  stats_.OnEvent(an_event);
}

void BackendImpl::OnRead(int32_t bytes) {
  DCHECK_GE(bytes, 0);
  byte_count_ += bytes;
  if (byte_count_ < 0)
    byte_count_ = std::numeric_limits<int32_t>::max();
}

void BackendImpl::OnWrite(int32_t bytes) {
  // We use the same implementation as OnRead... just log the number of bytes.
  OnRead(bytes);
}

void BackendImpl::OnStatsTimer() {
  if (disabled_)
    return;

  stats_.OnEvent(Stats::TIMER);
  int64_t time = stats_.GetCounter(Stats::TIMER);
  int64_t current = stats_.GetCounter(Stats::OPEN_ENTRIES);

  // OPEN_ENTRIES is a sampled average of the number of open entries, avoiding
  // the bias towards 0.
  if (num_refs_ && (current != num_refs_)) {
    int64_t diff = (num_refs_ - current) / 50;
    if (!diff)
      diff = num_refs_ > current ? 1 : -1;
    current = current + diff;
    stats_.SetCounter(Stats::OPEN_ENTRIES, current);
    stats_.SetCounter(Stats::MAX_ENTRIES, max_refs_);
  }

  // These values cover about 99.5% of the population (Oct 2011).
  user_load_ = (entry_count_ > 300 || byte_count_ > 7 * 1024 * 1024);
  entry_count_ = 0;
  byte_count_ = 0;
  up_ticks_++;

  if (!data_)
    first_timer_ = false;
  if (first_timer_) {
    first_timer_ = false;
    if (ShouldUpdateStats()) {
      UpdateStats();
    }
  }

  // Save stats to disk at 5 min intervals.
  if (time % 10 == 0)
    StoreStats();
}

void BackendImpl::IncrementIoCount() {
  num_pending_io_++;
}

void BackendImpl::DecrementIoCount() {
  num_pending_io_--;
}

void BackendImpl::SetUnitTestMode() {
  user_flags_ |= kUnitTestMode;
  unit_test_ = true;
}

void BackendImpl::SetUpgradeMode() {
  user_flags_ |= kUpgradeMode;
  read_only_ = true;
}

void BackendImpl::SetNewEviction() {
  user_flags_ |= kNewEviction;
  new_eviction_ = true;
}

void BackendImpl::SetFlags(uint32_t flags) {
  user_flags_ |= flags;
}

void BackendImpl::ClearRefCountForTest() {
  num_refs_ = 0;
}

int BackendImpl::FlushQueueForTest(CompletionOnceCallback callback) {
  background_queue_.FlushQueue(std::move(callback));
  return net::ERR_IO_PENDING;
}

int BackendImpl::RunTaskForTest(base::OnceClosure task,
                                CompletionOnceCallback callback) {
  background_queue_.RunTask(std::move(task), std::move(callback));
  return net::ERR_IO_PENDING;
}

void BackendImpl::TrimForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimCache(empty);
}

void BackendImpl::TrimDeletedListForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimDeletedList(empty);
}

base::RepeatingTimer* BackendImpl::GetTimerForTest() {
  return timer_.get();
}

int BackendImpl::SelfCheck() {
  if (!init_) {
    LOG(ERROR) << "Init failed";
    return ERR_INIT_FAILED;
  }

  int num_entries = rankings_.SelfCheck();
  if (num_entries < 0) {
    LOG(ERROR) << "Invalid rankings list, error " << num_entries;
#if !defined(NET_BUILD_STRESS_CACHE)
    return num_entries;
#endif
  }

  if (num_entries != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
#if !defined(NET_BUILD_STRESS_CACHE)
    return ERR_NUM_ENTRIES_MISMATCH;
#endif
  }

  return CheckAllEntries();
}

void BackendImpl::FlushIndex() {
  if (index_.get() && !disabled_)
    index_->Flush();
}

// ------------------------------------------------------------------------

int32_t BackendImpl::GetEntryCount() const {
  if (!index_.get() || disabled_)
    return 0;
  // num_entries includes entries already evicted.
  int32_t not_deleted =
      data_->header.num_entries - data_->header.lru.sizes[Rankings::DELETED];

  if (not_deleted < 0) {
    DUMP_WILL_BE_NOTREACHED();
    not_deleted = 0;
  }

  return not_deleted;
}

EntryResult BackendImpl::OpenOrCreateEntry(
    const std::string& key,
    net::RequestPriority request_priority,
    EntryResultCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.OpenOrCreateEntry(key, std::move(callback));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

EntryResult BackendImpl::OpenEntry(const std::string& key,
                                   net::RequestPriority request_priority,
                                   EntryResultCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.OpenEntry(key, std::move(callback));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

EntryResult BackendImpl::CreateEntry(const std::string& key,
                                     net::RequestPriority request_priority,
                                     EntryResultCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.CreateEntry(key, std::move(callback));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

net::Error BackendImpl::DoomEntry(const std::string& key,
                                  net::RequestPriority priority,
                                  CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.DoomEntry(key, std::move(callback));
  return net::ERR_IO_PENDING;
}

net::Error BackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.DoomAllEntries(std::move(callback));
  return net::ERR_IO_PENDING;
}

net::Error BackendImpl::DoomEntriesBetween(const base::Time initial_time,
                                           const base::Time end_time,
                                           CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.DoomEntriesBetween(initial_time, end_time,
                                       std::move(callback));
  return net::ERR_IO_PENDING;
}

net::Error BackendImpl::DoomEntriesSince(const base::Time initial_time,
                                         CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.DoomEntriesSince(initial_time, std::move(callback));
  return net::ERR_IO_PENDING;
}

int64_t BackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  background_queue_.CalculateSizeOfAllEntries(BindOnce(
      [](Int64CompletionOnceCallback callback, int result) {
        std::move(callback).Run(static_cast<int64_t>(result));
      },
      std::move(callback)));
  return net::ERR_IO_PENDING;
}

class BackendImpl::IteratorImpl : public Backend::Iterator {
 public:
  explicit IteratorImpl(base::WeakPtr<InFlightBackendIO> background_queue)
      : background_queue_(background_queue),
        iterator_(std::make_unique<Rankings::Iterator>()) {}

  ~IteratorImpl() override {
    if (background_queue_)
      background_queue_->EndEnumeration(std::move(iterator_));
  }

  EntryResult OpenNextEntry(EntryResultCallback callback) override {
    if (!background_queue_)
      return EntryResult::MakeError(net::ERR_FAILED);
    background_queue_->OpenNextEntry(iterator_.get(), std::move(callback));
    return EntryResult::MakeError(net::ERR_IO_PENDING);
  }

 private:
  const base::WeakPtr<InFlightBackendIO> background_queue_;
  std::unique_ptr<Rankings::Iterator> iterator_;
};

std::unique_ptr<Backend::Iterator> BackendImpl::CreateIterator() {
  return std::make_unique<IteratorImpl>(GetBackgroundQueue());
}

void BackendImpl::GetStats(StatsItems* stats) {
  if (disabled_)
    return;

  std::pair<std::string, std::string> item;

  item.first = "Entries";
  item.second = base::NumberToString(data_->header.num_entries);
  stats->push_back(item);

  item.first = "Pending IO";
  item.second = base::NumberToString(num_pending_io_);
  stats->push_back(item);

  item.first = "Max size";
  item.second = base::NumberToString(max_size_);
  stats->push_back(item);

  item.first = "Current size";
  item.second = base::NumberToString(data_->header.num_bytes);
  stats->push_back(item);

  item.first = "Cache type";
  item.second = "Blockfile Cache";
  stats->push_back(item);

  stats_.GetItems(stats);
}

void BackendImpl::OnExternalCacheHit(const std::string& key) {
  background_queue_.OnExternalCacheHit(key);
}

// ------------------------------------------------------------------------

// We just created a new file so we're going to write the header and set the
// file length to include the hash table (zero filled).
bool BackendImpl::CreateBackingStore(disk_cache::File* file) {
  AdjustMaxCacheSize(0);

  IndexHeader header;
  header.table_len = DesiredIndexTableLen(max_size_);
  header.create_time = Time::Now().ToInternalValue();

  if (!file->Write(&header, sizeof(header), 0))
    return false;

  size_t size = GetIndexSize(header.table_len);
  if (!file->SetLength(size))
    return false;

  // The call to SetLength() above is supposed to have already expanded the file
  // to |size| and zero-filled it, but on some systems the actual storage may
  // not get allocated until the pages are actually touched... resulting in a
  // SIGBUS trying to search through the index if the system is out of disk
  // space. So actually write out the zeroes (for pages after the one with the
  // header), to force allocation now and fail cleanly if there is no space.
  //
  // See https://crbug.com/1097518
  const int kPageSize = 4096;
  static_assert(sizeof(disk_cache::IndexHeader) < kPageSize,
                "Code below assumes it wouldn't overwrite header by starting "
                "at kPageSize");
  auto page = std::make_unique<char[]>(kPageSize);
  memset(page.get(), 0, kPageSize);

  for (size_t offset = kPageSize; offset < size; offset += kPageSize) {
    size_t end = std::min(offset + kPageSize, size);
    if (!file->Write(page.get(), end - offset, offset))
      return false;
  }
  return true;
}

bool BackendImpl::InitBackingStore(bool* file_created) {
  if (!base::CreateDirectory(path_))
    return false;

  base::FilePath index_name = path_.AppendASCII(kIndexName);

  int flags = base::File::FLAG_READ | base::File::FLAG_WRITE |
              base::File::FLAG_OPEN_ALWAYS |
              base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  base::File base_file(index_name, flags);
  if (!base_file.IsValid())
    return false;

  bool ret = true;
  *file_created = base_file.created();

  auto file = base::MakeRefCounted<disk_cache::File>(std::move(base_file));
  if (*file_created)
    ret = CreateBackingStore(file.get());

  file = nullptr;
  if (!ret)
    return false;

  index_ = base::MakeRefCounted<MappedFile>();
  data_ = static_cast<Index*>(index_->Init(index_name, 0));
  if (!data_) {
    LOG(ERROR) << "Unable to map Index file";
    return false;
  }

  if (index_->GetLength() < sizeof(Index)) {
    // We verify this again on CheckIndex() but it's easier to make sure now
    // that the header is there.
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  return true;
}

// The maximum cache size will be either set explicitly by the caller, or
// calculated by this code.
void BackendImpl::AdjustMaxCacheSize(int table_len) {
  if (max_size_)
    return;

  // If table_len is provided, the index file exists.
  DCHECK(!table_len || data_->header.magic);

  // The user is not setting the size, let's figure it out.
  int64_t available = base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (available < 0) {
    max_size_ = kDefaultCacheSize;
    return;
  }

  if (table_len)
    available += data_->header.num_bytes;

  max_size_ = PreferredCacheSize(available, GetCacheType());

  if (!table_len)
    return;

  // If we already have a table, adjust the size to it.
  max_size_ = std::min(max_size_, MaxStorageSizeForTable(table_len));
}

bool BackendImpl::InitStats() {
  Addr address(data_->header.stats);
  int size = stats_.StorageSize();

  if (!address.is_initialized()) {
    FileType file_type = Addr::RequiredFileType(size);
    DCHECK_NE(file_type, EXTERNAL);
    int num_blocks = Addr::RequiredBlocks(size, file_type);

    if (!CreateBlock(file_type, num_blocks, &address))
      return false;

    data_->header.stats = address.value();
    return stats_.Init(nullptr, 0, address);
  }

  if (!address.is_block_file()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // Load the required data.
  size = address.num_blocks() * address.BlockSize();
  MappedFile* file = File(address);
  if (!file)
    return false;

  auto data = std::make_unique<char[]>(size);
  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (!file->Read(data.get(), size, offset))
    return false;

  if (!stats_.Init(data.get(), size, address))
    return false;
  if (GetCacheType() == net::DISK_CACHE && ShouldUpdateStats()) {
    stats_.InitSizeHistogram();
  }
  return true;
}

void BackendImpl::StoreStats() {
  int size = stats_.StorageSize();
  auto data = std::make_unique<char[]>(size);
  Addr address;
  size = stats_.SerializeStats(data.get(), size, &address);
  DCHECK(size);
  if (!address.is_initialized())
    return;

  MappedFile* file = File(address);
  if (!file)
    return;

  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  file->Write(data.get(), size, offset);  // ignore result.
}

void BackendImpl::RestartCache(bool failure) {
  TRACE_EVENT0("disk_cache", "BackendImpl::RestartCache");

  int64_t errors = stats_.GetCounter(Stats::FATAL_ERROR);
  int64_t full_dooms = stats_.GetCounter(Stats::DOOM_CACHE);
  int64_t partial_dooms = stats_.GetCounter(Stats::DOOM_RECENT);
  int64_t last_report = stats_.GetCounter(Stats::LAST_REPORT);

  PrepareForRestart();
  if (failure) {
    DCHECK(!num_refs_);
    DCHECK(open_entries_.empty());
    CleanupDirectorySync(path_);
  } else {
    DeleteCache(path_, false);
  }

  // Don't call Init() if directed by the unit test: we are simulating a failure
  // trying to re-enable the cache.
  if (unit_test_) {
    init_ = true;  // Let the destructor do proper cleanup.
  } else if (SyncInit() == net::OK) {
    stats_.SetCounter(Stats::FATAL_ERROR, errors);
    stats_.SetCounter(Stats::DOOM_CACHE, full_dooms);
    stats_.SetCounter(Stats::DOOM_RECENT, partial_dooms);
    stats_.SetCounter(Stats::LAST_REPORT, last_report);
  }
}

void BackendImpl::PrepareForRestart() {
  // Reset the mask_ if it was not given by the user.
  if (!(user_flags_ & kMask))
    mask_ = 0;

  if (!(user_flags_ & kNewEviction))
    new_eviction_ = false;

  disabled_ = true;
  data_->header.crash = 0;
  index_->Flush();
  index_ = nullptr;
  data_ = nullptr;
  block_files_.CloseFiles();
  rankings_.Reset();
  init_ = false;
  restarted_ = true;
}

int BackendImpl::NewEntry(Addr address, scoped_refptr<EntryImpl>* entry) {
  auto it = open_entries_.find(address.value());
  if (it != open_entries_.end()) {
    // Easy job. This entry is already in memory.
    *entry = base::WrapRefCounted(it->second);
    return 0;
  }

  STRESS_DCHECK(block_files_.IsValid(address));

  if (!address.SanityCheckForEntry()) {
    LOG(WARNING) << "Wrong entry address.";
    STRESS_NOTREACHED();
    return ERR_INVALID_ADDRESS;
  }

  auto cache_entry = base::MakeRefCounted<EntryImpl>(this, address, read_only_);
  IncreaseNumRefs();
  *entry = nullptr;

  if (!cache_entry->entry()->Load())
    return ERR_READ_FAILURE;

  if (!cache_entry->SanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    STRESS_NOTREACHED();
    return ERR_INVALID_ENTRY;
  }

  STRESS_DCHECK(block_files_.IsValid(
                    Addr(cache_entry->entry()->Data()->rankings_node)));

  if (!cache_entry->LoadNodeAddress())
    return ERR_READ_FAILURE;

  if (!rankings_.SanityCheck(cache_entry->rankings(), false)) {
    STRESS_NOTREACHED();
    cache_entry->SetDirtyFlag(0);
    // Don't remove this from the list (it is not linked properly). Instead,
    // break the link back to the entry because it is going away, and leave the
    // rankings node to be deleted if we find it through a list.
    rankings_.SetContents(cache_entry->rankings(), 0);
  } else if (!rankings_.DataSanityCheck(cache_entry->rankings(), false)) {
    STRESS_NOTREACHED();
    cache_entry->SetDirtyFlag(0);
    rankings_.SetContents(cache_entry->rankings(), address.value());
  }

  if (!cache_entry->DataSanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    cache_entry->SetDirtyFlag(0);
    cache_entry->FixForDelete();
  }

  // Prevent overwriting the dirty flag on the destructor.
  cache_entry->SetDirtyFlag(GetCurrentEntryId());

  open_entries_[address.value()] = cache_entry.get();

  cache_entry->BeginLogging(net_log_, false);
  *entry = std::move(cache_entry);
  return 0;
}

scoped_refptr<EntryImpl> BackendImpl::MatchEntry(const std::string& key,
                                                 uint32_t hash,
                                                 bool find_parent,
                                                 Addr entry_addr,
                                                 bool* match_error) {
  TRACE_EVENT0("disk_cache", "BackendImpl::MatchEntry");

  Addr address(data_->table[hash & mask_]);
  scoped_refptr<EntryImpl> cache_entry, parent_entry;
  bool found = false;
  std::set<CacheAddr> visited;
  *match_error = false;

  for (;;) {
    if (disabled_)
      break;

    if (visited.find(address.value()) != visited.end()) {
      // It's possible for a buggy version of the code to write a loop. Just
      // break it.
      address.set_value(0);
      parent_entry->SetNextAddress(address);
    }
    visited.insert(address.value());

    if (!address.is_initialized()) {
      if (find_parent)
        found = true;
      break;
    }

    int error = NewEntry(address, &cache_entry);
    if (error || cache_entry->dirty()) {
      // This entry is dirty on disk (it was not properly closed): we cannot
      // trust it.
      Addr child(0);
      if (!error)
        child.set_value(cache_entry->GetNextAddress());

      if (parent_entry.get()) {
        parent_entry->SetNextAddress(child);
        parent_entry = nullptr;
      } else {
        data_->table[hash & mask_] = child.value();
      }

      if (!error) {
        // It is important to call DestroyInvalidEntry after removing this
        // entry from the table.
        DestroyInvalidEntry(cache_entry.get());
        cache_entry = nullptr;
      }

      // Restart the search.
      address.set_value(data_->table[hash & mask_]);
      visited.clear();
      continue;
    }

    DCHECK_EQ(hash & mask_, cache_entry->entry()->Data()->hash & mask_);
    if (cache_entry->IsSameEntry(key, hash)) {
      if (!cache_entry->Update())
        cache_entry = nullptr;
      found = true;
      if (find_parent && entry_addr.value() != address.value()) {
        *match_error = true;
        parent_entry = nullptr;
      }
      break;
    }
    if (!cache_entry->Update())
      cache_entry = nullptr;
    parent_entry = cache_entry;
    cache_entry = nullptr;
    if (!parent_entry.get())
      break;

    address.set_value(parent_entry->GetNextAddress());
  }

  if (parent_entry.get() && (!find_parent || !found))
    parent_entry = nullptr;

  if (find_parent && entry_addr.is_initialized() && !cache_entry.get()) {
    *match_error = true;
    parent_entry = nullptr;
  }

  if (cache_entry.get() && (find_parent || !found))
    cache_entry = nullptr;

  FlushIndex();

  return find_parent ? std::move(parent_entry) : std::move(cache_entry);
}

bool BackendImpl::OpenFollowingEntryFromList(
    Rankings::List list,
    CacheRankingsBlock** from_entry,
    scoped_refptr<EntryImpl>* next_entry) {
  if (disabled_)
    return false;

  if (!new_eviction_ && Rankings::NO_USE != list)
    return false;

  Rankings::ScopedRankingsBlock rankings(&rankings_, *from_entry);
  CacheRankingsBlock* next_block = rankings_.GetNext(rankings.get(), list);
  Rankings::ScopedRankingsBlock next(&rankings_, next_block);
  *from_entry = nullptr;

  *next_entry = GetEnumeratedEntry(next.get(), list);
  if (!*next_entry)
    return false;

  *from_entry = next.release();
  return true;
}

scoped_refptr<EntryImpl> BackendImpl::GetEnumeratedEntry(
    CacheRankingsBlock* next,
    Rankings::List list) {
  if (!next || disabled_)
    return nullptr;

  scoped_refptr<EntryImpl> entry;
  int rv = NewEntry(Addr(next->Data()->contents), &entry);
  if (rv) {
    STRESS_NOTREACHED();
    rankings_.Remove(next, list, false);
    if (rv == ERR_INVALID_ADDRESS) {
      // There is nothing linked from the index. Delete the rankings node.
      DeleteBlock(next->address(), true);
    }
    return nullptr;
  }

  if (entry->dirty()) {
    // We cannot trust this entry.
    InternalDoomEntry(entry.get());
    return nullptr;
  }

  if (!entry->Update()) {
    STRESS_NOTREACHED();
    return nullptr;
  }

  // Note that it is unfortunate (but possible) for this entry to be clean, but
  // not actually the real entry. In other words, we could have lost this entry
  // from the index, and it could have been replaced with a newer one. It's not
  // worth checking that this entry is "the real one", so we just return it and
  // let the enumeration continue; this entry will be evicted at some point, and
  // the regular path will work with the real entry. With time, this problem
  // will disasappear because this scenario is just a bug.

  // Make sure that we save the key for later.
  entry->GetKey();

  return entry;
}

scoped_refptr<EntryImpl> BackendImpl::ResurrectEntry(
    scoped_refptr<EntryImpl> deleted_entry) {
  if (ENTRY_NORMAL == deleted_entry->entry()->Data()->state) {
    deleted_entry = nullptr;
    stats_.OnEvent(Stats::CREATE_MISS);
    return nullptr;
  }

  // We are attempting to create an entry and found out that the entry was
  // previously deleted.

  eviction_.OnCreateEntry(deleted_entry.get());
  entry_count_++;

  stats_.OnEvent(Stats::RESURRECT_HIT);
  return deleted_entry;
}

void BackendImpl::DestroyInvalidEntry(EntryImpl* entry) {
  LOG(WARNING) << "Destroying invalid entry.";

  entry->SetPointerForInvalidEntry(GetCurrentEntryId());

  eviction_.OnDoomEntry(entry);
  entry->InternalDoom();

  if (!new_eviction_)
    DecreaseNumEntries();
  stats_.OnEvent(Stats::INVALID_ENTRY);
}

void BackendImpl::AddStorageSize(int32_t bytes) {
  data_->header.num_bytes += bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImpl::SubstractStorageSize(int32_t bytes) {
  data_->header.num_bytes -= bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImpl::IncreaseNumRefs() {
  num_refs_++;
  if (max_refs_ < num_refs_)
    max_refs_ = num_refs_;
}

void BackendImpl::DecreaseNumRefs() {
  DCHECK(num_refs_);
  num_refs_--;

  if (!num_refs_ && disabled_)
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackendImpl::RestartCache, GetWeakPtr(), true));
}

void BackendImpl::IncreaseNumEntries() {
  data_->header.num_entries++;
  DCHECK_GT(data_->header.num_entries, 0);
}

void BackendImpl::DecreaseNumEntries() {
  data_->header.num_entries--;
  if (data_->header.num_entries < 0) {
    STRESS_NOTREACHED();
    data_->header.num_entries = 0;
  }
}

void BackendImpl::LogStats() {
  StatsItems stats;
  GetStats(&stats);

  for (const auto& stat : stats)
    VLOG(1) << stat.first << ": " << stat.second;
}

void BackendImpl::UpdateStats() {
  // Previously this function was used to periodically emit histograms, however
  // now it just performs some regular maintenance on the cache statistics.
  stats_.SetCounter(Stats::MAX_ENTRIES, 0);
  stats_.SetCounter(Stats::FATAL_ERROR, 0);
  stats_.SetCounter(Stats::DOOM_CACHE, 0);
  stats_.SetCounter(Stats::DOOM_RECENT, 0);

  int64_t total_hours = stats_.GetCounter(Stats::TIMER) / 120;
  if (!data_->header.create_time || !data_->header.lru.filled) {
    return;
  }

  int64_t use_hours = stats_.GetCounter(Stats::LAST_REPORT_TIMER) / 120;
  stats_.SetCounter(Stats::LAST_REPORT_TIMER, stats_.GetCounter(Stats::TIMER));

  // We may see users with no use_hours at this point if this is the first time
  // we are running this code.
  if (use_hours)
    use_hours = total_hours - use_hours;

  if (!use_hours || !GetEntryCount() || !data_->header.num_bytes)
    return;

  stats_.ResetRatios();
  stats_.SetCounter(Stats::TRIM_ENTRY, 0);
}

void BackendImpl::UpgradeTo2_1() {
  // 2.1 is basically the same as 2.0, except that new fields are actually
  // updated by the new eviction algorithm.
  DCHECK_EQ(kVersion2_0, data_->header.version);
  data_->header.version = kVersion2_1;
  data_->header.lru.sizes[Rankings::NO_USE] = data_->header.num_entries;
}

void BackendImpl::UpgradeTo3_0() {
  // 3.0 uses a 64-bit size field.
  DCHECK(kVersion2_0 == data_->header.version ||
         kVersion2_1 == data_->header.version);
  data_->header.version = kVersion3_0;
  data_->header.num_bytes = data_->header.old_v2_num_bytes;
}

bool BackendImpl::CheckIndex() {
  DCHECK(data_);

  size_t current_size = index_->GetLength();
  if (current_size < sizeof(Index)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  if (data_->header.magic != kIndexMagic) {
    LOG(ERROR) << "Invalid file magic";
    return false;
  }

  // 2.0 + new_eviction needs conversion to 2.1.
  if (data_->header.version == kVersion2_0 && new_eviction_) {
    UpgradeTo2_1();
  }

  // 2.0 or 2.1 can be upgraded to 3.0
  if (data_->header.version == kVersion2_0 ||
      data_->header.version == kVersion2_1) {
    UpgradeTo3_0();
  }

  if (kCurrentVersion != data_->header.version) {
    LOG(ERROR) << "Invalid file version";
    return false;
  }

  if (!data_->header.table_len) {
    LOG(ERROR) << "Invalid table size";
    return false;
  }

  if (current_size < GetIndexSize(data_->header.table_len) ||
      data_->header.table_len & (kBaseTableLen - 1)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  AdjustMaxCacheSize(data_->header.table_len);

#if !defined(NET_BUILD_STRESS_CACHE)
  if (data_->header.num_bytes < 0 ||
      (max_size_ < std::numeric_limits<int32_t>::max() - kDefaultCacheSize &&
       data_->header.num_bytes > max_size_ + kDefaultCacheSize)) {
    LOG(ERROR) << "Invalid cache (current) size";
    return false;
  }
#endif

  if (data_->header.num_entries < 0) {
    LOG(ERROR) << "Invalid number of entries";
    return false;
  }

  if (!mask_)
    mask_ = data_->header.table_len - 1;

  // Load the table into memory.
  return index_->Preload();
}

int BackendImpl::CheckAllEntries() {
  int num_dirty = 0;
  int num_entries = 0;
  DCHECK(mask_ < std::numeric_limits<uint32_t>::max());
  for (unsigned int i = 0; i <= mask_; i++) {
    Addr address(data_->table[i]);
    if (!address.is_initialized())
      continue;
    for (;;) {
      scoped_refptr<EntryImpl> cache_entry;
      int ret = NewEntry(address, &cache_entry);
      if (ret) {
        STRESS_NOTREACHED();
        return ret;
      }

      if (cache_entry->dirty())
        num_dirty++;
      else if (CheckEntry(cache_entry.get()))
        num_entries++;
      else
        return ERR_INVALID_ENTRY;

      DCHECK_EQ(i, cache_entry->entry()->Data()->hash & mask_);
      address.set_value(cache_entry->GetNextAddress());
      if (!address.is_initialized())
        break;
    }
  }

  if (num_entries + num_dirty != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries " << num_entries << " " << num_dirty <<
                  " " << data_->header.num_entries;
    DCHECK_LT(num_entries, data_->header.num_entries);
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return num_dirty;
}

bool BackendImpl::CheckEntry(EntryImpl* cache_entry) {
  bool ok = block_files_.IsValid(cache_entry->entry()->address());
  ok = ok && block_files_.IsValid(cache_entry->rankings()->address());
  EntryStore* data = cache_entry->entry()->Data();
  for (size_t i = 0; i < std::size(data->data_addr); i++) {
    if (data->data_addr[i]) {
      Addr address(data->data_addr[i]);
      if (address.is_block_file())
        ok = ok && block_files_.IsValid(address);
    }
  }

  return ok && cache_entry->rankings()->VerifyHash();
}

// static
int BackendImpl::MaxBuffersSize() {
  // Calculate based on total memory the first time this function is called,
  // then cache the result.
  static const int max_buffers_size = ([]() {
    constexpr uint64_t kMaxMaxBuffersSize = 30 * 1024 * 1024;
    const uint64_t total_memory = base::SysInfo::AmountOfPhysicalMemory();
    if (total_memory == 0u) {
      return int{kMaxMaxBuffersSize};
    }
    const uint64_t two_percent = total_memory * 2 / 100;
    return static_cast<int>(std::min(two_percent, kMaxMaxBuffersSize));
  })();

  return max_buffers_size;
}

void BackendImpl::FlushForTesting() {
  if (!g_internal_cache_thread.IsCreated()) {
    return;
  }

  g_internal_cache_thread.Get().FlushForTesting();
}

void BackendImpl::FlushAsynchronouslyForTesting(base::OnceClosure callback) {
  if (!g_internal_cache_thread.IsCreated()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  InternalCacheThread()->PostTaskAndReply(FROM_HERE, base::BindOnce([]() {}),
                                          std::move(callback));
}

}  // namespace disk_cache

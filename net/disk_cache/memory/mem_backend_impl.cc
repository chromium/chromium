// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_backend_impl.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/memory/mem_entry_impl.h"

using base::Time;

namespace disk_cache {

namespace {

const int32_t kDefaultInMemoryCacheSize = 10 * 1024 * 1024;
const int32_t kMaxMemoryCacheSize = kDefaultInMemoryCacheSize * 5;

int32_t CalculateDefaultMaxSize() {
  // The default max size is based on amount of physical memory of the machine.

  base::ByteSize total_memory = base::SysInfo::AmountOfTotalPhysicalMemory();
  if (total_memory.is_zero()) {
    return kDefaultInMemoryCacheSize;
  }

  // We want to use up to 2% of the computer's memory, with a limit of 50 MB,
  // reached on system with more than 2.5 GB of RAM.
  if (total_memory >= base::MiBU(2500)) {
    return kMaxMemoryCacheSize;
  }

  base::ByteSize max_size = total_memory * 2 / 100;
  return base::checked_cast<int32_t>(max_size.InBytes());
}

// Returns the next entry after |node| in |lru_list| that's not a child
// of |node|.  This is useful when dooming, since dooming a parent entry
// will also doom its children.
base::LinkNode<MemEntryImpl>* NextSkippingChildren(
    const base::LinkedList<MemEntryImpl>& lru_list,
    base::LinkNode<MemEntryImpl>* node) {
  MemEntryImpl* cur = node->value();
  do {
    node = node->next();
  } while (node != lru_list.end() && node->value()->parent() == cur);
  return node;
}

}  // namespace

MemBackendImpl::MemBackendImpl(net::NetLog* net_log)
    : Backend(net::MEMORY_CACHE),
      net_log_(net_log),
      memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kMemBackend,
          this) {}

MemBackendImpl::~MemBackendImpl() {
  while (!entries_.empty())
    entries_.begin()->second->Doom();

  if (!post_cleanup_callback_.is_null())
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(post_cleanup_callback_));
}

// static
std::unique_ptr<MemBackendImpl> MemBackendImpl::CreateBackend(
    int64_t max_bytes,
    net::NetLog* net_log) {
  if (max_bytes < 0 || max_bytes > std::numeric_limits<int32_t>::max()) {
    LOG(ERROR) << "Unable to create cache";
    return nullptr;
  }

  auto cache = std::make_unique<MemBackendImpl>(net_log);

  // `max_bytes` is guaranteed to fit because of the check above.
  cache->Init(base::checked_cast<int32_t>(max_bytes));

  return cache;
}

int64_t MemBackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

void MemBackendImpl::OnEntryInserted(MemEntryImpl* entry) {
  lru_list_.Append(entry);
}

void MemBackendImpl::OnEntryUpdated(MemEntryImpl* entry) {
  // LinkedList<>::RemoveFromList() removes |entry| from |lru_list_|.
  entry->RemoveFromList();
  lru_list_.Append(entry);
}

void MemBackendImpl::OnEntryDoomed(MemEntryImpl* entry) {
  if (entry->type() == MemEntryImpl::EntryType::kParent)
    entries_.erase(entry->key());
  // LinkedList<>::RemoveFromList() removes |entry| from |lru_list_|.
  entry->RemoveFromList();
}

void MemBackendImpl::ModifyStorageSize(int32_t delta) {
  current_size_ += delta;
  if (delta > 0)
    EvictIfNeeded();
}

bool MemBackendImpl::HasExceededStorageSize() const {
  return current_size_ > current_max_size_;
}

void MemBackendImpl::SetPostCleanupCallback(base::OnceClosure cb) {
  DCHECK(post_cleanup_callback_.is_null());
  post_cleanup_callback_ = std::move(cb);
}

// static
base::Time MemBackendImpl::Now(const base::WeakPtr<MemBackendImpl>& self) {
  MemBackendImpl* instance = self.get();
  if (instance && instance->custom_clock_for_testing_)
    return instance->custom_clock_for_testing_->Now();
  return Time::Now();
}

void MemBackendImpl::SetClockForTesting(base::Clock* clock) {
  custom_clock_for_testing_ = clock;
}

base::expected<int32_t, net::Error> MemBackendImpl::GetEntryCount(
    GetEntryCountCallback callback) const {
  return base::ok(static_cast<int32_t>(entries_.size()));
}

EntryResult MemBackendImpl::OpenOrCreateEntry(const std::string& key,
                                              net::RequestPriority priority,
                                              EntryResultCallback callback) {
  EntryResult result = OpenEntry(key, priority, EntryResultCallback());
  if (result.net_error() == net::OK)
    return result;

  // Key was not opened, try creating it instead.
  return CreateEntry(key, priority, EntryResultCallback());
}

EntryResult MemBackendImpl::OpenEntry(const std::string& key,
                                      net::RequestPriority request_priority,
                                      EntryResultCallback callback) {
  auto it = entries_.find(key);
  if (it == entries_.end())
    return EntryResult::MakeError(net::ERR_FAILED);

  it->second->Open();

  return EntryResult::MakeOpened(it->second);
}

EntryResult MemBackendImpl::CreateEntry(const std::string& key,
                                        net::RequestPriority request_priority,
                                        EntryResultCallback callback) {
  std::pair<EntryMap::iterator, bool> create_result =
      entries_.insert(EntryMap::value_type(key, nullptr));
  const bool did_insert = create_result.second;
  if (!did_insert)
    return EntryResult::MakeError(net::ERR_FAILED);

  MemEntryImpl* cache_entry =
      new MemEntryImpl(weak_factory_.GetWeakPtr(), key, net_log_);
  create_result.first->second = cache_entry;
  return EntryResult::MakeCreated(cache_entry);
}

net::Error MemBackendImpl::DoomEntry(const std::string& key,
                                     net::RequestPriority priority,
                                     CompletionOnceCallback callback) {
  auto it = entries_.find(key);
  if (it == entries_.end())
    return net::ERR_FAILED;

  it->second->Doom();
  return net::OK;
}

net::Error MemBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  return DoomEntriesBetween(Time(), Time(), std::move(callback));
}

net::Error MemBackendImpl::DoomEntriesBetween(Time initial_time,
                                              Time end_time,
                                              CompletionOnceCallback callback) {
  if (end_time.is_null())
    end_time = Time::Max();
  DCHECK_GE(end_time, initial_time);

  base::LinkNode<MemEntryImpl>* node = lru_list_.head();
  while (node != lru_list_.end()) {
    MemEntryImpl* candidate = node->value();
    node = NextSkippingChildren(lru_list_, node);

    if (candidate->GetLastUsed() >= initial_time &&
        candidate->GetLastUsed() < end_time) {
      candidate->Doom();
    }
  }

  return net::OK;
}

net::Error MemBackendImpl::DoomEntriesSince(Time initial_time,
                                            CompletionOnceCallback callback) {
  return DoomEntriesBetween(initial_time, Time::Max(), std::move(callback));
}

int64_t MemBackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  return current_size_;
}

int64_t MemBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  if (end_time.is_null())
    end_time = Time::Max();
  DCHECK_GE(end_time, initial_time);

  int size = 0;
  base::LinkNode<MemEntryImpl>* node = lru_list_.head();
  while (node != lru_list_.end()) {
    MemEntryImpl* entry = node->value();
    if (entry->GetLastUsed() >= initial_time &&
        entry->GetLastUsed() < end_time) {
      size += entry->GetStorageSize();
    }
    node = node->next();
  }
  return size;
}

class MemBackendImpl::MemIterator final : public Backend::Iterator {
 public:
  explicit MemIterator(base::WeakPtr<MemBackendImpl> backend)
      : backend_(backend) {}

  EntryResult OpenNextEntry(EntryResultCallback callback) override {
    if (!backend_)
      return EntryResult::MakeError(net::ERR_FAILED);

    if (!backend_keys_) {
      backend_keys_ = std::make_unique<Strings>(backend_->entries_.size());
      for (const auto& iter : backend_->entries_)
        backend_keys_->push_back(iter.first);
      current_ = backend_keys_->begin();
    } else {
      current_++;
    }

    while (true) {
      if (current_ == backend_keys_->end()) {
        backend_keys_.reset();
        return EntryResult::MakeError(net::ERR_FAILED);
      }

      const auto& entry_iter = backend_->entries_.find(*current_);
      if (entry_iter == backend_->entries_.end()) {
        // The key is no longer in the cache, move on to the next key.
        current_++;
        continue;
      }

      entry_iter->second->Open();
      return EntryResult::MakeOpened(entry_iter->second);
    }
  }

 private:
  using Strings = std::vector<std::string>;

  base::WeakPtr<MemBackendImpl> backend_;
  std::unique_ptr<Strings> backend_keys_;
  Strings::iterator current_;
};

std::unique_ptr<Backend::Iterator> MemBackendImpl::CreateIterator() {
  return std::make_unique<MemIterator>(weak_factory_.GetWeakPtr());
}

void MemBackendImpl::OnExternalCacheHit(const std::string& key) {
  auto it = entries_.find(key);
  if (it != entries_.end())
    it->second->UpdateStateOnUse();
}

void MemBackendImpl::Init(int32_t max_bytes) {
  max_size_ = max_bytes ? max_bytes : CalculateDefaultMaxSize();
  current_max_size_ = max_size_;
}

void MemBackendImpl::EvictIfNeeded() {
  if (current_size_ <= current_max_size_) {
    return;
  }
  // Evict 10% more than necessary to avoid evicting on every insertion when the
  // cache is full.
  int target_size = current_max_size_ - (current_max_size_ / 10);
  EvictTill(target_size);
}

void MemBackendImpl::EvictTill(int target_size) {
  base::LinkNode<MemEntryImpl>* entry = lru_list_.head();
  while (current_size_ > target_size && entry != lru_list_.end()) {
    MemEntryImpl* to_doom = entry->value();
    entry = NextSkippingChildren(lru_list_, entry);

    if (!to_doom->InUse())
      to_doom->Doom();
  }
}

void MemBackendImpl::OnMemoryPressure(
    base::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      current_max_size_ = max_size_;
      break;
    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      current_max_size_ = max_size_ / 2;
      break;
    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      current_max_size_ = max_size_ / 10;
      break;
  }
  EvictTill(current_max_size_);
}

}  // namespace disk_cache

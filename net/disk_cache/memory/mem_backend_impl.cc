// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_backend_impl.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/memory/mem_entry_impl.h"

using base::Time;

namespace disk_cache {

namespace {

const int kDefaultInMemoryCacheSize = 10 * 1024 * 1024;
const int kDefaultEvictionSize = kDefaultInMemoryCacheSize / 10;

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
      memory_pressure_listener_(
          FROM_HERE,
          base::BindRepeating(&MemBackendImpl::OnMemoryPressure,
                              base::Unretained(this))) {}

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
  std::unique_ptr<MemBackendImpl> cache(
      std::make_unique<MemBackendImpl>(net_log));
  if (cache->SetMaxSize(max_bytes) && cache->Init())
    return cache;

  LOG(ERROR) << "Unable to create cache";
  return nullptr;
}

bool MemBackendImpl::Init() {
  if (max_size_)
    return true;

  uint64_t total_memory = base::SysInfo::AmountOfPhysicalMemory();

  if (total_memory == 0) {
    max_size_ = kDefaultInMemoryCacheSize;
    return true;
  }

  // We want to use up to 2% of the computer's memory, with a limit of 50 MB,
  // reached on system with more than 2.5 GB of RAM.
  total_memory = total_memory * 2 / 100;
  if (total_memory > static_cast<uint64_t>(kDefaultInMemoryCacheSize) * 5)
    max_size_ = kDefaultInMemoryCacheSize * 5;
  else
    max_size_ = static_cast<int32_t>(total_memory);

  return true;
}

bool MemBackendImpl::SetMaxSize(int64_t max_bytes) {
  if (max_bytes < 0 || max_bytes > std::numeric_limits<int>::max())
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  max_size_ = max_bytes;
  return true;
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
  return current_size_ > max_size_;
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

int32_t MemBackendImpl::GetEntryCount() const {
  return static_cast<int32_t>(entries_.size());
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
    it->second->UpdateStateOnUse(MemEntryImpl::ENTRY_WAS_NOT_MODIFIED);
}

void MemBackendImpl::EvictIfNeeded() {
  if (current_size_ <= max_size_)
    return;
  int target_size = std::max(0, max_size_ - kDefaultEvictionSize);
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
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      EvictTill(max_size_ / 2);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      EvictTill(max_size_ / 10);
      break;
  }
}

}  // namespace disk_cache

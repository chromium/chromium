// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/cache_type.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"

namespace content {

// A callback shim that provides storage for the 'backend_ptr' value
// and will delete a resulting ptr if completion occurs after the
// callback has been canceled.
class ServiceWorkerDiskCache::CreateBackendCallbackShim
    : public base::RefCounted<CreateBackendCallbackShim> {
 public:
  explicit CreateBackendCallbackShim(ServiceWorkerDiskCache* object)
      : service_worker_disk_cache_(object) {}

  void Cancel() { service_worker_disk_cache_ = nullptr; }

  void Callback(int return_value) {
    if (service_worker_disk_cache_)
      service_worker_disk_cache_->OnCreateBackendComplete(return_value);
  }

  std::unique_ptr<disk_cache::Backend> backend_ptr_;  // Accessed directly.

 private:
  friend class base::RefCounted<CreateBackendCallbackShim>;

  ~CreateBackendCallbackShim() = default;

  ServiceWorkerDiskCache* service_worker_disk_cache_;  // Unowned pointer.
};

ServiceWorkerDiskCacheEntry::ServiceWorkerDiskCacheEntry(
    disk_cache::Entry* disk_cache_entry,
    ServiceWorkerDiskCache* cache)
    : disk_cache_entry_(disk_cache_entry), cache_(cache) {
  DCHECK(disk_cache_entry);
  DCHECK(cache);
  cache_->AddOpenEntry(this);
}

ServiceWorkerDiskCacheEntry::~ServiceWorkerDiskCacheEntry() {
  if (cache_)
    cache_->RemoveOpenEntry(this);
}

int ServiceWorkerDiskCacheEntry::Read(int index,
                                      int64_t offset,
                                      net::IOBuffer* buf,
                                      int buf_len,
                                      net::CompletionOnceCallback callback) {
  if (offset < 0 || offset > std::numeric_limits<int32_t>::max())
    return net::ERR_INVALID_ARGUMENT;
  if (!disk_cache_entry_)
    return net::ERR_ABORTED;
  return disk_cache_entry_->ReadData(index, static_cast<int>(offset), buf,
                                     buf_len, std::move(callback));
}

int ServiceWorkerDiskCacheEntry::Write(int index,
                                       int64_t offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       net::CompletionOnceCallback callback) {
  if (offset < 0 || offset > std::numeric_limits<int32_t>::max())
    return net::ERR_INVALID_ARGUMENT;
  if (!disk_cache_entry_)
    return net::ERR_ABORTED;
  const bool kTruncate = true;
  return disk_cache_entry_->WriteData(index, static_cast<int>(offset), buf,
                                      buf_len, std::move(callback), kTruncate);
}

int64_t ServiceWorkerDiskCacheEntry::GetSize(int index) {
  return disk_cache_entry_ ? disk_cache_entry_->GetDataSize(index) : 0L;
}

void ServiceWorkerDiskCacheEntry::Close() {
  if (disk_cache_entry_)
    disk_cache_entry_->Close();
  delete this;
}

void ServiceWorkerDiskCacheEntry::Abandon() {
  cache_ = nullptr;
  disk_cache_entry_->Close();
  disk_cache_entry_ = nullptr;
}

namespace {

// Separate object to hold state for each Create, Delete, or Doom call
// while the call is in-flight and to produce an EntryImpl upon completion.
class ActiveCall : public base::RefCounted<ActiveCall> {
 public:
  ActiveCall(const base::WeakPtr<ServiceWorkerDiskCache>& owner,
             ServiceWorkerDiskCacheEntry** entry,
             net::CompletionOnceCallback callback)
      : owner_(owner), entry_(entry), callback_(std::move(callback)) {
    DCHECK(owner_);
  }

  static net::Error CreateEntry(
      const base::WeakPtr<ServiceWorkerDiskCache>& owner,
      int64_t key,
      ServiceWorkerDiskCacheEntry** entry,
      net::CompletionOnceCallback callback) {
    scoped_refptr<ActiveCall> active_call =
        base::MakeRefCounted<ActiveCall>(owner, entry, std::move(callback));
    disk_cache::EntryResult result = owner->disk_cache()->CreateEntry(
        base::NumberToString(key), net::HIGHEST,
        base::BindOnce(&ActiveCall::OnAsyncCompletion, active_call));
    return active_call->HandleImmediateReturnValue(std::move(result));
  }

  static net::Error OpenEntry(
      const base::WeakPtr<ServiceWorkerDiskCache>& owner,
      int64_t key,
      ServiceWorkerDiskCacheEntry** entry,
      net::CompletionOnceCallback callback) {
    scoped_refptr<ActiveCall> active_call =
        base::MakeRefCounted<ActiveCall>(owner, entry, std::move(callback));
    disk_cache::EntryResult result = owner->disk_cache()->OpenEntry(
        base::NumberToString(key), net::HIGHEST,
        base::BindOnce(&ActiveCall::OnAsyncCompletion, active_call));
    return active_call->HandleImmediateReturnValue(std::move(result));
  }

  static net::Error DoomEntry(
      const base::WeakPtr<ServiceWorkerDiskCache>& owner,
      int64_t key,
      net::CompletionOnceCallback callback) {
    return owner->disk_cache()->DoomEntry(base::NumberToString(key),
                                          net::HIGHEST, std::move(callback));
  }

 private:
  friend class base::RefCounted<ActiveCall>;

  ~ActiveCall() = default;

  net::Error HandleImmediateReturnValue(disk_cache::EntryResult result) {
    net::Error rv = result.net_error();
    if (rv == net::ERR_IO_PENDING) {
      // OnAsyncCompletion will be called later.
      return rv;
    }

    if (rv == net::OK) {
      *entry_ =
          new ServiceWorkerDiskCacheEntry(result.ReleaseEntry(), owner_.get());
    }

    return rv;
  }

  void OnAsyncCompletion(disk_cache::EntryResult result) {
    int rv = result.net_error();
    if (rv == net::OK) {
      if (owner_) {
        *entry_ = new ServiceWorkerDiskCacheEntry(result.ReleaseEntry(),
                                                  owner_.get());
      } else {
        result.ReleaseEntry()->Close();
        rv = net::ERR_ABORTED;
      }
    }
    std::move(callback_).Run(rv);
  }

  base::WeakPtr<ServiceWorkerDiskCache> owner_;
  ServiceWorkerDiskCacheEntry** entry_;
  net::CompletionOnceCallback callback_;
};

}  // namespace

ServiceWorkerDiskCache::ServiceWorkerDiskCache() = default;

ServiceWorkerDiskCache::~ServiceWorkerDiskCache() {
  Disable();
}

net::Error ServiceWorkerDiskCache::InitWithDiskBackend(
    const base::FilePath& disk_cache_directory,
    bool force,
    base::OnceClosure post_cleanup_callback,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Init(net::APP_CACHE, disk_cache_directory,
              std::numeric_limits<int64_t>::max(), force,
              std::move(post_cleanup_callback), std::move(callback));
}

net::Error ServiceWorkerDiskCache::InitWithMemBackend(
    int64_t mem_cache_size,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Init(net::MEMORY_CACHE, base::FilePath(), mem_cache_size, false,
              base::OnceClosure(), std::move(callback));
}

void ServiceWorkerDiskCache::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_disabled_)
    return;

  is_disabled_ = true;

  if (create_backend_callback_.get()) {
    create_backend_callback_->Cancel();
    create_backend_callback_ = nullptr;
    OnCreateBackendComplete(net::ERR_ABORTED);
  }

  // We need to close open file handles in order to reinitialize the
  // service worker system on the fly. File handles held in both entries and in
  // the main disk_cache::Backend class need to be released.
  for (ServiceWorkerDiskCacheEntry* entry : open_entries_) {
    entry->Abandon();
  }
  open_entries_.clear();
  disk_cache_.reset();
}

net::Error ServiceWorkerDiskCache::CreateEntry(
    int64_t key,
    ServiceWorkerDiskCacheEntry** entry,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.emplace_back(PendingCallType::kCreate, key, entry,
                                std::move(callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::CreateEntry(weak_factory_.GetWeakPtr(), key, entry,
                                 std::move(callback));
}

net::Error ServiceWorkerDiskCache::OpenEntry(
    int64_t key,
    ServiceWorkerDiskCacheEntry** entry,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.emplace_back(PendingCallType::kOpen, key, entry,
                                std::move(callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::OpenEntry(weak_factory_.GetWeakPtr(), key, entry,
                               std::move(callback));
}

net::Error ServiceWorkerDiskCache::DoomEntry(
    int64_t key,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.emplace_back(PendingCallType::kDoom, key, nullptr,
                                std::move(callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::DoomEntry(weak_factory_.GetWeakPtr(), key,
                               std::move(callback));
}

base::WeakPtr<ServiceWorkerDiskCache> ServiceWorkerDiskCache::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

ServiceWorkerDiskCache::PendingCall::PendingCall(
    PendingCallType call_type,
    int64_t key,
    ServiceWorkerDiskCacheEntry** entry,
    net::CompletionOnceCallback callback)
    : call_type(call_type),
      key(key),
      entry(entry),
      callback(std::move(callback)) {}

ServiceWorkerDiskCache::PendingCall::PendingCall(PendingCall&& other) = default;

ServiceWorkerDiskCache::PendingCall::~PendingCall() = default;

net::Error ServiceWorkerDiskCache::Init(net::CacheType cache_type,
                                        const base::FilePath& cache_directory,
                                        int64_t cache_size,
                                        bool force,
                                        base::OnceClosure post_cleanup_callback,
                                        net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_initializing_or_waiting_to_initialize() && !disk_cache_.get());
  is_disabled_ = false;
  create_backend_callback_ =
      base::MakeRefCounted<CreateBackendCallbackShim>(this);
  disk_cache::ResetHandling reset_handling =
      force ? disk_cache::ResetHandling::kResetOnError
            : disk_cache::ResetHandling::kNeverReset;

  net::Error return_value = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, cache_directory, cache_size,
      reset_handling, nullptr, &(create_backend_callback_->backend_ptr_),
      std::move(post_cleanup_callback),
      base::BindOnce(&CreateBackendCallbackShim::Callback,
                     create_backend_callback_));
  if (return_value == net::ERR_IO_PENDING)
    init_callback_ = std::move(callback);
  else
    OnCreateBackendComplete(return_value);
  return return_value;
}

void ServiceWorkerDiskCache::OnCreateBackendComplete(int return_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (return_value == net::OK) {
    disk_cache_ = std::move(create_backend_callback_->backend_ptr_);
  }
  create_backend_callback_ = nullptr;

  // Invoke our clients callback function.
  if (!init_callback_.is_null()) {
    std::move(init_callback_).Run(return_value);
  }

  // Service pending calls that were queued up while we were initializing.
  for (auto& call : pending_calls_) {
    // This is safe, because the callback will only be called once.
    net::CompletionRepeatingCallback copyable_callback =
        base::AdaptCallbackForRepeating(std::move(call.callback));
    return_value = net::ERR_FAILED;
    switch (call.call_type) {
      case PendingCallType::kCreate:
        return_value = CreateEntry(call.key, call.entry, copyable_callback);
        break;
      case PendingCallType::kOpen:
        return_value = OpenEntry(call.key, call.entry, copyable_callback);
        break;
      case PendingCallType::kDoom:
        return_value = DoomEntry(call.key, copyable_callback);
        break;
    }
    // disk_cache::{Create,Open,Doom}Entry() call their callbacks iff they
    // return net::ERR_IO_PENDING. In this case, the callback was not called.
    // However, the corresponding ServiceWorkerDiskCache wrapper returned
    // net::ERR_IO_PENDING as it queued up the pending call. To follow the
    // disk_cache API contract, we need to call the callback ourselves here.
    if (return_value != net::ERR_IO_PENDING)
      copyable_callback.Run(return_value);
  }
  pending_calls_.clear();
}

void ServiceWorkerDiskCache::AddOpenEntry(ServiceWorkerDiskCacheEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  open_entries_.insert(entry);
}

void ServiceWorkerDiskCache::RemoveOpenEntry(
    ServiceWorkerDiskCacheEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  open_entries_.erase(entry);
}

}  // namespace content

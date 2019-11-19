// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_disk_cache.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/cache_type.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"

namespace content {

// A callback shim that provides storage for the 'backend_ptr' value
// and will delete a resulting ptr if completion occurs after its
// been canceled.
class AppCacheDiskCache::CreateBackendCallbackShim
    : public base::RefCounted<CreateBackendCallbackShim> {
 public:
  explicit CreateBackendCallbackShim(AppCacheDiskCache* object)
      : appcache_diskcache_(object) {
  }

  void Cancel() { appcache_diskcache_ = nullptr; }

  void Callback(int return_value) {
    if (appcache_diskcache_)
      appcache_diskcache_->OnCreateBackendComplete(return_value);
  }

  std::unique_ptr<disk_cache::Backend> backend_ptr_;  // Accessed directly.

 private:
  friend class base::RefCounted<CreateBackendCallbackShim>;

  ~CreateBackendCallbackShim() {
  }

  AppCacheDiskCache* appcache_diskcache_;  // Unowned pointer.
};

AppCacheDiskCacheEntry::AppCacheDiskCacheEntry(
    disk_cache::Entry* disk_cache_entry,
    AppCacheDiskCache* cache)
    : disk_cache_entry_(disk_cache_entry), cache_(cache) {
  DCHECK(disk_cache_entry);
  DCHECK(cache);
  cache_->AddOpenEntry(this);
}

AppCacheDiskCacheEntry::~AppCacheDiskCacheEntry() {
  if (cache_)
    cache_->RemoveOpenEntry(this);
}

int AppCacheDiskCacheEntry::Read(int index,
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

int AppCacheDiskCacheEntry::Write(int index,
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

int64_t AppCacheDiskCacheEntry::GetSize(int index) {
  return disk_cache_entry_ ? disk_cache_entry_->GetDataSize(index) : 0L;
}

void AppCacheDiskCacheEntry::Close() {
  if (disk_cache_entry_)
    disk_cache_entry_->Close();
  delete this;
}

void AppCacheDiskCacheEntry::Abandon() {
  cache_ = nullptr;
  disk_cache_entry_->Close();
  disk_cache_entry_ = nullptr;
}

namespace {

// Separate object to hold state for each Create, Delete, or Doom call
// while the call is in-flight and to produce an EntryImpl upon completion.
class ActiveCall : public base::RefCounted<ActiveCall> {
 public:
  ActiveCall(const base::WeakPtr<AppCacheDiskCache>& owner,
             AppCacheDiskCacheEntry** entry,
             net::CompletionOnceCallback callback)
      : owner_(owner), entry_(entry), callback_(std::move(callback)) {
    DCHECK(owner_);
  }

  static net::Error CreateEntry(const base::WeakPtr<AppCacheDiskCache>& owner,
                                int64_t key,
                                AppCacheDiskCacheEntry** entry,
                                net::CompletionOnceCallback callback) {
    scoped_refptr<ActiveCall> active_call =
        base::MakeRefCounted<ActiveCall>(owner, entry, std::move(callback));
    disk_cache::EntryResult result = owner->disk_cache()->CreateEntry(
        base::NumberToString(key), net::HIGHEST,
        base::BindOnce(&ActiveCall::OnAsyncCompletion, active_call));
    return active_call->HandleImmediateReturnValue(std::move(result));
  }

  static net::Error OpenEntry(const base::WeakPtr<AppCacheDiskCache>& owner,
                              int64_t key,
                              AppCacheDiskCacheEntry** entry,
                              net::CompletionOnceCallback callback) {
    scoped_refptr<ActiveCall> active_call =
        base::MakeRefCounted<ActiveCall>(owner, entry, std::move(callback));
    disk_cache::EntryResult result = owner->disk_cache()->OpenEntry(
        base::NumberToString(key), net::HIGHEST,
        base::BindOnce(&ActiveCall::OnAsyncCompletion, active_call));
    return active_call->HandleImmediateReturnValue(std::move(result));
  }

  static net::Error DoomEntry(const base::WeakPtr<AppCacheDiskCache>& owner,
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
      *entry_ = new AppCacheDiskCacheEntry(result.ReleaseEntry(), owner_.get());
    }

    return rv;
  }

  void OnAsyncCompletion(disk_cache::EntryResult result) {
    int rv = result.net_error();
    if (rv == net::OK) {
      if (owner_) {
        *entry_ =
            new AppCacheDiskCacheEntry(result.ReleaseEntry(), owner_.get());
      } else {
        result.ReleaseEntry()->Close();
        rv = net::ERR_ABORTED;
      }
    }
    std::move(callback_).Run(rv);
  }

  base::WeakPtr<AppCacheDiskCache> owner_;
  AppCacheDiskCacheEntry** entry_;
  net::CompletionOnceCallback callback_;
};

}  // namespace

AppCacheDiskCache::AppCacheDiskCache()
#if defined(APPCACHE_USE_SIMPLE_CACHE)
    : AppCacheDiskCache("DiskCache.AppCache", true)
#else
    : AppCacheDiskCache("DiskCache.AppCache", false)
#endif
{
}

AppCacheDiskCache::~AppCacheDiskCache() {
  Disable();
}

net::Error AppCacheDiskCache::InitWithDiskBackend(
    const base::FilePath& disk_cache_directory,
    bool force,
    base::OnceClosure post_cleanup_callback,
    net::CompletionOnceCallback callback) {
  return Init(net::APP_CACHE, disk_cache_directory,
              std::numeric_limits<int64_t>::max(), force,
              std::move(post_cleanup_callback), std::move(callback));
}

net::Error AppCacheDiskCache::InitWithMemBackend(
    int64_t mem_cache_size,
    net::CompletionOnceCallback callback) {
  return Init(net::MEMORY_CACHE, base::FilePath(), mem_cache_size, false,
              base::OnceClosure(), std::move(callback));
}

void AppCacheDiskCache::Disable() {
  if (is_disabled_)
    return;

  is_disabled_ = true;

  if (create_backend_callback_.get()) {
    create_backend_callback_->Cancel();
    create_backend_callback_ = nullptr;
    OnCreateBackendComplete(net::ERR_ABORTED);
  }

  // We need to close open file handles in order to reinitalize the
  // appcache system on the fly. File handles held in both entries and in
  // the main disk_cache::Backend class need to be released.
  for (AppCacheDiskCacheEntry* entry : open_entries_) {
    entry->Abandon();
  }
  open_entries_.clear();
  disk_cache_.reset();
}

net::Error AppCacheDiskCache::CreateEntry(
    int64_t key,
    AppCacheDiskCacheEntry** entry,
    net::CompletionOnceCallback callback) {
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.push_back(
        PendingCall(CREATE, key, entry, std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::CreateEntry(weak_factory_.GetWeakPtr(), key, entry,
                                 std::move(callback));
}

net::Error AppCacheDiskCache::OpenEntry(int64_t key,
                                        AppCacheDiskCacheEntry** entry,
                                        net::CompletionOnceCallback callback) {
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.push_back(
        PendingCall(OPEN, key, entry, std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::OpenEntry(weak_factory_.GetWeakPtr(), key, entry,
                               std::move(callback));
}

net::Error AppCacheDiskCache::DoomEntry(int64_t key,
                                        net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing_or_waiting_to_initialize()) {
    pending_calls_.push_back(
        PendingCall(DOOM, key, nullptr, std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return ActiveCall::DoomEntry(weak_factory_.GetWeakPtr(), key,
                               std::move(callback));
}

base::WeakPtr<AppCacheDiskCache> AppCacheDiskCache::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

AppCacheDiskCache::AppCacheDiskCache(const char* uma_name,
                                     bool use_simple_cache)
    : use_simple_cache_(use_simple_cache),
      is_disabled_(false),
      is_waiting_to_initialize_(false),
      uma_name_(uma_name) {}

AppCacheDiskCache::PendingCall::PendingCall()
    : call_type(CREATE), key(0), entry(nullptr) {}

AppCacheDiskCache::PendingCall::PendingCall(
    PendingCallType call_type,
    int64_t key,
    AppCacheDiskCacheEntry** entry,
    net::CompletionOnceCallback callback)
    : call_type(call_type),
      key(key),
      entry(entry),
      callback(std::move(callback)) {}

AppCacheDiskCache::PendingCall::PendingCall(PendingCall&& other) = default;

AppCacheDiskCache::PendingCall::~PendingCall() = default;

net::Error AppCacheDiskCache::Init(net::CacheType cache_type,
                                   const base::FilePath& cache_directory,
                                   int64_t cache_size,
                                   bool force,
                                   base::OnceClosure post_cleanup_callback,
                                   net::CompletionOnceCallback callback) {
  DCHECK(!is_initializing_or_waiting_to_initialize() && !disk_cache_.get());
  is_disabled_ = false;
  create_backend_callback_ =
      base::MakeRefCounted<CreateBackendCallbackShim>(this);
  disk_cache::ResetHandling reset_handling =
      force ? disk_cache::ResetHandling::kResetOnError
            : disk_cache::ResetHandling::kNeverReset;

  net::Error return_value = disk_cache::CreateCacheBackend(
      cache_type,
      use_simple_cache_ ? net::CACHE_BACKEND_SIMPLE
                        : net::CACHE_BACKEND_DEFAULT,
      cache_directory, cache_size, reset_handling, nullptr,
      &(create_backend_callback_->backend_ptr_),
      std::move(post_cleanup_callback),
      base::BindOnce(&CreateBackendCallbackShim::Callback,
                     create_backend_callback_));
  if (return_value == net::ERR_IO_PENDING)
    init_callback_ = std::move(callback);
  else
    OnCreateBackendComplete(return_value);
  return return_value;
}

void AppCacheDiskCache::OnCreateBackendComplete(int return_value) {
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
      case CREATE:
        return_value = CreateEntry(call.key, call.entry, copyable_callback);
        break;
      case OPEN:
        return_value = OpenEntry(call.key, call.entry, copyable_callback);
        break;
      case DOOM:
        return_value = DoomEntry(call.key, copyable_callback);
        break;
    }
    if (return_value != net::ERR_IO_PENDING)
      copyable_callback.Run(return_value);
  }
  pending_calls_.clear();
}

}  // namespace content

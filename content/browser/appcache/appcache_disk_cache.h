// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace content {

class AppCacheDiskCache;

// Thin wrapper around disk_cache::Entry.
class CONTENT_EXPORT AppCacheDiskCacheEntry {
 public:
  // The newly created entry takes ownership of |disk_cache_entry| and closes it
  // on destruction. |cache| must outlive the newly created entry.
  AppCacheDiskCacheEntry(disk_cache::Entry* disk_cache_entry,
                         AppCacheDiskCache* cache);

  int Read(int index,
           int64_t offset,
           net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback);

  int Write(int index,
            int64_t offset,
            net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback);
  int64_t GetSize(int index);
  void Close();

  // Should only be called by AppCacheDiskCache.
  void Abandon();

 private:
  // Call Close() instead of calling this directly.
  ~AppCacheDiskCacheEntry();

  // The disk_cache::Entry is owned by this entry and closed on destruction.
  disk_cache::Entry* disk_cache_entry_;

  // The cache that this entry belongs to.
  AppCacheDiskCache* cache_;
};

// An implementation of AppCacheDiskCache that
// uses net::DiskCache as the backing store.
class CONTENT_EXPORT AppCacheDiskCache {
 public:
  AppCacheDiskCache();
  virtual ~AppCacheDiskCache();

  // Initializes the object to use disk backed storage.
  net::Error InitWithDiskBackend(const base::FilePath& disk_cache_directory,
                                 bool force,
                                 base::OnceClosure post_cleanup_callback,
                                 net::CompletionOnceCallback callback);

  // Initializes the object to use memory only storage.
  // This is used for Chrome's incognito browsing.
  net::Error InitWithMemBackend(int64_t disk_cache_size,
                                net::CompletionOnceCallback callback);

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  net::Error CreateEntry(int64_t key,
                         AppCacheDiskCacheEntry** entry,
                         net::CompletionOnceCallback callback);
  net::Error OpenEntry(int64_t key,
                       AppCacheDiskCacheEntry** entry,
                       net::CompletionOnceCallback callback);
  net::Error DoomEntry(int64_t key, net::CompletionOnceCallback callback);

  base::WeakPtr<AppCacheDiskCache> GetWeakPtr();

  void set_is_waiting_to_initialize(bool is_waiting_to_initialize) {
    is_waiting_to_initialize_ = is_waiting_to_initialize;
  }

  const char* uma_name() { return uma_name_; }

  disk_cache::Backend* disk_cache() { return disk_cache_.get(); }

 protected:
  // |uma_name| must remain valid for the life of the object.
  explicit AppCacheDiskCache(const char* uma_name, bool use_simple_cache);

 private:
  class CreateBackendCallbackShim;
  friend class AppCacheDiskCacheEntry;

  // PendingCalls allow CreateEntry, OpenEntry, and DoomEntry to be called
  // immediately after construction, without waiting for the
  // underlying disk_cache::Backend to be fully constructed. Early
  // calls are queued up and serviced once the disk_cache::Backend is
  // really ready to go.
  enum PendingCallType {
    CREATE,
    OPEN,
    DOOM
  };
  struct PendingCall {
    PendingCall();
    PendingCall(PendingCallType call_type,
                int64_t key,
                AppCacheDiskCacheEntry** entry,
                net::CompletionOnceCallback callback);
    PendingCall(PendingCall&& other);

    ~PendingCall();

    PendingCallType call_type;
    int64_t key;
    AppCacheDiskCacheEntry** entry;
    net::CompletionOnceCallback callback;
  };

  bool is_initializing_or_waiting_to_initialize() const {
    return create_backend_callback_.get() != NULL || is_waiting_to_initialize_;
  }

  net::Error Init(net::CacheType cache_type,
                  const base::FilePath& directory,
                  int64_t cache_size,
                  bool force,
                  base::OnceClosure post_cleanup_callback,
                  net::CompletionOnceCallback callback);
  void OnCreateBackendComplete(int return_value);

  // Called by AppCacheDiskCacheEntry constructor.
  void AddOpenEntry(AppCacheDiskCacheEntry* entry) {
    open_entries_.insert(entry);
  }
  // Called by AppCacheDiskCacheEntry destructor.
  void RemoveOpenEntry(AppCacheDiskCacheEntry* entry) {
    open_entries_.erase(entry);
  }

  bool use_simple_cache_;
  bool is_disabled_;
  bool is_waiting_to_initialize_;
  net::CompletionOnceCallback init_callback_;
  scoped_refptr<CreateBackendCallbackShim> create_backend_callback_;
  std::vector<PendingCall> pending_calls_;
  std::set<AppCacheDiskCacheEntry*> open_entries_;
  std::unique_ptr<disk_cache::Backend> disk_cache_;
  const char* const uma_name_;

  base::WeakPtrFactory<AppCacheDiskCache> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_

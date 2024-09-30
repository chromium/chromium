// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_disk_cache.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace gpu {

// GpuDiskCacheEntry handles the work of caching/updating the cached
// blobs.
class GpuDiskCacheEntry {
 public:
  GpuDiskCacheEntry(GpuDiskCache* cache,
                    const std::string& key,
                    const std::string& blob);

  GpuDiskCacheEntry(const GpuDiskCacheEntry&) = delete;
  GpuDiskCacheEntry& operator=(const GpuDiskCacheEntry&) = delete;

  ~GpuDiskCacheEntry();

  void Cache();
  void OnOpComplete(int rv);
  void OnEntryOpenComplete(disk_cache::EntryResult result);

 private:
  enum OpType {
    OPEN_ENTRY,
    WRITE_DATA,
    CREATE_ENTRY,
    REOPEN_ENTRY,
  };

  int OpenEntry();

  int OpenCallback(int rv);
  int ReopenCallback(int rv);
  int WriteCallback(int rv);
  int IOComplete(int rv);

  THREAD_CHECKER(thread_checker_);

  raw_ptr<GpuDiskCache> cache_;
  OpType op_type_ = OPEN_ENTRY;
  std::string key_;
  std::string blob_;
  raw_ptr<disk_cache::Entry, AcrossTasksDanglingUntriaged> entry_;
  base::WeakPtr<GpuDiskCacheEntry> weak_ptr_;
  base::WeakPtrFactory<GpuDiskCacheEntry> weak_ptr_factory_{this};
};

// GpuDiskCacheReadHelper is used to load all of the cached blobs from the
// disk cache and send to the memory cache.
class GpuDiskCacheReadHelper {
 public:
  using BlobLoadedCallback = GpuDiskCache::BlobLoadedCallback;
  GpuDiskCacheReadHelper(GpuDiskCache* cache,
                         const BlobLoadedCallback& callback);

  GpuDiskCacheReadHelper(const GpuDiskCacheReadHelper&) = delete;
  GpuDiskCacheReadHelper& operator=(const GpuDiskCacheReadHelper&) = delete;

  ~GpuDiskCacheReadHelper();

  void LoadCache();
  void OnOpComplete(int rv);
  void OnEntryOpenComplete(disk_cache::EntryResult result);

 private:
  enum OpType {
    TERMINATE,
    OPEN_NEXT,
    OPEN_NEXT_COMPLETE,
    READ_COMPLETE,
    ITERATION_FINISHED
  };

  int OpenNextEntry();
  int OpenNextEntryComplete(int rv);
  int ReadComplete(int rv);
  int IterationComplete(int rv);

  THREAD_CHECKER(thread_checker_);

  raw_ptr<GpuDiskCache> cache_;
  BlobLoadedCallback blob_loaded_callback_;
  OpType op_type_ = OPEN_NEXT;
  std::unique_ptr<disk_cache::Backend::Iterator> iter_;
  scoped_refptr<net::IOBufferWithSize> buf_;
  raw_ptr<disk_cache::Entry, AcrossTasksDanglingUntriaged> entry_;
  base::WeakPtrFactory<GpuDiskCacheReadHelper> weak_ptr_factory_{this};
};

class GpuDiskCacheClearHelper {
 public:
  GpuDiskCacheClearHelper(GpuDiskCacheFactory* factory,
                          scoped_refptr<GpuDiskCache> cache,
                          base::Time delete_begin,
                          base::Time delete_end,
                          base::OnceClosure callback);

  GpuDiskCacheClearHelper(const GpuDiskCacheClearHelper&) = delete;
  GpuDiskCacheClearHelper& operator=(const GpuDiskCacheClearHelper&) = delete;

  ~GpuDiskCacheClearHelper();

  void Clear();

 private:
  enum OpType { TERMINATE, VERIFY_CACHE_SETUP, DELETE_CACHE };

  void DoClearGpuCache(int rv);

  THREAD_CHECKER(thread_checker_);

  raw_ptr<GpuDiskCacheFactory> factory_;
  scoped_refptr<GpuDiskCache> cache_;
  OpType op_type_ = VERIFY_CACHE_SETUP;
  base::Time delete_begin_;
  base::Time delete_end_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<GpuDiskCacheClearHelper> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// GpuDiskCacheEntry

GpuDiskCacheEntry::GpuDiskCacheEntry(GpuDiskCache* cache,
                                     const std::string& key,
                                     const std::string& blob)
    : cache_(cache), key_(key), blob_(blob), entry_(nullptr) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuDiskCacheEntry::~GpuDiskCacheEntry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (entry_)
    entry_->Close();
}

int GpuDiskCacheEntry::OpenEntry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto callback = base::BindOnce(&GpuDiskCacheEntry::OnEntryOpenComplete,
                                 weak_ptr_factory_.GetWeakPtr());

  disk_cache::EntryResult result =
      cache_->backend()->OpenEntry(key_, net::HIGHEST, std::move(callback));
  int rv = result.net_error();
  if (rv != net::ERR_IO_PENDING) {
    OnEntryOpenComplete(std::move(result));
  }
  return rv;
}

void GpuDiskCacheEntry::Cache() {
  OpenEntry();
}

void GpuDiskCacheEntry::OnOpComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The function calls inside the switch block below can end up destroying
  // |this|. So hold on to a WeakPtr<>, and terminate the while loop if |this|
  // has been destroyed.
  auto weak_ptr = std::move(weak_ptr_);
  do {
    switch (op_type_) {
      case OPEN_ENTRY:
        rv = OpenCallback(rv);
        break;
      case CREATE_ENTRY:
        rv = WriteCallback(rv);
        break;
      case WRITE_DATA:
        rv = IOComplete(rv);
        break;
      case REOPEN_ENTRY:
        rv = ReopenCallback(rv);
        break;
    }
  } while (rv != net::ERR_IO_PENDING && weak_ptr);
  if (weak_ptr)
    weak_ptr_ = std::move(weak_ptr);
}

void GpuDiskCacheEntry::OnEntryOpenComplete(disk_cache::EntryResult result) {
  int rv = result.net_error();
  entry_ = result.ReleaseEntry();
  OnOpComplete(rv);
}

int GpuDiskCacheEntry::OpenCallback(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (rv == net::OK) {
    cache_->backend()->OnExternalCacheHit(key_);
    cache_->EntryComplete(this);
    return rv;
  }

  op_type_ = CREATE_ENTRY;

  auto callback = base::BindOnce(&GpuDiskCacheEntry::OnEntryOpenComplete,
                                 weak_ptr_factory_.GetWeakPtr());

  disk_cache::EntryResult create_result =
      cache_->backend()->CreateEntry(key_, net::HIGHEST, std::move(callback));
  rv = create_result.net_error();

  if (rv != net::ERR_IO_PENDING)
    entry_ = create_result.ReleaseEntry();  // may be nullptr
  return rv;
}

int GpuDiskCacheEntry::ReopenCallback(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (rv == net::OK) {
    cache_->backend()->OnExternalCacheHit(key_);
  } else {
    LOG(ERROR) << "Failed retry to open blob cache entry: " << rv;
  }
  cache_->EntryComplete(this);
  return rv;
}

int GpuDiskCacheEntry::WriteCallback(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (rv != net::OK) {
    // We might have failed to create the entry because another create on the
    // same key happened before. To verify, try re-opening the entry.
    if (rv == net::ERR_FAILED) {
      op_type_ = REOPEN_ENTRY;
      return OpenEntry();
    } else {
      LOG(ERROR) << "Failed to create blob cache entry: " << rv;
      cache_->EntryComplete(this);
      return rv;
    }
  }

  op_type_ = WRITE_DATA;
  auto io_buf = base::MakeRefCounted<net::StringIOBuffer>(blob_);
  return entry_->WriteData(1, 0, io_buf.get(), blob_.length(),
                           base::BindOnce(&GpuDiskCacheEntry::OnOpComplete,
                                          weak_ptr_factory_.GetWeakPtr()),
                           false);
}

int GpuDiskCacheEntry::IOComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cache_->EntryComplete(this);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// GpuDiskCacheReadHelper

GpuDiskCacheReadHelper::GpuDiskCacheReadHelper(
    GpuDiskCache* cache,
    const BlobLoadedCallback& callback)
    : cache_(cache),
      blob_loaded_callback_(callback),
      buf_(nullptr),
      entry_(nullptr) {}

GpuDiskCacheReadHelper::~GpuDiskCacheReadHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (entry_)
    entry_->Close();
  iter_ = nullptr;
}

void GpuDiskCacheReadHelper::LoadCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnOpComplete(net::OK);
}

void GpuDiskCacheReadHelper::OnOpComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  do {
    switch (op_type_) {
      case OPEN_NEXT:
        rv = OpenNextEntry();
        break;
      case OPEN_NEXT_COMPLETE:
        rv = OpenNextEntryComplete(rv);
        break;
      case READ_COMPLETE:
        rv = ReadComplete(rv);
        break;
      case ITERATION_FINISHED:
        rv = IterationComplete(rv);
        break;
      case TERMINATE:
        cache_->ReadComplete();
        rv = net::ERR_IO_PENDING;  // break the loop
        break;
    }
  } while (rv != net::ERR_IO_PENDING);
}

void GpuDiskCacheReadHelper::OnEntryOpenComplete(
    disk_cache::EntryResult result) {
  int rv = result.net_error();
  entry_ = result.ReleaseEntry();
  OnOpComplete(rv);
}

int GpuDiskCacheReadHelper::OpenNextEntry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  op_type_ = OPEN_NEXT_COMPLETE;
  if (!iter_)
    iter_ = cache_->backend()->CreateIterator();

  auto callback = base::BindOnce(&GpuDiskCacheReadHelper::OnEntryOpenComplete,
                                 weak_ptr_factory_.GetWeakPtr());

  disk_cache::EntryResult result = iter_->OpenNextEntry(std::move(callback));
  int rv = result.net_error();

  if (rv != net::ERR_IO_PENDING)
    entry_ = result.ReleaseEntry();
  return rv;
}

int GpuDiskCacheReadHelper::OpenNextEntryComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (rv == net::ERR_FAILED) {
    iter_.reset();
    op_type_ = ITERATION_FINISHED;
    return net::OK;
  }

  if (rv < 0)
    return rv;

  op_type_ = READ_COMPLETE;
  buf_ = base::MakeRefCounted<net::IOBufferWithSize>(entry_->GetDataSize(1));
  return entry_->ReadData(1, 0, buf_.get(), buf_->size(),
                          base::BindOnce(&GpuDiskCacheReadHelper::OnOpComplete,
                                         weak_ptr_factory_.GetWeakPtr()));
}

int GpuDiskCacheReadHelper::ReadComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (rv && rv == buf_->size() && !blob_loaded_callback_.is_null()) {
    blob_loaded_callback_.Run(entry_->GetKey(),
                              std::string(buf_->data(), buf_->size()));
  }

  buf_ = nullptr;
  entry_->Close();
  entry_ = nullptr;

  op_type_ = OPEN_NEXT;
  return net::OK;
}

int GpuDiskCacheReadHelper::IterationComplete(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  iter_.reset();
  op_type_ = TERMINATE;
  return net::OK;
}

////////////////////////////////////////////////////////////////////////////////
// GpuDiskCacheClearHelper

GpuDiskCacheClearHelper::GpuDiskCacheClearHelper(
    GpuDiskCacheFactory* factory,
    scoped_refptr<GpuDiskCache> cache,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure callback)
    : factory_(factory),
      cache_(std::move(cache)),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      callback_(std::move(callback)) {}

GpuDiskCacheClearHelper::~GpuDiskCacheClearHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void GpuDiskCacheClearHelper::Clear() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DoClearGpuCache(net::OK);
}

void GpuDiskCacheClearHelper::DoClearGpuCache(int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  while (rv != net::ERR_IO_PENDING) {
    switch (op_type_) {
      case VERIFY_CACHE_SETUP:
        rv = cache_->SetAvailableCallback(
            base::BindOnce(&GpuDiskCacheClearHelper::DoClearGpuCache,
                           weak_ptr_factory_.GetWeakPtr()));
        op_type_ = DELETE_CACHE;
        break;
      case DELETE_CACHE:
        rv = cache_->Clear(
            delete_begin_, delete_end_,
            base::BindOnce(&GpuDiskCacheClearHelper::DoClearGpuCache,
                           weak_ptr_factory_.GetWeakPtr()));
        op_type_ = TERMINATE;
        break;
      case TERMINATE:
        std::move(callback_).Run();
        // Calling CacheCleared() destroys |this|.
        factory_->CacheCleared(cache_.get());
        rv = net::ERR_IO_PENDING;  // Break the loop.
        break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// GpuDiskCacheFactory

GpuDiskCacheFactory::GpuDiskCacheFactory(
    const HandleToPathMap& reserved_handles) {
  for (const auto& [handle, path] : reserved_handles) {
    CHECK(IsReservedGpuDiskCacheHandle(handle));
    handle_to_path_map_[handle] = path;
    path_to_handle_map_[path] = handle;
  }
}

GpuDiskCacheFactory::~GpuDiskCacheFactory() = default;

GpuDiskCacheHandle GpuDiskCacheFactory::GetCacheHandle(
    GpuDiskCacheType type,
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = path_to_handle_map_.find(path);
  if (it != path_to_handle_map_.end()) {
    DCHECK(GetHandleType(it->second) == type);
    handle_ref_counts_[it->second]++;
    return it->second;
  }

  // Create a new handle and record it. We don't expect large number of handles
  // and the system cannot handle it since the map is not culled. As a result,
  // we are returning an empty for the handle which will result in Get/Create
  // returning nullptr, effectively stopping cache usage when we run out of
  // handles.
  GpuDiskCacheHandle handle;
  if (next_available_handle_ == std::numeric_limits<int32_t>::max()) {
    LOG(ERROR) << "GpuDiskCacheFactory ran out of handles for caches, caching "
                  "will be disabled until a new session.";
    return handle;
  }

  int32_t raw_handle = next_available_handle_++;
  switch (type) {
    case GpuDiskCacheType::kGlShaders:
      handle = GpuDiskCacheGlShaderHandle(raw_handle);
      break;
    case GpuDiskCacheType::kDawnWebGPU:
      handle = GpuDiskCacheDawnWebGPUHandle(raw_handle);
      break;
    case GpuDiskCacheType::kDawnGraphite:
      handle = GpuDiskCacheDawnGraphiteHandle(raw_handle);
      break;
  }
  handle_to_path_map_[handle] = path;
  path_to_handle_map_[path] = handle;
  handle_ref_counts_[handle]++;

  return handle;
}

void GpuDiskCacheFactory::ReleaseCacheHandle(GpuDiskCache* cache) {
  // Get the handle related to the cache via the path.
  auto it = path_to_handle_map_.find(cache->cache_path_);
  CHECK(it != path_to_handle_map_.end(), base::NotFatalUntil::M130);
  const base::FilePath& path = it->first;
  const GpuDiskCacheHandle& handle = it->second;

  // Special case where we don't need to do anything if the handle is a reserved
  // handle.
  if (gpu::IsReservedGpuDiskCacheHandle(handle)) {
    return;
  }

  // We should never be decrementing the ref-count past 0.
  DCHECK_GT(handle_ref_counts_[handle], 0u);
  if (--handle_ref_counts_[handle] > 0u) {
    return;
  }
  handle_to_path_map_.erase(handle);
  handle_ref_counts_.erase(handle);
  path_to_handle_map_.erase(path);
}

scoped_refptr<GpuDiskCache> GpuDiskCacheFactory::Get(
    const GpuDiskCacheHandle& handle) {
  auto handle_it = handle_to_path_map_.find(handle);
  if (handle_it != handle_to_path_map_.end()) {
    auto path_it = gpu_cache_map_.find(handle_it->second);
    if (path_it != gpu_cache_map_.end()) {
      return path_it->second.get();
    }
  }
  return nullptr;
}

scoped_refptr<GpuDiskCache> GpuDiskCacheFactory::Create(
    const GpuDiskCacheHandle& handle,
    const BlobLoadedForCacheCallback& blob_loaded_cb,
    CacheDestroyedCallback cache_destroyed_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(Get(handle) == nullptr);

  auto it = handle_to_path_map_.find(handle);
  if (it == handle_to_path_map_.end()) {
    return nullptr;
  }
  return GetOrCreateByPath(
      it->second, base::BindRepeating(blob_loaded_cb, handle),
      base::BindOnce(std::move(cache_destroyed_cb), handle));
}

scoped_refptr<GpuDiskCache> GpuDiskCacheFactory::GetOrCreateByPath(
    const base::FilePath& path,
    const GpuDiskCache::BlobLoadedCallback& blob_loaded_cb,
    base::OnceClosure cache_destroyed_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = gpu_cache_map_.find(path);
  if (iter != gpu_cache_map_.end())
    return iter->second.get();

  auto cache = base::WrapRefCounted(new GpuDiskCache(
      this, path, blob_loaded_cb, std::move(cache_destroyed_cb)));

  cache->Init();
  return cache;
}

void GpuDiskCacheFactory::AddToCache(const base::FilePath& key,
                                     GpuDiskCache* cache) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_cache_map_[key] = cache;
}

void GpuDiskCacheFactory::RemoveFromCache(const base::FilePath& key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_cache_map_.erase(key);
}

void GpuDiskCacheFactory::ClearByCache(scoped_refptr<GpuDiskCache> cache,
                                       base::Time delete_begin,
                                       base::Time delete_end,
                                       base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!callback.is_null());
  if (!cache) {
    std::move(callback).Run();
    return;
  }

  auto helper = std::make_unique<GpuDiskCacheClearHelper>(
      this, cache, delete_begin, delete_end, std::move(callback));

  // We could receive requests to clear the same cache with different
  // begin/end times. So, we keep a list of requests. If we haven't seen this
  // cache before we kick off the clear and add it to the list. If we have see
  // it already, then we already have a clear running. We add this clear to the
  // list and wait for any previous clears to finish.
  auto& queued_requests = gpu_clear_map_[cache.get()];
  queued_requests.push(std::move(helper));

  // If this is the first clear request, then we need to start the clear
  // operation on the helper.
  if (queued_requests.size() == 1) {
    queued_requests.front()->Clear();
  }
}

void GpuDiskCacheFactory::ClearByPath(const base::FilePath& path,
                                      base::Time delete_begin,
                                      base::Time delete_end,
                                      base::OnceClosure callback) {
  // Don't need to do anything if the path is empty.
  if (path.empty()) {
    std::move(callback).Run();
    return;
  }

  // Don't need to do anything if there is no cache in the path. This happens
  // when clearing an in-memory storage partition.
  auto iter = gpu_cache_map_.find(path);
  if (iter == gpu_cache_map_.end()) {
    std::move(callback).Run();
    return;
  }

  ClearByCache(iter->second.get(), delete_begin, delete_end,
               std::move(callback));
}

void GpuDiskCacheFactory::CacheCleared(GpuDiskCache* cache) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = gpu_clear_map_.find(cache);
  if (iter == gpu_clear_map_.end()) {
    LOG(ERROR) << "Completed clear but missing clear helper.";
    return;
  }

  iter->second.pop();

  // If there are remaining items in the list we trigger the Clear on the
  // next one.
  if (!iter->second.empty()) {
    iter->second.front()->Clear();
    return;
  }

  gpu_clear_map_.erase(iter);
}

////////////////////////////////////////////////////////////////////////////////
// GpuDiskCache

GpuDiskCache::GpuDiskCache(GpuDiskCacheFactory* factory,
                           const base::FilePath& cache_path,
                           const BlobLoadedCallback& blob_loaded_cb,
                           base::OnceClosure cache_destroyed_cb)
    : factory_(factory),
      cache_path_(cache_path),
      blob_loaded_cb_(blob_loaded_cb),
      cache_destroyed_cb_(std::move(cache_destroyed_cb)) {
  factory_->AddToCache(cache_path_, this);
}

GpuDiskCache::~GpuDiskCache() {
  factory_->RemoveFromCache(cache_path_);
  std::move(cache_destroyed_cb_).Run();
}

void GpuDiskCache::Init() {
  if (is_initialized_) {
    NOTREACHED_IN_MIGRATION();  // can't initialize disk cache twice.
    return;
  }
  is_initialized_ = true;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::SHADER_CACHE, net::CACHE_BACKEND_DEFAULT,
      /*file_operations=*/nullptr, cache_path_, GetDefaultGpuDiskCacheSize(),
      disk_cache::ResetHandling::kResetOnError,
      /*net_log=*/nullptr,
      base::BindOnce(&GpuDiskCache::CacheCreatedCallback, this));

  if (rv.net_error == net::OK) {
    NOTREACHED_IN_MIGRATION();  // This shouldn't actually happen with a
                                // non-memory backend.
    backend_ = std::move(rv.backend);
  }
}

void GpuDiskCache::Cache(const std::string& key, const std::string& blob) {
  if (!cache_available_)
    return;

  auto shim = std::make_unique<GpuDiskCacheEntry>(this, key, blob);
  shim->Cache();
  auto* ptr = shim.get();
  entries_.insert(std::make_pair(ptr, std::move(shim)));
}

int GpuDiskCache::Clear(base::Time begin_time,
                        base::Time end_time,
                        net::CompletionOnceCallback completion_callback) {
  int rv;
  if (begin_time.is_null()) {
    rv = backend_->DoomAllEntries(std::move(completion_callback));
  } else {
    rv = backend_->DoomEntriesBetween(begin_time, end_time,
                                      std::move(completion_callback));
  }
  return rv;
}

int32_t GpuDiskCache::Size() {
  if (!cache_available_)
    return -1;
  return backend_->GetEntryCount();
}

int GpuDiskCache::SetAvailableCallback(net::CompletionOnceCallback callback) {
  if (cache_available_)
    return net::OK;
  available_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

void GpuDiskCache::CacheCreatedCallback(disk_cache::BackendResult result) {
  if (result.net_error != net::OK) {
    LOG(ERROR) << "Gpu Cache Creation failed: " << result.net_error;
    return;
  }
  backend_ = std::move(result.backend);
  helper_ = std::make_unique<GpuDiskCacheReadHelper>(this, blob_loaded_cb_);
  helper_->LoadCache();
}

void GpuDiskCache::EntryComplete(GpuDiskCacheEntry* entry) {
  entries_.erase(entry);
  if (entries_.empty() && cache_complete_callback_)
    std::move(cache_complete_callback_).Run(net::OK);
}

void GpuDiskCache::ReadComplete() {
  helper_ = nullptr;

  // The cache is considered available after we have finished reading any
  // of the old cache values off disk. This prevents a potential race where we
  // are reading from disk and execute a cache clear at the same time.
  cache_available_ = true;
  if (available_callback_)
    std::move(available_callback_).Run(net::OK);
}

int GpuDiskCache::SetCacheCompleteCallback(
    net::CompletionOnceCallback callback) {
  if (entries_.empty()) {
    return net::OK;
  }
  cache_complete_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

}  // namespace gpu

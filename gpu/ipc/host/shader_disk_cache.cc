// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/shader_disk_cache.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace gpu {

namespace {

static const base::FilePath::CharType kGpuCachePath[] =
    FILE_PATH_LITERAL("GPUCache");

#if !defined(OS_ANDROID)
size_t GetCustomCacheSizeBytesIfExists(base::StringPiece switch_string) {
  const base::CommandLine& process_command_line =
      *base::CommandLine::ForCurrentProcess();
  size_t cache_size;
  if (process_command_line.HasSwitch(switch_string)) {
    if (base::StringToSizeT(
            process_command_line.GetSwitchValueASCII(switch_string),
            &cache_size)) {
      return cache_size * 1024;  // Bytes
    }
  }
  return 0;
}
#endif

}  // namespace

// ShaderDiskCacheEntry handles the work of caching/updating the cached
// shaders.
class ShaderDiskCacheEntry : public base::ThreadChecker {
 public:
  ShaderDiskCacheEntry(ShaderDiskCache* cache,
                       const std::string& key,
                       const std::string& shader);
  ~ShaderDiskCacheEntry();

  void Cache();
  void OnOpComplete(int rv);
  void set_entry(disk_cache::Entry* entry) { entry_ = entry; }

 private:
  enum OpType {
    OPEN_ENTRY,
    WRITE_DATA,
    CREATE_ENTRY,
  };

  int OpenCallback(int rv);
  int WriteCallback(int rv);
  int IOComplete(int rv);

  ShaderDiskCache* cache_;
  OpType op_type_;
  std::string key_;
  std::string shader_;
  disk_cache::Entry* entry_;
  base::WeakPtr<ShaderDiskCacheEntry> weak_ptr_;
  base::WeakPtrFactory<ShaderDiskCacheEntry> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCacheEntry);
};

// ShaderDiskReadHelper is used to load all of the cached shaders from the
// disk cache and send to the memory cache.
class ShaderDiskReadHelper : public base::ThreadChecker {
 public:
  using ShaderLoadedCallback = ShaderDiskCache::ShaderLoadedCallback;
  ShaderDiskReadHelper(ShaderDiskCache* cache,
                       const ShaderLoadedCallback& callback);
  ~ShaderDiskReadHelper();

  void LoadCache();
  void OnOpComplete(int rv);
  void set_entry(disk_cache::Entry* entry) { entry_ = entry; }

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

  ShaderDiskCache* cache_;
  ShaderLoadedCallback shader_loaded_callback_;
  OpType op_type_;
  std::unique_ptr<disk_cache::Backend::Iterator> iter_;
  scoped_refptr<net::IOBufferWithSize> buf_;
  disk_cache::Entry* entry_;
  base::WeakPtrFactory<ShaderDiskReadHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskReadHelper);
};

class ShaderClearHelper : public base::ThreadChecker {
 public:
  ShaderClearHelper(ShaderCacheFactory* factory,
                    scoped_refptr<ShaderDiskCache> cache,
                    const base::FilePath& path,
                    const base::Time& delete_begin,
                    const base::Time& delete_end,
                    base::OnceClosure callback);
  ~ShaderClearHelper();

  void Clear();

 private:
  enum OpType { TERMINATE, VERIFY_CACHE_SETUP, DELETE_CACHE };

  void DoClearShaderCache(int rv);

  ShaderCacheFactory* factory_;
  scoped_refptr<ShaderDiskCache> cache_;
  OpType op_type_;
  base::FilePath path_;
  base::Time delete_begin_;
  base::Time delete_end_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<ShaderClearHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShaderClearHelper);
};

// When the cache is asked to open an entry an Entry** is passed to it. The
// underying Entry* must stay alive for the duration of the call, so it is
// owned by the callback. If the underlying state machine is deleted before
// the callback runs, close the entry.
template <typename T>
void OnEntryOpenComplete(base::WeakPtr<T> state_machine,
                         std::unique_ptr<disk_cache::Entry*> entry,
                         int rv) {
  if (!state_machine) {
    if (rv == net::OK)
      (*entry)->Close();
    return;
  }
  state_machine->set_entry(*entry);
  state_machine->OnOpComplete(rv);
}

////////////////////////////////////////////////////////////////////////////////
// ShaderDiskCacheEntry

ShaderDiskCacheEntry::ShaderDiskCacheEntry(ShaderDiskCache* cache,
                                           const std::string& key,
                                           const std::string& shader)
    : cache_(cache),
      op_type_(OPEN_ENTRY),
      key_(key),
      shader_(shader),
      entry_(nullptr),
      weak_ptr_factory_(this) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

ShaderDiskCacheEntry::~ShaderDiskCacheEntry() {
  DCHECK(CalledOnValidThread());
  if (entry_)
    entry_->Close();
}

void ShaderDiskCacheEntry::Cache() {
  DCHECK(CalledOnValidThread());

  // The Entry* passed to the cache must stay alive even if this class is
  // deleted, so store it in the callback.
  auto entry = std::make_unique<disk_cache::Entry*>(nullptr);
  disk_cache::Entry** closure_owned_entry_ptr = entry.get();
  auto callback = base::Bind(&OnEntryOpenComplete<ShaderDiskCacheEntry>,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Passed(std::move(entry)));

  int rv = cache_->backend()->OpenEntry(key_, net::HIGHEST,
                                        closure_owned_entry_ptr, callback);

  if (rv != net::ERR_IO_PENDING) {
    entry_ = *closure_owned_entry_ptr;
    OnOpComplete(rv);
  }
}

void ShaderDiskCacheEntry::OnOpComplete(int rv) {
  DCHECK(CalledOnValidThread());
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
    }
  } while (rv != net::ERR_IO_PENDING && weak_ptr);
  if (weak_ptr)
    weak_ptr_ = std::move(weak_ptr);
}

int ShaderDiskCacheEntry::OpenCallback(int rv) {
  DCHECK(CalledOnValidThread());
  if (rv == net::OK) {
    cache_->backend()->OnExternalCacheHit(key_);
    cache_->EntryComplete(this);
    return rv;
  }

  op_type_ = CREATE_ENTRY;

  // The Entry* passed to the cache must stay alive even if this class is
  // deleted, so store it in the callback.
  auto entry = std::make_unique<disk_cache::Entry*>(nullptr);
  disk_cache::Entry** closure_owned_entry_ptr = entry.get();
  auto callback = base::Bind(&OnEntryOpenComplete<ShaderDiskCacheEntry>,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Passed(std::move(entry)));

  int create_rv = cache_->backend()->CreateEntry(
      key_, net::HIGHEST, closure_owned_entry_ptr, callback);

  if (create_rv != net::ERR_IO_PENDING)
    entry_ = *closure_owned_entry_ptr;
  return create_rv;
}

int ShaderDiskCacheEntry::WriteCallback(int rv) {
  DCHECK(CalledOnValidThread());
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to create shader cache entry: " << rv;
    cache_->EntryComplete(this);
    return rv;
  }

  op_type_ = WRITE_DATA;
  auto io_buf = base::MakeRefCounted<net::StringIOBuffer>(shader_);
  return entry_->WriteData(1, 0, io_buf.get(), shader_.length(),
                           base::Bind(&ShaderDiskCacheEntry::OnOpComplete,
                                      weak_ptr_factory_.GetWeakPtr()),
                           false);
}

int ShaderDiskCacheEntry::IOComplete(int rv) {
  DCHECK(CalledOnValidThread());
  cache_->EntryComplete(this);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// ShaderDiskReadHelper

ShaderDiskReadHelper::ShaderDiskReadHelper(ShaderDiskCache* cache,
                                           const ShaderLoadedCallback& callback)
    : cache_(cache),
      shader_loaded_callback_(callback),
      op_type_(OPEN_NEXT),
      buf_(nullptr),
      entry_(nullptr),
      weak_ptr_factory_(this) {}

ShaderDiskReadHelper::~ShaderDiskReadHelper() {
  DCHECK(CalledOnValidThread());
  if (entry_)
    entry_->Close();
  iter_ = nullptr;
}

void ShaderDiskReadHelper::LoadCache() {
  DCHECK(CalledOnValidThread());
  OnOpComplete(net::OK);
}

void ShaderDiskReadHelper::OnOpComplete(int rv) {
  DCHECK(CalledOnValidThread());
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

int ShaderDiskReadHelper::OpenNextEntry() {
  DCHECK(CalledOnValidThread());
  op_type_ = OPEN_NEXT_COMPLETE;
  if (!iter_)
    iter_ = cache_->backend()->CreateIterator();

  // The Entry* passed to the cache must stay alive even if this class is
  // deleted, so store it in the callback.
  auto entry = std::make_unique<disk_cache::Entry*>(nullptr);
  disk_cache::Entry** closure_owned_entry_ptr = entry.get();
  auto callback = base::Bind(&OnEntryOpenComplete<ShaderDiskReadHelper>,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Passed(std::move(entry)));

  int rv = iter_->OpenNextEntry(closure_owned_entry_ptr, callback);

  if (rv != net::ERR_IO_PENDING)
    entry_ = *closure_owned_entry_ptr;
  return rv;
}

int ShaderDiskReadHelper::OpenNextEntryComplete(int rv) {
  DCHECK(CalledOnValidThread());
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
                          base::Bind(&ShaderDiskReadHelper::OnOpComplete,
                                     weak_ptr_factory_.GetWeakPtr()));
}

int ShaderDiskReadHelper::ReadComplete(int rv) {
  DCHECK(CalledOnValidThread());
  if (rv && rv == buf_->size() && !shader_loaded_callback_.is_null()) {
    shader_loaded_callback_.Run(entry_->GetKey(),
                                std::string(buf_->data(), buf_->size()));
  }

  buf_ = nullptr;
  entry_->Close();
  entry_ = nullptr;

  op_type_ = OPEN_NEXT;
  return net::OK;
}

int ShaderDiskReadHelper::IterationComplete(int rv) {
  DCHECK(CalledOnValidThread());
  iter_.reset();
  op_type_ = TERMINATE;
  return net::OK;
}

////////////////////////////////////////////////////////////////////////////////
// ShaderClearHelper

ShaderClearHelper::ShaderClearHelper(ShaderCacheFactory* factory,
                                     scoped_refptr<ShaderDiskCache> cache,
                                     const base::FilePath& path,
                                     const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     base::OnceClosure callback)
    : factory_(factory),
      cache_(std::move(cache)),
      op_type_(VERIFY_CACHE_SETUP),
      path_(path),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

ShaderClearHelper::~ShaderClearHelper() {
  DCHECK(CalledOnValidThread());
}

void ShaderClearHelper::Clear() {
  DCHECK(CalledOnValidThread());
  DoClearShaderCache(net::OK);
}

void ShaderClearHelper::DoClearShaderCache(int rv) {
  DCHECK(CalledOnValidThread());
  while (rv != net::ERR_IO_PENDING) {
    switch (op_type_) {
      case VERIFY_CACHE_SETUP:
        rv = cache_->SetAvailableCallback(
            base::Bind(&ShaderClearHelper::DoClearShaderCache,
                       weak_ptr_factory_.GetWeakPtr()));
        op_type_ = DELETE_CACHE;
        break;
      case DELETE_CACHE:
        rv = cache_->Clear(delete_begin_, delete_end_,
                           base::Bind(&ShaderClearHelper::DoClearShaderCache,
                                      weak_ptr_factory_.GetWeakPtr()));
        op_type_ = TERMINATE;
        break;
      case TERMINATE:
        std::move(callback_).Run();
        // Calling CacheCleared() destroys |this|.
        factory_->CacheCleared(path_);
        rv = net::ERR_IO_PENDING;  // Break the loop.
        break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShaderCacheFactory

ShaderCacheFactory::ShaderCacheFactory() = default;

ShaderCacheFactory::~ShaderCacheFactory() = default;

void ShaderCacheFactory::SetCacheInfo(int32_t client_id,
                                      const base::FilePath& path) {
  DCHECK(CalledOnValidThread());
  client_id_to_path_map_[client_id] = path;
}

void ShaderCacheFactory::RemoveCacheInfo(int32_t client_id) {
  DCHECK(CalledOnValidThread());
  client_id_to_path_map_.erase(client_id);
}

scoped_refptr<ShaderDiskCache> ShaderCacheFactory::Get(int32_t client_id) {
  DCHECK(CalledOnValidThread());
  ClientIdToPathMap::iterator iter = client_id_to_path_map_.find(client_id);
  if (iter == client_id_to_path_map_.end())
    return nullptr;
  return ShaderCacheFactory::GetByPath(iter->second);
}

scoped_refptr<ShaderDiskCache> ShaderCacheFactory::GetByPath(
    const base::FilePath& path) {
  DCHECK(CalledOnValidThread());
  ShaderCacheMap::iterator iter = shader_cache_map_.find(path);
  if (iter != shader_cache_map_.end())
    return iter->second;

  auto cache = base::WrapRefCounted(new ShaderDiskCache(this, path));
  cache->Init();
  return cache;
}

void ShaderCacheFactory::AddToCache(const base::FilePath& key,
                                    ShaderDiskCache* cache) {
  DCHECK(CalledOnValidThread());
  shader_cache_map_[key] = cache;
}

void ShaderCacheFactory::RemoveFromCache(const base::FilePath& key) {
  DCHECK(CalledOnValidThread());
  shader_cache_map_.erase(key);
}

void ShaderCacheFactory::ClearByPath(const base::FilePath& path,
                                     const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     base::OnceClosure callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  auto helper = std::make_unique<ShaderClearHelper>(this, GetByPath(path), path,
                                                    delete_begin, delete_end,
                                                    std::move(callback));

  // We could receive requests to clear the same path with different
  // begin/end times. So, we keep a list of requests. If we haven't seen this
  // path before we kick off the clear and add it to the list. If we have see it
  // already, then we already have a clear running. We add this clear to the
  // list and wait for any previous clears to finish.
  ShaderClearMap::iterator iter = shader_clear_map_.find(path);
  if (iter != shader_clear_map_.end()) {
    iter->second.push(std::move(helper));
    return;
  }

  // Insert the helper in the map before calling Clear(), since it can lead to a
  // call back into CacheCleared().
  ShaderClearHelper* helper_ptr = helper.get();
  shader_clear_map_.insert(
      std::pair<base::FilePath, ShaderClearQueue>(path, ShaderClearQueue()));
  shader_clear_map_[path].push(std::move(helper));
  helper_ptr->Clear();
}

void ShaderCacheFactory::ClearByClientId(int32_t client_id,
                                         const base::Time& delete_begin,
                                         const base::Time& delete_end,
                                         base::OnceClosure callback) {
  DCHECK(CalledOnValidThread());
  ClientIdToPathMap::iterator iter = client_id_to_path_map_.find(client_id);
  if (iter == client_id_to_path_map_.end())
    return;
  return ClearByPath(iter->second, delete_begin, delete_end,
                     std::move(callback));
}

void ShaderCacheFactory::CacheCleared(const base::FilePath& path) {
  DCHECK(CalledOnValidThread());

  ShaderClearMap::iterator iter = shader_clear_map_.find(path);
  if (iter == shader_clear_map_.end()) {
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

  shader_clear_map_.erase(iter);
}

////////////////////////////////////////////////////////////////////////////////
// ShaderDiskCache

ShaderDiskCache::ShaderDiskCache(ShaderCacheFactory* factory,
                                 const base::FilePath& cache_path)
    : factory_(factory),
      cache_available_(false),
      cache_path_(cache_path),
      is_initialized_(false) {
  factory_->AddToCache(cache_path_, this);
}

ShaderDiskCache::~ShaderDiskCache() {
  factory_->RemoveFromCache(cache_path_);
}

void ShaderDiskCache::Init() {
  if (is_initialized_) {
    NOTREACHED();  // can't initialize disk cache twice.
    return;
  }
  is_initialized_ = true;

  int rv = disk_cache::CreateCacheBackend(
      net::SHADER_CACHE, net::CACHE_BACKEND_DEFAULT,
      cache_path_.Append(kGpuCachePath), CacheSizeBytes(), true, nullptr,
      &backend_, base::Bind(&ShaderDiskCache::CacheCreatedCallback, this));

  if (rv == net::OK)
    cache_available_ = true;
}

void ShaderDiskCache::Cache(const std::string& key, const std::string& shader) {
  if (!cache_available_)
    return;

  auto shim = std::make_unique<ShaderDiskCacheEntry>(this, key, shader);
  shim->Cache();
  auto* raw_ptr = shim.get();
  entries_.insert(std::make_pair(raw_ptr, std::move(shim)));
}

int ShaderDiskCache::Clear(const base::Time begin_time,
                           const base::Time end_time,
                           const net::CompletionCallback& completion_callback) {
  int rv;
  if (begin_time.is_null()) {
    rv = backend_->DoomAllEntries(completion_callback);
  } else {
    rv =
        backend_->DoomEntriesBetween(begin_time, end_time, completion_callback);
  }
  return rv;
}

int32_t ShaderDiskCache::Size() {
  if (!cache_available_)
    return -1;
  return backend_->GetEntryCount();
}

int ShaderDiskCache::SetAvailableCallback(
    const net::CompletionCallback& callback) {
  if (cache_available_)
    return net::OK;
  available_callback_ = callback;
  return net::ERR_IO_PENDING;
}

void ShaderDiskCache::CacheCreatedCallback(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Shader Cache Creation failed: " << rv;
    return;
  }
  helper_ =
      std::make_unique<ShaderDiskReadHelper>(this, shader_loaded_callback_);
  helper_->LoadCache();
}

void ShaderDiskCache::EntryComplete(ShaderDiskCacheEntry* entry) {
  entries_.erase(entry);
  if (entries_.empty() && !cache_complete_callback_.is_null())
    cache_complete_callback_.Run(net::OK);
}

void ShaderDiskCache::ReadComplete() {
  helper_ = nullptr;

  // The cache is considered available after we have finished reading any
  // of the old cache values off disk. This prevents a potential race where we
  // are reading from disk and execute a cache clear at the same time.
  cache_available_ = true;
  if (!available_callback_.is_null()) {
    available_callback_.Run(net::OK);
    available_callback_.Reset();
  }
}

int ShaderDiskCache::SetCacheCompleteCallback(
    const net::CompletionCallback& callback) {
  if (entries_.empty()) {
    return net::OK;
  }
  cache_complete_callback_ = callback;
  return net::ERR_IO_PENDING;
}

// static
size_t ShaderDiskCache::CacheSizeBytes() {
#if !defined(OS_ANDROID)
  size_t custom_cache_size =
      GetCustomCacheSizeBytesIfExists(switches::kShaderDiskCacheSizeKB);
  if (custom_cache_size)
    return custom_cache_size;
  return kDefaultMaxProgramCacheMemoryBytes;
#else   // !defined(OS_ANDROID)
  if (!base::SysInfo::IsLowEndDevice())
    return kDefaultMaxProgramCacheMemoryBytes;
  else
    return kLowEndMaxProgramCacheMemoryBytes;
#endif  // !defined(OS_ANDROID)
}

}  // namespace gpu

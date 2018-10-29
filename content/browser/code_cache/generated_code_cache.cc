// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/generated_code_cache.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/common/url_constants.h"
#include "net/base/completion_callback.h"
#include "net/base/completion_once_callback.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {
// We always expect to receive valid URLs that can be used as keys to the code
// cache. The relevant checks (for ex: resource_url is valid, origin_lock is
// not opque etc.,) must be done prior to requesting the code cache.
//
// This function doesn't enforce anything in the production code. It is here
// to make the assumptions explicit and to catch any errors when DCHECKs are
// enabled.
void CheckValidKeys(const GURL& resource_url, const GURL& origin_lock) {
  // If the resource url is invalid don't cache the code.
  DCHECK(resource_url.is_valid() && resource_url.SchemeIsHTTPOrHTTPS());

  // |origin_lock| should be either empty or should have Http/Https/chrome
  // schemes and it should not be a URL with opaque origin. Empty origin_locks
  // are allowed when the renderer is not locked to an origin.
  DCHECK(origin_lock.is_empty() ||
         ((origin_lock.SchemeIsHTTPOrHTTPS() ||
           origin_lock.SchemeIs(content::kChromeUIScheme)) &&
          !url::Origin::Create(origin_lock).opaque()));
}

// Generates the cache key for the given |resource_url| and the |origin_lock|.
//   |resource_url| is the url corresponding to the requested resource.
//   |origin_lock| is the origin that the renderer which requested this
//   resource is locked to.
// For example, if SitePerProcess is enabled and http://script.com/script1.js is
// requested by http://example.com, then http://script.com/script.js is the
// resource_url and http://example.com is the origin_lock.
//
// This returns the key by concatenating the serialized url and origin lock
// with a separator in between. |origin_lock| could be empty when renderer is
// not locked to an origin (ex: SitePerProcess is disabled) and it is safe to
// use only |resource_url| as the key in such cases.
std::string GetCacheKey(const GURL& resource_url, const GURL& origin_lock) {
  CheckValidKeys(resource_url, origin_lock);

  // Add a prefix _ so it can't be parsed as a valid URL.
  std::string key = "_key";
  // Remove reference, username and password sections of the URL.
  key.append(net::SimplifyUrlForRequest(resource_url).spec());
  // Add a separator between URL and origin to avoid any possibility of
  // attacks by crafting the URL. URLs do not contain any control ASCII
  // characters, and also space is encoded. So use ' \n' as a seperator.
  key.append(" \n");

  if (origin_lock.is_valid())
    key.append(net::SimplifyUrlForRequest(origin_lock).spec());
  return key;
}
}  // namespace

void GeneratedCodeCache::CollectStatistics(
    GeneratedCodeCache::CacheEntryStatus status) {
  switch (cache_type_) {
    case GeneratedCodeCache::CodeCacheType::kJavaScript:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.JS.Behaviour", status);
      break;
    case GeneratedCodeCache::CodeCacheType::kWebAssembly:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.WASM.Behaviour", status);
      break;
  }
}

// Stores the information about a pending request while disk backend is
// being initialized.
class GeneratedCodeCache::PendingOperation {
 public:
  static std::unique_ptr<PendingOperation> CreateWritePendingOp(
      std::string key,
      scoped_refptr<net::IOBufferWithSize>);
  static std::unique_ptr<PendingOperation> CreateFetchPendingOp(
      std::string key,
      const ReadDataCallback&);
  static std::unique_ptr<PendingOperation> CreateDeletePendingOp(
      std::string key);
  static std::unique_ptr<PendingOperation> CreateClearCachePendingOp(
      net::CompletionCallback callback);

  ~PendingOperation();

  Operation operation() const { return op_; }
  const std::string& key() const { return key_; }
  const scoped_refptr<net::IOBufferWithSize> data() const { return data_; }
  ReadDataCallback ReleaseReadCallback() { return std::move(read_callback_); }
  net::CompletionCallback ReleaseCallback() { return std::move(callback_); }

 private:
  PendingOperation(Operation op,
                   std::string key,
                   scoped_refptr<net::IOBufferWithSize>,
                   const ReadDataCallback&,
                   net::CompletionCallback);

  const Operation op_;
  const std::string key_;
  const scoped_refptr<net::IOBufferWithSize> data_;
  ReadDataCallback read_callback_;
  net::CompletionCallback callback_;
};

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateWritePendingOp(
    std::string key,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  return base::WrapUnique(
      new PendingOperation(Operation::kWrite, std::move(key), buffer,
                           ReadDataCallback(), net::CompletionCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateFetchPendingOp(
    std::string key,
    const ReadDataCallback& read_callback) {
  return base::WrapUnique(new PendingOperation(
      Operation::kFetch, std::move(key), scoped_refptr<net::IOBufferWithSize>(),
      read_callback, net::CompletionCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateDeletePendingOp(std::string key) {
  return base::WrapUnique(
      new PendingOperation(Operation::kDelete, std::move(key),
                           scoped_refptr<net::IOBufferWithSize>(),
                           ReadDataCallback(), net::CompletionCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateClearCachePendingOp(
    net::CompletionCallback callback) {
  return base::WrapUnique(
      new PendingOperation(Operation::kClearCache, std::string(),
                           scoped_refptr<net::IOBufferWithSize>(),
                           ReadDataCallback(), std::move(callback)));
}

GeneratedCodeCache::PendingOperation::PendingOperation(
    Operation op,
    std::string key,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const ReadDataCallback& read_callback,
    net::CompletionCallback callback)
    : op_(op),
      key_(std::move(key)),
      data_(buffer),
      read_callback_(read_callback),
      callback_(std::move(callback)) {}

GeneratedCodeCache::PendingOperation::~PendingOperation() = default;

GeneratedCodeCache::GeneratedCodeCache(const base::FilePath& path,
                                       int max_size_bytes,
                                       CodeCacheType cache_type)
    : backend_state_(kUnInitialized),
      path_(path),
      max_size_bytes_(max_size_bytes),
      cache_type_(cache_type),
      weak_ptr_factory_(this) {
  CreateBackend();
}

GeneratedCodeCache::~GeneratedCodeCache() = default;

void GeneratedCodeCache::WriteData(const GURL& url,
                                   const GURL& origin_lock,
                                   const base::Time& response_time,
                                   const std::vector<uint8_t>& data) {
  // Silently ignore the requests.
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  // Append the response time to the metadata. Code caches store
  // response_time + generated code as a single entry.
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(data.size() +
                                                  kResponseTimeSizeInBytes);
  int64_t serialized_time =
      response_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  memcpy(buffer->data(), &serialized_time, kResponseTimeSizeInBytes);
  if (!data.empty())
    memcpy(buffer->data() + kResponseTimeSizeInBytes, &data.front(),
           data.size());

  std::string key = GetCacheKey(url, origin_lock);
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateWritePendingOp(
            std::move(key), buffer));
    return;
  }

  WriteDataImpl(key, buffer);
}

void GeneratedCodeCache::FetchEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    ReadDataCallback read_data_callback) {
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    // Silently ignore the requests.
    std::move(read_data_callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateFetchPendingOp(
            std::move(key), read_data_callback));
    return;
  }

  FetchEntryImpl(key, read_data_callback);
}

void GeneratedCodeCache::DeleteEntry(const GURL& url, const GURL& origin_lock) {
  // Silently ignore the requests.
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateDeletePendingOp(
            std::move(key)));
    return;
  }

  DeleteEntryImpl(key);
}

int GeneratedCodeCache::ClearCache(net::CompletionCallback callback) {
  if (backend_state_ == kFailed) {
    return net::ERR_FAILED;
  }

  if (backend_state_ != kInitialized) {
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateClearCachePendingOp(
            std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  return backend_->DoomAllEntries(std::move(callback));
}

void GeneratedCodeCache::CreateBackend() {
  // Create a new Backend pointer that cleans itself if the GeneratedCodeCache
  // instance is not live when the CreateCacheBackend finishes.
  scoped_refptr<base::RefCountedData<ScopedBackendPtr>> shared_backend_ptr =
      new base::RefCountedData<ScopedBackendPtr>();

  net::CompletionOnceCallback create_backend_complete =
      base::BindOnce(&GeneratedCodeCache::DidCreateBackend,
                     weak_ptr_factory_.GetWeakPtr(), shared_backend_ptr);

  // If the initialization of the existing cache fails, this call would delete
  // all the contents and recreates a new one.
  int rv = disk_cache::CreateCacheBackend(
      net::GENERATED_CODE_CACHE, net::CACHE_BACKEND_SIMPLE, path_,
      max_size_bytes_, true, nullptr, &shared_backend_ptr->data,
      std::move(create_backend_complete));
  if (rv != net::ERR_IO_PENDING) {
    DidCreateBackend(shared_backend_ptr, rv);
  }
}

void GeneratedCodeCache::DidCreateBackend(
    scoped_refptr<base::RefCountedData<ScopedBackendPtr>> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    backend_state_ = kFailed;
    // Process pending operations to process any required callbacks.
    IssuePendingOperations();
    return;
  }

  backend_ = std::move(backend_ptr->data);
  backend_state_ = kInitialized;
  IssuePendingOperations();
}

void GeneratedCodeCache::IssuePendingOperations() {
  DCHECK_EQ(backend_state_, kInitialized);
  // Issue all the pending operations that were received when creating
  // the backend.
  for (auto const& op : pending_ops_) {
    switch (op->operation()) {
      case kFetch:
        FetchEntryImpl(op->key(), op->ReleaseReadCallback());
        break;
      case kWrite:
        WriteDataImpl(op->key(), op->data());
        break;
      case kDelete:
        DeleteEntryImpl(op->key());
        break;
      case kClearCache:
        DoPendingClearCache(op->ReleaseCallback());
        break;
    }
  }
  pending_ops_.clear();
}

void GeneratedCodeCache::WriteDataImpl(
    const std::string& key,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  if (backend_state_ != kInitialized)
    return;

  scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry_ptr =
      new base::RefCountedData<disk_cache::Entry*>();
  net::CompletionOnceCallback callback =
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForWriteData,
                     weak_ptr_factory_.GetWeakPtr(), buffer, key, entry_ptr);

  int result =
      backend_->OpenEntry(key, net::LOW, &entry_ptr->data, std::move(callback));
  if (result != net::ERR_IO_PENDING) {
    OpenCompleteForWriteData(buffer, key, entry_ptr, result);
  }
}

void GeneratedCodeCache::OpenCompleteForWriteData(
    scoped_refptr<net::IOBufferWithSize> buffer,
    const std::string& key,
    scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
    int rv) {
  if (rv != net::OK) {
    net::CompletionOnceCallback callback =
        base::BindOnce(&GeneratedCodeCache::CreateCompleteForWriteData,
                       weak_ptr_factory_.GetWeakPtr(), buffer, entry);

    int result =
        backend_->CreateEntry(key, net::LOW, &entry->data, std::move(callback));
    if (result != net::ERR_IO_PENDING) {
      CreateCompleteForWriteData(buffer, entry, result);
    }
    return;
  }

  DCHECK(entry->data);
  disk_cache::ScopedEntryPtr disk_entry(entry->data);

  CollectStatistics(CacheEntryStatus::kUpdate);
  // This call will truncate the data. This is safe to do since we read the
  // entire data at the same time currently. If we want to read in parts we have
  // to doom the entry first.
  disk_entry->WriteData(kDataIndex, 0, buffer.get(), buffer->size(),
                        net::CompletionOnceCallback(), true);
}

void GeneratedCodeCache::CreateCompleteForWriteData(
    scoped_refptr<net::IOBufferWithSize> buffer,
    scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
    int rv) {
  if (rv != net::OK) {
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  DCHECK(entry->data);
  disk_cache::ScopedEntryPtr disk_entry(entry->data);
  CollectStatistics(CacheEntryStatus::kCreate);
  disk_entry->WriteData(kDataIndex, 0, buffer.get(), buffer->size(),
                        net::CompletionOnceCallback(), true);
}

void GeneratedCodeCache::FetchEntryImpl(const std::string& key,
                                        ReadDataCallback read_data_callback) {
  if (backend_state_ != kInitialized) {
    std::move(read_data_callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry_ptr =
      new base::RefCountedData<disk_cache::Entry*>();

  net::CompletionOnceCallback callback = base::BindOnce(
      &GeneratedCodeCache::OpenCompleteForReadData,
      weak_ptr_factory_.GetWeakPtr(), read_data_callback, entry_ptr);

  // This is a part of loading cycle and hence should run with a high priority.
  int result = backend_->OpenEntry(key, net::HIGHEST, &entry_ptr->data,
                                   std::move(callback));
  if (result != net::ERR_IO_PENDING) {
    OpenCompleteForReadData(read_data_callback, entry_ptr, result);
  }
}

void GeneratedCodeCache::OpenCompleteForReadData(
    ReadDataCallback read_data_callback,
    scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
    int rv) {
  if (rv != net::OK) {
    CollectStatistics(CacheEntryStatus::kMiss);
    std::move(read_data_callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  // There should be a valid entry if the open was successful.
  DCHECK(entry->data);

  disk_cache::ScopedEntryPtr disk_entry(entry->data);
  int size = disk_entry->GetDataSize(kDataIndex);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(size);
  net::CompletionOnceCallback callback = base::BindOnce(
      &GeneratedCodeCache::ReadDataComplete, weak_ptr_factory_.GetWeakPtr(),
      read_data_callback, buffer);
  int result = disk_entry->ReadData(kDataIndex, 0, buffer.get(), size,
                                    std::move(callback));
  if (result != net::ERR_IO_PENDING) {
    ReadDataComplete(read_data_callback, buffer, result);
  }
}

void GeneratedCodeCache::ReadDataComplete(
    ReadDataCallback callback,
    scoped_refptr<net::IOBufferWithSize> buffer,
    int rv) {
  if (rv != buffer->size()) {
    CollectStatistics(CacheEntryStatus::kMiss);
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
  } else if (buffer->size() < kResponseTimeSizeInBytes) {
    // TODO(crbug.com/886892): Change the implementation, so serialize requests
    // for the same key here. When we do that, this case should not arise.
    // We might be reading an entry before the write was completed. This can
    // happen if we have a write and read operation for the same key almost at
    // the same time and they interleave as:
    // W(Create) -> R(Open) -> R(Read) -> W(Write).
    CollectStatistics(CacheEntryStatus::kIncompleteEntry);
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
  } else {
    // DiskCache ensures that the operations that are queued for an entry
    // go in order. Hence, we would either read an empty data or read the full
    // data. Please look at comment in else to see why we read empty data.
    CollectStatistics(CacheEntryStatus::kHit);
    int64_t raw_response_time = *(reinterpret_cast<int64_t*>(buffer->data()));
    base::Time response_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(raw_response_time));
    std::vector<uint8_t> data;
    if (buffer->size() > kResponseTimeSizeInBytes) {
      data = std::vector<uint8_t>(buffer->data() + kResponseTimeSizeInBytes,
                                  buffer->data() + buffer->size());
    }
    std::move(callback).Run(response_time, data);
  }
}

void GeneratedCodeCache::DeleteEntryImpl(const std::string& key) {
  if (backend_state_ != kInitialized)
    return;

  CollectStatistics(CacheEntryStatus::kClear);
  backend_->DoomEntry(key, net::LOWEST, net::CompletionOnceCallback());
}

void GeneratedCodeCache::DoPendingClearCache(
    net::CompletionCallback user_callback) {
  int result = backend_->DoomAllEntries(user_callback);
  if (result != net::ERR_IO_PENDING) {
    // Call the callback here because we returned ERR_IO_PENDING for initial
    // request.
    std::move(user_callback).Run(result);
  }
}

}  // namespace content

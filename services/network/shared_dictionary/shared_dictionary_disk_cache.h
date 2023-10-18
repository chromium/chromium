// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DISK_CACHE_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DISK_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace base {
class FilePath;
}  //  namespace base

namespace disk_cache {
class BackendFileOperationsFactory;
}  // namespace disk_cache

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryDiskCache {
 public:
  SharedDictionaryDiskCache();
  virtual ~SharedDictionaryDiskCache();

  SharedDictionaryDiskCache(const SharedDictionaryDiskCache&) = delete;
  SharedDictionaryDiskCache& operator=(const SharedDictionaryDiskCache&) =
      delete;

  void Initialize(
      const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory);

  disk_cache::EntryResult OpenOrCreateEntry(
      const std::string& key,
      bool create,
      disk_cache::EntryResultCallback callback);
  int DoomEntry(const std::string& key, net::CompletionOnceCallback callback);
  int ClearAll(net::CompletionOnceCallback callback);
  void CreateIterator(
      base::OnceCallback<void(std::unique_ptr<disk_cache::Backend::Iterator>)>);

  base::WeakPtr<SharedDictionaryDiskCache> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // Virtual for testing
  virtual disk_cache::BackendResult CreateCacheBackend(
      const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory,
      disk_cache::BackendResultCallback callback);

 private:
  enum class State { kBeforeInitialize, kInitializing, kInitialized, kFailed };

  void DidCreateBackend(disk_cache::BackendResult result);

  State state_ = State::kBeforeInitialize;
  std::unique_ptr<disk_cache::Backend> backend_;
  std::vector<base::OnceClosure> pending_disk_cache_tasks_;
  base::WeakPtrFactory<SharedDictionaryDiskCache> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DISK_CACHE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"

namespace network {
namespace {

void RunTaksAndCallback(
    base::OnceCallback<int(net::CompletionOnceCallback)> task,
    net::CompletionOnceCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = std::move(task).Run(std::move(split_callback.first));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(std::move(result));
  }
}

void RunTaksAndEntryResultCallback(
    base::OnceCallback<disk_cache::EntryResult(disk_cache::EntryResultCallback)>
        task,
    disk_cache::EntryResultCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  disk_cache::EntryResult result =
      std::move(task).Run(std::move(split_callback.first));
  if (result.net_error() != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(std::move(result));
  }
}

}  // namespace

SharedDictionaryDiskCache::SharedDictionaryDiskCache() = default;

void SharedDictionaryDiskCache::Initialize(
    const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory) {
  DCHECK_EQ(State::kBeforeInitialize, state_);
  state_ = State::kInitializing;
  disk_cache::BackendResult result = CreateCacheBackend(
      cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
      std::move(file_operations_factory),
      base::BindOnce(&SharedDictionaryDiskCache::DidCreateBackend,
                     GetWeakPtr()));
  if (result.net_error != net::ERR_IO_PENDING) {
    DidCreateBackend(std::move(result));
  }
}

SharedDictionaryDiskCache::~SharedDictionaryDiskCache() = default;

disk_cache::BackendResult SharedDictionaryDiskCache::CreateCacheBackend(
    const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory,
    disk_cache::BackendResultCallback callback) {
  // We use APP_CACHE to avoid the auto-eviction.
  return disk_cache::CreateCacheBackend(
      cache_directory_path.empty() ? net::MEMORY_CACHE : net::APP_CACHE,
      net::CACHE_BACKEND_SIMPLE, file_operations_factory.get(),
      cache_directory_path, /*max_bytes=*/0,
      disk_cache::ResetHandling::kResetOnError, /*net_log=*/nullptr,
      std::move(callback)
#if BUILDFLAG(IS_ANDROID)
          ,
      app_status_listener
#endif  // BUILDFLAG(IS_ANDROID));
  );
}

disk_cache::EntryResult SharedDictionaryDiskCache::OpenOrCreateEntry(
    const std::string& key,
    bool create,
    disk_cache::EntryResultCallback callback) {
  switch (state_) {
    case State::kBeforeInitialize:
      NOTREACHED();
      return disk_cache::EntryResult::MakeError(net::ERR_FAILED);
    case State::kInitializing:
      // It is safe to use Unretained() below because
      // `pending_disk_cache_tasks_` is owned by `this` and the passed task
      // `SharedDictionaryDiskCache::OpenOrCreateEntry()` will be called only
      // when `this` is available.
      pending_disk_cache_tasks_.push_back(base::BindOnce(
          &RunTaksAndEntryResultCallback,
          base::BindOnce(&SharedDictionaryDiskCache::OpenOrCreateEntry,
                         base::Unretained(this), key, create),
          std::move(callback)));
      return disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING);
    case State::kInitialized:
      DCHECK(backend_);
      return create
                 ? backend_->CreateEntry(key, net::LOW, std::move(callback))
                 : backend_->OpenEntry(key, net::HIGHEST, std::move(callback));
    case State::kFailed:
      return disk_cache::EntryResult::MakeError(net::ERR_FAILED);
  }
}

int SharedDictionaryDiskCache::DoomEntry(const std::string& key,
                                         net::CompletionOnceCallback callback) {
  switch (state_) {
    case State::kBeforeInitialize:
      NOTREACHED();
      return net::ERR_FAILED;
    case State::kInitializing:
      // It is safe to use Unretained() below because
      // `pending_disk_cache_tasks_` is owned by `this` and the passed task
      // `SharedDictionaryDiskCache::DoomEntry()` will be called only when
      // `this` is available.
      pending_disk_cache_tasks_.push_back(
          base::BindOnce(&RunTaksAndCallback,
                         base::BindOnce(&SharedDictionaryDiskCache::DoomEntry,
                                        base::Unretained(this), key),
                         std::move(callback)));
      return net::ERR_IO_PENDING;
    case State::kInitialized:
      DCHECK(backend_);
      return backend_->DoomEntry(key, net::LOW, std::move(callback));
    case State::kFailed:
      return net::ERR_FAILED;
  }
}

int SharedDictionaryDiskCache::ClearAll(net::CompletionOnceCallback callback) {
  switch (state_) {
    case State::kBeforeInitialize:
      NOTREACHED();
      return net::ERR_FAILED;
    case State::kInitializing:
      // It is safe to use Unretained() below because
      // `pending_disk_cache_tasks_` is owned by `this` and the passed task
      // `SharedDictionaryDiskCache::ClearAll()` will be called only when `this`
      // is available.
      pending_disk_cache_tasks_.push_back(
          base::BindOnce(&RunTaksAndCallback,
                         base::BindOnce(&SharedDictionaryDiskCache::ClearAll,
                                        base::Unretained(this)),
                         std::move(callback)));
      return net::ERR_IO_PENDING;
    case State::kInitialized:
      DCHECK(backend_);
      return backend_->DoomAllEntries(std::move(callback));
    case State::kFailed:
      return net::ERR_FAILED;
  }
}

void SharedDictionaryDiskCache::DidCreateBackend(
    disk_cache::BackendResult result) {
  if (result.net_error != net::OK) {
    state_ = State::kFailed;
  } else {
    state_ = State::kInitialized;
    backend_ = std::move(result.backend);
  }
  auto tasks = std::move(pending_disk_cache_tasks_);
  base::WeakPtr<SharedDictionaryDiskCache> weak_ptr = GetWeakPtr();
  for (auto& task : tasks) {
    std::move(task).Run();
    if (!weak_ptr) {
      return;
    }
  }
}

}  // namespace network

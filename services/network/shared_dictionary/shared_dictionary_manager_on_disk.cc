// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"

namespace network {

SharedDictionaryManagerOnDisk::SharedDictionaryManagerOnDisk(
    const base::FilePath& database_path,
    const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory)
    : metadata_store_(database_path,
                      /*client_task_runner=*/
                      base::SingleThreadTaskRunner::GetCurrentDefault(),
                      /*background_task_runner=*/
                      base::ThreadPool::CreateSequencedTaskRunner(
                          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  disk_cache_.Initialize(cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
                         app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
                         std::move(file_operations_factory));
}

SharedDictionaryManagerOnDisk::~SharedDictionaryManagerOnDisk() = default;

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerOnDisk::CreateStorage(
    const net::SharedDictionaryStorageIsolationKey& isolation_key) {
  return base::MakeRefCounted<SharedDictionaryStorageOnDisk>(
      weak_factory_.GetWeakPtr(), isolation_key,
      base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
}

}  // namespace network

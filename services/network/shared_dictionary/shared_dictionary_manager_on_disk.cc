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

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryManagerOnDisk::CreateWriter(
    const net::SharedDictionaryStorageIsolationKey& isolation_key,
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback) {
  const base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();
  auto writer = base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
      disk_cache_key_token,
      base::BindOnce(
          &SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDiskCache,
          weak_factory_.GetWeakPtr(), isolation_key, url, response_time,
          expiration, match, disk_cache_key_token, std::move(callback)),
      disk_cache_.GetWeakPtr());
  writer->Initialize();
  return writer;
}

void SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDiskCache(
    const net::SharedDictionaryStorageIsolationKey& isolation_key,
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const base::UnguessableToken& disk_cache_key_token,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
    SharedDictionaryWriterOnDisk::Result result,
    size_t size,
    const net::SHA256HashValue& hash) {
  if (result != SharedDictionaryWriterOnDisk::Result::kSuccess) {
    return;
  }
  base::Time last_used_time = base::Time::Now();
  net::SharedDictionaryInfo info(url, response_time, expiration, match,
                                 last_used_time, size, hash,
                                 disk_cache_key_token,
                                 /*primary_key_in_database=*/absl::nullopt);
  metadata_store_.RegisterDictionary(
      isolation_key, info,
      base::BindOnce(
          &SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDatabase,
          weak_factory_.GetWeakPtr(), info, std::move(callback)));
}

void SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDatabase(
    net::SharedDictionaryInfo info,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
    net::SQLitePersistentSharedDictionaryStore::RegisterDictionaryResultOrError
        result) {
  if (!result.has_value()) {
    disk_cache_.DoomEntry(info.disk_cache_key_token().ToString(),
                          base::DoNothing());
    return;
  }
  CHECK(result.value().primary_key_in_database.has_value());
  info.set_primary_key_in_database(*result.value().primary_key_in_database);
  if (result.value().disk_cache_key_token_to_be_removed) {
    disk_cache_.DoomEntry(
        result.value().disk_cache_key_token_to_be_removed->ToString(),
        base::DoNothing());
  }
  std::move(callback).Run(std::move(info));
}

}  // namespace network

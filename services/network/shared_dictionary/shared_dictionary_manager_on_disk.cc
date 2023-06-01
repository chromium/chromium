// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"

namespace network {

class SharedDictionaryManagerOnDisk::ClearDataTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  ClearDataTask(raw_ptr<SharedDictionaryManagerOnDisk> manager,
                base::Time start_time,
                base::Time end_time,
                base::RepeatingCallback<bool(const GURL&)> url_matcher,
                base::OnceClosure callback)
      : manager_(manager),
        start_time_(start_time),
        end_time_(end_time),
        url_matcher_(std::move(url_matcher)),
        callback_(std::move(callback)) {}
  ~ClearDataTask() override = default;

  ClearDataTask(const ClearDataTask&) = delete;
  ClearDataTask& operator=(const ClearDataTask&) = delete;

  void Start() override {
    manager_->metadata_store().ClearDictionaries(
        start_time_, end_time_, std::move(url_matcher_),
        base::BindOnce(&ClearDataTask::OnClearDictionariesFinished,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnClearDictionariesFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeletedFromDatabase(result.value());
    }
    std::move(callback_).Run();
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  const base::Time start_time_;
  const base::Time end_time_;
  base::RepeatingCallback<bool(const GURL&)> url_matcher_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<ClearDataTask> weak_factory_{this};
};

class SharedDictionaryManagerOnDisk::ClearDataTaskInfo
    : public SharedDictionaryManagerOnDisk::SerializedTaskInfo {
 public:
  ClearDataTaskInfo(base::Time start_time,
                    base::Time end_time,
                    base::RepeatingCallback<bool(const GURL&)> url_matcher,
                    base::OnceClosure callback)
      : start_time_(start_time),
        end_time_(end_time),
        url_matcher_(std::move(url_matcher)),
        callback_(std::move(callback)) {}
  ~ClearDataTaskInfo() override = default;

  ClearDataTaskInfo(const ClearDataTaskInfo&) = delete;
  ClearDataTaskInfo& operator=(const ClearDataTaskInfo&) = delete;

  std::unique_ptr<SerializedTask> CreateTask(
      SharedDictionaryManagerOnDisk* manager) override {
    return std::make_unique<ClearDataTask>(manager, start_time_, end_time_,
                                           std::move(url_matcher_),
                                           std::move(callback_));
  }

 private:
  const base::Time start_time_;
  const base::Time end_time_;
  base::RepeatingCallback<bool(const GURL&)> url_matcher_;
  base::OnceClosure callback_;
};

class SharedDictionaryManagerOnDisk::CacheEvictionTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  CacheEvictionTask(raw_ptr<SharedDictionaryManagerOnDisk> manager,
                    uint64_t cache_max_size)
      : manager_(manager), cache_max_size_(cache_max_size) {}
  ~CacheEvictionTask() override = default;

  CacheEvictionTask(const CacheEvictionTask&) = delete;
  CacheEvictionTask& operator=(const CacheEvictionTask&) = delete;

  void Start() override {
    manager_->metadata_store().ProcessEviction(
        cache_max_size_, cache_max_size_ * 0.9,
        base::BindOnce(&CacheEvictionTask::OnProcessEvictionFinished,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnProcessEvictionFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeletedFromDatabase(result.value());
    }
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  const uint64_t cache_max_size_;
  base::WeakPtrFactory<CacheEvictionTask> weak_factory_{this};
};

class SharedDictionaryManagerOnDisk::CacheEvictionTaskInfo
    : public SharedDictionaryManagerOnDisk::SerializedTaskInfo {
 public:
  explicit CacheEvictionTaskInfo(base::OnceClosure task_created_callback)
      : task_created_callback_(std::move(task_created_callback)) {}
  ~CacheEvictionTaskInfo() override = default;
  CacheEvictionTaskInfo(const CacheEvictionTaskInfo&) = delete;
  CacheEvictionTaskInfo& operator=(const CacheEvictionTaskInfo&) = delete;

  std::unique_ptr<SerializedTask> CreateTask(
      SharedDictionaryManagerOnDisk* manager) override {
    std::move(task_created_callback_).Run();
    return std::make_unique<CacheEvictionTask>(manager,
                                               manager->cache_max_size());
  }

 private:
  base::OnceClosure task_created_callback_;
};

class SharedDictionaryManagerOnDisk::ExpiredDictionaryDeletionTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  explicit ExpiredDictionaryDeletionTask(
      raw_ptr<SharedDictionaryManagerOnDisk> manager)
      : manager_(manager) {}
  ~ExpiredDictionaryDeletionTask() override = default;

  ExpiredDictionaryDeletionTask(const ExpiredDictionaryDeletionTask&) = delete;
  ExpiredDictionaryDeletionTask& operator=(
      const ExpiredDictionaryDeletionTask&) = delete;

  void Start() override {
    manager_->metadata_store().DeleteExpiredDictionaries(
        base::Time::Now(),
        base::BindOnce(
            &ExpiredDictionaryDeletionTask::OnDeleteExpiredDictionariesFinished,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void OnDeleteExpiredDictionariesFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeletedFromDatabase(result.value());
    }
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  base::WeakPtrFactory<ExpiredDictionaryDeletionTask> weak_factory_{this};
};

class SharedDictionaryManagerOnDisk::ExpiredDictionaryDeletionTaskInfo
    : public SharedDictionaryManagerOnDisk::SerializedTaskInfo {
 public:
  explicit ExpiredDictionaryDeletionTaskInfo(
      base::OnceClosure task_created_callback)
      : task_created_callback_(std::move(task_created_callback)) {}
  ~ExpiredDictionaryDeletionTaskInfo() override = default;
  ExpiredDictionaryDeletionTaskInfo(const ExpiredDictionaryDeletionTaskInfo&) =
      delete;
  ExpiredDictionaryDeletionTaskInfo& operator=(
      const ExpiredDictionaryDeletionTaskInfo&) = delete;

  std::unique_ptr<SerializedTask> CreateTask(
      SharedDictionaryManagerOnDisk* manager) override {
    std::move(task_created_callback_).Run();
    return std::make_unique<ExpiredDictionaryDeletionTask>(manager);
  }

 private:
  base::OnceClosure task_created_callback_;
};

SharedDictionaryManagerOnDisk::SharedDictionaryManagerOnDisk(
    const base::FilePath& database_path,
    const base::FilePath& cache_directory_path,
    uint64_t cache_max_size,
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory)
    : cache_max_size_(cache_max_size),
      metadata_store_(database_path,
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
  MaybePostExpiredDictionaryDeletionTask();
  MaybePostCacheEvictionTask();
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

void SharedDictionaryManagerOnDisk::SetCacheMaxSize(uint64_t cache_max_size) {
  cache_max_size_ = cache_max_size;
  MaybePostExpiredDictionaryDeletionTask();
  MaybePostCacheEvictionTask();
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

  base::UmaHistogramCustomCounts(
      "Net.SharedDictionaryManagerOnDisk.DictionarySize", info.size(), 1,
      100000000, 50);
  base::UmaHistogramCustomCounts(
      "Net.SharedDictionaryManagerOnDisk.TotalDictionarySizeWhenAdded",
      result.value().total_dictionary_size, 1, 200000000, 50);
  info.set_primary_key_in_database(result.value().primary_key_in_database);
  if (result.value().disk_cache_key_token_to_be_removed) {
    disk_cache_.DoomEntry(
        result.value().disk_cache_key_token_to_be_removed->ToString(),
        base::DoNothing());
  }
  std::move(callback).Run(std::move(info));

  MaybePostExpiredDictionaryDeletionTask();
  if (cache_max_size_ > 0 &&
      result.value().total_dictionary_size > cache_max_size_) {
    MaybePostCacheEvictionTask();
  }
}

void SharedDictionaryManagerOnDisk::UpdateDictionaryLastUsedTime(
    net::SharedDictionaryInfo& info) {
  info.set_last_used_time(base::Time::Now());
  CHECK(info.primary_key_in_database());
  metadata_store_.UpdateDictionaryLastUsedTime(*info.primary_key_in_database(),
                                               info.last_used_time());
}

void SharedDictionaryManagerOnDisk::ClearData(
    base::Time start_time,
    base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher,
    base::OnceClosure callback) {
  PostSerializedTask(std::make_unique<ClearDataTaskInfo>(
      start_time, end_time, std::move(url_matcher), std::move(callback)));
  MaybePostExpiredDictionaryDeletionTask();
  MaybePostCacheEvictionTask();
}

void SharedDictionaryManagerOnDisk::PostSerializedTask(
    std::unique_ptr<SerializedTaskInfo> task_info) {
  pending_serialized_task_info_.push_back(std::move(task_info));
  MaybeStartSerializedTask();
}

void SharedDictionaryManagerOnDisk::OnFinishSerializedTask() {
  CHECK(running_serialized_task_);
  running_serialized_task_.reset();
  MaybeStartSerializedTask();
}

void SharedDictionaryManagerOnDisk::MaybeStartSerializedTask() {
  if (running_serialized_task_ || pending_serialized_task_info_.empty()) {
    return;
  }
  std::unique_ptr<SerializedTaskInfo> serialized_task_info =
      std::move(*pending_serialized_task_info_.begin());
  pending_serialized_task_info_.pop_front();
  running_serialized_task_ = serialized_task_info->CreateTask(this);
  running_serialized_task_->Start();
}

void SharedDictionaryManagerOnDisk::OnDictionaryDeletedFromDatabase(
    const std::set<base::UnguessableToken>& disk_cache_key_tokens) {
  for (const base::UnguessableToken& token : disk_cache_key_tokens) {
    disk_cache().DoomEntry(token.ToString(), base::DoNothing());
  }
  for (auto& it : storages()) {
    reinterpret_cast<SharedDictionaryStorageOnDisk*>(it.second.get())
        ->OnDictionaryDeleted(disk_cache_key_tokens);
  }
}

void SharedDictionaryManagerOnDisk::MaybePostCacheEvictionTask() {
  if (cache_eviction_task_queued_ || cache_max_size_ == 0) {
    return;
  }
  cache_eviction_task_queued_ = true;
  PostSerializedTask(std::make_unique<CacheEvictionTaskInfo>(base::BindOnce(
      [](base::WeakPtr<SharedDictionaryManagerOnDisk> weak_ptr) {
        if (weak_ptr) {
          CHECK(weak_ptr->cache_eviction_task_queued_);
          weak_ptr->cache_eviction_task_queued_ = false;
        }
      },
      weak_factory_.GetWeakPtr())));
}

void SharedDictionaryManagerOnDisk::MaybePostExpiredDictionaryDeletionTask() {
  if (expired_entry_deletion_task_queued_) {
    return;
  }
  expired_entry_deletion_task_queued_ = true;
  PostSerializedTask(
      std::make_unique<ExpiredDictionaryDeletionTaskInfo>(base::BindOnce(
          [](base::WeakPtr<SharedDictionaryManagerOnDisk> weak_ptr) {
            if (weak_ptr) {
              CHECK(weak_ptr->expired_entry_deletion_task_queued_);
              weak_ptr->expired_entry_deletion_task_queued_ = false;
            }
          },
          weak_factory_.GetWeakPtr())));
}

}  // namespace network

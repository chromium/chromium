// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"
#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"

namespace network {
namespace {

std::optional<base::UnguessableToken> DeserializeToUnguessableToken(
    const std::string& token_string) {
  std::optional<base::Token> token = base::Token::FromString(token_string);
  if (!token) {
    return std::nullopt;
  }
  return base::UnguessableToken::Deserialize(token->high(), token->low());
}

std::string ToCommaSeparatedString(
    const std::set<mojom::RequestDestination>& match_dest) {
  std::vector<std::string_view> destinations;
  for (auto dest : match_dest) {
    std::string_view result = RequestDestinationToString(
        dest, EmptyRequestDestinationOption::kUseFiveCharEmptyString);
    destinations.push_back(result);
  }
  return base::JoinString(destinations, ",");
}

}  // namespace

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
      manager_->OnDictionaryDeleted(result.value(),
                                    /*need_to_doom_disk_cache_entries=*/true);
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

class SharedDictionaryManagerOnDisk::ClearDataForIsolationKeyTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  ClearDataForIsolationKeyTask(
      raw_ptr<SharedDictionaryManagerOnDisk> manager,
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback)
      : manager_(manager),
        isolation_key_(isolation_key),
        callback_(std::move(callback)) {}
  ~ClearDataForIsolationKeyTask() override = default;

  ClearDataForIsolationKeyTask(const ClearDataForIsolationKeyTask&) = delete;
  ClearDataForIsolationKeyTask& operator=(const ClearDataForIsolationKeyTask&) =
      delete;

  void Start() override {
    manager_->metadata_store().ClearDictionariesForIsolationKey(
        isolation_key_,
        base::BindOnce(
            &ClearDataForIsolationKeyTask::OnClearDictionariesFinished,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void OnClearDictionariesFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeleted(result.value(),
                                    /*need_to_doom_disk_cache_entries=*/true);
    }
    std::move(callback_).Run();
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  const net::SharedDictionaryIsolationKey isolation_key_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<ClearDataForIsolationKeyTask> weak_factory_{this};
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

class SharedDictionaryManagerOnDisk::ClearDataForIsolationKeyTaskInfo
    : public SharedDictionaryManagerOnDisk::SerializedTaskInfo {
 public:
  ClearDataForIsolationKeyTaskInfo(
      net::SharedDictionaryIsolationKey isolation_key,
      base::OnceClosure callback)
      : isolation_key_(std::move(isolation_key)),
        callback_(std::move(callback)) {}
  ~ClearDataForIsolationKeyTaskInfo() override = default;

  ClearDataForIsolationKeyTaskInfo(const ClearDataForIsolationKeyTaskInfo&) =
      delete;
  ClearDataForIsolationKeyTaskInfo& operator=(
      const ClearDataForIsolationKeyTaskInfo&) = delete;

  std::unique_ptr<SerializedTask> CreateTask(
      SharedDictionaryManagerOnDisk* manager) override {
    return std::make_unique<ClearDataForIsolationKeyTask>(
        manager, isolation_key_, std::move(callback_));
  }

 private:
  const net::SharedDictionaryIsolationKey isolation_key_;
  base::OnceClosure callback_;
};

class SharedDictionaryManagerOnDisk::MismatchingEntryDeletionTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  explicit MismatchingEntryDeletionTask(
      raw_ptr<SharedDictionaryManagerOnDisk> manager)
      : manager_(manager) {}
  ~MismatchingEntryDeletionTask() override = default;
  MismatchingEntryDeletionTask(const MismatchingEntryDeletionTask&) = delete;
  MismatchingEntryDeletionTask& operator=(const MismatchingEntryDeletionTask&) =
      delete;

  void Start() override {
    // 1) Get the disk cache key tokens currently being written by the manager.
    writing_disk_cache_key_tokens_ = manager_->writing_disk_cache_key_tokens();
    // 2) Get the all disk cache key tokens in the metadata store.
    manager_->metadata_store().GetAllDiskCacheKeyTokens(BindOnce(
        &MismatchingEntryDeletionTask::OnAllDiskCacheKeyTokensInDatabase,
        weak_factory_.GetWeakPtr()));
  }

 private:
  void OnAllDiskCacheKeyTokensInDatabase(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (!result.has_value()) {
      manager_->OnFinishSerializedTask();
      return;
    }
    disk_cache_key_tokens_ = std::move(result.value());

    // 3) Get the iterator of the disk cache.
    manager_->disk_cache().CreateIterator(
        BindOnce(&MismatchingEntryDeletionTask::OnDiskCacheIterator,
                 weak_factory_.GetWeakPtr()));
  }
  void OnDiskCacheIterator(
      std::unique_ptr<disk_cache::Backend::Iterator> disk_cache_iterator) {
    if (!disk_cache_iterator) {
      // Disk cache is corrupted. So delete all entry from the metadata.
      CleanupDatabase();
      return;
    }
    disk_cache_iterator_ = std::move(disk_cache_iterator);
    OpenNextEntry();
  }

  void OpenNextEntry() {
    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&MismatchingEntryDeletionTask::OnDiskCacheEntry,
                       weak_factory_.GetWeakPtr()));

    // 4) For each disk cache entry, opens the entry.
    disk_cache::EntryResult result =
        disk_cache_iterator_->OpenNextEntry(std::move(split_callback.first));
    if (result.net_error() != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(std::move(result));
    }
  }
  void OnDiskCacheEntry(disk_cache::EntryResult result) {
    if (result.net_error() == net::ERR_FAILED) {
      // 8) The iteration is complete.
      CleanupDatabase();
      return;
    }
    if (result.net_error() < 0) {
      manager_->OnFinishSerializedTask();
      return;
    }
    disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());
    // 5) Get the disk cache key token of the entry.
    std::optional<base::UnguessableToken> token =
        DeserializeToUnguessableToken(entry->GetKey());
    if (!token) {
      // 6) If the disk cache entry key is not a valid token, deletes the entry.
      entry->Doom();
      ++invalid_disk_cache_entry_count_;
    } else if (disk_cache_key_tokens_.erase(*token) != 1) {
      if (!base::Contains(writing_disk_cache_key_tokens_, *token)) {
        // 7) If the disk cache key token is not in the metadata, and is not in
        //    the set of tokens currently being written by the manager, deletes
        //    the entry.
        entry->Doom();
        ++metadata_missing_dictionary_count_;
      }
    }
    OpenNextEntry();
  }
  void CleanupDatabase() {
    base::UmaHistogramCounts100(
        "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount",
        invalid_disk_cache_entry_count_);
    base::UmaHistogramCounts100(
        "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount",
        metadata_missing_dictionary_count_);
    base::UmaHistogramCounts100(
        "Net.SharedDictionaryManagerOnDisk."
        "DiskCacheEntryMissingDictionaryCount",
        disk_cache_key_tokens_.size());

    if (disk_cache_key_tokens_.empty()) {
      manager_->OnFinishSerializedTask();
      return;
    }
    // 9) `disk_cache_key_tokens_` contains the tokens which were in the
    //    metadata store, but not in the disk cache.

    // 10) Let the manager know such dictionaries are unavailable.
    manager_->OnDictionaryDeleted(disk_cache_key_tokens_,
                                  /*need_to_doom_disk_cache_entries=*/false);

    // 11) Deletes such dictionaries from the metadata store.
    manager_->metadata_store().DeleteDictionariesByDiskCacheKeyTokens(
        std::move(disk_cache_key_tokens_),
        base::BindOnce(&MismatchingEntryDeletionTask::CleanupDatabaseDone,
                       weak_factory_.GetWeakPtr()));
  }
  void CleanupDatabaseDone(
      net::SQLitePersistentSharedDictionaryStore::Error error) {
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  base::OnceCallback<void(int)> callback_;
  std::set<base::UnguessableToken> disk_cache_key_tokens_;
  std::set<base::UnguessableToken> writing_disk_cache_key_tokens_;
  std::unique_ptr<disk_cache::Backend::Iterator> disk_cache_iterator_;
  uint32_t invalid_disk_cache_entry_count_ = 0;
  uint32_t metadata_missing_dictionary_count_ = 0;
  base::WeakPtrFactory<MismatchingEntryDeletionTask> weak_factory_{this};
};

class SharedDictionaryManagerOnDisk::MismatchingEntryDeletionTaskInfo
    : public SharedDictionaryManagerOnDisk::SerializedTaskInfo {
 public:
  MismatchingEntryDeletionTaskInfo() = default;
  ~MismatchingEntryDeletionTaskInfo() override = default;
  MismatchingEntryDeletionTaskInfo(const MismatchingEntryDeletionTaskInfo&) =
      delete;
  MismatchingEntryDeletionTaskInfo& operator=(
      const MismatchingEntryDeletionTaskInfo&) = delete;
  std::unique_ptr<SerializedTask> CreateTask(
      SharedDictionaryManagerOnDisk* manager) override {
    return std::make_unique<MismatchingEntryDeletionTask>(manager);
  }
};

class SharedDictionaryManagerOnDisk::CacheEvictionTask
    : public SharedDictionaryManagerOnDisk::SerializedTask {
 public:
  CacheEvictionTask(raw_ptr<SharedDictionaryManagerOnDisk> manager,
                    uint64_t cache_max_size,
                    uint64_t cache_max_count)
      : manager_(manager),
        cache_max_size_(cache_max_size),
        cache_max_count_(cache_max_count) {}
  ~CacheEvictionTask() override = default;

  CacheEvictionTask(const CacheEvictionTask&) = delete;
  CacheEvictionTask& operator=(const CacheEvictionTask&) = delete;

  void Start() override {
    manager_->metadata_store().ProcessEviction(
        cache_max_size_, cache_max_size_ * 0.9, cache_max_count_,
        cache_max_count_ * 0.9,
        base::BindOnce(&CacheEvictionTask::OnProcessEvictionFinished,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnProcessEvictionFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeleted(result.value(),
                                    /*need_to_doom_disk_cache_entries=*/true);
    } else if (result.error() == net::SQLitePersistentSharedDictionaryStore::
                                     Error::kFailedToGetTotalDictSize) {
      // Assume the database gets corrupted for some reason, so call
      // ClearAllDictionaries() to reset the database.
      manager_->metadata_store().ClearAllDictionaries(
          base::BindOnce(&CacheEvictionTask::OnClearAllDictionariesFinished,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    manager_->OnFinishSerializedTask();
  }
  void OnClearAllDictionariesFinished(
      net::SQLitePersistentSharedDictionaryStore::UnguessableTokenSetOrError
          result) {
    if (result.has_value()) {
      manager_->OnDictionaryDeleted(result.value(),
                                    /*need_to_doom_disk_cache_entries=*/true);
    }
    manager_->OnFinishSerializedTask();
  }

  raw_ptr<SharedDictionaryManagerOnDisk> manager_;
  const uint64_t cache_max_size_;
  const uint64_t cache_max_count_;
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
    return std::make_unique<CacheEvictionTask>(
        manager, manager->cache_max_size(), manager->cache_max_count());
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
      manager_->OnDictionaryDeleted(result.value(),
                                    /*need_to_doom_disk_cache_entries=*/true);
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
    uint64_t cache_max_count,
#if BUILDFLAG(IS_ANDROID)
    disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory)
    : cache_max_size_(cache_max_size),
      cache_max_count_(cache_max_count),
      metadata_store_(database_path,
                      /*client_task_runner=*/
                      base::SingleThreadTaskRunner::GetCurrentDefault(),
                      /*background_task_runner=*/
                      base::ThreadPool::CreateSequencedTaskRunner(
                          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      cleanup_task_disabled_for_testing_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableSharedDictionaryStorageCleanupForTesting)) {
  disk_cache_.Initialize(cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
                         app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
                         std::move(file_operations_factory));
  MaybePostExpiredDictionaryDeletionTask();
  if (cache_max_size_ != 0u) {
    MaybePostCacheEvictionTask();
  }
}

SharedDictionaryManagerOnDisk::~SharedDictionaryManagerOnDisk() = default;

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerOnDisk::CreateStorage(
    const net::SharedDictionaryIsolationKey& isolation_key) {
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

void SharedDictionaryManagerOnDisk::GetUsageInfo(
    base::OnceCallback<void(const std::vector<net::SharedDictionaryUsageInfo>&)>
        callback) {
  metadata_store_.GetUsageInfo(base::BindOnce(
      [](base::OnceCallback<void(
             const std::vector<net::SharedDictionaryUsageInfo>&)> callback,
         net::SQLitePersistentSharedDictionaryStore::UsageInfoOrError result) {
        if (result.has_value()) {
          std::move(callback).Run(std::move(result.value()));
        } else {
          std::move(callback).Run({});
        }
      },
      std::move(callback)));
}

void SharedDictionaryManagerOnDisk::GetSharedDictionaryInfo(
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::OnceCallback<
        void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback) {
  metadata_store_.GetDictionaries(
      isolation_key,
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::vector<network::mojom::SharedDictionaryInfoPtr>)>
                 callback,
             net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError
                 result) {
            std::vector<network::mojom::SharedDictionaryInfoPtr> dictionaries;
            if (!result.has_value()) {
              std::move(callback).Run(std::move(dictionaries));
              return;
            }
            for (auto& info : result.value()) {
              dictionaries.push_back(ToMojoSharedDictionaryInfo(
                  SharedDictionaryStorageOnDisk::WrappedDictionaryInfo(
                      info, /*matcher=*/nullptr)));
            }
            std::move(callback).Run(std::move(dictionaries));
          },
          std::move(callback)));
}

void SharedDictionaryManagerOnDisk::GetOriginsBetween(
    base::Time start_time,
    base::Time end_time,
    base::OnceCallback<void(const std::vector<url::Origin>&)> callback) {
  metadata_store_.GetOriginsBetween(
      start_time, end_time,
      base::BindOnce(
          [](base::OnceCallback<void(const std::vector<url::Origin>&)> callback,
             net::SQLitePersistentSharedDictionaryStore::OriginListOrError
                 result) {
            std::move(callback).Run(
                result.value_or(std::vector<url::Origin>()));
          },
          std::move(callback)));
}

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryManagerOnDisk::CreateWriter(
    const net::SharedDictionaryIsolationKey& isolation_key,
    const GURL& url,
    base::Time last_fetch_time,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback) {
  const base::UnguessableToken disk_cache_key_token =
      base::UnguessableToken::Create();
  CHECK(writing_disk_cache_key_tokens_.insert(disk_cache_key_token).second);
  auto writer = base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
      disk_cache_key_token,
      base::BindOnce(
          &SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDiskCache,
          weak_factory_.GetWeakPtr(), isolation_key, url, last_fetch_time,
          response_time, expiration, match, match_dest, id,
          disk_cache_key_token, std::move(callback)),
      disk_cache_.GetWeakPtr());
  writer->Initialize();
  return writer;
}

void SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDiskCache(
    const net::SharedDictionaryIsolationKey& isolation_key,
    const GURL& url,
    base::Time last_fetch_time,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    const base::UnguessableToken& disk_cache_key_token,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
    SharedDictionaryWriterOnDisk::Result result,
    size_t size,
    const net::SHA256HashValue& hash) {
  if (result != SharedDictionaryWriterOnDisk::Result::kSuccess) {
    CHECK(writing_disk_cache_key_tokens_.erase(disk_cache_key_token) == 1);

    if (result ==
        SharedDictionaryWriterOnDisk::Result::kErrorCreateEntryFailed) {
      MaybePostMismatchingEntryDeletionTask();
    }
    return;
  }
  base::Time last_used_time = base::Time::Now();
  net::SharedDictionaryInfo info(
      url, last_fetch_time, response_time, expiration, match,
      ToCommaSeparatedString(match_dest), id, last_used_time, size, hash,
      disk_cache_key_token,
      /*primary_key_in_database=*/std::nullopt);
  metadata_store_.RegisterDictionary(
      isolation_key, info,
      /*max_size_per_site*/ cache_max_size_ / 2,
      /*max_count_per_site*/ cache_max_count_ / 2,
      base::BindOnce(
          &SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDatabase,
          weak_factory_.GetWeakPtr(), info, std::move(callback)));
}

void SharedDictionaryManagerOnDisk::OnDictionaryWrittenInDatabase(
    net::SharedDictionaryInfo info,
    base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
    net::SQLitePersistentSharedDictionaryStore::RegisterDictionaryResultOrError
        result) {
  CHECK(writing_disk_cache_key_tokens_.erase(info.disk_cache_key_token()) == 1);
  if (!result.has_value()) {
    disk_cache_.DoomEntry(info.disk_cache_key_token().ToString(),
                          base::DoNothing());
    return;
  }

  base::UmaHistogramMemoryKB(
      "Net.SharedDictionaryManagerOnDisk.DictionarySizeKB", info.size());
  base::UmaHistogramMemoryKB(
      "Net.SharedDictionaryManagerOnDisk.TotalDictionarySizeKBWhenAdded",
      result.value().total_dictionary_size());
  base::UmaHistogramCounts1000(
      "Net.SharedDictionaryManagerOnDisk.TotalDictionaryCountWhenAdded",
      result.value().total_dictionary_count());
  info.set_primary_key_in_database(result.value().primary_key_in_database());

  if (result.value().replaced_disk_cache_key_token()) {
    disk_cache_.DoomEntry(
        result.value().replaced_disk_cache_key_token()->ToString(),
        base::DoNothing());
  }
  if (!result.value().evicted_disk_cache_key_tokens().empty()) {
    OnDictionaryDeleted(result.value().evicted_disk_cache_key_tokens(),
                        /*need_to_doom_disk_cache_entries=*/true);
  }
  std::move(callback).Run(std::move(info));

  MaybePostExpiredDictionaryDeletionTask();

  if ((cache_max_size_ == 0 ||
       result.value().total_dictionary_size() <= cache_max_size_) &&
      result.value().total_dictionary_count() <= cache_max_count_) {
    return;
  }
  MaybePostCacheEvictionTask();
}

void SharedDictionaryManagerOnDisk::UpdateDictionaryLastFetchTime(
    net::SharedDictionaryInfo& info,
    base::Time last_fetch_time) {
  info.set_last_fetch_time(last_fetch_time);
  CHECK(info.primary_key_in_database());
  metadata_store_.UpdateDictionaryLastFetchTime(
      *info.primary_key_in_database(), info.last_fetch_time(),
      base::BindOnce([](net::SQLitePersistentSharedDictionaryStore::Error) {}));
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
  MaybePostMismatchingEntryDeletionTask();
}

void SharedDictionaryManagerOnDisk::ClearDataForIsolationKey(
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::OnceClosure callback) {
  PostSerializedTask(std::make_unique<ClearDataForIsolationKeyTaskInfo>(
      isolation_key, std::move(callback)));
  MaybePostExpiredDictionaryDeletionTask();
}

void SharedDictionaryManagerOnDisk::PostSerializedTask(
    std::unique_ptr<SerializedTaskInfo> task_info) {
  if (cleanup_task_disabled_for_testing_) {
    return;
  }
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

void SharedDictionaryManagerOnDisk::OnDictionaryDeleted(
    const std::set<base::UnguessableToken>& disk_cache_key_tokens,
    bool need_to_doom_disk_cache_entries) {
  if (need_to_doom_disk_cache_entries) {
    for (const base::UnguessableToken& token : disk_cache_key_tokens) {
      disk_cache().DoomEntry(token.ToString(), base::DoNothing());
    }
  }
  for (auto& it : storages()) {
    reinterpret_cast<SharedDictionaryStorageOnDisk*>(it.second.get())
        ->OnDictionaryDeleted(disk_cache_key_tokens);
  }
}

void SharedDictionaryManagerOnDisk::MaybePostMismatchingEntryDeletionTask() {
  // MismatchingEntryDeletionTask is intended to resolve the mismatch between
  // the disk cache and metadata database. We run MismatchingEntryDeletionTask
  // only once in the lifetime of the manager when ClearData() is called or
  // disk cache error is detected.
  if (mismatching_entry_deletion_task_posted_) {
    return;
  }
  mismatching_entry_deletion_task_posted_ = true;
  PostSerializedTask(std::make_unique<MismatchingEntryDeletionTaskInfo>());
}

void SharedDictionaryManagerOnDisk::MaybePostCacheEvictionTask() {
  if (cache_eviction_task_queued_) {
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

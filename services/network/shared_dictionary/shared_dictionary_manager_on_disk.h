// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "net/disk_cache/disk_cache.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"

class GURL;

namespace base {
class FilePath;
}  //  namespace base

namespace disk_cache {
class BackendFileOperationsFactory;
}  // namespace disk_cache

namespace network {
namespace mojom {
enum class RequestDestination : int32_t;
}  // namespace mojom

class SharedDictionaryStorage;

// A SharedDictionaryManager which persists dictionary information on disk.
class SharedDictionaryManagerOnDisk : public SharedDictionaryManager {
 public:
  SharedDictionaryManagerOnDisk(
      const base::FilePath& database_path,
      const base::FilePath& cache_directory_path,
      uint64_t cache_max_size,
      uint64_t cache_max_count,
#if BUILDFLAG(IS_ANDROID)
      disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory);

  SharedDictionaryManagerOnDisk(const SharedDictionaryManagerOnDisk&) = delete;
  SharedDictionaryManagerOnDisk& operator=(
      const SharedDictionaryManagerOnDisk&) = delete;

  ~SharedDictionaryManagerOnDisk() override;

  // SharedDictionaryManager
  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) override;
  void SetCacheMaxSize(uint64_t cache_max_size) override;
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override;
  void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) override;
  void GetUsageInfo(base::OnceCallback<
                    void(const std::vector<net::SharedDictionaryUsageInfo>&)>
                        callback) override;
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<
          void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback)
      override;
  void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback)
      override;

  SharedDictionaryDiskCache& disk_cache() { return disk_cache_; }
  net::SQLitePersistentSharedDictionaryStore& metadata_store() {
    return metadata_store_;
  }

  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const net::SharedDictionaryIsolationKey& isolation_key,
      const GURL& url,
      base::Time last_fetch_time,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      const std::set<mojom::RequestDestination>& match_dest,
      const std::string& id,
      base::OnceCallback<void(net::SharedDictionaryInfo)> callback);

  void UpdateDictionaryLastFetchTime(net::SharedDictionaryInfo& info,
                                     base::Time last_fetch_time);
  void UpdateDictionaryLastUsedTime(net::SharedDictionaryInfo& info);

  // Posts a MismatchingEntryDeletionTask if this method is called for the first
  // time.
  void MaybePostMismatchingEntryDeletionTask();

  // Posts a ExpiredDictionaryDeletionTask if there is no ongoing or queued
  // MismatchingEntryDeletionTask.
  void MaybePostExpiredDictionaryDeletionTask();

 private:
  class SerializedTask {
   public:
    virtual ~SerializedTask() = default;
    virtual void Start() = 0;
  };
  class SerializedTaskInfo {
   public:
    virtual ~SerializedTaskInfo() = default;
    virtual std::unique_ptr<SerializedTask> CreateTask(
        SharedDictionaryManagerOnDisk*) = 0;
  };

  class ClearDataTask;
  class ClearDataForIsolationKeyTask;
  class MismatchingEntryDeletionTask;
  class CacheEvictionTask;
  class ExpiredDictionaryDeletionTask;

  class ClearDataTaskInfo;
  class ClearDataForIsolationKeyTaskInfo;
  class MismatchingEntryDeletionTaskInfo;
  class CacheEvictionTaskInfo;
  class ExpiredDictionaryDeletionTaskInfo;

  void OnDictionaryWrittenInDiskCache(
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
      const net::SHA256HashValue& hash);

  void OnDictionaryWrittenInDatabase(
      net::SharedDictionaryInfo info,
      base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
      net::SQLitePersistentSharedDictionaryStore::
          RegisterDictionaryResultOrError result);

  void PostSerializedTask(std::unique_ptr<SerializedTaskInfo> task_info);
  void OnFinishSerializedTask();
  void MaybeStartSerializedTask();

  void MaybePostCacheEvictionTask();

  void OnDictionaryDeleted(
      const std::set<base::UnguessableToken>& disk_cache_key_tokens,
      bool need_to_doom_disk_cache_entries);

  const std::set<base::UnguessableToken>& writing_disk_cache_key_tokens()
      const {
    return writing_disk_cache_key_tokens_;
  }

  uint64_t cache_max_size() const { return cache_max_size_; }
  uint64_t cache_max_count() const { return cache_max_count_; }

  uint64_t cache_max_size_;
  const uint64_t cache_max_count_;
  SharedDictionaryDiskCache disk_cache_;
  net::SQLitePersistentSharedDictionaryStore metadata_store_;

  std::unique_ptr<SerializedTask> running_serialized_task_;
  std::deque<std::unique_ptr<SerializedTaskInfo>> pending_serialized_task_info_;

  std::set<base::UnguessableToken> writing_disk_cache_key_tokens_;

  bool mismatching_entry_deletion_task_posted_ = false;
  bool cache_eviction_task_queued_ = false;
  bool expired_entry_deletion_task_queued_ = false;

  bool cleanup_task_disabled_for_testing_ = false;

  base::WeakPtrFactory<SharedDictionaryManagerOnDisk> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_

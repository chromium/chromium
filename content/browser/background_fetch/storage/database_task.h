// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class QuotaManagerProxy;
}  // namespace storage

namespace content {

class BackgroundFetchDataManager;
class ChromeBlobStorageContext;
class ServiceWorkerContextWrapper;

// Note that this also handles non-error cases where the NONE is NONE.
using HandleBackgroundFetchErrorCallback =
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;

namespace background_fetch {

class DatabaseTask;

// A DatabaseTaskHost is the entity responsible for scheduling and running
// DatabaseTasks, usually the BackgroundFetchDataManager. Note that a
// DatabaseTask can be a DatabaseTaskHost, in order to run subtasks.
class DatabaseTaskHost {
 public:
  virtual void OnTaskFinished(DatabaseTask* task) = 0;
  virtual BackgroundFetchDataManager* data_manager() = 0;
  virtual ~DatabaseTaskHost();

  virtual base::WeakPtr<DatabaseTaskHost> GetWeakPtr() = 0;

 protected:
  DatabaseTaskHost();
};

// A DatabaseTask is an asynchronous "transaction" that needs to read/write the
// Service Worker Database.
//
// Only one DatabaseTask can run at once per StoragePartition, and no other code
// reads/writes Background Fetch keys, so each task effectively has an exclusive
// lock, except that core Service Worker code may delete all keys for a
// ServiceWorkerRegistration or the entire database at any time.
//
// A DatabaseTask can also optionally run other DatabaseTasks as subtasks. This
// allows us to re-use commonly used DatabaseTasks. Subtasks are started as soon
// as they are added, and cannot outlive the parent DatabaseTask.
class DatabaseTask : public DatabaseTaskHost {
 public:
  using IsQuotaAvailableCallback = base::OnceCallback<void(bool is_available)>;
  using StorageVersionCallback =
      base::OnceCallback<void(proto::BackgroundFetchStorageVersion)>;

  DatabaseTask(const DatabaseTask&) = delete;
  DatabaseTask& operator=(const DatabaseTask&) = delete;

  ~DatabaseTask() override;

  virtual void Start() = 0;

 protected:
  // This enum is append-only since it is used by UMA.
  enum class BackgroundFetchStorageError {
    kNone,
    kServiceWorkerStorageError,
    kCacheStorageError,
    kStorageError,
    kMaxValue = kStorageError
  };

  explicit DatabaseTask(DatabaseTaskHost* host);

  // Each task MUST call this once finished, even if exceptions occur, to
  // release their lock and allow the next task to execute.
  // This should be called in FinishWithError() for consistency.
  void Finished();

  void AddDatabaseTask(std::unique_ptr<DatabaseTask> task);
  void AddSubTask(std::unique_ptr<DatabaseTask> task);

  // Abandon all fetches for a given service worker.
  void AbandonFetches(int64_t service_worker_registration_id);

  // Getters.
  ServiceWorkerContextWrapper* service_worker_context();
  std::set<std::string>& ref_counted_unique_ids();
  ChromeBlobStorageContext* blob_storage_context();
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy();

  // DatabaseTaskHost implementation.
  void OnTaskFinished(DatabaseTask* finished_subtask) override;
  BackgroundFetchDataManager* data_manager() override;

  // UMA reporting.
  void SetStorageError(BackgroundFetchStorageError error);
  void SetStorageErrorAndFinish(BackgroundFetchStorageError error);
  bool HasStorageError();

  // Quota.
  void IsQuotaAvailable(const blink::StorageKey& storage_key,
                        int64_t size,
                        IsQuotaAvailableCallback callback);

  void GetStorageVersion(int64_t service_worker_registration_id,
                         const std::string& unique_id,
                         StorageVersionCallback callback);

  // Opens a cache and returns the CacheStorageError from doing so.
  // If the CacheStorageError is kSuccess, then cache_storage_cache_remote()
  // will have the bound receiver.  Can only be called once per task.
  // DatabaseTask owns the remote so that callers don't have to chain it
  // through callbacks to keep it alive.
  void OpenCache(
      const BackgroundFetchRegistrationId& registration_id,
      int64_t trace_id,
      base::OnceCallback<void(blink::mojom::CacheStorageError)> callback);
  void DeleteCache(const blink::StorageKey& storage_key,
                   const std::string& unique_id,
                   int64_t trace_id,
                   blink::mojom::CacheStorage::DeleteCallback callback);

  const mojo::AssociatedRemote<blink::mojom::CacheStorageCache>&
  cache_storage_cache_remote() const {
    return cache_storage_cache_remote_;
  }

 private:
  // Each task must override this function and perform the following steps:
  // 1) Report storage error (UMA) if applicable.
  // 2) Run the provided callback.
  // 3) Call Finished().
  virtual void FinishWithError(blink::mojom::BackgroundFetchError error) = 0;

  void DidGetStorageVersion(StorageVersionCallback callback,
                            const std::vector<std::string>& data,
                            blink::ServiceWorkerStatusCode status);
  void DidOpenCache(
      base::OnceCallback<void(blink::mojom::CacheStorageError)> callback,
      blink::mojom::OpenResultPtr result);

  base::WeakPtr<DatabaseTaskHost> GetWeakPtr() override;

  raw_ptr<DatabaseTaskHost> host_;

  // Map the raw pointer to its unique_ptr, to make lookups easier.
  std::map<DatabaseTask*, std::unique_ptr<DatabaseTask>> active_subtasks_;

  // The storage error to report.
  BackgroundFetchStorageError storage_error_ =
      BackgroundFetchStorageError::kNone;

  mojo::AssociatedRemote<blink::mojom::CacheStorageCache>
      cache_storage_cache_remote_;

  base::WeakPtrFactory<DatabaseTask> weak_ptr_factory_{this};
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DATABASE_TASK_H_

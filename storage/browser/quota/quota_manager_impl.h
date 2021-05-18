// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace quota_internals {
class QuotaInternalsProxy;
}  // namespace quota_internals

namespace storage {

class QuotaManagerProxy;
class QuotaOverrideHandle;
class QuotaTemporaryStorageEvictor;
class UsageTracker;

// An interface called by QuotaTemporaryStorageEvictor. This is a grab bag of
// methods called by QuotaTemporaryStorageEvictor that need to be stubbed for
// testing.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaEvictionHandler {
 public:
  using EvictionRoundInfoCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode status,
                              const QuotaSettings& settings,
                              int64_t available_space,
                              int64_t total_space,
                              int64_t global_usage,
                              bool global_usage_is_complete)>;

  // Called at the beginning of an eviction round to gather the info about
  // the current settings, capacity, and usage.
  virtual void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) = 0;

  // Returns next origin to evict, or nullopt if there are no evictable
  // origins.
  virtual void GetEvictionOrigin(blink::mojom::StorageType type,
                                 int64_t global_quota,
                                 GetOriginCallback callback) = 0;

  // Called to evict an origin.
  virtual void EvictOriginData(const url::Origin& origin,
                               blink::mojom::StorageType type,
                               StatusCallback callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() = default;
};

struct UsageInfo {
  UsageInfo(std::string host, blink::mojom::StorageType type, int64_t usage)
      : host(std::move(host)), type(type), usage(usage) {}
  const std::string host;
  const blink::mojom::StorageType type;
  const int64_t usage;
};

// Entry point into the Quota System
//
// Each StoragePartition has exactly one QuotaManagerImpl instance, which
// coordinates quota across the Web platform features subject to quota.
// Each storage system interacts with quota via their own implementations of
// the QuotaClient interface.
//
// The class sets limits and defines the parameters of the systems heuristics.
// QuotaManagerImpl coordinates clients to orchestrate the collection of usage
// information, enforce quota limits, and evict stale data.
//
// The constructor and proxy() methods can be called on any thread. All other
// methods must be called on the IO thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManagerImpl
    : public QuotaTaskObserver,
      public QuotaEvictionHandler,
      public base::RefCountedDeleteOnSequence<QuotaManagerImpl> {
 public:
  using UsageAndQuotaCallback = base::OnceCallback<
      void(blink::mojom::QuotaStatusCode, int64_t usage, int64_t quota)>;

  using UsageAndQuotaWithBreakdownCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode,
                              int64_t usage,
                              int64_t quota,
                              blink::mojom::UsageBreakdownPtr usage_breakdown)>;

  using UsageAndQuotaForDevtoolsCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode,
                              int64_t usage,
                              int64_t quota,
                              bool is_override_enabled,
                              blink::mojom::UsageBreakdownPtr usage_breakdown)>;

  // Function pointer type used to store the function which returns
  // information about the volume containing the given FilePath.
  // The value returned is std::tuple<total_space, available_space>.
  using GetVolumeInfoFn =
      std::tuple<int64_t, int64_t> (*)(const base::FilePath&);

  static constexpr int64_t kGBytes = 1024 * 1024 * 1024;
  static constexpr int64_t kNoLimit = INT64_MAX;
  static constexpr int64_t kMBytes = 1024 * 1024;
  static constexpr int kMinutesInMilliSeconds = 60 * 1000;

  QuotaManagerImpl(bool is_incognito,
                   const base::FilePath& profile_path,
                   scoped_refptr<base::SingleThreadTaskRunner> io_thread,
                   base::RepeatingClosure quota_change_callback,
                   scoped_refptr<SpecialStoragePolicy> special_storage_policy,
                   const GetQuotaSettingsFunc& get_settings_function);
  QuotaManagerImpl(const QuotaManagerImpl&) = delete;
  QuotaManagerImpl& operator=(const QuotaManagerImpl&) = delete;

  const QuotaSettings& settings() const { return settings_; }
  void SetQuotaSettings(const QuotaSettings& settings);

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Creates a bucket for `origin` with `bucket_name` and returns the BucketId
  // to the callback. Will return a QuotaError to the callback on failure.
  void CreateBucket(const url::Origin& origin,
                    const std::string& bucket_name,
                    base::OnceCallback<void(QuotaErrorOr<BucketId>)>);

  // Retrieves the BucketId of the bucket with `bucket_name` for `origin` and
  // returns it to the callback. Will return an empty BucketId if a bucket does
  // not exist. Will return a QuotaError on operation failure.
  void GetBucketId(const url::Origin& origin,
                   const std::string& bucket_name,
                   base::OnceCallback<void(QuotaErrorOr<BucketId>)>);

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(GetUsageInfoCallback callback);

  // Called by Web Apps (deprecated quota API).
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaForWebApps(const url::Origin& origin,
                                          blink::mojom::StorageType type,
                                          UsageAndQuotaCallback callback);

  // Called by Web Apps (navigator.storage.estimate())
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaWithBreakdown(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      UsageAndQuotaWithBreakdownCallback callback);

  // Called by DevTools.
  virtual void GetUsageAndQuotaForDevtools(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      UsageAndQuotaForDevtoolsCallback callback);

  // Called by storage backends.
  //
  // For UnlimitedStorage origins, this version skips usage and quota handling
  // to avoid extra query cost. Do not call this method for apps/user-facing
  // code.
  //
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuota(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                UsageAndQuotaCallback callback);

  // Called by storage backends via proxy.
  //
  // Quota-managed storage backends should call this method when storage is
  // accessed. Used to maintain LRU ordering.
  void NotifyStorageAccessed(const url::Origin& origin,
                             blink::mojom::StorageType type,
                             base::Time access_time);

  // Called by storage backends via proxy.
  //
  // Quota-managed storage backends must call this method when they have made
  // any modifications that change the amount of data stored in their storage.
  void NotifyStorageModified(QuotaClientType client_id,
                             const url::Origin& origin,
                             blink::mojom::StorageType type,
                             int64_t delta,
                             base::Time modification_time,
                             base::OnceClosure callback);

  // Called by storage backends via proxy.
  //
  // Client storage must call this method whenever they run into disk
  // write errors. Used as a hint to determine if the storage partition is out
  // of space, and trigger actions if deemed appropriate.
  //
  // This method is declared as virtual to allow test code to override it.
  virtual void NotifyWriteFailed(const url::Origin& origin);

  // Used to avoid evicting origins with open pages.
  // A call to NotifyOriginInUse must be balanced by a later call
  // to NotifyOriginNoLongerInUse.
  void NotifyOriginInUse(const url::Origin& origin);
  void NotifyOriginNoLongerInUse(const url::Origin& origin);
  bool IsOriginInUse(const url::Origin& origin) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::Contains(origins_in_use_, origin);
  }

  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const url::Origin& origin,
                            blink::mojom::StorageType type,
                            bool enabled);

  // DeleteOriginData and DeleteHostData (surprisingly enough) delete data of a
  // particular blink::mojom::StorageType associated with either a specific
  // origin or set of origins. Each method additionally requires a
  // |quota_client_types| which specifies the types of QuotaClients to delete
  // from the origin. Pass in QuotaClientType::AllClients() to remove all
  // clients from the origin, regardless of type.
  virtual void DeleteOriginData(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                QuotaClientTypes quota_client_types,
                                StatusCallback callback);
  void DeleteHostData(const std::string& host,
                      blink::mojom::StorageType type,
                      QuotaClientTypes quota_client_types,
                      StatusCallback callback);

  // Instructs each QuotaClient to remove possible traces of deleted
  // data on the disk.
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             QuotaClientTypes quota_client_types,
                             base::OnceClosure callback);

  // Called by UI and internal modules.
  void GetPersistentHostQuota(const std::string& host, QuotaCallback callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64_t new_quota,
                              QuotaCallback callback);
  void GetGlobalUsage(blink::mojom::StorageType type,
                      GlobalUsageCallback callback);
  void GetHostUsageWithBreakdown(const std::string& host,
                                 blink::mojom::StorageType type,
                                 UsageWithBreakdownCallback callback);

  std::map<std::string, std::string> GetStatistics();

  bool IsStorageUnlimited(const url::Origin& origin,
                          blink::mojom::StorageType type) const;

  virtual void GetOriginsModifiedBetween(blink::mojom::StorageType type,
                                         base::Time begin,
                                         base::Time end,
                                         GetOriginsCallback callback);

  bool ResetUsageTracker(blink::mojom::StorageType type);

  // Called when StoragePartition is initialized if embedder has an
  // implementation of StorageNotificationService.
  void SetStoragePressureCallback(
      base::RepeatingCallback<void(url::Origin)> storage_pressure_callback);

  // DevTools Quota Override methods:
  int GetOverrideHandleId();
  void OverrideQuotaForOrigin(int handle_id,
                              const url::Origin& origin,
                              absl::optional<int64_t> quota_size);
  // Called when a DevTools client releases all overrides, however, overrides
  // will not be disabled for any origins for which there are other DevTools
  // clients/QuotaOverrideHandle with an active override.
  void WithdrawOverridesForHandle(int handle_id);

  // Cap size for per-host persistent quota determined by the histogram.
  // Cap size for per-host persistent quota determined by the histogram.
  // This is a bit lax value because the histogram says nothing about per-host
  // persistent storage usage and we determined by global persistent storage
  // usage that is less than 10GB for almost all users.
  static constexpr int64_t kPerHostPersistentQuotaLimit = 10 * 1024 * kMBytes;

  static constexpr int kEvictionIntervalInMilliSeconds =
      30 * kMinutesInMilliSeconds;
  static constexpr int kThresholdOfErrorsToBeDenylisted = 3;
  static constexpr int kThresholdRandomizationPercent = 5;

  static constexpr char kDatabaseName[] = "QuotaManager";
  static constexpr char kDaysBetweenRepeatedOriginEvictionsHistogram[] =
      "Quota.DaysBetweenRepeatedOriginEvictions";
  static constexpr char kEvictedOriginAccessedCountHistogram[] =
      "Quota.EvictedOriginAccessCount";
  static constexpr char kEvictedOriginDaysSinceAccessHistogram[] =
      "Quota.EvictedOriginDaysSinceAccess";

  // Kept non-const so that test code can change the value.
  // TODO(kinuko): Make this a real const value and add a proper way to set
  // the quota for syncable storage. (http://crbug.com/155488)
  static int64_t kSyncableStorageDefaultHostQuota;

  void DisableDatabaseForTesting() { db_disabled_ = true; }

  void SetGetVolumeInfoFnForTesting(GetVolumeInfoFn fn) {
    get_volume_info_fn_ = fn;
  }

 protected:
  ~QuotaManagerImpl() override;
  void SetQuotaChangeCallbackForTesting(
      base::RepeatingClosure storage_pressure_event_callback);

 private:
  friend class base::DeleteHelper<QuotaManagerImpl>;
  friend class base::RefCountedDeleteOnSequence<QuotaManagerImpl>;
  friend class quota_internals::QuotaInternalsProxy;
  friend class MockQuotaManager;
  friend class MockQuotaClient;
  friend class QuotaManagerProxy;
  friend class QuotaManagerImplTest;
  friend class QuotaTemporaryStorageEvictor;

  class EvictionRoundInfoHelper;
  class UsageAndQuotaInfoGatherer;
  class GetUsageInfoTask;
  class OriginDataDeleter;
  class HostDataDeleter;
  class GetModifiedSinceHelper;
  class DumpQuotaTableHelper;
  class DumpBucketTableHelper;
  class StorageCleanupHelper;

  struct QuotaOverride {
    QuotaOverride();
    ~QuotaOverride();

    QuotaOverride(const QuotaOverride& quota_override) = delete;
    QuotaOverride& operator=(const QuotaOverride&) = delete;

    int64_t quota_size;

    // Keeps track of the DevTools clients that have an active override.
    std::set<int> active_override_session_ids;
  };

  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using BucketTableEntry = QuotaDatabase::BucketTableEntry;
  using QuotaTableEntries = std::vector<QuotaTableEntry>;
  using BucketTableEntries = std::vector<BucketTableEntry>;

  using QuotaSettingsCallback = base::OnceCallback<void(const QuotaSettings&)>;

  using DumpQuotaTableCallback =
      base::OnceCallback<void(const QuotaTableEntries&)>;
  using DumpBucketTableCallback =
      base::OnceCallback<void(const BucketTableEntries&)>;

  // The values returned total_space, available_space.
  using StorageCapacityCallback = base::OnceCallback<void(int64_t, int64_t)>;

  struct EvictionContext {
    EvictionContext();
    ~EvictionContext();
    url::Origin evicted_origin;
    blink::mojom::StorageType evicted_type;
    StatusCallback evict_origin_data_callback;
  };

  // Lazily called on the IO thread when the first quota manager API is called.
  //
  // Initialize() must be called after all quota clients are added to the
  // manager by RegisterClient().
  void LazyInitialize();
  void FinishLazyInitialize(bool is_database_bootstraped);
  void BootstrapDatabaseForEviction(GetOriginCallback did_get_origin_callback,
                                    int64_t unused_usage,
                                    int64_t unused_unlimited_usage);
  void DidBootstrapDatabase(GetOriginCallback did_get_origin_callback,
                            bool success);

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types);

  // Legacy overload for QuotaClients that have not been mojofied yet.
  //
  // TODO(crbug.com/1163009): Remove this overload after all QuotaClients have
  //                          been mojofied.
  void RegisterLegacyClient(
      scoped_refptr<QuotaClient> client,
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types);

  UsageTracker* GetUsageTracker(blink::mojom::StorageType type) const;

  // Extract cached origins list from the usage tracker.
  // (Might return empty list if no origin is tracked by the tracker.)
  std::set<url::Origin> GetCachedOrigins(blink::mojom::StorageType type);

  void DumpQuotaTable(DumpQuotaTableCallback callback);
  void DumpBucketTable(DumpBucketTableCallback callback);

  void DeleteOriginDataInternal(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                QuotaClientTypes quota_client_types,
                                bool is_eviction,
                                StatusCallback callback);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteOriginFromDatabase(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                bool is_eviction);

  void DidOriginDataEvicted(blink::mojom::QuotaStatusCode status);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(int64_t usage,
                                              int64_t unlimited_usage);
  void DidGetStorageCapacityForHistogram(int64_t usage,
                                         int64_t total_space,
                                         int64_t available_space);
  void DidGetPersistentGlobalUsageForHistogram(int64_t usage,
                                               int64_t unlimited_usage);
  void DidDumpBucketTableForHistogram(const BucketTableEntries& entries);

  std::set<url::Origin> GetEvictionOriginExceptions();
  void DidGetEvictionOrigin(GetOriginCallback callback,
                            const absl::optional<url::Origin>& origin);

  // QuotaEvictionHandler.
  void GetEvictionOrigin(blink::mojom::StorageType type,
                         int64_t global_quota,
                         GetOriginCallback callback) override;
  void EvictOriginData(const url::Origin& origin,
                       blink::mojom::StorageType type,
                       StatusCallback callback) override;
  void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) override;

  void GetLRUOrigin(blink::mojom::StorageType type, GetOriginCallback callback);

  void DidGetPersistentHostQuota(const std::string& host,
                                 const int64_t* quota,
                                 bool success);
  void DidSetPersistentHostQuota(const std::string& host,
                                 QuotaCallback callback,
                                 const int64_t* new_quota,
                                 bool success);
  void DidGetLRUOrigin(std::unique_ptr<absl::optional<url::Origin>> origin,
                       bool success);
  void GetQuotaSettings(QuotaSettingsCallback callback);
  void DidGetSettings(absl::optional<QuotaSettings> settings);
  void GetStorageCapacity(StorageCapacityCallback callback);
  void ContinueIncognitoGetStorageCapacity(const QuotaSettings& settings);
  void DidGetStorageCapacity(
      const std::tuple<int64_t, int64_t>& total_and_available);

  void DidDatabaseWork(bool success);

  void DidGetBucketId(base::OnceCallback<void(QuotaErrorOr<BucketId>)> callback,
                      QuotaErrorOr<BucketId> result);

  void DeleteOnCorrectThread() const;

  void MaybeRunStoragePressureCallback(const url::Origin& origin,
                                       int64_t total_space,
                                       int64_t available_space);
  // Used from quota-internals page to test behavior of the storage pressure
  // callback.
  void SimulateStoragePressure(const url::Origin origin);

  // Evaluates disk statistics to identify storage pressure
  // (low disk space availability) and starts the storage
  // pressure event dispatch if appropriate.
  // TODO(crbug.com/1088004): Implement UsageAndQuotaInfoGatherer::Completed()
  // to use DetermineStoragePressure().
  // TODO(crbug.com/1102433): Define and explain StoragePressure in the README.
  void DetermineStoragePressure(int64_t free_space, int64_t total_space);

  absl::optional<int64_t> GetQuotaOverrideForOrigin(const url::Origin&);

  // TODO(ayui): Replace instances to use result with QuotaErrorOr.
  void PostTaskAndReplyWithResultForDBThread(
      const base::Location& from_here,
      base::OnceCallback<bool(QuotaDatabase*)> task,
      base::OnceCallback<void(bool)> reply);

  template <typename ValueType>
  void PostTaskAndReplyWithResultForDBThread(
      base::OnceCallback<QuotaErrorOr<ValueType>(QuotaDatabase*)> task,
      base::OnceCallback<void(QuotaErrorOr<ValueType>)> reply,
      const base::Location& from_here = base::Location::Current());

  static std::tuple<int64_t, int64_t> CallGetVolumeInfo(
      GetVolumeInfoFn get_volume_info_fn,
      const base::FilePath& path);
  static std::tuple<int64_t, int64_t> GetVolumeInfo(const base::FilePath& path);

  const bool is_incognito_;
  const base::FilePath profile_path_;

  // This member is thread-safe. The scoped_refptr is immutable (the object it
  // points to never changes), and the underlying object is thread-safe.
  const scoped_refptr<QuotaManagerProxy> proxy_;

  bool db_disabled_;
  bool eviction_disabled_;
  absl::optional<url::Origin> origin_for_pending_storage_pressure_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_runner_;
  mutable std::unique_ptr<QuotaDatabase> database_;
  bool is_database_bootstrapped_ = false;

  GetQuotaSettingsFunc get_settings_function_;
  scoped_refptr<base::TaskRunner> get_settings_task_runner_;
  base::RepeatingCallback<void(url::Origin)> storage_pressure_callback_;
  base::RepeatingClosure quota_change_callback_;
  QuotaSettings settings_;
  base::TimeTicks settings_timestamp_;
  std::tuple<base::TimeTicks, int64_t, int64_t>
      cached_disk_stats_for_storage_pressure_;
  CallbackQueue<QuotaSettingsCallback, const QuotaSettings&>
      settings_callbacks_;
  CallbackQueue<StorageCapacityCallback, int64_t, int64_t>
      storage_capacity_callbacks_;

  GetOriginCallback lru_origin_callback_;
  std::set<url::Origin> access_notified_origins_;

  std::map<url::Origin, QuotaOverride> devtools_overrides_;
  int next_override_handle_id_ = 0;

  // Owns the QuotaClient remotes registered via RegisterClient().
  //
  // Iterating over this list is almost always incorrect. Most algorithms should
  // iterate over an entry in |client_types_|.
  //
  // TODO(crbug.com/1016065): Handle Storage Service crashes. Will likely entail
  //                          using a mojo::RemoteSet here.
  std::vector<mojo::Remote<mojom::QuotaClient>> clients_for_ownership_;

  // Owns the QuotaClient instances registered by RegisterLegacyClient() and
  // their wrappers.
  //
  // TODO(crbug.com/1163009): Remove this member after all QuotaClients have
  //                          been mojofied.
  std::vector<scoped_refptr<QuotaClient>> legacy_clients_for_ownership_;

  // Maps QuotaClient instances to client types.
  //
  // The QuotaClient instances pointed to by the map keys are guaranteed to be
  // alive, because they are owned by `legacy_clients_for_ownership_`.
  //
  // TODO(crbug.com/1163009): Replace the map key with mojom::QuotaClient* after
  //                          all QuotaClients have been mojofied.
  base::flat_map<blink::mojom::StorageType,
                 base::flat_map<QuotaClient*, QuotaClientType>>
      client_types_;

  std::unique_ptr<UsageTracker> temporary_usage_tracker_;
  std::unique_ptr<UsageTracker> persistent_usage_tracker_;
  std::unique_ptr<UsageTracker> syncable_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  EvictionContext eviction_context_;
  bool is_getting_eviction_origin_;

  CallbackQueueMap<QuotaCallback,
                   std::string,
                   blink::mojom::QuotaStatusCode,
                   int64_t>
      persistent_host_quota_callbacks_;

  // Map from origin to count.
  std::map<url::Origin, int> origins_in_use_;
  // Map from origin to error count.
  std::map<url::Origin, int> origins_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::RepeatingTimer histogram_timer_;

  // Pointer to the function used to get volume information. This is
  // overwritten by QuotaManagerImplTest in order to attain deterministic
  // reported values. The default value points to
  // QuotaManagerImpl::GetVolumeInfo.
  GetVolumeInfoFn get_volume_info_fn_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<QuotaManagerImpl> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_

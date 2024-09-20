// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "storage/browser/quota/quota_availability.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_internals.mojom.h"
#include "storage/browser/quota/quota_manager_observer.mojom.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

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

  // Deletes all buckets that have explicit expiration dates which have passed.
  virtual void EvictExpiredBuckets(StatusCallback done) = 0;

  // Called at the beginning of an eviction round to gather the info about
  // the current settings, capacity, and usage.
  virtual void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) = 0;

  // Returns the next bucket to evict, or nullopt if there are no evictable
  // buckets.
  virtual void GetEvictionBuckets(int64_t target_usage,
                                  GetBucketsCallback callback) = 0;

  // Called to evict a set of buckets. The callback will be run with the number
  // of successfully evicted buckets.
  virtual void EvictBucketData(const std::set<BucketLocator>& buckets,
                               base::OnceCallback<void(int)> callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() = default;
};

struct UsageInfo {
  UsageInfo(std::string host, blink::mojom::StorageType type, int64_t usage)
      : host(std::move(host)), type(type), usage(usage) {}
  const std::string host;
  const blink::mojom::StorageType type;
  const int64_t usage;

  bool operator==(const UsageInfo& that) const {
    return std::tie(host, usage, type) ==
           std::tie(that.host, that.usage, that.type);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const UsageInfo& usage_info) {
    return os << "{\"" << usage_info.host << "\", " << usage_info.type << ", "
              << usage_info.usage << "}";
  }
};

struct AccumulateQuotaInternalsInfo {
  int64_t total_space = 0;
  int64_t available_space = 0;
  int64_t temp_pool_size = 0;
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
      public base::RefCountedDeleteOnSequence<QuotaManagerImpl>,
      public storage::mojom::QuotaInternalsHandler {
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
  // The value returned is the QuotaAvailability struct.
  using GetVolumeInfoFn = QuotaAvailability (*)(const base::FilePath&);

  static constexpr int64_t kGBytes = 1024 * 1024 * 1024;
  static constexpr int64_t kNoLimit = INT64_MAX;
  static constexpr int64_t kMBytes = 1024 * 1024;

  // A "typical" amount of usage expected for a bucket. This is used to
  // dynamically limit the number of buckets that may be created: the quota for
  // a site divided by this number is an upper bound for the number of buckets
  // it's allowed.
  static constexpr int64_t kTypicalBucketUsage = 20 * kMBytes;

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

  void BindInternalsHandler(
      mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver);

  // Gets the bucket with `bucket_name` for the `storage_key` for
  // StorageType kTemporary and returns the BucketInfo. This may update
  // expiration and persistence if the existing attributes don't match those
  // found in `bucket_params`, and may clobber the bucket and rebuild it if it's
  // expired. If a bucket doesn't exist, a new bucket is created with the
  // specified policies. If the existing bucket exists but has expired, it will
  // be clobbered and recreated. Returns a QuotaError if the operation has
  // failed. This method is declared as virtual to allow test code to override
  // it.
  virtual void UpdateOrCreateBucket(
      const BucketInitParams& bucket_params,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>);
  // Same as UpdateOrCreateBucket but takes in StorageType. This should only be
  // used by FileSystem, and is expected to be removed when
  // StorageType::kSyncable and StorageType::kPersistent are deprecated.
  // (crbug.com/1233525, crbug.com/1286964).
  virtual void GetOrCreateBucketDeprecated(
      const BucketInitParams& bucket_params,
      blink::mojom::StorageType storage_type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>);

  // Creates a bucket for `origin` with `bucket_name` and returns BucketInfo
  // to the callback. Will return a QuotaError to the callback on operation
  // failure.
  //
  // TODO(crbug.com/40181609): Remove `storage_type` when the only supported
  // StorageType is kTemporary.
  virtual void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>);

  // Retrieves the BucketInfo of the bucket with `bucket_name` for `storage_key`
  // and returns it to the callback. Will return a QuotaError if the bucket does
  // not exist or on operation failure.
  // This SHOULD NOT be used once you have the ID for a bucket. Prefer
  // GetBucketById.
  virtual void GetBucketByNameUnsafe(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>);

  // Retrieves the BucketInfo of the bucket with `bucket_id` and returns it to
  // the callback. Will return a QuotaError if the bucket does not exist or on
  // operation failure. This method is declared as virtual to allow test code
  // to override it.
  virtual void GetBucketById(
      const BucketId& bucket_id,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>);

  // Retrieves all storage keys for `type` that are in the buckets table.
  // Used for listing storage keys when showing storage key quota usage.
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysCallback callback);

  // Retrieves all buckets for `type` that are in the buckets table.
  // Used for retrieving global usage data in the UsageTracker.
  void GetBucketsForType(
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback);

  // Retrieves all buckets for `host` and `type` that are in the buckets table.
  // Used for retrieving host usage data in the UsageTracker.
  void GetBucketsForHost(
      const std::string& host,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback);

  // Retrieves all buckets for `storage_key` and `type` that are in the buckets
  // table. When `delete_expired` is true, the expired buckets will be filtered
  // out of the reply and also deleted from disk.
  virtual void GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
      bool delete_expired = false);

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(GetUsageInfoCallback callback);

  // Called by Web Apps (deprecated quota API).
  // This method is declared as virtual to allow test code to override it.
  void GetUsageAndQuotaForWebApps(const blink::StorageKey& storage_key,
                                  blink::mojom::StorageType type,
                                  UsageAndQuotaCallback callback);

  // Called by Web Apps (navigator.storage.estimate())
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaWithBreakdown(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      UsageAndQuotaWithBreakdownCallback callback);

  // Called by DevTools.
  virtual void GetUsageAndQuotaForDevtools(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      UsageAndQuotaForDevtoolsCallback callback);

  // Called by storage backends.
  //
  // For UnlimitedStorage storage keys, this version skips usage and quota
  // handling to avoid extra query cost. Do not call this method for
  // apps/user-facing code.
  //
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuota(const blink::StorageKey& storage_key,
                                blink::mojom::StorageType type,
                                UsageAndQuotaCallback callback);

  // Called by storage backends via proxy.
  //
  // Quota-managed storage backends should call this method when a bucket is
  // accessed. Used to maintain LRU ordering.
  void NotifyBucketAccessed(const BucketLocator& bucket,
                            base::Time access_time);

  // Called by storage backends via proxy.
  //
  // Quota-managed storage backends must call this method when they have made
  // any modifications that change the amount of data stored in a bucket.
  // If `delta` is non-null, the cached usage for the bucket and the give client
  // type will be updated by that amount. A null `delta` value will cause the
  // cache to instead be discarded, after which it will be lazily recalculated.
  void NotifyBucketModified(QuotaClientType client_id,
                            const BucketLocator& bucket,
                            std::optional<int64_t> delta,
                            base::Time modification_time,
                            base::OnceClosure callback);

  // Client storage must call this method whenever they run into disk
  // write errors. Used as a hint to determine if the storage partition is out
  // of space, and trigger actions if deemed appropriate.
  //
  // This method is declared as virtual to allow test code to override it.
  virtual void OnClientWriteFailed(const blink::StorageKey& storage_key);

  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            bool enabled);

  // Deletes `bucket` data for the specified `quota_client_types`. Pass in
  // QuotaClientType::AllClients() to remove bucket data for all quota clients.
  //
  // `callback` is always called. If this QuotaManager gets destroyed during
  // deletion, `callback` may be called with a kErrorAbort status.
  virtual void DeleteBucketData(const BucketLocator& bucket,
                                QuotaClientTypes quota_client_types,
                                StatusCallback callback);

  // Deletes buckets of a particular blink::mojom::StorageType with storage keys
  // that match the specified host.
  //
  // `callback` is always called. If this QuotaManager gets destroyed during
  // deletion, `callback` may be called with a kErrorAbort status.
  // TODO(estade): Consider removing the status code from `callback` as it's
  // unused outside of tests.
  // TODO(crbug.com/40273188): DEPRECATED please prefer using
  // `DeleteStorageKeyData`. This should be removed as part of
  // `CookiesTreeModel` deprecation.
  void DeleteHostData(const std::string& host,
                      blink::mojom::StorageType type,
                      StatusCallback callback);

  // Deletes buckets of a particular blink::StorageKey.
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            StatusCallback callback);

  // Queries QuotaDatabase for the bucket with `storage_key` and `bucket_name`
  // for StorageType::kTemporary and deletes bucket data for all clients for the
  // bucket. Used by the Storage Bucket API for bucket deletion. If no bucket is
  // found, it will return QuotaStatusCode::kOk since it has no bucket data to
  // delete.
  virtual void FindAndDeleteBucketData(const blink::StorageKey& storage_key,
                                       const std::string& bucket_name,
                                       StatusCallback callback);

  // Updates the expiration for the given bucket.
  void UpdateBucketExpiration(
      BucketId bucket,
      const base::Time& expiration,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Updates the persistence bit for the given bucket.
  virtual void UpdateBucketPersistence(
      BucketId bucket,
      bool persistent,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback);

  // Instructs each QuotaClient to remove possible traces of deleted
  // data on the disk.
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             QuotaClientTypes quota_client_types,
                             base::OnceClosure callback);

  // storage::mojom::QuotaInternalsHandler implementation
  void GetDiskAvailabilityAndTempPoolSize(
      GetDiskAvailabilityAndTempPoolSizeCallback callback) override;
  void GetStatistics(GetStatisticsCallback callback) override;
  void RetrieveBucketsTable(RetrieveBucketsTableCallback callback) override;
  void GetGlobalUsageForInternals(
      blink::mojom::StorageType storage_type,
      GetGlobalUsageForInternalsCallback callback) override;
  // Used from quota-internals page to test behavior of the storage pressure
  // callback.
  void SimulateStoragePressure(const url::Origin& origin_url) override;
  void IsSimulateStoragePressureAvailable(
      IsSimulateStoragePressureAvailableCallback callback) override;

  // QuotaEvictionHandler.
  void EvictExpiredBuckets(StatusCallback done) override;
  void GetEvictionBuckets(int64_t target_usage,
                          GetBucketsCallback callback) override;
  void EvictBucketData(const std::set<BucketLocator>& buckets,
                       base::OnceCallback<void(int)> callback) override;
  void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) override;

  // Called by UI and internal modules.
  void GetGlobalUsage(blink::mojom::StorageType type, UsageCallback callback);
  void GetStorageKeyUsageWithBreakdown(const blink::StorageKey& storage_key,
                                       blink::mojom::StorageType type,
                                       UsageWithBreakdownCallback callback);
  void GetBucketUsageWithBreakdown(const BucketLocator& bucket,
                                   UsageWithBreakdownCallback callback);
  void GetBucketUsageAndQuota(BucketId id, UsageAndQuotaCallback callback);
  void GetBucketSpaceRemaining(
      const BucketLocator& bucket,
      base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback);

  bool IsStorageUnlimited(const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type) const;

  // Calculates the quota for the given storage key, taking into account whether
  // the storage should be session only for this key. This will return 0 for
  // unlimited storage situations.
  // Virtual for testing.
  virtual int64_t GetQuotaForStorageKey(const blink::StorageKey& storage_key,
                                        blink::mojom::StorageType type,
                                        const QuotaSettings& settings) const;

  virtual void GetBucketsModifiedBetween(blink::mojom::StorageType type,
                                         base::Time begin,
                                         base::Time end,
                                         GetBucketsCallback callback);

  bool ResetUsageTracker(blink::mojom::StorageType type);

  // Called when StoragePartition is initialized if embedder has an
  // implementation of StorageNotificationService.
  void SetStoragePressureCallback(
      base::RepeatingCallback<void(const blink::StorageKey&)>
          storage_pressure_callback);

  // DevTools Quota Override methods:
  int GetOverrideHandleId();
  void OverrideQuotaForStorageKey(int handle_id,
                                  const blink::StorageKey& storage_key,
                                  std::optional<int64_t> quota_size);
  // Called when a DevTools client releases all overrides, however, overrides
  // will not be disabled for any storage keys for which there are other
  // DevTools clients/QuotaOverrideHandle with an active override.
  void WithdrawOverridesForHandle(int handle_id);

  static constexpr int kEvictionIntervalInMilliSeconds =
      30 * kMinutesInMilliSeconds;
  static constexpr int kThresholdOfErrorsToBeDenylisted = 3;
  static constexpr int kThresholdRandomizationPercent = 5;

  static constexpr char kDatabaseName[] = "QuotaManager";

  static constexpr char kEvictedBucketAccessedCountHistogram[] =
      "Quota.EvictedBucketAccessCount";
  static constexpr char kEvictedBucketDaysSinceAccessHistogram[] =
      "Quota.EvictedBucketDaysSinceAccess";

  // Kept non-const so that test code can change the value.
  // TODO(kinuko): Make this a real const value and add a proper way to set
  // the quota for syncable storage. (http://crbug.com/155488)
  static int64_t kSyncableStorageDefaultStorageKeyQuota;

  void SetGetVolumeInfoFnForTesting(GetVolumeInfoFn fn) {
    get_volume_info_fn_ = fn;
  }

  void SetEvictionDisabledForTesting(bool disable) {
    eviction_disabled_ = disable;
  }

  // Testing support for handling corruption in the underlying database.
  //
  // Runs `corrupter` on the same sequence used to do database I/O,
  // guaranteeing that no other database operation is performed at the same
  // time. `corrupter` receives the path to the underlying SQLite database as an
  // argument. The underlying SQLite database is closed while `corrupter` runs,
  // and reopened afterwards.
  //
  // `callback` is called with QuotaError::kNone if the database was
  // successfully reopened after `corrupter` was run, or with
  // QuotaError::kDatabaseError otherwise.
  void CorruptDatabaseForTesting(
      base::OnceCallback<void(const base::FilePath&)> corrupter,
      base::OnceCallback<void(QuotaError)> callback);

  void SetBootstrapDisabledForTesting(bool disable) {
    bootstrap_disabled_for_testing_ = disable;
  }

  bool is_bootstrapping_database_for_testing() {
    return is_bootstrapping_database_;
  }

  bool is_db_disabled_for_testing() { return db_disabled_; }

  void AddObserver(
      mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer);

 protected:
  ~QuotaManagerImpl() override;
  void SetQuotaChangeCallbackForTesting(
      base::RepeatingClosure storage_pressure_event_callback);

 private:
  friend class base::DeleteHelper<QuotaManagerImpl>;
  friend class base::RefCountedDeleteOnSequence<QuotaManagerImpl>;
  friend class MockQuotaManager;
  friend class MockQuotaClient;
  friend class QuotaManagerProxy;
  friend class QuotaManagerImplTest;
  friend class QuotaTemporaryStorageEvictor;
  friend class UsageTrackerTest;
  FRIEND_TEST_ALL_PREFIXES(QuotaManagerImplTest,
                           UpdateOrCreateBucket_Expiration);

  class EvictionRoundInfoHelper;
  class UsageAndQuotaInfoGatherer;
  class GetUsageInfoTask;
  class StorageKeyGathererTask;
  class BucketDataDeleter;
  class BucketSetDataDeleter;
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

  using BucketTableEntries = std::vector<mojom::BucketTableEntryPtr>;
  using StorageKeysByType =
      base::flat_map<blink::mojom::StorageType, std::set<blink::StorageKey>>;

  using QuotaSettingsCallback = base::OnceCallback<void(const QuotaSettings&)>;

  using DumpBucketTableCallback = base::OnceCallback<void(BucketTableEntries)>;

  // The values returned total_space, available_space.
  using StorageCapacityCallback = base::OnceCallback<void(int64_t, int64_t)>;

  // Lazily called on the IO thread when the first quota manager API is called.
  //
  // Initialize() must be called after all quota clients are added to the
  // manager by RegisterClient().
  void EnsureDatabaseOpened();

  // Bootstraps only if it hasn't already happened.
  void MaybeBootstrapDatabase();
  // Bootstraps database with storage keys that may not have been registered.
  // Bootstrapping ensures that there is a bucket entry in the buckets table for
  // all storage keys that have stored data by quota managed Storage APIs. Will
  // queue calls to QuotaDatabase during bootstrap to be run after bootstrapping
  // is complete.
  void BootstrapDatabase();
  void DidGetBootstrapFlag(bool is_database_bootstrapped);
  void DidGetStorageKeysForBootstrap(StorageKeysByType storage_keys_by_type);
  void DidBootstrapDatabase(QuotaError error);
  void DidSetDatabaseBootstrapped(QuotaError error);
  // Runs all callbacks to QuotaDatabase that have been queued during bootstrap.
  void RunDatabaseCallbacks();

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
      QuotaClientType client_type,
      const base::flat_set<blink::mojom::StorageType>& storage_types);

  UsageTracker* GetUsageTracker(blink::mojom::StorageType type) const;

  void DumpBucketTable(DumpBucketTableCallback callback);
  void UpdateQuotaInternalsDiskAvailability(base::OnceClosure barrier_callback,
                                            AccumulateQuotaInternalsInfo* info,
                                            int64_t total_space,
                                            int64_t available_space);
  void UpdateQuotaInternalsTempPoolSpace(base::OnceClosure barrier_callback,
                                         AccumulateQuotaInternalsInfo* info,
                                         const QuotaSettings& settings);
  void FinallySendDiskAvailabilityAndTempPoolSize(
      GetDiskAvailabilityAndTempPoolSizeCallback callback,
      std::unique_ptr<AccumulateQuotaInternalsInfo> info);
  void RetrieveBucketUsageForBucketTable(RetrieveBucketsTableCallback callback,
                                         BucketTableEntries entries);
  void AddBucketTableEntry(
      mojom::BucketTableEntryPtr entry,
      base::OnceCallback<void(mojom::BucketTableEntryPtr)> barrier_callback,
      int64_t usage,
      blink::mojom::UsageBreakdownPtr bucket_usage_breakdown);

  // Runs BucketDataDeleter which calls QuotaClients to clear data for the
  // bucket. Once the task is complete, calls the QuotaDatabase to delete the
  // bucket from the bucket table.
  void DeleteBucketDataInternal(
      const BucketLocator& bucket,
      QuotaClientTypes quota_client_types,
      base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
          callback);

  // Removes the BucketSetDataDeleter that completed its work.
  //
  // This method is static because it must call `callback` even if the
  // QuotaManagerImpl was destroyed.
  static void DidDeleteBuckets(base::WeakPtr<QuotaManagerImpl> quota_manager,
                               StatusCallback callback,
                               BucketSetDataDeleter* deleter,
                               blink::mojom::QuotaStatusCode status_code);

  // Removes the BucketDataDeleter that completed its work.
  //
  // This method is static because it must call `delete_bucket_data_callback`
  // even if the QuotaManagerImpl was destroyed.
  static void DidDeleteBucketData(
      base::WeakPtr<QuotaManagerImpl> quota_manager,
      base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
          callback,
      BucketDataDeleter* deleter,
      QuotaErrorOr<mojom::BucketTableEntryPtr> result);

  // Called after bucket data has been deleted from clients as well as the
  // database due to bucket expiration. This will recreate the bucket in the
  // database and pass it to `callback`.
  void DidDeleteBucketForRecreation(
      const BucketInitParams& params,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
      BucketInfo bucket_info,
      QuotaErrorOr<mojom::BucketTableEntryPtr> result);

  // Called when the quota database encounters an error.
  void OnDbError(int error_code);

  // Called when the quota database or a quota client run into low disk space
  // errors.
  void OnFullDiskError(std::optional<blink::StorageKey> storage_key);

  // Notifies the embedder that space is too low. This ends up showing a
  // user-facing dialog in Chrome.
  void NotifyWriteFailed(const blink::StorageKey& storage_key);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteBucketFromDatabase(
      const BucketLocator& bucket,
      bool commit_immediately,
      base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
          callback);

  // `barrier` should be called with true for a successful eviction or false if
  // there's an error.
  void DidEvictBucketData(BucketId evicted_bucket_id,
                          base::RepeatingCallback<void(bool)> barrier,
                          QuotaErrorOr<mojom::BucketTableEntryPtr> entry);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(int64_t usage,
                                              int64_t unlimited_usage);
  void DidGetStorageCapacityForHistogram(int64_t usage,
                                         int64_t total_space,
                                         int64_t available_space);
  void DidDumpBucketTableForHistogram(BucketTableEntries entries);

  // Returns the list of bucket ids that should be excluded from eviction due to
  // consistent errors after multiple attempts.
  std::set<BucketId> GetEvictionBucketExceptions();
  void DidGetEvictionBuckets(GetBucketsCallback callback,
                             const std::set<BucketLocator>& buckets);

  void DidGetEvictionRoundInfo();

  void GetBucketsForEvictionFromDatabase(
      int64_t target_usage,
      std::map<BucketLocator, int64_t> usage_map,
      GetBucketsCallback callback);

  void DidGetBucketsForEvictionFromDatabase(
      GetBucketsCallback callback,
      QuotaErrorOr<std::set<BucketLocator>> result);
  void GetQuotaSettings(QuotaSettingsCallback callback);
  void DidGetSettings(std::optional<QuotaSettings> settings);
  void GetStorageCapacity(StorageCapacityCallback callback);
  void ContinueIncognitoGetStorageCapacity(const QuotaSettings& settings);
  void DidGetStorageCapacity(const QuotaAvailability& total_and_available);

  void DidRecoverOrRazeForReBootstrap(bool success);

  void NotifyUpdatedBucket(const QuotaErrorOr<BucketInfo>& result);
  void OnBucketDeleted(
      base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
          callback,
      QuotaErrorOr<mojom::BucketTableEntryPtr> result);

  void DidGetQuotaSettingsForBucketCreation(
      const BucketInitParams& bucket_params,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
      const QuotaSettings& settings);
  void DidGetBucket(bool notify_update_bucket,
                    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
                    QuotaErrorOr<BucketInfo> result);
  void DidGetBucketCheckExpiration(
      const BucketInitParams& params,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
      QuotaErrorOr<BucketInfo> result);
  void DidGetBucketForDeletion(StatusCallback callback,
                               QuotaErrorOr<BucketInfo> result);
  void DidGetBucketForUsageAndQuota(UsageAndQuotaCallback callback,
                                    QuotaErrorOr<BucketInfo> result);
  void DidGetStorageKeys(GetStorageKeysCallback callback,
                         QuotaErrorOr<std::set<blink::StorageKey>> result);
  void DidGetBuckets(
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
      QuotaErrorOr<std::set<BucketInfo>> result);
  void DidGetBucketsCheckExpiration(
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
      QuotaErrorOr<std::set<BucketInfo>> result);
  void DidGetModifiedBetween(GetBucketsCallback callback,
                             QuotaErrorOr<std::set<BucketLocator>> result);

  void MaybeRunStoragePressureCallback(const blink::StorageKey& storage_key,
                                       int64_t total_space,
                                       int64_t available_space);

  // Evaluates disk statistics to identify storage pressure
  // (low disk space availability) and starts the storage
  // pressure event dispatch if appropriate.
  // TODO(crbug.com/40133191): Implement UsageAndQuotaInfoGatherer::Completed()
  // to use DetermineStoragePressure().
  void DetermineStoragePressure(int64_t free_space, int64_t total_space);

  std::optional<int64_t> GetQuotaOverrideForStorageKey(
      const blink::StorageKey&);

  template <typename ValueType>
  void PostTaskAndReplyWithResultForDBThread(
      base::OnceCallback<QuotaErrorOr<ValueType>(QuotaDatabase*)> task,
      base::OnceCallback<void(QuotaErrorOr<ValueType>)> reply,
      const base::Location& from_here = base::Location::Current(),
      bool is_bootstrap_task = false);

  void PostTaskAndReplyWithResultForDBThread(
      base::OnceCallback<QuotaError(QuotaDatabase*)> task,
      base::OnceCallback<void(QuotaError)> reply,
      const base::Location& from_here = base::Location::Current(),
      bool is_bootstrap_task = false);

  static QuotaAvailability CallGetVolumeInfo(GetVolumeInfoFn get_volume_info_fn,
                                             const base::FilePath& path);
  static QuotaAvailability GetVolumeInfo(const base::FilePath& path);

  const bool is_incognito_;
  const base::FilePath profile_path_;

  // This member is thread-safe. The scoped_refptr is immutable (the object it
  // points to never changes), and the underlying object is thread-safe.
  const scoped_refptr<QuotaManagerProxy> proxy_;

  bool db_disabled_ = false;
  bool eviction_disabled_ = false;
  bool bootstrap_disabled_for_testing_ = false;

  std::optional<blink::StorageKey>
      storage_key_for_pending_storage_pressure_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_runner_;

  // QuotaManagerImpl creates `database_` and later schedules it for deletion on
  // `db_runner_`. Thus, `database_` may outlive `this`.
  std::unique_ptr<QuotaDatabase> database_;

  bool is_bootstrapping_database_ = false;
  // Queued callbacks to QuotaDatabase that will run after database bootstrap is
  // complete.
  std::vector<base::OnceClosure> database_callbacks_;

  GetQuotaSettingsFunc get_settings_function_;
  scoped_refptr<base::TaskRunner> get_settings_task_runner_;
  base::RepeatingCallback<void(const blink::StorageKey&)>
      storage_pressure_callback_;
  base::RepeatingClosure quota_change_callback_;
  QuotaSettings settings_;
  base::TimeTicks settings_timestamp_;
  std::tuple<base::TimeTicks, int64_t, int64_t>
      cached_disk_stats_for_storage_pressure_;
  CallbackQueue<QuotaSettingsCallback, const QuotaSettings&>
      settings_callbacks_;
  CallbackQueue<StorageCapacityCallback, int64_t, int64_t>
      storage_capacity_callbacks_;

  // The storage key for the last time a bucket was opened. This is used as an
  // imperfect estimate of which site may have encountered the last quota
  // database full disk error.
  std::optional<blink::StorageKey> last_opened_bucket_site_;

  // The last time that an eviction round was started due to a full disk error.
  base::TimeTicks last_full_disk_eviction_time_;

  // Buckets that have been notified of access during LRU task to exclude from
  // eviction.
  std::set<BucketLocator> access_notified_buckets_;

  std::map<blink::StorageKey, QuotaOverride> devtools_overrides_;
  int next_override_handle_id_ = 0;

  // Serve mojo connections for chrome://quota-internals pages.
  mojo::ReceiverSet<mojom::QuotaInternalsHandler> internals_handlers_receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Owns the QuotaClient remotes registered via RegisterClient().
  //
  // Iterating over this list is almost always incorrect. Most algorithms should
  // iterate over an entry in |client_types_|.
  //
  // TODO(crbug.com/40103974): Handle Storage Service crashes. Will likely
  // entail
  //                          using a mojo::RemoteSet here.
  std::vector<mojo::Remote<mojom::QuotaClient>> clients_for_ownership_;

  // Maps QuotaClient instances to client types.
  //
  // The QuotaClient instances pointed to by the map keys are guaranteed to be
  // alive, because they are owned by `legacy_clients_for_ownership_`.
  base::flat_map<blink::mojom::StorageType,
                 base::flat_map<mojom::QuotaClient*, QuotaClientType>>
      client_types_;

  std::unique_ptr<UsageTracker> temporary_usage_tracker_;
  std::unique_ptr<UsageTracker> syncable_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  // Set when there is an eviction task in-flight.
  bool is_getting_eviction_bucket_ = false;

  // Map from bucket id to eviction error count.
  std::map<BucketId, int> buckets_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::RepeatingTimer histogram_timer_;

  // Pointer to the function used to get volume information. This is
  // overwritten by QuotaManagerImplTest in order to attain deterministic
  // reported values. The default value points to
  // QuotaManagerImpl::GetVolumeInfo.
  GetVolumeInfoFn get_volume_info_fn_;

  std::unique_ptr<EvictionRoundInfoHelper> eviction_helper_;
  std::map<BucketSetDataDeleter*, std::unique_ptr<BucketSetDataDeleter>>
      bucket_set_data_deleters_;
  std::map<BucketDataDeleter*, std::unique_ptr<BucketDataDeleter>>
      bucket_data_deleters_;
  std::unique_ptr<StorageKeyGathererTask> storage_key_gatherer_;

  mojo::RemoteSet<storage::mojom::QuotaManagerObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<QuotaManagerImpl> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_IMPL_H_

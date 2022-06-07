// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_macros.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/quota/usage_tracker.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr int64_t kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour

// Take action on write errors if there is <= 2% disk space
// available.
constexpr double kStoragePressureThresholdRatio = 0.02;

// Limit how frequently QuotaManagerImpl polls for free disk space when
// only using that information to identify storage pressure.
constexpr base::TimeDelta kStoragePressureCheckDiskStatsInterval =
    base::Minutes(5);

// Modifies a given value by a uniformly random amount from
// -percent to +percent.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value * (1 + (random_percent / 100.0));
}
}  // namespace

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64_t QuotaManagerImpl::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

namespace {

bool IsSupportedType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent ||
         type == StorageType::kSyncable;
}

bool IsSupportedIncognitoType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent;
}

std::string StorageTypeEnumToString(StorageType type) {
  switch (type) {
    case StorageType::kTemporary:
      return "temporary";
    case StorageType::kPersistent:
      return "persistent";
    case StorageType::kSyncable:
      return "syncable";
    case StorageType::kQuotaNotManaged:
      return "quota-not-managed";
    case StorageType::kUnknown:
      return "unknown";
  }
}

StorageType GetBlinkStorageType(storage::mojom::StorageType type) {
  switch (type) {
    case storage::mojom::StorageType::kTemporary:
      return StorageType::kTemporary;
    case storage::mojom::StorageType::kPersistent:
      return StorageType::kPersistent;
    case storage::mojom::StorageType::kSyncable:
      return StorageType::kSyncable;
  }
}

void DidGetUsageAndQuotaStripBreakdown(
    QuotaManagerImpl::UsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK(callback);
  std::move(callback).Run(status, usage, quota);
}

void DidGetUsageAndQuotaStripOverride(
    QuotaManagerImpl::UsageAndQuotaWithBreakdownCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    bool is_override_enabled,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK(callback);
  std::move(callback).Run(status, usage, quota, std::move(usage_breakdown));
}

}  // namespace

constexpr int64_t QuotaManagerImpl::kGBytes;
constexpr int64_t QuotaManagerImpl::kNoLimit;
constexpr int64_t QuotaManagerImpl::kPerHostPersistentQuotaLimit;
constexpr int QuotaManagerImpl::kEvictionIntervalInMilliSeconds;
constexpr int QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted;
constexpr int QuotaManagerImpl::kThresholdOfErrorsToDisableDatabase;
constexpr int QuotaManagerImpl::kThresholdRandomizationPercent;
constexpr char QuotaManagerImpl::kDatabaseName[];
constexpr char QuotaManagerImpl::kEvictedBucketAccessedCountHistogram[];
constexpr char QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram[];

QuotaManagerImpl::QuotaOverride::QuotaOverride() = default;
QuotaManagerImpl::QuotaOverride::~QuotaOverride() = default;

class QuotaManagerImpl::UsageAndQuotaInfoGatherer : public QuotaTask {
 public:
  UsageAndQuotaInfoGatherer(QuotaManagerImpl* manager,
                            const StorageKey& storage_key,
                            StorageType type,
                            bool is_incognito,
                            UsageAndQuotaForDevtoolsCallback callback)
      : QuotaTask(manager),
        storage_key_(storage_key),
        callback_(std::move(callback)),
        type_(type),
        is_unlimited_(manager->IsStorageUnlimited(storage_key_, type_)),
        is_incognito_(is_incognito) {
    DCHECK(manager);
    DCHECK(callback_);
  }

  UsageAndQuotaInfoGatherer(QuotaManagerImpl* manager,
                            const BucketInfo& bucket_info,
                            bool is_incognito,
                            UsageAndQuotaForDevtoolsCallback callback)
      : UsageAndQuotaInfoGatherer(manager,
                                  bucket_info.storage_key,
                                  bucket_info.type,
                                  is_incognito,
                                  std::move(callback)) {
    bucket_info_ = bucket_info;
    DCHECK_EQ(StorageType::kTemporary, type_);
  }

 protected:
  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Start the async process of gathering the info we need.
    // Gather info before computing an answer:
    // settings, host_usage, host_quota and device_storage_capacity if
    // unlimited.
    int callback_count = is_unlimited_ ? 4 : 3;
    base::RepeatingClosure barrier = base::BarrierClosure(
        callback_count,
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnBarrierComplete,
                       weak_factory_.GetWeakPtr()));

    manager()->GetQuotaSettings(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));

    if (bucket_info_) {
      manager()->GetBucketUsageWithBreakdown(
          bucket_info_->ToBucketLocator(),
          base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotUsage,
                         weak_factory_.GetWeakPtr(), barrier));
    } else {
      manager()->GetStorageKeyUsageWithBreakdown(
          storage_key_, type_,
          base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotUsage,
                         weak_factory_.GetWeakPtr(), barrier));
    }

    // Determine host_quota differently depending on type.
    if (is_unlimited_) {
      manager()->GetStorageCapacity(
          base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotCapacity,
                         weak_factory_.GetWeakPtr(), barrier));
      SetDesiredStorageKeyQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                                kNoLimit);
    } else if (type_ == StorageType::kSyncable) {
      SetDesiredStorageKeyQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                                kSyncableStorageDefaultHostQuota);
    } else if (type_ == StorageType::kPersistent) {
      const std::string& host = storage_key_.origin().host();
      manager()->GetPersistentHostQuota(
          host,
          base::BindOnce(&UsageAndQuotaInfoGatherer::SetDesiredStorageKeyQuota,
                         weak_factory_.GetWeakPtr(), barrier));
    } else {
      DCHECK_EQ(StorageType::kTemporary, type_);
      // For temporary storage,  OnGotSettings will set the host quota.
    }
  }

  void Aborted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort,
                             /*usage=*/0,
                             /*quota=*/0,
                             /*is_override_enabled=*/false,
                             /*usage_breakdown=*/nullptr);
    DeleteSoon();
  }

  void Completed() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();

    int64_t quota = desired_storage_key_quota_;
    absl::optional<int64_t> quota_override_size =
        manager()->GetQuotaOverrideForStorageKey(storage_key_);
    if (quota_override_size)
      quota = *quota_override_size;

    // For an individual bucket, the quota is the minimum of the requested quota
    // and the host quota.
    if (bucket_info_ && bucket_info_->quota > 0)
      quota = std::min(quota, bucket_info_->quota);

    if (is_unlimited_) {
      int64_t temp_pool_free_space =
          available_space_ - settings_.must_remain_available;
      // Constrain the desired quota to something that fits.
      if (quota > temp_pool_free_space)
        quota = available_space_ + usage_;
    }

    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, usage_, quota,
                             quota_override_size.has_value(),
                             std::move(usage_breakdown_));
    if (type_ == StorageType::kTemporary && !is_incognito_ && !is_unlimited_ &&
        !bucket_info_) {
      UMA_HISTOGRAM_MBYTES("Quota.QuotaForOrigin", quota);
      UMA_HISTOGRAM_MBYTES("Quota.UsageByOrigin", usage_);
      if (quota > 0) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Quota.PercentUsedByOrigin",
            std::min(100, static_cast<int>((usage_ * 100) / quota)));
      }
    }
    DeleteSoon();
  }

 private:
  QuotaManagerImpl* manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<QuotaManagerImpl*>(observer());
  }

  void OnGotSettings(base::RepeatingClosure barrier_closure,
                     const QuotaSettings& settings) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(barrier_closure);

    settings_ = settings;
    barrier_closure.Run();
    if (type_ == StorageType::kTemporary && !is_unlimited_) {
      int64_t storage_key_quota = manager()->IsSessionOnly(storage_key_, type_)
                                      ? settings.session_only_per_host_quota
                                      : settings.per_host_quota;
      SetDesiredStorageKeyQuota(std::move(barrier_closure),
                                blink::mojom::QuotaStatusCode::kOk,
                                storage_key_quota);
    }
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(barrier_closure);
    DCHECK_GE(total_space, 0);
    DCHECK_GE(available_space, 0);

    total_space_ = total_space;
    available_space_ = available_space;
    std::move(barrier_closure).Run();
  }

  void OnGotUsage(base::OnceClosure barrier_closure,
                  int64_t usage,
                  blink::mojom::UsageBreakdownPtr usage_breakdown) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(barrier_closure);
    DCHECK_GE(usage, -1);
    DCHECK(usage_breakdown);
    DCHECK_GE(usage_breakdown->backgroundFetch, 0);
    DCHECK_GE(usage_breakdown->fileSystem, 0);
    DCHECK_GE(usage_breakdown->indexedDatabase, 0);
    DCHECK_GE(usage_breakdown->serviceWorker, 0);
    DCHECK_GE(usage_breakdown->serviceWorkerCache, 0);
    DCHECK_GE(usage_breakdown->webSql, 0);

    usage_ = usage;
    usage_breakdown_ = std::move(usage_breakdown);
    std::move(barrier_closure).Run();
  }

  void SetDesiredStorageKeyQuota(base::OnceClosure barrier_closure,
                                 blink::mojom::QuotaStatusCode status,
                                 int64_t quota) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(barrier_closure);
    DCHECK_GE(quota, 0);

    desired_storage_key_quota_ = quota;
    std::move(barrier_closure).Run();
  }

  void OnBarrierComplete() { CallCompleted(); }

  // These fields are passed at construction time.
  const StorageKey storage_key_;
  // Non-null iff usage info is to be gathered for an individual bucket. If
  // null, usage is gathered for all buckets in the given host/StorageKey.
  absl::optional<BucketInfo> bucket_info_;
  QuotaManagerImpl::UsageAndQuotaForDevtoolsCallback callback_;
  const StorageType type_;
  const bool is_unlimited_;
  const bool is_incognito_;

  // Fields retrieved while running.
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t desired_storage_key_quota_ = 0;
  int64_t usage_ = 0;
  blink::mojom::UsageBreakdownPtr usage_breakdown_;
  QuotaSettings settings_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers are used to support cancelling work.
  base::WeakPtrFactory<UsageAndQuotaInfoGatherer> weak_factory_{this};
};

class QuotaManagerImpl::EvictionRoundInfoHelper {
 public:
  // `callback` is called when the helper successfully retrieves quota settings
  // and capacity data. `completion_closure` is called after data has been
  // retrieved and `callback` has been run and to clean up itself and delete
  // this EvictionRoundInfoHelper instance.
  EvictionRoundInfoHelper(QuotaManagerImpl* manager,
                          EvictionRoundInfoCallback callback,
                          base::OnceClosure completion_closure)
      : manager_(manager),
        callback_(std::move(callback)),
        completion_closure_(std::move(completion_closure)) {
    DCHECK(manager_);
    DCHECK(callback_);
    DCHECK(completion_closure_);
  }

  void Run() {
#if DCHECK_IS_ON()
    DCHECK(!run_called_) << __func__ << " already called";
    run_called_ = true;
#endif  // DCHECK_IS_ON()

    // Gather 2 pieces of info before deciding if we need to get GlobalUsage:
    // settings and device_storage_capacity.
    base::RepeatingClosure barrier = base::BarrierClosure(
        2, base::BindOnce(&EvictionRoundInfoHelper::OnBarrierComplete,
                          weak_factory_.GetWeakPtr()));

    manager_->GetQuotaSettings(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));
    manager_->GetStorageCapacity(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotCapacity,
                       weak_factory_.GetWeakPtr(), barrier));
  }

 private:
  void Completed() {
#if DCHECK_IS_ON()
    DCHECK(!completed_called_) << __func__ << " already called";
    completed_called_ = true;
#endif  // DCHECK_IS_ON()
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, settings_,
                             available_space_, total_space_, global_usage_,
                             global_usage_is_complete_);
    // May delete `this`.
    std::move(completion_closure_).Run();
  }

  void OnGotSettings(base::OnceClosure barrier_closure,
                     const QuotaSettings& settings) {
    DCHECK(barrier_closure);

    settings_ = settings;
    std::move(barrier_closure).Run();
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    DCHECK(barrier_closure);
    DCHECK_GE(total_space, 0);
    DCHECK_GE(available_space, 0);

    total_space_ = total_space;
    available_space_ = available_space;
    std::move(barrier_closure).Run();
  }

  void OnBarrierComplete() {
    // Avoid computing the full current_usage when there's no pressure.
    int64_t consumed_space = total_space_ - available_space_;
    if (consumed_space < settings_.pool_size &&
        available_space_ > settings_.should_remain_available) {
      DCHECK(!global_usage_is_complete_);
      global_usage_ =
          manager_->GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
      // `this` may be deleted during this Complete() call.
      Completed();
      return;
    }
    manager_->GetGlobalUsage(
        StorageType::kTemporary,
        base::BindOnce(&EvictionRoundInfoHelper::OnGotGlobalUsage,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGotGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    global_usage_ = std::max(int64_t{0}, usage - unlimited_usage);
    global_usage_is_complete_ = true;
    // `this` may be deleted during this Complete() call.
    Completed();
  }

  const raw_ptr<QuotaManagerImpl> manager_;
  EvictionRoundInfoCallback callback_;
  base::OnceClosure completion_closure_;
  QuotaSettings settings_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t global_usage_ = 0;
  bool global_usage_is_complete_ = false;

#if DCHECK_IS_ON()
  bool run_called_ = false;
  bool completed_called_ = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<EvictionRoundInfoHelper> weak_factory_{this};
};

class QuotaManagerImpl::GetUsageInfoTask : public QuotaTask {
 public:
  GetUsageInfoTask(QuotaManagerImpl* manager, GetUsageInfoCallback callback)
      : QuotaTask(manager), callback_(std::move(callback)) {
    DCHECK(manager);
    DCHECK(callback_);
  }

 protected:
  void Run() override {
    remaining_trackers_ = 3;
    // This will populate cached hosts and usage info.
    manager()
        ->GetUsageTracker(StorageType::kTemporary)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kTemporary));
    manager()
        ->GetUsageTracker(StorageType::kPersistent)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kPersistent));
    manager()
        ->GetUsageTracker(StorageType::kSyncable)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kSyncable));
  }

  void Completed() override {
    std::move(callback_).Run(std::move(entries_));
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker& tracker) {
    std::map<std::string, int64_t> host_usage = tracker.GetCachedHostsUsage();
    for (const auto& host_usage_pair : host_usage) {
      entries_.emplace_back(host_usage_pair.first, type,
                            host_usage_pair.second);
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64_t, int64_t) {
    UsageTracker* tracker = manager()->GetUsageTracker(type);
    DCHECK(tracker);
    AddEntries(type, *tracker);
  }

  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  int remaining_trackers_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_{this};
};

class QuotaManagerImpl::StorageKeyGathererTask {
 public:
  StorageKeyGathererTask(QuotaManagerImpl* manager,
                         base::OnceCallback<void(StorageKeysByType)> callback)
      : manager_(manager), callback_(std::move(callback)) {
    DCHECK(manager_);
    DCHECK(callback_);
  }

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    DCHECK(!run_called_) << __func__ << " already called";
    run_called_ = true;
#endif  // DCHECK_IS_ON()

    size_t client_call_count = 0;
    for (const auto& client_and_type : manager_->client_types_)
      client_call_count += client_and_type.second.size();

    // Registered clients can be empty in tests.
    if (!client_call_count) {
      Completed();
      return;
    }

    base::RepeatingClosure barrier = base::BarrierClosure(
        client_call_count,
        base::BindOnce(&QuotaManagerImpl::StorageKeyGathererTask::Completed,
                       weak_factory_.GetWeakPtr()));

    GetStorageKeysForType(StorageType::kTemporary, barrier);
    GetStorageKeysForType(StorageType::kPersistent, barrier);
    GetStorageKeysForType(StorageType::kSyncable, barrier);
  }

 private:
  void GetStorageKeysForType(StorageType type, base::RepeatingClosure barrier) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto client_map_it = manager_->client_types_.find(type);
    DCHECK(client_map_it != manager_->client_types_.end());
    DCHECK(barrier);

    for (const auto& client_and_type : client_map_it->second) {
      client_and_type.first->GetStorageKeysForType(
          type, base::BindOnce(&StorageKeyGathererTask::DidGetStorageKeys,
                               weak_factory_.GetWeakPtr(), type, barrier));
    }
  }

  void DidGetStorageKeys(StorageType type,
                         base::OnceClosure barrier,
                         const std::vector<StorageKey>& storage_keys) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(barrier);

    storage_keys_by_type_[type].insert(storage_keys.begin(),
                                       storage_keys.end());
    std::move(barrier).Run();
  }

  void Completed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    DCHECK(!completed_called_) << __func__ << " already called";
    completed_called_ = true;
#endif  // DCHECK_IS_ON()

    // Deletes `this`.
    std::move(callback_).Run(storage_keys_by_type_);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<QuotaManagerImpl> manager_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceCallback<void(StorageKeysByType)> callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  StorageKeysByType storage_keys_by_type_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool run_called_ = false;
  bool completed_called_ = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<StorageKeyGathererTask> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// Calls QuotaClients in `quota_client_types` for the `bucket` to delete bucket
// data. Will delete bucket entries from the QuotaDatabase if bucket data has
// been successfully deleted from all registered QuotaClient.
// This is currently only for the default bucket. If a non-default bucket is to
// be deleted, it will immediately complete the task since non-default bucket
// usage is not being tracked by QuotaClients yet.
class QuotaManagerImpl::BucketDataDeleter {
 public:
  // `callback` will be run to report the status of the deletion on task
  // completion. `callback` will only be called while this BucketDataDeleter
  // instance is alive. `callback` may destroy this BucketDataDeleter instance.
  BucketDataDeleter(
      QuotaManagerImpl* manager,
      const BucketLocator& bucket,
      QuotaClientTypes quota_client_types,
      base::OnceCallback<void(BucketDataDeleter*,
                              blink::mojom::QuotaStatusCode)> callback)
      : manager_(manager),
        bucket_(bucket),
        quota_client_types_(std::move(quota_client_types)),
        callback_(std::move(callback)) {
    DCHECK(manager_);
    // TODO(crbug/1292216): Convert back into DCHECKs once issue is resolved.
    CHECK(callback_);
  }

  ~BucketDataDeleter() {
    // `callback` is non-null if the deleter gets destroyed before completing.
    if (callback_) {
      std::move(callback_).Run(this,
                               blink::mojom::QuotaStatusCode::kErrorAbort);
    }
  }

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    // TODO(crbug/1292216): Convert back into DCHECK once issue is resolved.
    CHECK(!run_called_) << __func__ << " already called";
    run_called_ = true;
#endif  // DCHECK_IS_ON()

    DCHECK(manager_->client_types_.contains(bucket_.type));

    remaining_clients_ = manager_->client_types_[bucket_.type].size();
    UsageTracker* usage_tracker = manager_->GetUsageTracker(bucket_.type);

    for (const auto& client_and_type : manager_->client_types_[bucket_.type]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        // Delete cached usage.
        usage_tracker->DeleteBucketCache(client_type, bucket_);

        static int tracing_id = 0;
        std::string bucket_params = base::StrCat(
            {"storage_key: ", bucket_.storage_key.Serialize(),
             ", is_default: ", bucket_.is_default ? "true" : "false",
             ", id: ", base::NumberToString(bucket_.id.value())});
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
            "browsing_data", "QuotaManagerImpl::BucketDataDeleter",
            ++tracing_id, "client_type", client_type, "bucket", bucket_params);
        client->DeleteBucketData(
            bucket_, base::BindOnce(&BucketDataDeleter::DidDeleteBucketData,
                                    weak_factory_.GetWeakPtr(), tracing_id));
      } else {
        ++skipped_clients_;
        --remaining_clients_;
      }
    }

    if (remaining_clients_ == 0)
      FinishDeletion();
  }

  bool completed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !callback_;
  }

 private:
  void DidDeleteBucketData(int tracing_id,
                           blink::mojom::QuotaStatusCode status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GT(remaining_clients_, 0u);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "browsing_data", "QuotaManagerImpl::BucketDataDeleter", tracing_id);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      FinishDeletion();
  }

  void FinishDeletion() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(crbug/1292216): Convert back into DCHECKs once issue is resolved.
    CHECK_EQ(remaining_clients_, 0u);
    CHECK(callback_) << __func__ << " called after Complete";

    // Only remove the bucket from the database if we didn't skip any client
    // types.
    if (skipped_clients_ == 0 && error_count_ == 0) {
      manager_->DeleteBucketFromDatabase(
          bucket_,
          base::BindOnce(&BucketDataDeleter::DidDeleteBucketFromDatabase,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    Complete(/*success=*/error_count_ == 0);
  }

  void DidDeleteBucketFromDatabase(QuotaError result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    manager_->DidDatabaseWork(result != QuotaError::kDatabaseError);
    Complete(result == QuotaError::kNone);
  }

  void Complete(bool success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(crbug/1292216): Convert back into DCHECKs once issue is resolved.
    CHECK_EQ(remaining_clients_, 0u);
    CHECK(callback_);

    // May delete `this`.
    std::move(callback_).Run(
        this, success
                  ? blink::mojom::QuotaStatusCode::kOk
                  : blink::mojom::QuotaStatusCode::kErrorInvalidModification);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<QuotaManagerImpl> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const BucketLocator bucket_;
  const QuotaClientTypes quota_client_types_;
  int error_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  size_t remaining_clients_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int skipped_clients_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Running the callback may delete this instance.
  base::OnceCallback<void(BucketDataDeleter*, blink::mojom::QuotaStatusCode)>
      callback_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool run_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<BucketDataDeleter> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// Retrieves all buckets for `host` from QuotaDatabase and calls
// BucketDataDeleter for all registered QuotaClientTypes.
class QuotaManagerImpl::HostDataDeleter {
 public:
  // `callback` will be run to report the status of the deletion on task
  // completion. `callback` will only be called while this HostDataDeleter
  // instance is alive. `callback` may destroy this HostDataDeleter instance.
  HostDataDeleter(
      QuotaManagerImpl* manager,
      const std::string& host,
      StorageType type,
      base::OnceCallback<void(HostDataDeleter*, blink::mojom::QuotaStatusCode)>
          callback)
      : manager_(manager),
        host_(host),
        type_(type),
        callback_(std::move(callback)) {
    DCHECK(manager_);
    DCHECK(callback_);
  }

  ~HostDataDeleter() {
    if (callback_) {
      std::move(callback_).Run(this,
                               blink::mojom::QuotaStatusCode::kErrorAbort);
    }
  }

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    DCHECK(!run_called_) << __func__ << " already called";
    run_called_ = true;
#endif  // DCHECK_IS_ON()
    manager_->GetBucketsForHost(
        host_, type_,
        base::BindOnce(&HostDataDeleter::DidGetBucketsForHost,
                       weak_factory_.GetWeakPtr()));
  }

  bool completed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !callback_;
  }

 private:
  void DidGetBucketsForHost(QuotaErrorOr<std::set<BucketLocator>> result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!result.ok()) {
      Complete(/*success=*/false);
      return;
    }

    buckets_ = result.value();
    if (!buckets_.empty()) {
      ScheduleBucketsDeletion();
      return;
    }
    Complete(/*success=*/true);
  }

  void ScheduleBucketsDeletion() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& bucket : buckets_) {
      // base::Unretained() is safe here because `this` is guaranteed to outlive
      // the callback, thanks to an indirect ownership chain. `this` owns the
      // BucketDataDeleter created here, which guarantees it will only use the
      // callback when it's alive.
      auto bucket_deleter = std::make_unique<BucketDataDeleter>(
          manager_, bucket, AllQuotaClientTypes(),
          base::BindOnce(&HostDataDeleter::DidDeleteBucketData,
                         base::Unretained(this)));
      auto* bucket_deleter_ptr = bucket_deleter.get();
      bucket_deleters_[bucket_deleter_ptr] = std::move(bucket_deleter);
      bucket_deleter_ptr->Run();
    }
  }

  void DidDeleteBucketData(BucketDataDeleter* deleter,
                           blink::mojom::QuotaStatusCode status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(deleter->completed());

    DCHECK(base::Contains(bucket_deleters_, deleter));
    bucket_deleters_.erase(deleter);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (bucket_deleters_.empty())
      Complete(/*success=*/error_count_ == 0);
  }

  void Complete(bool success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback_);

    // May delete `this`.
    std::move(callback_).Run(
        this, success
                  ? blink::mojom::QuotaStatusCode::kOk
                  : blink::mojom::QuotaStatusCode::kErrorInvalidModification);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<QuotaManagerImpl> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const std::string host_;
  const StorageType type_;
  std::map<BucketDataDeleter*, std::unique_ptr<BucketDataDeleter>>
      bucket_deleters_;
  std::set<BucketLocator> buckets_ GUARDED_BY_CONTEXT(sequence_checker_);
  int error_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::OnceCallback<void(HostDataDeleter*, blink::mojom::QuotaStatusCode)>
      callback_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool run_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<HostDataDeleter> weak_factory_{this};
};

class QuotaManagerImpl::StorageCleanupHelper : public QuotaTask {
 public:
  StorageCleanupHelper(QuotaManagerImpl* manager,
                       StorageType type,
                       QuotaClientTypes quota_client_types,
                       base::OnceClosure callback)
      : QuotaTask(manager),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        callback_(std::move(callback)) {
    DCHECK(manager);
    DCHECK(manager->client_types_.contains(type_));
    DCHECK(callback_);
  }

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    base::RepeatingClosure barrier = base::BarrierClosure(
        manager()->client_types_[type_].size(),
        base::BindOnce(&StorageCleanupHelper::CallCompleted,
                       weak_factory_.GetWeakPtr()));

    // This may synchronously trigger |callback_| at the end of the for loop,
    // make sure we do nothing after this block.
    for (const auto& client_and_type : manager()->client_types_[type_]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        client->PerformStorageCleanup(type_, barrier);
      } else {
        barrier.Run();
      }
    }
  }

  void Aborted() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run();
    DeleteSoon();
  }

  void Completed() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run();
    DeleteSoon();
  }

 private:
  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<StorageCleanupHelper> weak_factory_{this};
};

// Gather storage key info table for quota-internals page.
//
// This class is granted ownership of itself when it is passed to
// DidDumpBucketTable() via base::Owned(). When the closure for said function
// goes out of scope, the object is deleted.
// This class is not thread-safe because there can be races when entries_ is
// modified.
class QuotaManagerImpl::DumpBucketTableHelper {
 public:
  QuotaError DumpBucketTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpBucketTable(base::BindRepeating(
        &DumpBucketTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpBucketTable(const base::WeakPtr<QuotaManagerImpl>& manager,
                          DumpBucketTableCallback callback,
                          QuotaError error) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(BucketTableEntries());
      return;
    }
    manager->DidDatabaseWork(error != QuotaError::kDatabaseError);
    std::move(callback).Run(entries_);
  }

 private:
  bool AppendEntry(const BucketTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  BucketTableEntries entries_;
};

// QuotaManagerImpl -----------------------------------------------------------

QuotaManagerImpl::QuotaManagerImpl(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    base::RepeatingClosure quota_change_callback,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const GetQuotaSettingsFunc& get_settings_function)
    : RefCountedDeleteOnSequence<QuotaManagerImpl>(io_thread),
      is_incognito_(is_incognito),
      profile_path_(profile_path),
      proxy_(base::MakeRefCounted<QuotaManagerProxy>(this,
                                                     io_thread,
                                                     profile_path)),
      io_thread_(std::move(io_thread)),
      db_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      get_settings_function_(get_settings_function),
      quota_change_callback_(std::move(quota_change_callback)),
      special_storage_policy_(std::move(special_storage_policy)),
      get_volume_info_fn_(&QuotaManagerImpl::GetVolumeInfo) {
  DCHECK_EQ(settings_.refresh_interval, base::TimeDelta::Max());
  if (!get_settings_function.is_null()) {
    // Reset the interval to ensure we use the get_settings_function
    // the first times settings_ is needed.
    settings_.refresh_interval = base::TimeDelta();
    get_settings_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void QuotaManagerImpl::SetQuotaSettings(const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  settings_ = settings;
  settings_timestamp_ = base::TimeTicks::Now();
}

void QuotaManagerImpl::UpdateOrCreateBucket(
    const BucketInitParams& bucket_params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  if (!bucket_params.expiration.is_null() &&
      (bucket_params.expiration <= QuotaDatabase::GetNow())) {
    std::move(callback).Run(QuotaError::kIllegalOperation);
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketInitParams& params, QuotaDatabase* database) {
            DCHECK(database);
            return database->UpdateOrCreateBucket(params);
          },
          bucket_params),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketCheckExpiration,
                     weak_factory_.GetWeakPtr(), bucket_params,
                     std::move(callback)));
}

void QuotaManagerImpl::GetOrCreateBucketDeprecated(
    const BucketInitParams& bucket_params,
    blink::mojom::StorageType storage_type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketInitParams& params,
             blink::mojom::StorageType storage_type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetOrCreateBucketDeprecated(params, storage_type);
          },
          bucket_params, storage_type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, const std::string& bucket_name,
             blink::mojom::StorageType storage_type, QuotaDatabase* database) {
            DCHECK(database);
            return database->CreateBucketForTesting(  // IN-TEST
                storage_key, bucket_name, storage_type);
          },
          storage_key, bucket_name, storage_type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, const std::string& bucket_name,
             blink::mojom::StorageType type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucket(storage_key, bucket_name, type);
          },
          storage_key, bucket_name, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketById(
    const BucketId& bucket_id,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketId bucket_id, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketById(bucket_id);
          },
          bucket_id),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetStorageKeysForType(blink::mojom::StorageType type,
                                             GetStorageKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(std::set<StorageKey>());
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](blink::mojom::StorageType type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetStorageKeysForType(type);
          },
          type),
      base::BindOnce(&QuotaManagerImpl::DidGetStorageKeys,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForType(
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](StorageType type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsForType(type);
          },
          type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForHost(
    const std::string& host,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const std::string& host, StorageType type,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsForHost(host, type);
          },
          host, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForStorageKey(
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, StorageType type,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsForStorageKey(storage_key, type);
          },
          storage_key, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetUsageInfo(GetUsageInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  GetUsageInfoTask* get_usage_info =
      new GetUsageInfoTask(this, std::move(callback));
  get_usage_info->Start();
}

void QuotaManagerImpl::GetUsageAndQuotaForWebApps(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  GetUsageAndQuotaWithBreakdown(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuotaStripBreakdown, std::move(callback)));
}

void QuotaManagerImpl::GetUsageAndQuotaWithBreakdown(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  GetUsageAndQuotaForDevtools(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuotaStripOverride, std::move(callback)));
}

void QuotaManagerImpl::GetUsageAndQuotaForDevtools(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaForDevtoolsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorNotSupported,
                            /*usage=*/0,
                            /*quota=*/0,
                            /*is_override_enabled=*/false,
                            /*usage_breakdown=*/nullptr);
    return;
  }
  EnsureDatabaseOpened();

  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, storage_key, type, is_incognito_, std::move(callback));
  helper->Start();
}

void QuotaManagerImpl::GetUsageAndQuota(const StorageKey& storage_key,
                                        StorageType type,
                                        UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (IsStorageUnlimited(storage_key, type)) {
    // TODO(michaeln): This seems like a non-obvious odd behavior, probably for
    // apps/extensions, but it would be good to eliminate this special case.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0, kNoLimit);
    return;
  }

  GetUsageAndQuotaWithBreakdown(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuotaStripBreakdown, std::move(callback)));
}

void QuotaManagerImpl::GetBucketUsageAndQuota(const BucketInfo& bucket,
                                              UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, bucket, is_incognito_,
      base::BindOnce(&DidGetUsageAndQuotaStripOverride,
                     base::BindOnce(&DidGetUsageAndQuotaStripBreakdown,
                                    std::move(callback))));
  helper->Start();
}

void QuotaManagerImpl::NotifyWriteFailed(const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto age_of_disk_stats = base::TimeTicks::Now() -
                           std::get<0>(cached_disk_stats_for_storage_pressure_);

  // Avoid polling for free disk space if disk stats have been recently
  // queried.
  if (age_of_disk_stats < kStoragePressureCheckDiskStatsInterval) {
    int64_t total_space = std::get<1>(cached_disk_stats_for_storage_pressure_);
    int64_t available_space =
        std::get<2>(cached_disk_stats_for_storage_pressure_);
    MaybeRunStoragePressureCallback(storage_key, total_space, available_space);
  }

  GetStorageCapacity(
      base::BindOnce(&QuotaManagerImpl::MaybeRunStoragePressureCallback,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void QuotaManagerImpl::SetUsageCacheEnabled(QuotaClientType client_id,
                                            const StorageKey& storage_key,
                                            StorageType type,
                                            bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  UsageTracker* usage_tracker = GetUsageTracker(type);
  DCHECK(usage_tracker);

  usage_tracker->SetUsageCacheEnabled(client_id, storage_key, enabled);
}

void QuotaManagerImpl::DeleteBucketData(const BucketLocator& bucket,
                                        QuotaClientTypes quota_client_types,
                                        StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DeleteBucketDataInternal(bucket, std::move(quota_client_types),
                           std::move(callback));
}

void QuotaManagerImpl::FindAndDeleteBucketData(const StorageKey& storage_key,
                                               const std::string& bucket_name,
                                               StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, const std::string& bucket_name,
             blink::mojom::StorageType type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucket(storage_key, bucket_name, type);
          },
          storage_key, bucket_name, StorageType::kTemporary),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketForDeletion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::UpdateBucketExpiration(
    BucketId bucket,
    const base::Time& expiration,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket, const base::Time& expiration,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->UpdateBucketExpiration(bucket, expiration);
          },
          bucket, expiration),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket, bool persistent, QuotaDatabase* database) {
            DCHECK(database);
            return database->UpdateBucketPersistence(bucket, persistent);
          },
          bucket, persistent),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::PerformStorageCleanup(
    StorageType type,
    QuotaClientTypes quota_client_types,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  StorageCleanupHelper* deleter = new StorageCleanupHelper(
      this, type, std::move(quota_client_types), std::move(callback));
  deleter->Start();
}

void QuotaManagerImpl::DeleteHostData(const std::string& host,
                                      StorageType type,
                                      StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  DCHECK(client_types_.contains(type));
  if (host.empty() || client_types_[type].empty()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }
  auto host_deleter = std::make_unique<HostDataDeleter>(
      this, host, type,
      base::BindOnce(&QuotaManagerImpl::DidDeleteHostData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  auto* host_deleter_ptr = host_deleter.get();
  host_data_deleters_[host_deleter_ptr] = std::move(host_deleter);
  host_deleter_ptr->Run();
}

// static
void QuotaManagerImpl::DidDeleteHostData(
    base::WeakPtr<QuotaManagerImpl> quota_manager,
    StatusCallback delete_host_data_callback,
    HostDataDeleter* deleter,
    blink::mojom::QuotaStatusCode status_code) {
  DCHECK(delete_host_data_callback);
  DCHECK(deleter);
  DCHECK(deleter->completed());

  if (quota_manager)
    quota_manager->host_data_deleters_.erase(deleter);

  std::move(delete_host_data_callback).Run(status_code);
}

void QuotaManagerImpl::BindInternalsHandler(
    mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  internals_handlers_receivers_.Add(this, std::move(receiver));
}

void QuotaManagerImpl::GetDiskAvailabilityAndTempPoolSize(
    GetDiskAvailabilityAndTempPoolSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  auto info = std::make_unique<AccumulateQuotaInternalsInfo>();
  auto* info_ptr = info.get();

  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(
             &QuotaManagerImpl::FinallySendDiskAvailabilityAndTempPoolSize,
             weak_factory_.GetWeakPtr(), std::move(callback), std::move(info)));

  // base::Unretained usage is safe here because BarrierClosure holds
  // the std::unque_ptr that keeps AccumulateQuotaInternalsInfo alive, and the
  // BarrierClosure will outlive the UpdateQuotaInternalsDiskAvailability
  // and UpdateQuotaInternalsTempPoolSpace closures.
  GetStorageCapacity(base::BindOnce(
      &QuotaManagerImpl::UpdateQuotaInternalsDiskAvailability,
      weak_factory_.GetWeakPtr(), barrier, base::Unretained(info_ptr)));
  GetQuotaSettings(base::BindOnce(
      &QuotaManagerImpl::UpdateQuotaInternalsTempPoolSpace,
      weak_factory_.GetWeakPtr(), barrier, base::Unretained(info_ptr)));
}

void QuotaManagerImpl::UpdateQuotaInternalsDiskAvailability(
    base::OnceClosure barrier_callback,
    AccumulateQuotaInternalsInfo* info,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(total_space, 0);
  DCHECK_GE(total_space, available_space);

  info->total_space = total_space;
  info->available_space = available_space;

  std::move(barrier_callback).Run();
}

void QuotaManagerImpl::UpdateQuotaInternalsTempPoolSpace(
    base::OnceClosure barrier_callback,
    AccumulateQuotaInternalsInfo* info,
    const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(settings.pool_size, 0);
  info->temp_pool_size = settings.pool_size;

  std::move(barrier_callback).Run();
}

void QuotaManagerImpl::FinallySendDiskAvailabilityAndTempPoolSize(
    GetDiskAvailabilityAndTempPoolSizeCallback callback,
    std::unique_ptr<AccumulateQuotaInternalsInfo> info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(info->total_space, 0);
  DCHECK_GE(info->total_space, info->available_space);
  DCHECK_GE(info->temp_pool_size, 0);

  std::move(callback).Run(info->total_space, info->available_space,
                          info->temp_pool_size);
}

void QuotaManagerImpl::GetStatistics(GetStatisticsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  base::flat_map<std::string, std::string> statistics;
  if (temporary_storage_evictor_) {
    std::map<std::string, int64_t> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (const auto& storage_key_usage_pair : stats) {
      statistics[storage_key_usage_pair.first] =
          base::NumberToString(storage_key_usage_pair.second);
    }
  }
  std::move(callback).Run(statistics);
}

void QuotaManagerImpl::GetPersistentHostQuota(const std::string& host,
                                              QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0);
    return;
  }

  if (!persistent_host_quota_callbacks_.Add(host, std::move(callback)))
    return;

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const std::string& host, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetHostQuota(host, StorageType::kPersistent);
          },
          host),
      base::BindOnce(&QuotaManagerImpl::DidGetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host));
}

void QuotaManagerImpl::SetPersistentHostQuota(const std::string& host,
                                              int64_t new_quota,
                                              QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(new_quota, 0);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (host.empty()) {
    // This could happen if we are called on file:///.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorNotSupported,
                            0);
    return;
  }

  if (new_quota < 0) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification, -1);
    return;
  }

  // Cap the requested size at the per-host quota limit.
  new_quota = std::min(new_quota, kPerHostPersistentQuotaLimit);

  if (db_disabled_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess,
                            -1);
    return;
  }
  int64_t* new_quota_ptr = new int64_t(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const std::string& host, int64_t* new_quota,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->SetHostQuota(host, StorageType::kPersistent,
                                          *new_quota);
          },
          host, base::Unretained(new_quota_ptr)),
      base::BindOnce(&QuotaManagerImpl::DidSetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host, std::move(callback),
                     base::Owned(new_quota_ptr)));
}

void QuotaManagerImpl::GetGlobalUsage(StorageType type,
                                      UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  UsageTracker* usage_tracker = GetUsageTracker(type);
  DCHECK(usage_tracker);
  usage_tracker->GetGlobalUsage(std::move(callback));
}

void QuotaManagerImpl::GetGlobalUsageForInternals(
    storage::mojom::StorageType storage_type,
    GetGlobalUsageForInternalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  StorageType type = GetBlinkStorageType(storage_type);
  UsageTracker* usage_tracker = GetUsageTracker(type);
  DCHECK(usage_tracker);
  usage_tracker->GetGlobalUsage(std::move(callback));
}

void QuotaManagerImpl::GetStorageKeyUsageWithBreakdown(
    const blink::StorageKey& storage_key,
    StorageType type,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  UsageTracker* usage_tracker = GetUsageTracker(type);
  DCHECK(usage_tracker);
  usage_tracker->GetStorageKeyUsageWithBreakdown(storage_key,
                                                 std::move(callback));
}

void QuotaManagerImpl::GetBucketUsageWithBreakdown(
    const BucketLocator& bucket,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  UsageTracker* usage_tracker = GetUsageTracker(bucket.type);
  DCHECK(usage_tracker);
  usage_tracker->GetBucketUsageWithBreakdown(bucket, std::move(callback));
}

void QuotaManagerImpl::GetHostUsageForInternals(
    const std::string& host,
    storage::mojom::StorageType storage_type,
    GetHostUsageForInternalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  StorageType type = GetBlinkStorageType(storage_type);
  UsageTracker* usage_tracker = GetUsageTracker(type);
  DCHECK(usage_tracker);

  usage_tracker->GetHostUsageWithBreakdown(
      host, base::BindOnce(&QuotaManagerImpl::OnGetHostUsageForInternals,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool QuotaManagerImpl::IsSessionOnly(const StorageKey& storage_key,
                                     StorageType type) const {
  return type == StorageType::kTemporary && special_storage_policy_ &&
         special_storage_policy_->IsStorageSessionOnly(
             storage_key.origin().GetURL());
}

bool QuotaManagerImpl::IsStorageUnlimited(const StorageKey& storage_key,
                                          StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == StorageType::kSyncable)
    return false;
  if (type == StorageType::kQuotaNotManaged)
    return true;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(
             storage_key.origin().GetURL());
}

void QuotaManagerImpl::GetBucketsModifiedBetween(StorageType type,
                                                 base::Time begin,
                                                 base::Time end,
                                                 GetBucketsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(std::set<BucketLocator>(), type);
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](blink::mojom::StorageType type, base::Time begin, base::Time end,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsModifiedBetween(type, begin, end);
          },
          type, begin, end),
      base::BindOnce(&QuotaManagerImpl::DidGetModifiedBetween,
                     weak_factory_.GetWeakPtr(), std::move(callback), type));
}

bool QuotaManagerImpl::ResetUsageTracker(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UsageTracker* previous_usage_tracker = GetUsageTracker(type);
  DCHECK(previous_usage_tracker);
  if (previous_usage_tracker->IsWorking())
    return false;

  auto usage_tracker = std::make_unique<UsageTracker>(
      this, client_types_[type], type, special_storage_policy_.get());
  switch (type) {
    case StorageType::kTemporary:
      temporary_usage_tracker_ = std::move(usage_tracker);
      return true;
    case StorageType::kPersistent:
      persistent_usage_tracker_ = std::move(usage_tracker);
      return true;
    case StorageType::kSyncable:
      syncable_usage_tracker_ = std::move(usage_tracker);
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

QuotaManagerImpl::~QuotaManagerImpl() {
  proxy_->InvalidateQuotaManagerImpl(base::PassKey<QuotaManagerImpl>());

  if (database_)
    db_runner_->DeleteSoon(FROM_HERE, database_.get());
}

QuotaManagerImpl::EvictionContext::EvictionContext() = default;
QuotaManagerImpl::EvictionContext::~EvictionContext() = default;

void QuotaManagerImpl::EnsureDatabaseOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Already initialized.
    return;
  }

  // Use an empty path to open an in-memory only database for incognito.
  database_ =
      new QuotaDatabase(is_incognito_ ? base::FilePath() : profile_path_);

  temporary_usage_tracker_ = std::make_unique<UsageTracker>(
      this, client_types_[StorageType::kTemporary], StorageType::kTemporary,
      special_storage_policy_.get());
  persistent_usage_tracker_ = std::make_unique<UsageTracker>(
      this, client_types_[StorageType::kPersistent], StorageType::kPersistent,
      special_storage_policy_.get());
  syncable_usage_tracker_ = std::make_unique<UsageTracker>(
      this, client_types_[StorageType::kSyncable], StorageType::kSyncable,
      special_storage_policy_.get());

  if (!is_incognito_) {
    histogram_timer_.Start(FROM_HERE,
                           base::Milliseconds(kReportHistogramInterval), this,
                           &QuotaManagerImpl::ReportHistogram);
  }

  if (bootstrap_disabled_for_testing_)
    return;

  is_bootstrapping_database_ = true;
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaDatabase::IsBootstrapped,
                     base::Unretained(database_.get())),
      base::BindOnce(&QuotaManagerImpl::DidGetBootstrapFlag,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetBootstrapFlag(bool is_database_bootstrapped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_bootstrapping_database_);
  if (!is_database_bootstrapped) {
    BootstrapDatabase();
    return;
  }
  is_bootstrapping_database_ = false;
  RunDatabaseCallbacks();
  StartEviction();
}

void QuotaManagerImpl::BootstrapDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!storage_key_gatherer_);
  storage_key_gatherer_ = std::make_unique<StorageKeyGathererTask>(
      this, base::BindOnce(&QuotaManagerImpl::DidGetStorageKeysForBootstrap,
                           weak_factory_.GetWeakPtr()));
  storage_key_gatherer_->Run();
}

void QuotaManagerImpl::DidGetStorageKeysForBootstrap(
    StorageKeysByType storage_keys_by_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(storage_key_gatherer_);
  storage_key_gatherer_.reset();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](base::flat_map<StorageType, std::set<StorageKey>>
                 storage_keys_by_type,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->RegisterInitialStorageKeyInfo(
                std::move(storage_keys_by_type));
          },
          std::move(storage_keys_by_type)),
      base::BindOnce(&QuotaManagerImpl::DidBootstrapDatabase,
                     weak_factory_.GetWeakPtr()),
      FROM_HERE,
      /*is_bootstrap_task=*/true);
}

void QuotaManagerImpl::DidBootstrapDatabase(QuotaError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(error != QuotaError::kDatabaseError,
                  /*is_bootstrap_work=*/true);

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce([](QuotaDatabase* database) {
        DCHECK(database);
        return database->SetIsBootstrapped(true);
      }),
      base::BindOnce(&QuotaManagerImpl::DidSetDatabaseBootstrapped,
                     weak_factory_.GetWeakPtr()),
      FROM_HERE,
      /*is_bootstrap_task=*/true);
}

void QuotaManagerImpl::DidSetDatabaseBootstrapped(QuotaError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_bootstrapping_database_);
  is_bootstrapping_database_ = false;
  DidDatabaseWork(error != QuotaError::kDatabaseError,
                  /*is_bootstrap_work=*/true);

  RunDatabaseCallbacks();
  StartEviction();
}

void QuotaManagerImpl::RunDatabaseCallbacks() {
  for (auto& callback : database_callbacks_)
    std::move(callback).Run();
  database_callbacks_.clear();
}

void QuotaManagerImpl::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!database_.get())
      << "All clients must be registered before the database is initialized";

  clients_for_ownership_.emplace_back(std::move(client));
  mojom::QuotaClient* client_ptr = clients_for_ownership_.back().get();

  for (blink::mojom::StorageType storage_type : storage_types)
    client_types_[storage_type].insert({client_ptr, client_type});
}

UsageTracker* QuotaManagerImpl::GetUsageTracker(StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case StorageType::kTemporary:
      return temporary_usage_tracker_.get();
    case StorageType::kPersistent:
      return persistent_usage_tracker_.get();
    case StorageType::kSyncable:
      return syncable_usage_tracker_.get();
    case StorageType::kQuotaNotManaged:
      return nullptr;
    case StorageType::kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

void QuotaManagerImpl::OnGetHostUsageForInternals(
    GetHostUsageForInternalsCallback callback,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(usage, -1);

  std::move(callback).Run(usage);
}

void QuotaManagerImpl::NotifyStorageAccessed(const StorageKey& storage_key,
                                             StorageType type,
                                             base::Time access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (type == StorageType::kTemporary && is_getting_eviction_bucket_) {
    // Record the accessed storage keys while GetLruEvictableBucket task is
    // running to filter out them from eviction.
    access_notified_storage_keys_.insert(storage_key);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, StorageType type,
             base::Time accessed_time, QuotaDatabase* database) {
            DCHECK(database);
            return database->SetStorageKeyLastAccessTime(storage_key, type,
                                                         accessed_time);
          },
          storage_key, type, access_time),
      base::BindOnce(&QuotaManagerImpl::OnComplete,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::NotifyBucketAccessed(BucketId bucket_id,
                                            base::Time access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (is_getting_eviction_bucket_) {
    // Record the accessed buckets while GetLRUStorageKey task is running
    // to filter out them from eviction.
    access_notified_buckets_.insert(bucket_id);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket_id, base::Time accessed_time,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->SetBucketLastAccessTime(bucket_id, accessed_time);
          },
          bucket_id, access_time),
      base::BindOnce(&QuotaManagerImpl::OnComplete,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::NotifyStorageModified(QuotaClientType client_id,
                                             const StorageKey& storage_key,
                                             StorageType type,
                                             int64_t delta,
                                             base::Time modification_time,
                                             base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));

  if (db_disabled_) {
    if (callback)
      std::move(callback).Run();
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, const std::string& bucket_name,
             blink::mojom::StorageType type, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucket(storage_key, bucket_name, type);
          },
          storage_key, kDefaultBucketName, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketForUsage,
                     weak_factory_.GetWeakPtr(), client_id, delta,
                     modification_time, std::move(callback)));
}

void QuotaManagerImpl::NotifyBucketModified(QuotaClientType client_id,
                                            BucketId bucket_id,
                                            int64_t delta,
                                            base::Time modification_time,
                                            base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket_id, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketById(bucket_id);
          },
          bucket_id),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketForUsage,
                     weak_factory_.GetWeakPtr(), client_id, delta,
                     modification_time, std::move(callback)));
}

void QuotaManagerImpl::DumpBucketTable(DumpBucketTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (db_disabled_ || !database_) {
    std::move(callback).Run(BucketTableEntries());
    return;
  }
  DumpBucketTableHelper* helper = new DumpBucketTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&DumpBucketTableHelper::DumpBucketTableOnDBThread,
                     base::Unretained(helper)),
      base::BindOnce(&DumpBucketTableHelper::DidDumpBucketTable,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void QuotaManagerImpl::RetrieveBucketsTable(
    RetrieveBucketsTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (db_disabled_) {
    std::move(callback).Run({});
    return;
  }

  DumpBucketTable(
      base::BindOnce(&QuotaManagerImpl::RetrieveBucketUsageForBucketTable,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::RetrieveBucketUsageForBucketTable(
    RetrieveBucketsTableCallback callback,
    const BucketTableEntries& entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* buckets = new std::vector<storage::mojom::BucketTableEntryPtr>;

  base::RepeatingClosure barrier = base::BarrierClosure(
      entries.size(),
      base::BindOnce(
          [](RetrieveBucketsTableCallback callback,
             std::vector<storage::mojom::BucketTableEntryPtr>* buckets) {
            std::move(callback).Run(std::move(*buckets));
          },
          std::move(callback), base::Owned(buckets)));

  for (auto& entry : entries) {
    DCHECK(IsSupportedType(entry.type));

    GetBucketUsageWithBreakdown(
        entry.ToBucketLocator(),
        base::BindOnce(&QuotaManagerImpl::AddBucketTableEntry,
                       weak_factory_.GetWeakPtr(), entry, barrier, buckets));
  }
}

void QuotaManagerImpl::AddBucketTableEntry(
    const BucketTableEntry& entry,
    base::OnceClosure barrier_callback,
    std::vector<storage::mojom::BucketTableEntryPtr>* buckets,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr bucketUsageBreakdown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage::mojom::BucketTableEntryPtr mojo_entry =
      storage::mojom::BucketTableEntry::New();
  mojo_entry->bucket_id = entry.bucket_id.value();
  mojo_entry->storage_key = entry.storage_key.Serialize();
  mojo_entry->host = entry.storage_key.origin().host();
  mojo_entry->type = StorageTypeEnumToString(entry.type);
  mojo_entry->name = entry.name;
  mojo_entry->use_count = entry.use_count;
  mojo_entry->last_accessed = entry.last_accessed;
  mojo_entry->last_modified = entry.last_modified;
  mojo_entry->usage = usage;

  buckets->emplace_back(std::move(mojo_entry));
  std::move(barrier_callback).Run();
}

void QuotaManagerImpl::StartEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!temporary_storage_evictor_.get());

  if (eviction_disabled_)
    return;
  temporary_storage_evictor_ = std::make_unique<QuotaTemporaryStorageEvictor>(
      this, kEvictionIntervalInMilliSeconds);
  temporary_storage_evictor_->Start();
}

void QuotaManagerImpl::DeleteBucketFromDatabase(
    const BucketLocator& bucket,
    base::OnceCallback<void(QuotaError)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketLocator& bucket, QuotaDatabase* database) {
            DCHECK(database);
            return database->DeleteBucketData(bucket);
          },
          bucket),
      std::move(callback));
}

void QuotaManagerImpl::DidBucketDataEvicted(
    QuotaDatabase::BucketTableEntry entry,
    blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try to evict buckets that are not in use, so basically deletion
  // attempt for eviction should not fail.  Let's record the bucket if we get
  // an error and exclude it from future eviction if the error happens
  // consistently (> kThresholdOfErrorsToBeDenylisted).
  if (status != blink::mojom::QuotaStatusCode::kOk)
    buckets_in_error_[eviction_context_.evicted_bucket.id]++;

  if (status == blink::mojom::QuotaStatusCode::kOk) {
    base::Time now = QuotaDatabase::GetNow();
    base::UmaHistogramCounts1M(
        QuotaManagerImpl::kEvictedBucketAccessedCountHistogram,
        entry.use_count);
    base::UmaHistogramCounts1000(
        QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram,
        (now - entry.last_accessed).InDays());
  }

  std::move(eviction_context_.evict_bucket_data_callback).Run(status);
}

void QuotaManagerImpl::DeleteBucketDataInternal(
    const BucketLocator& bucket,
    QuotaClientTypes quota_client_types,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess);
    return;
  }
  auto bucket_deleter = std::make_unique<BucketDataDeleter>(
      this, bucket, std::move(quota_client_types),
      base::BindOnce(&QuotaManagerImpl::DidDeleteBucketData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  auto* bucket_deleter_ptr = bucket_deleter.get();
  bucket_data_deleters_[bucket_deleter_ptr] = std::move(bucket_deleter);
  bucket_deleter_ptr->Run();
}

// static
void QuotaManagerImpl::DidDeleteBucketData(
    base::WeakPtr<QuotaManagerImpl> quota_manager,
    StatusCallback delete_bucket_data_callback,
    BucketDataDeleter* deleter,
    blink::mojom::QuotaStatusCode status_code) {
  DCHECK(delete_bucket_data_callback);
  DCHECK(deleter);
  DCHECK(deleter->completed());

  if (quota_manager)
    quota_manager->bucket_data_deleters_.erase(deleter);

  std::move(delete_bucket_data_callback).Run(status_code);
}

void QuotaManagerImpl::DidDeleteBucketForRecreation(
    const BucketInitParams& params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    BucketInfo bucket_info,
    blink::mojom::QuotaStatusCode status_code) {
  if (status_code == blink::mojom::QuotaStatusCode::kOk) {
    UpdateOrCreateBucket(params, std::move(callback));
  } else {
    std::move(callback).Run(QuotaError::kDatabaseError);
  }
}

void QuotaManagerImpl::MaybeRunStoragePressureCallback(
    const StorageKey& storage_key,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(total_space, 0);
  DCHECK_GE(available_space, 0);

  // TODO(https://crbug.com/1059560): Figure out what 0 total_space means
  // and how to handle the storage pressure callback in these cases.
  if (total_space == 0)
    return;

  if (!storage_pressure_callback_) {
    // Quota will hold onto a storage pressure notification if no storage
    // pressure callback is set.
    storage_key_for_pending_storage_pressure_callback_ = std::move(storage_key);
    return;
  }

  if (available_space < kStoragePressureThresholdRatio * total_space) {
    storage_pressure_callback_.Run(std::move(storage_key));
  }
}

void QuotaManagerImpl::SimulateStoragePressure(const url::Origin& origin_url) {
  StorageKey key(origin_url);
  storage_pressure_callback_.Run(key);
}

void QuotaManagerImpl::DetermineStoragePressure(int64_t total_space,
                                                int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(total_space, 0);
  DCHECK_GE(free_space, 0);

  if (!base::FeatureList::IsEnabled(features::kStoragePressureEvent)) {
    return;
  }
  int64_t threshold_bytes =
      RandomizeByPercent(kGBytes, kThresholdRandomizationPercent);
  int64_t threshold = RandomizeByPercent(
      static_cast<int64_t>(total_space *
                           (kThresholdRandomizationPercent / 100.0)),
      kThresholdRandomizationPercent);
  threshold = std::min(threshold_bytes, threshold);
  if (free_space < threshold && !quota_change_callback_.is_null()) {
    quota_change_callback_.Run();
  }
}

void QuotaManagerImpl::SetStoragePressureCallback(
    base::RepeatingCallback<void(StorageKey)> storage_pressure_callback) {
  storage_pressure_callback_ = storage_pressure_callback;
  if (storage_key_for_pending_storage_pressure_callback_.has_value()) {
    storage_pressure_callback_.Run(
        std::move(storage_key_for_pending_storage_pressure_callback_.value()));
    storage_key_for_pending_storage_pressure_callback_ = absl::nullopt;
  }
}

int QuotaManagerImpl::GetOverrideHandleId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++next_override_handle_id_;
}

void QuotaManagerImpl::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    absl::optional<int64_t> quota_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(quota_size.value_or(0), 0)
      << "negative quota override: " << quota_size.value_or(0);

  if (quota_size.has_value()) {
    DCHECK_GE(next_override_handle_id_, handle_id);
    // Bracket notation is safe here because we want to construct a new
    // QuotaOverride in the case that one does not exist for storage key.
    devtools_overrides_[storage_key].active_override_session_ids.insert(
        handle_id);
    devtools_overrides_[storage_key].quota_size = quota_size.value();
  } else {
    devtools_overrides_.erase(storage_key);
  }
}

void QuotaManagerImpl::WithdrawOverridesForHandle(int handle_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<StorageKey> storage_keys_to_clear;
  for (auto& devtools_override : devtools_overrides_) {
    auto& quota_override = devtools_override.second;
    auto& storage_key = devtools_override.first;

    quota_override.active_override_session_ids.erase(handle_id);

    if (!quota_override.active_override_session_ids.size()) {
      storage_keys_to_clear.push_back(storage_key);
    }
  }

  for (auto& storage_key : storage_keys_to_clear) {
    devtools_overrides_.erase(storage_key);
  }
}

absl::optional<int64_t> QuotaManagerImpl::GetQuotaOverrideForStorageKey(
    const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::Contains(devtools_overrides_, storage_key)) {
    return absl::nullopt;
  }
  return devtools_overrides_[storage_key].quota_size;
}

void QuotaManagerImpl::SetQuotaChangeCallbackForTesting(
    base::RepeatingClosure storage_pressure_event_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  quota_change_callback_ = std::move(storage_pressure_event_callback);
}

void QuotaManagerImpl::CorruptDatabaseForTesting(
    base::OnceCallback<void(const base::FilePath&)> corrupter,
    base::OnceCallback<void(QuotaError)> callback) {
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](base::OnceCallback<void(const base::FilePath&)> corrupter,
             QuotaDatabase* database) {
            return database->CorruptForTesting(  // IN-TEST
                std::move(corrupter));
          },
          std::move(corrupter)),
      std::move(callback));
}

void QuotaManagerImpl::ReportHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_incognito_);

  GetGlobalUsage(
      StorageType::kTemporary,
      base::BindOnce(&QuotaManagerImpl::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetTemporaryGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_GE(usage, -1);
  DCHECK_GE(unlimited_usage, -1);

  GetStorageCapacity(
      base::BindOnce(&QuotaManagerImpl::DidGetStorageCapacityForHistogram,
                     weak_factory_.GetWeakPtr(), usage));
}

void QuotaManagerImpl::DidGetStorageCapacityForHistogram(
    int64_t usage,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(usage, -1);
  DCHECK_GE(total_space, 0);
  DCHECK_GE(available_space, 0);

  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfTemporaryStorage", usage);
  if (total_space > 0) {
    UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedForTemporaryStorage2",
                             static_cast<int>((usage * 100) / total_space));
    UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace2", available_space);
    UMA_HISTOGRAM_PERCENTAGE(
        "Quota.PercentDiskAvailable2",
        std::min(100, static_cast<int>((available_space * 100 / total_space))));
  }

  GetGlobalUsage(
      StorageType::kPersistent,
      base::BindOnce(&QuotaManagerImpl::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetPersistentGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(usage, -1);
  DCHECK_GE(unlimited_usage, -1);

  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  // We DumpBucketTable last to ensure the trackers caches are loaded.
  DumpBucketTable(
      base::BindOnce(&QuotaManagerImpl::DidDumpBucketTableForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidDumpBucketTableForHistogram(
    const BucketTableEntries& entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::map<StorageKey, int64_t> usage_map =
      GetUsageTracker(StorageType::kTemporary)->GetCachedStorageKeysUsage();
  base::Time now = QuotaDatabase::GetNow();
  for (const auto& info : entries) {
    if (info.type != StorageType::kTemporary)
      continue;

    // Ignore stale database entries. If there is no map entry, the storage
    // key's data has been deleted.
    auto it = usage_map.find(info.storage_key);
    if (it == usage_map.end() || it->second == 0)
      continue;

    base::TimeDelta age =
        now - std::max(info.last_accessed, info.last_modified);
    UMA_HISTOGRAM_COUNTS_1000("Quota.AgeOfOriginInDays", age.InDays());

    int64_t kilobytes = std::max(it->second / int64_t{1024}, int64_t{1});
    base::Histogram::FactoryGet(
        "Quota.AgeOfDataInDays", 1, 1000, 50,
        base::HistogramBase::kUmaTargetedHistogramFlag)->
            AddCount(age.InDays(),
                     base::saturated_cast<int>(kilobytes));
  }
}

std::set<BucketId> QuotaManagerImpl::GetEvictionBucketExceptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<BucketId> exceptions;
  for (const auto& p : buckets_in_error_) {
    if (p.second > QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted)
      exceptions.insert(p.first);
  }

  return exceptions;
}

void QuotaManagerImpl::DidGetEvictionBucket(
    GetBucketCallback callback,
    const absl::optional<BucketLocator>& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  // Make sure the returned bucket has not been accessed since we posted the
  // eviction task.
  DCHECK(!bucket.has_value() ||
         !bucket->storage_key.origin().GetURL().is_empty());
  // TODO(crbug.com/1208141): Remove this evaluation for storage key once
  // QuotaClient is migrated to operate on buckets and NotifyStorageAccessed
  // no longer used.
  if (bucket.has_value() && bucket->is_default &&
      base::Contains(access_notified_storage_keys_, bucket->storage_key)) {
    std::move(callback).Run(absl::nullopt);
  } else if (bucket.has_value() &&
             base::Contains(access_notified_buckets_, bucket->id)) {
    std::move(callback).Run(absl::nullopt);
  } else {
    std::move(callback).Run(bucket);
  }
  access_notified_storage_keys_.clear();
  access_notified_buckets_.clear();

  is_getting_eviction_bucket_ = false;
}

void QuotaManagerImpl::GetEvictionBucket(StorageType type,
                                         GetBucketCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  // This must not be called while there's an in-flight task.
  DCHECK(!is_getting_eviction_bucket_);
  is_getting_eviction_bucket_ = true;

  GetLruEvictableBucket(
      type, base::BindOnce(&QuotaManagerImpl::DidGetEvictionBucket,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::EvictBucketData(const BucketLocator& bucket,
                                       StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(bucket.type, StorageType::kTemporary);
  DCHECK(callback);

  eviction_context_.evicted_bucket = bucket;
  eviction_context_.evict_bucket_data_callback = std::move(callback);

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket_id, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketInfo(bucket_id);
          },
          bucket.id),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketInfoForEviction,
                     weak_factory_.GetWeakPtr(), bucket));
}

void QuotaManagerImpl::DidGetBucketInfoForEviction(
    const BucketLocator& bucket,
    QuotaErrorOr<QuotaDatabase::BucketTableEntry> result) {
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (!result.ok()) {
    std::move(eviction_context_.evict_bucket_data_callback)
        .Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess);
    return;
  }

  DeleteBucketDataInternal(
      bucket, AllQuotaClientTypes(),
      base::BindOnce(&QuotaManagerImpl::DidBucketDataEvicted,
                     weak_factory_.GetWeakPtr(), result.value()));
}

void QuotaManagerImpl::GetEvictionRoundInfo(
    EvictionRoundInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(callback);
  EnsureDatabaseOpened();

  DCHECK(!eviction_helper_);
  eviction_helper_ = std::make_unique<EvictionRoundInfoHelper>(
      this, std::move(callback),
      base::BindOnce(&QuotaManagerImpl::DidGetEvictionRoundInfo,
                     weak_factory_.GetWeakPtr()));
  eviction_helper_->Run();
}

void QuotaManagerImpl::DidGetEvictionRoundInfo() {
  DCHECK(eviction_helper_);
  eviction_helper_.reset();
}

void QuotaManagerImpl::GetLruEvictableBucket(StorageType type,
                                             GetBucketCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  // This must not be called while there's an in-flight task.
  DCHECK(lru_bucket_callback_.is_null());
  lru_bucket_callback_ = std::move(callback);
  if (db_disabled_) {
    std::move(lru_bucket_callback_).Run(absl::nullopt);
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](StorageType type, const std::set<BucketId>& bucket_exceptions,
             SpecialStoragePolicy* policy, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetLruEvictableBucket(type, bucket_exceptions,
                                                   policy);
          },
          type, GetEvictionBucketExceptions(),
          base::RetainedRef(special_storage_policy_)),
      base::BindOnce(&QuotaManagerImpl::DidGetLruEvictableBucket,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetPersistentHostQuota(const std::string& host,
                                                 QuotaErrorOr<int64_t> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (!result.ok() && result.error() != QuotaError::kNotFound) {
    persistent_host_quota_callbacks_.Run(
        host, blink::mojom::QuotaStatusCode::kErrorInvalidAccess, /*quota=*/0);
    return;
  }
  persistent_host_quota_callbacks_.Run(
      host, blink::mojom::QuotaStatusCode::kOk,
      std::min(result.ok() ? result.value() : 0, kPerHostPersistentQuotaLimit));
}

void QuotaManagerImpl::DidSetPersistentHostQuota(const std::string& host,
                                                 QuotaCallback callback,
                                                 const int64_t* new_quota,
                                                 QuotaError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(error != QuotaError::kDatabaseError);

  if (error == QuotaError::kNone) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, *new_quota);
    return;
  }
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess,
                          /*new_quota=*/0);
}

void QuotaManagerImpl::DidGetLruEvictableBucket(
    QuotaErrorOr<BucketLocator> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (result.ok()) {
    std::move(lru_bucket_callback_)
        .Run(absl::make_optional(std::move(result.value())));
  } else {
    std::move(lru_bucket_callback_).Run(absl::nullopt);
  }
}

void QuotaManagerImpl::GetQuotaSettings(QuotaSettingsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (base::TimeTicks::Now() - settings_timestamp_ <
      settings_.refresh_interval) {
    std::move(callback).Run(settings_);
    return;
  }

  if (!settings_callbacks_.Add(std::move(callback)))
    return;

  // We invoke our clients GetQuotaSettingsFunc on the
  // UI thread and plumb the resulting value back to this thread.
  get_settings_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_settings_function_,
          base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                             base::BindOnce(&QuotaManagerImpl::DidGetSettings,
                                            weak_factory_.GetWeakPtr()))));
}

void QuotaManagerImpl::DidGetSettings(absl::optional<QuotaSettings> settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!settings) {
    settings = settings_;
    settings->refresh_interval = base::Minutes(1);
  }
  SetQuotaSettings(*settings);
  settings_callbacks_.Run(*settings);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalTemporaryPoolSize", settings->pool_size);
  LOG_IF(WARNING, settings->pool_size == 0)
      << "No storage quota provided in QuotaSettings.";
}

void QuotaManagerImpl::GetStorageCapacity(StorageCapacityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (!storage_capacity_callbacks_.Add(std::move(callback)))
    return;
  if (is_incognito_) {
    GetQuotaSettings(
        base::BindOnce(&QuotaManagerImpl::ContinueIncognitoGetStorageCapacity,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaManagerImpl::CallGetVolumeInfo, get_volume_info_fn_,
                     profile_path_),
      base::BindOnce(&QuotaManagerImpl::DidGetStorageCapacity,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::ContinueIncognitoGetStorageCapacity(
    const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t temporary_usage =
      GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
  DCHECK_GE(temporary_usage, -1);

  int64_t persistent_usage =
      GetUsageTracker(StorageType::kPersistent)->GetCachedUsage();
  DCHECK_GE(persistent_usage, -1);

  int64_t current_usage = temporary_usage + persistent_usage;
  int64_t available_space =
      std::max(int64_t{0}, settings.pool_size - current_usage);
  DidGetStorageCapacity(std::make_tuple(settings.pool_size, available_space));
}

void QuotaManagerImpl::DidGetStorageCapacity(
    const std::tuple<int64_t, int64_t>& total_and_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t total_space = std::get<0>(total_and_available);
  DCHECK_GE(total_space, 0);

  int64_t available_space = std::get<1>(total_and_available);
  DCHECK_GE(available_space, 0);

  cached_disk_stats_for_storage_pressure_ =
      std::make_tuple(base::TimeTicks::Now(), total_space, available_space);
  storage_capacity_callbacks_.Run(total_space, available_space);
  DetermineStoragePressure(total_space, available_space);
}

void QuotaManagerImpl::DidDatabaseWork(bool success, bool is_bootstrap_work) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success)
    return;

  // Ignore any errors that happen while a new bootstrap attempt is already in
  // progress or queued.
  if (is_bootstrapping_database_)
    return;

  db_error_count_++;

  if (db_error_count_ >=
      QuotaManagerImpl::kThresholdOfErrorsToDisableDatabase) {
    if (bootstrap_disabled_for_testing_ || is_bootstrap_work) {
      // If we got an error during bootstrapping there is no point in
      // immediately trying again. Disable the database instead.
      db_disabled_ = true;
      return;
    }

    // Start another bootstrapping process. Pause eviction while bootstrapping
    // is in progress. When bootstrapping finishes a new Evictor will be
    // created.
    is_bootstrapping_database_ = true;
    temporary_storage_evictor_ = nullptr;
    db_error_count_ = 0;

    // Wipe the database before triggering another bootstrap.
    base::PostTaskAndReplyWithResult(
        db_runner_.get(), FROM_HERE,
        base::BindOnce(&QuotaDatabase::RazeAndReopen,
                       base::Unretained(database_.get())),
        base::BindOnce(&QuotaManagerImpl::DidRazeForReBootstrap,
                       weak_factory_.GetWeakPtr()));
  }
}

void QuotaManagerImpl::DidRazeForReBootstrap(
    QuotaError raze_and_reopen_result) {
  if (raze_and_reopen_result == QuotaError::kNone) {
    BootstrapDatabase();
    return;
  }

  // Deleting the database failed. Disable the database and hope we'll recover
  // after Chrome restarts instead.
  db_disabled_ = true;
  is_bootstrapping_database_ = false;
  RunDatabaseCallbacks();
  // No reason to restart eviction here. Without a working database there is
  // nothing to evict.
}

void QuotaManagerImpl::OnComplete(QuotaError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result != QuotaError::kDatabaseError);
}

void QuotaManagerImpl::DidGetBucket(
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketCheckExpiration(
    const BucketInitParams& params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (result.ok() && !result->expiration.is_null() &&
      result->expiration <= QuotaDatabase::GetNow()) {
    DeleteBucketDataInternal(
        result->ToBucketLocator(), AllQuotaClientTypes(),
        base::BindOnce(&QuotaManagerImpl::DidDeleteBucketForRecreation,
                       weak_factory_.GetWeakPtr(), params, std::move(callback),
                       result.value()));
    return;
  }

  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketForDeletion(
    StatusCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (!result.ok()) {
    // Return QuotaStatusCode::kOk if bucket not found. No work needed.
    std::move(callback).Run(result.error() == QuotaError::kNotFound
                                ? blink::mojom::QuotaStatusCode::kOk
                                : blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  DeleteBucketDataInternal(result->ToBucketLocator(), AllQuotaClientTypes(),
                           std::move(callback));
  return;
}

void QuotaManagerImpl::DidGetBucketForUsage(QuotaClientType client_type,
                                            int64_t delta,
                                            base::Time modification_time,
                                            base::OnceClosure callback,
                                            QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (!result.ok()) {
    if (callback)
      std::move(callback).Run();
    return;
  }

  BucketLocator bucket(result->id, result->storage_key, result->type,
                       result->is_default());
  GetUsageTracker(bucket.type)
      ->UpdateBucketUsageCache(client_type, bucket, delta);

  // Return once usage cache is updated for callers waiting for quota changes to
  // be reflected before querying for usage.
  if (callback)
    std::move(callback).Run();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketId bucket_id, base::Time modified_time,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->SetBucketLastModifiedTime(bucket_id,
                                                       modified_time);
          },
          bucket.id, modification_time),
      base::BindOnce(&QuotaManagerImpl::OnComplete,
                     weak_factory_.GetWeakPtr()));
  return;
}

void QuotaManagerImpl::DidGetStorageKeys(
    GetStorageKeysCallback callback,
    QuotaErrorOr<std::set<StorageKey>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  if (!result.ok()) {
    std::move(callback).Run(std::set<StorageKey>());
    return;
  }
  std::move(callback).Run(std::move(result.value()));
}

void QuotaManagerImpl::DidGetBuckets(
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetModifiedBetween(
    GetBucketsCallback callback,
    StorageType type,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  if (!result.ok()) {
    std::move(callback).Run(std::set<BucketLocator>(), type);
    return;
  }
  std::move(callback).Run(result.value(), type);
}

template <typename ValueType>
void QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread(
    base::OnceCallback<QuotaErrorOr<ValueType>(QuotaDatabase*)> task,
    base::OnceCallback<void(QuotaErrorOr<ValueType>)> reply,
    const base::Location& from_here,
    bool is_bootstrap_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task);
  DCHECK(reply);
  // Deleting manager will post another task to DB sequence to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.

  if (!is_bootstrap_task && is_bootstrapping_database_) {
    database_callbacks_.push_back(base::BindOnce(
        &QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread<ValueType>,
        weak_factory_.GetWeakPtr(), std::move(task), std::move(reply),
        from_here, is_bootstrap_task));
    return;
  }

  base::PostTaskAndReplyWithResult(
      db_runner_.get(), from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

void QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread(
    base::OnceCallback<QuotaError(QuotaDatabase*)> task,
    base::OnceCallback<void(QuotaError)> reply,
    const base::Location& from_here,
    bool is_bootstrap_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task);
  DCHECK(reply);
  // Deleting manager will post another task to DB sequence to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.

  if (!is_bootstrap_task && is_bootstrapping_database_) {
    database_callbacks_.push_back(base::BindOnce(
        static_cast<void (QuotaManagerImpl::*)(
            base::OnceCallback<QuotaError(QuotaDatabase*)>,
            base::OnceCallback<void(QuotaError)>, const base::Location&, bool)>(
            &QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread),
        weak_factory_.GetWeakPtr(), std::move(task), std::move(reply),
        from_here, is_bootstrap_task));
    return;
  }

  base::PostTaskAndReplyWithResult(
      db_runner_.get(), from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

// static
std::tuple<int64_t, int64_t> QuotaManagerImpl::CallGetVolumeInfo(
    GetVolumeInfoFn get_volume_info_fn,
    const base::FilePath& path) {
  if (!base::CreateDirectory(path)) {
    LOG(WARNING) << "Create directory failed for path" << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }

  const auto [total, available] = get_volume_info_fn(path);
  if (total < 0 || available < 0) {
    LOG(WARNING) << "Unable to get volume info: " << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }
  DCHECK_GE(total, 0);
  DCHECK_GE(available, 0);

  UMA_HISTOGRAM_MBYTES("Quota.TotalDiskSpace", total);
  UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace", available);
  if (total > 0) {
    UMA_HISTOGRAM_PERCENTAGE("Quota.PercentDiskAvailable",
        std::min(100, static_cast<int>((available * 100) / total)));
  }
  return std::make_tuple(total, available);
}

// static
std::tuple<int64_t, int64_t> QuotaManagerImpl::GetVolumeInfo(
    const base::FilePath& path) {
  return std::make_tuple(base::SysInfo::AmountOfTotalDiskSpace(path),
                         base::SysInfo::AmountOfFreeDiskSpace(path));
}

}  // namespace storage

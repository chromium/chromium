// Copyright 2013 The Chromium Authors
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

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_availability.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_macros.h"
#include "storage/browser/quota/quota_manager_observer.mojom-forward.h"
#include "storage/browser/quota/quota_manager_observer.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/quota/usage_tracker.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

// These values are used in UMA, so the list should be append-only.
enum class DatabaseDisabledReason {
  kRegisterStorageKeyFailed = 0,
  kSetIsBootstrappedFailed = 1,
  kRazeFailed = 2,
  kMaxValue = kRazeFailed,
};

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

bool IsSupportedType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kSyncable;
}

bool IsSupportedIncognitoType(StorageType type) {
  return type == StorageType::kTemporary;
}

void ReportDatabaseDisabledReason(DatabaseDisabledReason reason) {
  base::UmaHistogramEnumeration("Quota.QuotaDatabaseDisabled", reason);
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

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64_t QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota =
    500 * kMBytes;

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
    // settings, host_usage, storage_key_quota and device_storage_capacity if
    // unlimited.
    base::ConcurrentClosures concurrent;
    manager()->GetQuotaSettings(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotSettings,
                       weak_factory_.GetWeakPtr(), concurrent.CreateClosure()));

    if (bucket_info_) {
      manager()->GetBucketUsageWithBreakdown(
          bucket_info_->ToBucketLocator(),
          base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotUsage,
                         weak_factory_.GetWeakPtr(),
                         concurrent.CreateClosure()));
    } else {
      manager()->GetStorageKeyUsageWithBreakdown(
          storage_key_, type_,
          base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotUsage,
                         weak_factory_.GetWeakPtr(),
                         concurrent.CreateClosure()));
    }

    // Determine storage_key_quota differently depending on type.
    if (is_unlimited_) {
      SetDesiredStorageKeyQuota(kNoLimit);
      manager()->GetStorageCapacity(base::BindOnce(
          &UsageAndQuotaInfoGatherer::OnGotCapacity, weak_factory_.GetWeakPtr(),
          concurrent.CreateClosure()));
    } else {
      // For limited storage,  OnGotSettings will set the host quota.
    }

    std::move(concurrent)
        .Done(base::BindOnce(&UsageAndQuotaInfoGatherer::CallCompleted,
                             weak_factory_.GetWeakPtr()));
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
    std::optional<int64_t> quota_override_size =
        manager()->GetQuotaOverrideForStorageKey(storage_key_);
    if (quota_override_size) {
      quota = *quota_override_size;
    }

    // For an individual bucket, the quota is the minimum of the requested quota
    // and the StorageKey quota.
    if (bucket_info_ && bucket_info_->quota > 0) {
      quota = std::min(quota, bucket_info_->quota);
    }

    if (is_unlimited_) {
      int64_t temp_pool_free_space =
          available_space_ - settings_.must_remain_available;
      // Constrain the desired quota to something that fits.
      if (quota > temp_pool_free_space) {
        quota = available_space_ + usage_;
      }
    }

    std::move(callback_).Run(usage_ >= 0
                                 ? blink::mojom::QuotaStatusCode::kOk
                                 : blink::mojom::QuotaStatusCode::kUnknown,
                             usage_, quota, quota_override_size.has_value(),
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

  void OnGotSettings(base::OnceClosure callback,
                     const QuotaSettings& settings) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);

    settings_ = settings;
    const int64_t quota =
        manager()->GetQuotaForStorageKey(storage_key_, type_, settings);
    if (quota != kNoLimit) {
      SetDesiredStorageKeyQuota(quota);
    }

    std::move(callback).Run();
  }

  void OnGotCapacity(base::OnceClosure callback,
                     int64_t total_space,
                     int64_t available_space) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);
    DCHECK_GE(total_space, 0);
    DCHECK_GE(available_space, 0);

    total_space_ = total_space;
    available_space_ = available_space;
    std::move(callback).Run();
  }

  void OnGotUsage(base::OnceClosure callback,
                  int64_t usage,
                  blink::mojom::UsageBreakdownPtr usage_breakdown) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);
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
    std::move(callback).Run();
  }

  void SetDesiredStorageKeyQuota(int64_t quota) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GE(quota, 0);

    desired_storage_key_quota_ = quota;
  }

  // These fields are passed at construction time.
  const StorageKey storage_key_;
  // Non-null iff usage info is to be gathered for an individual bucket. If
  // null, usage is gathered for all buckets in the given host/StorageKey.
  std::optional<BucketInfo> bucket_info_;
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
    remaining_trackers_ = 2;
    // This will populate cached hosts and usage info.
    manager()
        ->GetUsageTracker(StorageType::kTemporary)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kTemporary));
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
    if (--remaining_trackers_ == 0) {
      CallCompleted();
    }
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

    base::ConcurrentClosures concurrent;
    GetStorageKeysForType(StorageType::kTemporary, concurrent);
    GetStorageKeysForType(StorageType::kSyncable, concurrent);
    std::move(concurrent)
        .Done(
            base::BindOnce(&QuotaManagerImpl::StorageKeyGathererTask::Completed,
                           weak_factory_.GetWeakPtr()));
  }

 private:
  void GetStorageKeysForType(StorageType type,
                             base::ConcurrentClosures& concurrent) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto client_map_it = manager_->client_types_.find(type);
    CHECK(client_map_it != manager_->client_types_.end(),
          base::NotFatalUntil::M130);

    for (const auto& client_and_type : client_map_it->second) {
      client_and_type.first->GetStorageKeysForType(
          type, base::BindOnce(&StorageKeyGathererTask::DidGetStorageKeys,
                               weak_factory_.GetWeakPtr(), type,
                               concurrent.CreateClosure()));
    }
  }

  void DidGetStorageKeys(StorageType type,
                         base::OnceClosure callback,
                         const std::vector<StorageKey>& storage_keys) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);

    storage_keys_by_type_[type].insert(storage_keys.begin(),
                                       storage_keys.end());
    std::move(callback).Run();
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
      bool commit_immediately,
      base::OnceCallback<void(BucketDataDeleter*,
                              QuotaErrorOr<mojom::BucketTableEntryPtr>)>
          callback)
      : manager_(manager),
        bucket_(bucket),
        quota_client_types_(std::move(quota_client_types)),
        commit_immediately_(commit_immediately),
        callback_(std::move(callback)) {
    DCHECK(manager_);
    // TODO(crbug.com/40058632): Convert back into DCHECKs once issue is
    // resolved.
    CHECK(callback_);
  }

  ~BucketDataDeleter() {
    // `callback` is non-null if the deleter gets destroyed before completing.
    if (callback_) {
      std::move(callback_).Run(this,
                               base::unexpected(QuotaError::kUnknownError));
    }
  }

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    // TODO(crbug.com/40058632): Convert back into DCHECK once issue is
    // resolved.
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

    if (remaining_clients_ == 0) {
      FinishDeletion();
    }
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

    if (status != blink::mojom::QuotaStatusCode::kOk) {
      ++error_count_;
    }

    if (--remaining_clients_ == 0) {
      FinishDeletion();
    }
  }

  void FinishDeletion() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(crbug.com/40058632): Convert back into DCHECKs once issue is
    // resolved.
    CHECK_EQ(remaining_clients_, 0u);
    CHECK(callback_) << __func__ << " called after Complete";

    // Only remove the bucket from the database if we didn't skip any client
    // types.
    if (skipped_clients_ == 0 && error_count_ == 0) {
      manager_->DeleteBucketFromDatabase(
          bucket_, commit_immediately_,
          base::BindOnce(&BucketDataDeleter::DidDeleteBucketFromDatabase,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    if (error_count_ == 0) {
      Complete(base::ok(nullptr));
    } else {
      Complete(base::unexpected(QuotaError::kUnknownError));
    }
  }

  void DidDeleteBucketFromDatabase(
      QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Complete(std::move(result));
  }

  void Complete(QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(crbug.com/40058632): Convert back into DCHECKs once issue is
    // resolved.
    CHECK_EQ(remaining_clients_, 0u);
    CHECK(callback_);

    // May delete `this`.
    std::move(callback_).Run(this, std::move(result));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<QuotaManagerImpl> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const BucketLocator bucket_;
  const QuotaClientTypes quota_client_types_;
  // Whether the update to the database should be committed immediately (if not,
  // it will be scheduled to be committed as part of a batch).
  const bool commit_immediately_;
  int error_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  size_t remaining_clients_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int skipped_clients_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Running the callback may delete this instance.
  base::OnceCallback<void(BucketDataDeleter*,
                          QuotaErrorOr<mojom::BucketTableEntryPtr>)>
      callback_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool run_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<BucketDataDeleter> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// Deletes a set of buckets.
class QuotaManagerImpl::BucketSetDataDeleter {
 public:
  // `callback` will be run to report the status of the deletion on task
  // completion. `callback` will only be called while this BucketSetDataDeleter
  // instance is alive. `callback` may destroy this BucketSetDataDeleter
  // instance.
  BucketSetDataDeleter(
      QuotaManagerImpl* manager,
      base::OnceCallback<void(BucketSetDataDeleter*,
                              blink::mojom::QuotaStatusCode)> callback)
      : manager_(manager), callback_(std::move(callback)) {
    DCHECK(manager_);
    DCHECK(callback_);
  }

  ~BucketSetDataDeleter() {
    if (callback_) {
      std::move(callback_).Run(this,
                               blink::mojom::QuotaStatusCode::kErrorAbort);
    }
  }

  base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)>
  GetBucketDeletionCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
    DCHECK(!started_) << __func__ << " already called";
    started_ = true;
#endif  // DCHECK_IS_ON()
    return base::BindOnce(&BucketSetDataDeleter::DidGetBuckets,
                          weak_factory_.GetWeakPtr());
  }

  bool completed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !callback_;
  }

 private:
  void DidGetBuckets(QuotaErrorOr<std::set<BucketInfo>> result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (result.has_value()) {
      buckets_ = BucketInfosToBucketLocators(result.value());
      if (!buckets_.empty()) {
        ScheduleBucketsDeletion();
        return;
      }
    }
    Complete(/*success=*/result.has_value());
  }

  void ScheduleBucketsDeletion() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& bucket : buckets_) {
      // base::Unretained() is safe here because `this` is guaranteed to outlive
      // the callback, thanks to an indirect ownership chain. `this` owns the
      // BucketDataDeleter created here, which guarantees it will only use the
      // callback when it's alive.
      auto bucket_deleter = std::make_unique<BucketDataDeleter>(
          manager_, bucket, AllQuotaClientTypes(), /*commit_immediately=*/false,
          base::BindOnce(&BucketSetDataDeleter::DidDeleteBucketData,
                         base::Unretained(this)));
      auto* bucket_deleter_ptr = bucket_deleter.get();
      bucket_deleters_[bucket_deleter_ptr] = std::move(bucket_deleter);
      bucket_deleter_ptr->Run();
    }
  }

  void DidDeleteBucketData(BucketDataDeleter* deleter,
                           QuotaErrorOr<mojom::BucketTableEntryPtr> entry) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(deleter->completed());

    DCHECK(base::Contains(bucket_deleters_, deleter));
    bucket_deleters_.erase(deleter);

    if (!entry.has_value()) {
      ++error_count_;
    }

    if (bucket_deleters_.empty()) {
      Complete(/*success=*/error_count_ == 0);
    }
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
  std::map<BucketDataDeleter*, std::unique_ptr<BucketDataDeleter>>
      bucket_deleters_;
  std::set<BucketLocator> buckets_ GUARDED_BY_CONTEXT(sequence_checker_);
  int error_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::OnceCallback<void(BucketSetDataDeleter*, blink::mojom::QuotaStatusCode)>
      callback_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool started_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<BucketSetDataDeleter> weak_factory_{this};
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

    base::ConcurrentClosures concurrent;
    for (const auto& client_and_type : manager()->client_types_[type_]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        client->PerformStorageCleanup(type_, concurrent.CreateClosure());
      }
    }
    std::move(concurrent)
        .Done(base::BindOnce(&StorageCleanupHelper::CallCompleted,
                             weak_factory_.GetWeakPtr()));
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
    std::move(callback).Run(std::move(entries_));
  }

 private:
  bool AppendEntry(mojom::BucketTableEntryPtr entry) {
    entries_.push_back(std::move(entry));
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
    get_settings_task_runner_ =
        base::SingleThreadTaskRunner::GetCurrentDefault();
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
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
    return;
  }
  if (!bucket_params.expiration.is_null() &&
      (bucket_params.expiration <= QuotaDatabase::GetNow())) {
    std::move(callback).Run(base::unexpected(QuotaError::kInvalidExpiration));
    return;
  }

  last_opened_bucket_site_ = bucket_params.storage_key;

  // The default bucket skips the quota check.
  if (bucket_params.name == kDefaultBucketName) {
    PostTaskAndReplyWithResultForDBThread(
        base::BindOnce(
            [](const BucketInitParams& params, QuotaDatabase* database) {
              DCHECK(database);
              return database->UpdateOrCreateBucket(params,
                                                    /*max_bucket_count=*/0);
            },
            bucket_params),
        base::BindOnce(&QuotaManagerImpl::DidGetBucketCheckExpiration,
                       weak_factory_.GetWeakPtr(), bucket_params,
                       std::move(callback)));
    return;
  }

  GetQuotaSettings(
      base::BindOnce(&QuotaManagerImpl::DidGetQuotaSettingsForBucketCreation,
                     weak_factory_.GetWeakPtr(), std::move(bucket_params),
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
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(),
                     /*notify_update_bucket=*/true, std::move(callback)));
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
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(), /*notify_update_bucket=*/true,
                     std::move(callback)));
}

void QuotaManagerImpl::GetBucketByNameUnsafe(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(), /*notify_update_bucket=*/false,
                     std::move(callback)));
}

void QuotaManagerImpl::GetBucketById(
    const BucketId& bucket_id,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(), /*notify_update_bucket=*/false,
                     std::move(callback)));
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
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
    bool delete_expired) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
    return;
  }

  base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> reply;
  if (delete_expired) {
    reply = base::BindOnce(&QuotaManagerImpl::DidGetBucketsCheckExpiration,
                           weak_factory_.GetWeakPtr(), std::move(callback));
  } else {
    reply = base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                           weak_factory_.GetWeakPtr(), std::move(callback));
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const StorageKey& storage_key, StorageType type,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsForStorageKey(storage_key, type);
          },
          storage_key, type),
      std::move(reply));
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

void QuotaManagerImpl::GetBucketUsageAndQuota(BucketId id,
                                              UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetBucketById(
      id, base::BindOnce(&QuotaManagerImpl::DidGetBucketForUsageAndQuota,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketSpaceRemaining(
    const BucketLocator& bucket,
    base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // ConcurrentCallbacks is run once with each space restriction --- the
  // StorageKey usage/quota and the bucket's usage/quota (if it exists). The
  // final value is the more restrictive of the two.
  auto aggregator = base::BindOnce(
      [](base::OnceCallback<void(QuotaErrorOr<int64_t>)> final_space_remaining,
         std::vector<int64_t> space_checks) {
        int64_t space_left =
            *std::min_element(space_checks.begin(), space_checks.end());
        if (space_left == std::numeric_limits<int64_t>::min()) {
          std::move(final_space_remaining)
              .Run(base::unexpected(QuotaError::kUnknownError));
        } else {
          std::move(final_space_remaining).Run(space_left);
        }
      },
      std::move(callback));
  base::ConcurrentCallbacks<int64_t> concurrent;

  // Translates a UsageAndQuota result into a single number for the aggregator.
  auto on_got_usage =
      [](base::OnceCallback<void(int64_t)> report_space_remaining,
         blink::mojom::QuotaStatusCode code, int64_t usage, int64_t quota) {
        // Report the amount of allocated space remaining, or min() for an
        // error, or max() if there's no limit.
        int64_t leftover_space = 0;
        if (code != blink::mojom::QuotaStatusCode::kOk) {
          leftover_space = std::numeric_limits<int64_t>::min();
        } else if (quota == 0) {
          leftover_space = kNoLimit;
        } else {
          leftover_space = quota - usage;
        }
        std::move(report_space_remaining).Run(leftover_space);
      };

  // Check the usage for the whole StorageKey.
  GetUsageAndQuota(bucket.storage_key, bucket.type,
                   base::BindOnce(on_got_usage, concurrent.CreateCallback()));

  // If this is the default bucket, we're done. Otherwise, additionally check
  // the usage of the specific bucket against its quota.
  if (!bucket.is_default) {
    GetBucketUsageAndQuota(
        bucket.id, base::BindOnce(on_got_usage, concurrent.CreateCallback()));
  }
  std::move(concurrent).Done(std::move(aggregator));
}

void QuotaManagerImpl::OnClientWriteFailed(const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnFullDiskError(storage_key);
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

  auto result_callback = base::BindOnce(
      [](StatusCallback callback,
         QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
        std::move(callback).Run(result.has_value()
                                    ? blink::mojom::QuotaStatusCode::kOk
                                    : blink::mojom::QuotaStatusCode::kUnknown);
      },
      std::move(callback));
  DeleteBucketDataInternal(bucket, std::move(quota_client_types),
                           std::move(result_callback));
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
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(),
                     /*notify_update_bucket=*/true, std::move(callback)));
}

void QuotaManagerImpl::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
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
                     weak_factory_.GetWeakPtr(),
                     /*notify_update_bucket=*/true, std::move(callback)));
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
  auto buckets_deleter = std::make_unique<BucketSetDataDeleter>(
      this, base::BindOnce(&QuotaManagerImpl::DidDeleteBuckets,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
  auto* buckets_deleter_ptr = buckets_deleter.get();
  bucket_set_data_deleters_[buckets_deleter_ptr] = std::move(buckets_deleter);
  GetBucketsForHost(host, type,
                    buckets_deleter_ptr->GetBucketDeletionCallback());
}

void QuotaManagerImpl::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  DCHECK(client_types_.contains(type));
  if (client_types_[type].empty()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }
  auto buckets_deleter = std::make_unique<BucketSetDataDeleter>(
      this, base::BindOnce(&QuotaManagerImpl::DidDeleteBuckets,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
  auto* buckets_deleter_ptr = buckets_deleter.get();
  bucket_set_data_deleters_[buckets_deleter_ptr] = std::move(buckets_deleter);
  GetBucketsForStorageKey(storage_key, type,
                          buckets_deleter_ptr->GetBucketDeletionCallback());
}

// static
void QuotaManagerImpl::DidDeleteBuckets(
    base::WeakPtr<QuotaManagerImpl> quota_manager,
    StatusCallback callback,
    BucketSetDataDeleter* deleter,
    blink::mojom::QuotaStatusCode status_code) {
  DCHECK(callback);
  DCHECK(deleter);
  DCHECK(deleter->completed());

  if (quota_manager) {
    quota_manager->bucket_set_data_deleters_.erase(deleter);
  }

  std::move(callback).Run(status_code);
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
    blink::mojom::StorageType storage_type,
    GetGlobalUsageForInternalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  UsageTracker* usage_tracker = GetUsageTracker(storage_type);
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

bool QuotaManagerImpl::IsStorageUnlimited(const StorageKey& storage_key,
                                          StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type == StorageType::kTemporary && special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(
             storage_key.origin().GetURL());
}

int64_t QuotaManagerImpl::GetQuotaForStorageKey(
    const StorageKey& storage_key,
    StorageType type,
    const QuotaSettings& settings) const {
  if (IsStorageUnlimited(storage_key, type)) {
    return kNoLimit;
  }

  if (type == StorageType::kSyncable) {
    return kSyncableStorageDefaultStorageKeyQuota;
  }

  if (type == StorageType::kTemporary && special_storage_policy_ &&
      special_storage_policy_->IsStorageSessionOnly(
          storage_key.origin().GetURL())) {
    return settings.session_only_per_storage_key_quota;
  }

  return settings.per_storage_key_quota;
}

void QuotaManagerImpl::GetBucketsModifiedBetween(StorageType type,
                                                 base::Time begin,
                                                 base::Time end,
                                                 GetBucketsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(std::set<BucketLocator>());
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
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool QuotaManagerImpl::ResetUsageTracker(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UsageTracker* previous_usage_tracker = GetUsageTracker(type);
  DCHECK(previous_usage_tracker);
  if (previous_usage_tracker->IsWorking()) {
    return false;
  }

  auto usage_tracker = std::make_unique<UsageTracker>(
      this, client_types_[type], type, special_storage_policy_.get());
  switch (type) {
    case StorageType::kTemporary:
      temporary_usage_tracker_ = std::move(usage_tracker);
      return true;
    case StorageType::kSyncable:
      syncable_usage_tracker_ = std::move(usage_tracker);
      return true;
    default:
      NOTREACHED();
  }
}

QuotaManagerImpl::~QuotaManagerImpl() {
  // Delete this now because otherwise it may call back into `this` after the
  // `sequence_checker_` has been destroyed.
  temporary_storage_evictor_.reset();

  proxy_->InvalidateQuotaManagerImpl(base::PassKey<QuotaManagerImpl>());

  if (database_) {
    db_runner_->DeleteSoon(FROM_HERE, std::move(database_));
  }
}

void QuotaManagerImpl::EnsureDatabaseOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Already initialized.
    return;
  }

  // Use an empty path to open an in-memory only database for incognito.
  database_ = std::make_unique<QuotaDatabase>(is_incognito_ ? base::FilePath()
                                                            : profile_path_);

  database_->SetDbErrorCallback(
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &QuotaManagerImpl::OnDbError, weak_factory_.GetWeakPtr())));

  temporary_usage_tracker_ = std::make_unique<UsageTracker>(
      this, client_types_[StorageType::kTemporary], StorageType::kTemporary,
      special_storage_policy_.get());
  syncable_usage_tracker_ = std::make_unique<UsageTracker>(
      this, client_types_[StorageType::kSyncable], StorageType::kSyncable,
      special_storage_policy_.get());

  if (!is_incognito_) {
    histogram_timer_.Start(FROM_HERE,
                           base::Milliseconds(kReportHistogramInterval), this,
                           &QuotaManagerImpl::ReportHistogram);
  }

  if (bootstrap_disabled_for_testing_) {
    return;
  }

  MaybeBootstrapDatabase();
}

void QuotaManagerImpl::MaybeBootstrapDatabase() {
  is_bootstrapping_database_ = true;
  db_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
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
  if (error == QuotaError::kDatabaseError) {
    // If we got an error during bootstrapping there is no point in
    // trying again. Disable the database instead.
    db_disabled_ = true;
    ReportDatabaseDisabledReason(
        DatabaseDisabledReason::kRegisterStorageKeyFailed);
  }

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
  if (error == QuotaError::kDatabaseError) {
    // If we got an error during bootstrapping there is no point in
    // trying again. Disable the database instead.
    db_disabled_ = true;
    ReportDatabaseDisabledReason(
        DatabaseDisabledReason::kSetIsBootstrappedFailed);
  }

  RunDatabaseCallbacks();
  StartEviction();
}

void QuotaManagerImpl::RunDatabaseCallbacks() {
  for (auto& callback : database_callbacks_) {
    std::move(callback).Run();
  }
  database_callbacks_.clear();
}

void QuotaManagerImpl::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const base::flat_set<blink::mojom::StorageType>& storage_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!database_.get())
      << "All clients must be registered before the database is initialized";

  clients_for_ownership_.emplace_back(std::move(client));
  mojom::QuotaClient* client_ptr = clients_for_ownership_.back().get();

  for (blink::mojom::StorageType storage_type : storage_types) {
    client_types_[storage_type].insert({client_ptr, client_type});
  }
}

UsageTracker* QuotaManagerImpl::GetUsageTracker(StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case StorageType::kTemporary:
      return temporary_usage_tracker_.get();
    case StorageType::kSyncable:
      return syncable_usage_tracker_.get();
    case StorageType::kDeprecatedQuotaNotManaged:
    case StorageType::kDeprecatedPersistent:
    case StorageType::kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

void QuotaManagerImpl::NotifyBucketAccessed(const BucketLocator& bucket,
                                            base::Time access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (is_getting_eviction_bucket_) {
    // Record the accessed buckets while GetLRUStorageKey task is running
    // to filter out them from eviction.
    access_notified_buckets_.insert(bucket);
  }

  if (db_disabled_) {
    return;
  }
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketLocator bucket, base::Time accessed_time,
             QuotaDatabase* database) {
            DCHECK(database);
            if (bucket.is_default) {
              return database->SetStorageKeyLastAccessTime(
                  bucket.storage_key, bucket.type, accessed_time);
            } else {
              return database->SetBucketLastAccessTime(bucket.id,
                                                       accessed_time);
            }
          },
          bucket, access_time),
      base::DoNothing());
}

void QuotaManagerImpl::NotifyBucketModified(QuotaClientType client_id,
                                            const BucketLocator& bucket,
                                            std::optional<int64_t> delta,
                                            base::Time modification_time,
                                            base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  GetUsageTracker(bucket.type)
      ->UpdateBucketUsageCache(client_id, bucket, delta);
  // Return once usage cache is updated for callers waiting for quota changes to
  // be reflected before querying for usage.
  std::move(callback).Run();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](BucketLocator bucket, base::Time modification_time,
             QuotaDatabase* database) {
            DCHECK(database);
            BucketId id = bucket.id;
            if (!id) {
              CHECK(bucket.is_default);
              QuotaErrorOr<BucketInfo> result = database->GetBucket(
                  bucket.storage_key, kDefaultBucketName, bucket.type);
              if (!result.has_value()) {
                return QuotaError::kNotFound;
              }

              id = result->id;
            }
            return database->SetBucketLastModifiedTime(id, modification_time);
          },
          bucket, modification_time),
      base::DoNothing());
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
    BucketTableEntries entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ConcurrentCallbacks<mojom::BucketTableEntryPtr> concurrent;

  for (auto& entry : entries) {
    StorageType type = static_cast<StorageType>(entry->type);
    // TODO(crbug.com/40167820): Change to DCHECK once persistent type is
    // removed from QuotaDatabase.
    if (!IsSupportedType(type)) {
      continue;
    }

    std::optional<StorageKey> storage_key =
        StorageKey::Deserialize(entry->storage_key);
    // If the serialization format changes keys may not deserialize.
    if (!storage_key) {
      continue;
    }

    BucketId bucket_id = BucketId(entry->bucket_id);

    BucketLocator bucket_locator =
        BucketLocator(bucket_id, std::move(storage_key).value(), type,
                      entry->name == kDefaultBucketName);

    GetBucketUsageWithBreakdown(
        bucket_locator,
        base::BindOnce(&QuotaManagerImpl::AddBucketTableEntry,
                       weak_factory_.GetWeakPtr(), std::move(entry),
                       concurrent.CreateCallback()));
  }
  std::move(concurrent).Done(std::move(callback));
}

void QuotaManagerImpl::AddBucketTableEntry(
    mojom::BucketTableEntryPtr entry,
    base::OnceCallback<void(mojom::BucketTableEntryPtr)> callback,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr bucket_usage_breakdown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  entry->usage = usage;

  std::move(callback).Run(std::move(entry));
}

void QuotaManagerImpl::OnDbError(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::UmaHistogramSqliteResult("Quota.QuotaDatabaseError", error_code);

  // Start the storage eviction routine on a full disk error.
  if (static_cast<sql::SqliteErrorCode>(error_code) ==
      sql::SqliteErrorCode::kFullDisk) {
    OnFullDiskError(std::nullopt);
    return;
  }

  if (!sql::IsErrorCatastrophic(error_code)) {
    return;
  }

  // Db will be set to disabled after a bootstrapping failure.
  if (db_disabled_) {
    return;
  }

  // Ignore any errors that happen while a new bootstrap attempt is already in
  // progress or queued.
  if (is_bootstrapping_database_) {
    return;
  }

  if (bootstrap_disabled_for_testing_) {
    db_disabled_ = true;
    return;
  }

  // Start another bootstrapping process. Pause eviction while bootstrapping
  // is in progress. When bootstrapping finishes a new Evictor will be
  // created.
  is_bootstrapping_database_ = true;
  temporary_storage_evictor_ = nullptr;

  // Wipe the database before triggering another bootstrap.
  db_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&QuotaDatabase::RecoverOrRaze,
                     base::Unretained(database_.get()), error_code),
      base::BindOnce(&QuotaManagerImpl::DidRecoverOrRazeForReBootstrap,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::OnFullDiskError(std::optional<StorageKey> storage_key) {
  if ((base::TimeTicks::Now() - last_full_disk_eviction_time_) >
      base::Minutes(15)) {
    last_full_disk_eviction_time_ = base::TimeTicks::Now();
    StartEviction();
  }

  // We may already be evicting, either due to the above or just by chance. In
  // either case, do nothing more for now.
  if (temporary_storage_evictor_ && temporary_storage_evictor_->in_round()) {
    return;
  }

  if (storage_key) {
    NotifyWriteFailed(*storage_key);
  } else if (last_opened_bucket_site_) {
    NotifyWriteFailed(*last_opened_bucket_site_);
  }
}

void QuotaManagerImpl::NotifyWriteFailed(const blink::StorageKey& storage_key) {
  auto [time_of_last_stats, total_space, available_space] =
      cached_disk_stats_for_storage_pressure_;
  auto age_of_disk_stats = base::TimeTicks::Now() - time_of_last_stats;

  // Avoid polling for free disk space if disk stats have been recently
  // queried.
  if (age_of_disk_stats < kStoragePressureCheckDiskStatsInterval) {
    MaybeRunStoragePressureCallback(storage_key, total_space, available_space);
    return;
  }

  GetStorageCapacity(
      base::BindOnce(&QuotaManagerImpl::MaybeRunStoragePressureCallback,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void QuotaManagerImpl::StartEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (eviction_disabled_) {
    return;
  }
  if (!temporary_storage_evictor_) {
    temporary_storage_evictor_ = std::make_unique<QuotaTemporaryStorageEvictor>(
        this, kEvictionIntervalInMilliSeconds);
  }
  temporary_storage_evictor_->Start();
}

void QuotaManagerImpl::DeleteBucketFromDatabase(
    const BucketLocator& bucket,
    bool commit_immediately,
    base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketLocator& bucket, bool commit_immediately,
             QuotaDatabase* database) {
            DCHECK(database);
            auto result = database->DeleteBucketData(bucket);
            if (commit_immediately && result.has_value()) {
              database->CommitNow();
            }

            return result;
          },
          bucket, commit_immediately),
      base::BindOnce(&QuotaManagerImpl::OnBucketDeleted,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::DidEvictBucketData(
    BucketId evicted_bucket_id,
    base::RepeatingCallback<void(bool)> barrier,
    QuotaErrorOr<mojom::BucketTableEntryPtr> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());

  if (entry.has_value()) {
    DCHECK(entry.value());
    base::Time now = QuotaDatabase::GetNow();
    base::UmaHistogramCounts1M(
        QuotaManagerImpl::kEvictedBucketAccessedCountHistogram,
        entry.value()->use_count);
    base::UmaHistogramCounts1000(
        QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram,
        (now - entry.value()->last_accessed).InDays());
    barrier.Run(true);
  } else {
    // We only try to evict buckets that are not in use, so basically deletion
    // attempt for eviction should not fail.  Let's record the bucket if we get
    // an error and exclude it from future eviction if the error happens
    // consistently (> kThresholdOfErrorsToBeDenylisted).
    buckets_in_error_[evicted_bucket_id]++;
    barrier.Run(false);
  }
}

void QuotaManagerImpl::DeleteBucketDataInternal(
    const BucketLocator& bucket,
    QuotaClientTypes quota_client_types,
    base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseDisabled));
    return;
  }
  auto bucket_deleter = std::make_unique<BucketDataDeleter>(
      this, bucket, std::move(quota_client_types), /*commit_immediately=*/true,
      base::BindOnce(&QuotaManagerImpl::DidDeleteBucketData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  auto* bucket_deleter_ptr = bucket_deleter.get();
  bucket_data_deleters_[bucket_deleter_ptr] = std::move(bucket_deleter);
  bucket_deleter_ptr->Run();
}

// static
void QuotaManagerImpl::DidDeleteBucketData(
    base::WeakPtr<QuotaManagerImpl> quota_manager,
    base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)> callback,
    BucketDataDeleter* deleter,
    QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
  DCHECK(callback);
  DCHECK(deleter);
  DCHECK(deleter->completed());

  if (quota_manager) {
    quota_manager->bucket_data_deleters_.erase(deleter);
  }

  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidDeleteBucketForRecreation(
    const BucketInitParams& params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    BucketInfo bucket_info,
    QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
  if (result.has_value()) {
    UpdateOrCreateBucket(params, std::move(callback));
  } else {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseError));
  }
}

void QuotaManagerImpl::MaybeRunStoragePressureCallback(
    const StorageKey& storage_key,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(total_space, 0);
  DCHECK_GE(available_space, 0);

  // TODO(crbug.com/40121667): Figure out what 0 total_space means
  // and how to handle the storage pressure callback in these cases.
  if (total_space == 0) {
    return;
  }

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
  const StorageKey key = StorageKey::CreateFirstParty(origin_url);
  // In Incognito, since no data is stored on disk, storage pressure should be
  // ignored.
  DCHECK_EQ(is_incognito_, storage_pressure_callback_.is_null());

  if (storage_pressure_callback_.is_null()) {
    return;
  }

  storage_pressure_callback_.Run(key);
}

void QuotaManagerImpl::IsSimulateStoragePressureAvailable(
    IsSimulateStoragePressureAvailableCallback callback) {
  // We assume this is only the case in incognito. If it changes, update this.
  DCHECK_EQ(is_incognito_, storage_pressure_callback_.is_null());

  std::move(callback).Run(!storage_pressure_callback_.is_null());
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
    base::RepeatingCallback<void(const StorageKey&)>
        storage_pressure_callback) {
  storage_pressure_callback_ = storage_pressure_callback;
  if (storage_key_for_pending_storage_pressure_callback_.has_value()) {
    storage_pressure_callback_.Run(
        std::move(storage_key_for_pending_storage_pressure_callback_.value()));
    storage_key_for_pending_storage_pressure_callback_ = std::nullopt;
  }
}

int QuotaManagerImpl::GetOverrideHandleId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++next_override_handle_id_;
}

void QuotaManagerImpl::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    std::optional<int64_t> quota_size) {
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

std::optional<int64_t> QuotaManagerImpl::GetQuotaOverrideForStorageKey(
    const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::Contains(devtools_overrides_, storage_key)) {
    return std::nullopt;
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

  // We DumpBucketTable last to ensure the trackers caches are loaded.
  DumpBucketTable(
      base::BindOnce(&QuotaManagerImpl::DidDumpBucketTableForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidDumpBucketTableForHistogram(
    BucketTableEntries entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramCounts100000("Quota.TotalBucketCount", entries.size());

  std::map<StorageKey, int64_t> usage_map =
      GetUsageTracker(StorageType::kTemporary)->GetCachedStorageKeysUsage();
  base::Time now = QuotaDatabase::GetNow();
  for (const auto& info : entries) {
    if (info->type != blink::mojom::StorageType::kTemporary) {
      continue;
    }

    std::optional<StorageKey> storage_key =
        StorageKey::Deserialize(info->storage_key);
    if (!storage_key.has_value()) {
      continue;
    }
    auto it = usage_map.find(*storage_key);
    if (it == usage_map.end() || it->second == 0) {
      continue;
    }

    base::TimeDelta age =
        now - std::max(info->last_accessed, info->last_modified);
    base::UmaHistogramCounts1000("Quota.AgeOfOriginInDays", age.InDays());

    int64_t kilobytes = std::max(it->second / int64_t{1024}, int64_t{1});
    base::Histogram::FactoryGet("Quota.AgeOfDataInDays", 1, 1000, 50,
                                base::HistogramBase::kUmaTargetedHistogramFlag)
        ->AddCount(age.InDays(), base::saturated_cast<int>(kilobytes));
  }
}

std::set<BucketId> QuotaManagerImpl::GetEvictionBucketExceptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<BucketId> exceptions;
  for (const auto& p : buckets_in_error_) {
    if (p.second > QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted) {
      exceptions.insert(p.first);
    }
  }

  return exceptions;
}

void QuotaManagerImpl::DidGetEvictionBuckets(
    GetBucketsCallback callback,
    const std::set<BucketLocator>& buckets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  // Filter out buckets that were accessed while getting eviction buckets.
  auto bucket_wasnt_accessed =
      [this](const BucketLocator& to_be_evicted_bucket) {
        return !std::count_if(
            access_notified_buckets_.begin(), access_notified_buckets_.end(),
            [&to_be_evicted_bucket](const BucketLocator& accessed_bucket) {
              return to_be_evicted_bucket.IsEquivalentTo(accessed_bucket);
            });
      };

  std::set<BucketLocator> bucket_copies;
  std::copy_if(buckets.begin(), buckets.end(),
               std::inserter(bucket_copies, bucket_copies.end()),
               bucket_wasnt_accessed);
  std::move(callback).Run(bucket_copies);
  access_notified_buckets_.clear();
  is_getting_eviction_bucket_ = false;
}

void QuotaManagerImpl::GetEvictionBuckets(int64_t target_usage,
                                          GetBucketsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  // This must not be called while there's an in-flight task.
  DCHECK(!is_getting_eviction_bucket_);
  is_getting_eviction_bucket_ = true;

  // The usage map should have been cached recently due to
  // `GetEvictionRoundInfo()`.
  std::map<BucketLocator, int64_t> usage_map =
      GetUsageTracker(StorageType::kTemporary)->GetCachedBucketsUsage();

  GetBucketsForEvictionFromDatabase(
      target_usage, std::move(usage_map),
      base::BindOnce(&QuotaManagerImpl::DidGetEvictionBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::EvictExpiredBuckets(StatusCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(done).Run(blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  auto buckets_deleter = std::make_unique<BucketSetDataDeleter>(
      this, base::BindOnce(&QuotaManagerImpl::DidDeleteBuckets,
                           weak_factory_.GetWeakPtr(), std::move(done)));
  auto* buckets_deleter_ptr = buckets_deleter.get();
  bucket_set_data_deleters_[buckets_deleter_ptr] = std::move(buckets_deleter);

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](SpecialStoragePolicy* policy, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetExpiredBuckets(policy);
          },
          base::RetainedRef(special_storage_policy_)),
      buckets_deleter_ptr->GetBucketDeletionCallback());
}

void QuotaManagerImpl::EvictBucketData(
    const std::set<BucketLocator>& buckets,
    base::OnceCallback<void(int)> on_eviction_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());

  auto barrier = base::BarrierCallback<bool>(
      buckets.size(), base::BindOnce(
                          [](base::OnceCallback<void(int)> on_eviction_done,
                             std::vector<bool> results) {
                            const int evicted_count = std::count(
                                results.begin(), results.end(), true);
                            std::move(on_eviction_done).Run(evicted_count);
                          },
                          std::move(on_eviction_done)));

  for (const auto& bucket : buckets) {
    DeleteBucketDataInternal(
        bucket, AllQuotaClientTypes(),
        base::BindOnce(&QuotaManagerImpl::DidEvictBucketData,
                       weak_factory_.GetWeakPtr(), bucket.id, barrier));
  }
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

void QuotaManagerImpl::GetBucketsForEvictionFromDatabase(
    int64_t target_usage,
    std::map<BucketLocator, int64_t> usage_map,
    GetBucketsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  EnsureDatabaseOpened();

  if (db_disabled_) {
    std::move(callback).Run({});
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](int64_t target_usage, std::map<BucketLocator, int64_t> usage_map,
             const std::set<BucketId>& bucket_exceptions,
             SpecialStoragePolicy* policy, QuotaDatabase* database) {
            DCHECK(database);
            return database->GetBucketsForEviction(
                blink::mojom::StorageType::kTemporary, target_usage, usage_map,
                bucket_exceptions, policy);
          },
          target_usage, std::move(usage_map), GetEvictionBucketExceptions(),
          base::RetainedRef(special_storage_policy_)),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketsForEvictionFromDatabase,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::DidGetBucketsForEvictionFromDatabase(
    GetBucketsCallback callback,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    std::move(callback).Run(result.value());
  } else {
    std::move(callback).Run({});
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

  if (!settings_callbacks_.Add(std::move(callback))) {
    return;
  }

  // We invoke our clients GetQuotaSettingsFunc on the
  // UI thread and plumb the resulting value back to this thread.
  get_settings_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_settings_function_,
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::BindOnce(&QuotaManagerImpl::DidGetSettings,
                                            weak_factory_.GetWeakPtr()))));
}

void QuotaManagerImpl::DidGetSettings(std::optional<QuotaSettings> settings) {
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

  if (!storage_capacity_callbacks_.Add(std::move(callback))) {
    return;
  }
  if (is_incognito_) {
    GetQuotaSettings(
        base::BindOnce(&QuotaManagerImpl::ContinueIncognitoGetStorageCapacity,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  db_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&QuotaManagerImpl::CallGetVolumeInfo, get_volume_info_fn_,
                     profile_path_),
      base::BindOnce(&QuotaManagerImpl::DidGetStorageCapacity,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::ContinueIncognitoGetStorageCapacity(
    const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* temporary_usage_tracker = GetUsageTracker(StorageType::kTemporary);
  int64_t temporary_usage = temporary_usage_tracker == nullptr
                                ? 0
                                : temporary_usage_tracker->GetCachedUsage();
  DCHECK_GE(temporary_usage, -1);

  int64_t available_space =
      std::max(int64_t{0}, settings.pool_size - temporary_usage);
  DidGetStorageCapacity(QuotaAvailability(settings.pool_size, available_space));
}

void QuotaManagerImpl::DidGetStorageCapacity(
    const QuotaAvailability& quota_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t total_space = quota_usage.total;
  DCHECK_GE(total_space, 0);

  int64_t available_space = quota_usage.available;
  DCHECK_GE(available_space, 0);

  cached_disk_stats_for_storage_pressure_ =
      std::make_tuple(base::TimeTicks::Now(), total_space, available_space);
  storage_capacity_callbacks_.Run(total_space, available_space);
  DetermineStoragePressure(total_space, available_space);
}

void QuotaManagerImpl::DidRecoverOrRazeForReBootstrap(bool success) {
  if (success) {
    MaybeBootstrapDatabase();
    return;
  }

  // Deleting the database failed. Disable the database and hope we'll recover
  // after Chrome restarts instead.
  db_disabled_ = true;
  ReportDatabaseDisabledReason(DatabaseDisabledReason::kRazeFailed);
  is_bootstrapping_database_ = false;
  RunDatabaseCallbacks();
  // No reason to restart eviction here. Without a working database there is
  // nothing to evict.
}

void QuotaManagerImpl::NotifyUpdatedBucket(
    const QuotaErrorOr<BucketInfo>& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    return;
  }
  for (auto& observer : observers_) {
    observer->OnCreateOrUpdateBucket(result.value());
  }
}

void QuotaManagerImpl::OnBucketDeleted(
    base::OnceCallback<void(QuotaErrorOr<mojom::BucketTableEntryPtr>)> callback,
    QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.has_value()) {
    const mojom::BucketTableEntryPtr& entry = result.value();
    StorageType type = static_cast<StorageType>(entry->type);
    if (IsSupportedType(type)) {
      auto storage_key = blink::StorageKey::Deserialize(entry->storage_key);
      if (storage_key) {
        storage::BucketLocator bucket_locator(
            BucketId(entry->bucket_id), storage_key.value(), type,
            entry->name == kDefaultBucketName);
        for (auto& observer : observers_) {
          observer->OnDeleteBucket(bucket_locator);
        }
      }
    }
  }
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetQuotaSettingsForBucketCreation(
    const BucketInitParams& bucket_params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    const QuotaSettings& settings) {
  const int64_t quota = GetQuotaForStorageKey(
      bucket_params.storage_key, StorageType::kTemporary, settings);
  int64_t max_buckets = (quota == kNoLimit) ? 0 : (quota / kTypicalBucketUsage);
  DCHECK_EQ(max_buckets == 0, IsStorageUnlimited(bucket_params.storage_key,
                                                 StorageType::kTemporary));

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(
          [](const BucketInitParams& params, int max_buckets,
             QuotaDatabase* database) {
            DCHECK(database);
            return database->UpdateOrCreateBucket(params, max_buckets);
          },
          bucket_params, max_buckets),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketCheckExpiration,
                     weak_factory_.GetWeakPtr(), bucket_params,
                     std::move(callback)));
}

void QuotaManagerImpl::DidGetBucket(
    bool notify_update_bucket,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (notify_update_bucket) {
    NotifyUpdatedBucket(result);
  }

  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketCheckExpiration(
    const BucketInitParams& params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (result.has_value() && !result->expiration.is_null() &&
      result->expiration <= QuotaDatabase::GetNow()) {
    DeleteBucketDataInternal(
        result->ToBucketLocator(), AllQuotaClientTypes(),
        base::BindOnce(&QuotaManagerImpl::DidDeleteBucketForRecreation,
                       weak_factory_.GetWeakPtr(), params, std::move(callback),
                       result.value()));
    return;
  }

  NotifyUpdatedBucket(result);
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketForDeletion(
    StatusCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (!result.has_value()) {
    // Return QuotaStatusCode::kOk if bucket not found. No work needed.
    std::move(callback).Run(result.error() == QuotaError::kNotFound
                                ? blink::mojom::QuotaStatusCode::kOk
                                : blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  auto result_callback = base::BindOnce(
      [](StatusCallback callback,
         QuotaErrorOr<mojom::BucketTableEntryPtr> result) {
        std::move(callback).Run(result.has_value()
                                    ? blink::mojom::QuotaStatusCode::kOk
                                    : blink::mojom::QuotaStatusCode::kUnknown);
      },
      std::move(callback));
  DeleteBucketDataInternal(result->ToBucketLocator(), AllQuotaClientTypes(),
                           std::move(result_callback));
  return;
}

void QuotaManagerImpl::DidGetBucketForUsageAndQuota(
    UsageAndQuotaCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  if (!result.has_value()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kUnknown, 0, 0);
    return;
  }

  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, result.value(), is_incognito_,
      base::BindOnce(&DidGetUsageAndQuotaStripOverride,
                     base::BindOnce(&DidGetUsageAndQuotaStripBreakdown,
                                    std::move(callback))));
  helper->Start();
}

void QuotaManagerImpl::DidGetStorageKeys(
    GetStorageKeysCallback callback,
    QuotaErrorOr<std::set<StorageKey>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(result.value_or(std::set<StorageKey>()));
}

void QuotaManagerImpl::DidGetBuckets(
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
    QuotaErrorOr<std::set<BucketInfo>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketsCheckExpiration(
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
    QuotaErrorOr<std::set<BucketInfo>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (!result.has_value()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  std::set<BucketInfo> kept_buckets;
  std::set<BucketInfo> buckets_to_delete;
  for (const BucketInfo& bucket : result.value()) {
    if (!bucket.expiration.is_null() &&
        bucket.expiration <= QuotaDatabase::GetNow()) {
      buckets_to_delete.insert(bucket);
    } else {
      kept_buckets.insert(bucket);
    }
  }

  if (buckets_to_delete.empty()) {
    std::move(callback).Run(kept_buckets);
    return;
  }

  base::RepeatingClosure barrier =
      base::BarrierClosure(buckets_to_delete.size(),
                           base::BindOnce(std::move(callback), kept_buckets));
  for (const BucketInfo& bucket : buckets_to_delete) {
    DeleteBucketDataInternal(
        bucket.ToBucketLocator(), AllQuotaClientTypes(),
        base::IgnoreArgs<QuotaErrorOr<mojom::BucketTableEntryPtr>>(barrier));
  }
}

void QuotaManagerImpl::DidGetModifiedBetween(
    GetBucketsCallback callback,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(result.value_or(std::set<BucketLocator>()));
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

  db_runner_->PostTaskAndReplyWithResult(
      from_here,
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

  db_runner_->PostTaskAndReplyWithResult(
      from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

// static
QuotaAvailability QuotaManagerImpl::CallGetVolumeInfo(
    GetVolumeInfoFn get_volume_info_fn,
    const base::FilePath& path) {
  if (!base::CreateDirectory(path)) {
    LOG(WARNING) << "Create directory failed for path" << path.value();
    return QuotaAvailability(0, 0);
  }

  const QuotaAvailability quotaAvailability = get_volume_info_fn(path);
  const auto total = quotaAvailability.total;
  const auto available = quotaAvailability.available;

  if (total < 0 || available < 0) {
    LOG(WARNING) << "Unable to get volume info: " << path.value();
    return QuotaAvailability(0, 0);
  }
  DCHECK_GE(total, 0);
  DCHECK_GE(available, 0);

  UMA_HISTOGRAM_MBYTES("Quota.TotalDiskSpace", total);
  UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace", available);
  if (total > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Quota.PercentDiskAvailable",
        std::min(100, static_cast<int>((available * 100) / total)));
  }
  return QuotaAvailability(total, available);
}

// static
QuotaAvailability QuotaManagerImpl::GetVolumeInfo(const base::FilePath& path) {
  return QuotaAvailability(base::SysInfo::AmountOfTotalDiskSpace(path),
                           base::SysInfo::AmountOfFreeDiskSpace(path));
}

void QuotaManagerImpl::AddObserver(
    mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer) {
  // `MojoQuotaManagerObserver` is self owned and deletes itself when its remote
  // is disconnected.
  observers_.Add(std::move(observer));
}
}  // namespace storage

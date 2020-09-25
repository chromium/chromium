// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_macros.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/quota/usage_tracker.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

using blink::mojom::StorageType;

namespace storage {

namespace {

constexpr int64_t kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour

// Take action on write errors if there is <= 2% disk space
// available.
constexpr double kStoragePressureThresholdRatio = 0.02;

// Limit how frequently QuotaManager polls for free disk space when
// only using that information to identify storage pressure.
constexpr base::TimeDelta kStoragePressureCheckDiskStatsInterval =
    base::TimeDelta::FromMinutes(5);

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
int64_t QuotaManager::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

namespace {

bool IsSupportedType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent ||
         type == StorageType::kSyncable;
}

bool IsSupportedIncognitoType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent;
}

bool GetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  database->GetHostQuota(host, StorageType::kPersistent, quota);
  return true;
}

bool SetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* new_quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  if (database->SetHostQuota(host, StorageType::kPersistent, *new_quota))
    return true;
  *new_quota = 0;
  return false;
}

bool GetLRUOriginOnDBThread(StorageType type,
                            const std::set<url::Origin>& exceptions,
                            SpecialStoragePolicy* policy,
                            base::Optional<url::Origin>* origin,
                            QuotaDatabase* database) {
  DCHECK(database);
  database->GetLRUOrigin(type, exceptions, policy, origin);
  return true;
}

bool DeleteOriginInfoOnDBThread(const url::Origin& origin,
                                StorageType type,
                                bool is_eviction,
                                QuotaDatabase* database) {
  DCHECK(database);

  base::Time now = base::Time::Now();

  if (is_eviction) {
    QuotaDatabase::OriginInfoTableEntry entry;
    database->GetOriginInfo(origin, type, &entry);
    UMA_HISTOGRAM_COUNTS_1M(QuotaManager::kEvictedOriginAccessedCountHistogram,
                            entry.used_count);
    UMA_HISTOGRAM_COUNTS_1000(
        QuotaManager::kEvictedOriginDaysSinceAccessHistogram,
        (now - entry.last_access_time).InDays());
  }

  if (!database->DeleteOriginInfo(origin, type))
    return false;

  // If the deletion is not due to an eviction, delete the entry in the eviction
  // table as well due to privacy concerns.
  if (!is_eviction)
    return database->DeleteOriginLastEvictionTime(origin, type);

  base::Time last_eviction_time;
  database->GetOriginLastEvictionTime(origin, type, &last_eviction_time);

  if (last_eviction_time != base::Time()) {
    UMA_HISTOGRAM_COUNTS_1000(
        QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram,
        (now - last_eviction_time).InDays());
  }

  return database->SetOriginLastEvictionTime(origin, type, now);
}

bool BootstrapDatabaseOnDBThread(std::set<url::Origin> origins,
                                 QuotaDatabase* database) {
  DCHECK(database);
  if (database->IsOriginDatabaseBootstrapped())
    return true;

  // Register existing origins with 0 last time access.
  if (database->RegisterInitialOriginInfo(origins, StorageType::kTemporary)) {
    database->SetOriginDatabaseBootstrapped(true);
    return true;
  }
  return false;
}

bool UpdateAccessTimeOnDBThread(const url::Origin& origin,
                                StorageType type,
                                base::Time accessed_time,
                                QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastAccessTime(origin, type, accessed_time);
}

bool UpdateModifiedTimeOnDBThread(const url::Origin& origin,
                                  StorageType type,
                                  base::Time modified_time,
                                  QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastModifiedTime(origin, type, modified_time);
}

void DidGetUsageAndQuotaStripBreakdown(
    QuotaManager::UsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(status, usage, quota);
}

}  // namespace

constexpr int64_t QuotaManager::kGBytes;
constexpr int64_t QuotaManager::kNoLimit;
constexpr int64_t QuotaManager::kPerHostPersistentQuotaLimit;
constexpr int QuotaManager::kEvictionIntervalInMilliSeconds;
constexpr int QuotaManager::kThresholdOfErrorsToBeDenylisted;
constexpr int QuotaManager::kThresholdRandomizationPercent;
constexpr char QuotaManager::kDatabaseName[];
constexpr char QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram[];
constexpr char QuotaManager::kEvictedOriginAccessedCountHistogram[];
constexpr char QuotaManager::kEvictedOriginDaysSinceAccessHistogram[];

class QuotaManager::UsageAndQuotaInfoGatherer : public QuotaTask {
 public:
  UsageAndQuotaInfoGatherer(QuotaManager* manager,
                            const url::Origin& origin,
                            StorageType type,
                            bool is_unlimited,
                            bool is_session_only,
                            bool is_incognito,
                            UsageAndQuotaWithBreakdownCallback callback)
      : QuotaTask(manager),
        origin_(origin),
        callback_(std::move(callback)),
        type_(type),
        is_unlimited_(is_unlimited),
        is_session_only_(is_session_only),
        is_incognito_(is_incognito) {}

 protected:
  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Start the async process of gathering the info we need.
    // Gather 4 pieces of info before computing an answer:
    // settings, device_storage_capacity, host_usage, and host_quota.
    base::RepeatingClosure barrier = base::BarrierClosure(
        4, base::BindOnce(&UsageAndQuotaInfoGatherer::OnBarrierComplete,
                          weak_factory_.GetWeakPtr()));

    const std::string& host = origin_.host();

    manager()->GetQuotaSettings(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));
    manager()->GetStorageCapacity(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotCapacity,
                       weak_factory_.GetWeakPtr(), barrier));
    manager()->GetHostUsageWithBreakdown(
        host, type_,
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotHostUsage,
                       weak_factory_.GetWeakPtr(), barrier));

    // Determine host_quota differently depending on type.
    if (is_unlimited_) {
      SetDesiredHostQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                          kNoLimit);
    } else if (type_ == StorageType::kSyncable) {
      SetDesiredHostQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                          kSyncableStorageDefaultHostQuota);
    } else if (type_ == StorageType::kPersistent) {
      manager()->GetPersistentHostQuota(
          host, base::BindOnce(&UsageAndQuotaInfoGatherer::SetDesiredHostQuota,
                               weak_factory_.GetWeakPtr(), barrier));
    } else {
      DCHECK_EQ(StorageType::kTemporary, type_);
      // For temporary storage,  OnGotSettings will set the host quota.
    }
  }

  void Aborted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(
        blink::mojom::QuotaStatusCode::kErrorAbort, /*status*/
        0,                                          /*usage*/
        0,                                          /*quota*/
        nullptr);                                   /*usage_breakdown*/
    DeleteSoon();
  }

  void Completed() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();

    int64_t host_quota = desired_host_quota_;
    int64_t temp_pool_free_space =
        std::max(static_cast<int64_t>(0),
                 available_space_ - settings_.must_remain_available);

    // Constrain the desired |host_quota| to something that fits.
    if (host_quota > temp_pool_free_space) {
      if (is_unlimited_) {
        host_quota = available_space_ + host_usage_;
      }
    }

    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, host_usage_,
                             host_quota, std::move(host_usage_breakdown_));
    if (type_ == StorageType::kTemporary && !is_incognito_ &&
        !is_unlimited_) {
      UMA_HISTOGRAM_MBYTES("Quota.QuotaForOrigin", host_quota);
      UMA_HISTOGRAM_MBYTES("Quota.UsageByOrigin", host_usage_);
      if (host_quota > 0) {
        UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedByOrigin",
            std::min(100, static_cast<int>((host_usage_ * 100) / host_quota)));
      }
    }
    DeleteSoon();
  }

 private:
  QuotaManager* manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<QuotaManager*>(observer());
  }

  void OnGotSettings(base::RepeatingClosure barrier_closure,
                     const QuotaSettings& settings) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    settings_ = settings;
    barrier_closure.Run();
    if (type_ == StorageType::kTemporary && !is_unlimited_) {
      int64_t host_quota = is_session_only_
                               ? settings.session_only_per_host_quota
                               : settings.per_host_quota;
      SetDesiredHostQuota(barrier_closure, blink::mojom::QuotaStatusCode::kOk,
                          host_quota);
    }
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    total_space_ = total_space;
    available_space_ = available_space;
    std::move(barrier_closure).Run();
  }

  void OnGotHostUsage(base::OnceClosure barrier_closure,
                      int64_t usage,
                      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    host_usage_ = usage;
    host_usage_breakdown_ = std::move(usage_breakdown);
    std::move(barrier_closure).Run();
  }

  void SetDesiredHostQuota(base::OnceClosure barrier_closure,
                           blink::mojom::QuotaStatusCode status,
                           int64_t quota) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    desired_host_quota_ = quota;
    std::move(barrier_closure).Run();
  }

  void OnBarrierComplete() { CallCompleted(); }

  const url::Origin origin_;
  QuotaManager::UsageAndQuotaWithBreakdownCallback callback_;
  const StorageType type_;
  const bool is_unlimited_;
  const bool is_session_only_;
  const bool is_incognito_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t desired_host_quota_ = 0;
  int64_t host_usage_ = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_;
  QuotaSettings settings_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers are used to support cancelling work.
  base::WeakPtrFactory<UsageAndQuotaInfoGatherer> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaInfoGatherer);
};

class QuotaManager::EvictionRoundInfoHelper : public QuotaTask {
 public:
  EvictionRoundInfoHelper(QuotaManager* manager,
                          EvictionRoundInfoCallback callback)
      : QuotaTask(manager), callback_(std::move(callback)) {}

 protected:
  void Run() override {
    // Gather 2 pieces of info before deciding if we need to get GlobalUsage:
    // settings and device_storage_capacity.
    base::RepeatingClosure barrier = base::BarrierClosure(
        2, base::BindOnce(&EvictionRoundInfoHelper::OnBarrierComplete,
                          weak_factory_.GetWeakPtr()));

    manager()->GetQuotaSettings(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));
    manager()->GetStorageCapacity(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotCapacity,
                       weak_factory_.GetWeakPtr(), barrier));
  }

  void Aborted() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort,
                             QuotaSettings(), 0, 0, 0, false);
    DeleteSoon();
  }

  void Completed() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, settings_,
                             available_space_, total_space_, global_usage_,
                             global_usage_is_complete_);
    DeleteSoon();
  }

 private:
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  void OnGotSettings(base::OnceClosure barrier_closure,
                     const QuotaSettings& settings) {
    settings_ = settings;
    std::move(barrier_closure).Run();
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
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
          manager()->GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
      CallCompleted();
      return;
    }
    manager()->GetGlobalUsage(
        StorageType::kTemporary,
        base::BindOnce(&EvictionRoundInfoHelper::OnGotGlobalUsage,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGotGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    global_usage_ = std::max(INT64_C(0), usage - unlimited_usage);
    global_usage_is_complete_ = true;
    CallCompleted();
  }

  EvictionRoundInfoCallback callback_;
  QuotaSettings settings_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t global_usage_ = 0;
  bool global_usage_is_complete_ = false;
  base::WeakPtrFactory<EvictionRoundInfoHelper> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(EvictionRoundInfoHelper);
};

class QuotaManager::GetUsageInfoTask : public QuotaTask {
 public:
  GetUsageInfoTask(QuotaManager* manager, GetUsageInfoCallback callback)
      : QuotaTask(manager), callback_(std::move(callback)) {}

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

  void Aborted() override {
    std::move(callback_).Run(UsageInfoEntries());
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker* tracker) {
    std::map<std::string, int64_t> host_usage = tracker->GetCachedHostsUsage();
    for (const auto& host_usage_pair : host_usage) {
      entries_.emplace_back(host_usage_pair.first, type,
                            host_usage_pair.second);
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64_t, int64_t) {
    DCHECK(manager()->GetUsageTracker(type));
    AddEntries(type, manager()->GetUsageTracker(type));
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  int remaining_trackers_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetUsageInfoTask);
};

class QuotaManager::OriginDataDeleter : public QuotaTask {
 public:
  OriginDataDeleter(QuotaManager* manager,
                    const url::Origin& origin,
                    StorageType type,
                    QuotaClientTypes quota_client_types,
                    bool is_eviction,
                    StatusCallback callback)
      : QuotaTask(manager),
        origin_(origin),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        error_count_(0),
        remaining_clients_(0),
        skipped_clients_(0),
        is_eviction_(is_eviction),
        callback_(std::move(callback)) {}

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    remaining_clients_ = manager()->client_types_[type_].size();

    for (const auto& client_and_type : manager()->client_types_[type_]) {
      QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        static int tracing_id = 0;
        TRACE_EVENT_ASYNC_BEGIN2(
            "browsing_data", "QuotaManager::OriginDataDeleter", ++tracing_id,
            "client_type", client_type, "origin", origin_.Serialize());
        client->DeleteOriginData(
            origin_, type_,
            base::BindOnce(&OriginDataDeleter::DidDeleteOriginData,
                           weak_factory_.GetWeakPtr(), tracing_id));
      } else {
        ++skipped_clients_;
        --remaining_clients_;
      }
    }

    if (remaining_clients_ == 0)
      CallCompleted();
  }

  void Completed() override {
    if (error_count_ == 0) {
      // Only remove the entire origin if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteOriginFromDatabase(origin_, type_, is_eviction_);
      std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback_).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    DeleteSoon();
  }

 private:
  void DidDeleteOriginData(int tracing_id,
                           blink::mojom::QuotaStatusCode status) {
    DCHECK_GT(remaining_clients_, 0U);
    TRACE_EVENT_ASYNC_END0("browsing_data", "QuotaManager::OriginDataDeleter",
                           tracing_id);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  const url::Origin origin_;
  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  int error_count_;
  size_t remaining_clients_;
  int skipped_clients_;
  const bool is_eviction_;
  StatusCallback callback_;

  base::WeakPtrFactory<OriginDataDeleter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(OriginDataDeleter);
};

class QuotaManager::HostDataDeleter : public QuotaTask {
 public:
  HostDataDeleter(QuotaManager* manager,
                  const std::string& host,
                  StorageType type,
                  QuotaClientTypes quota_client_types,
                  StatusCallback callback)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        error_count_(0),
        remaining_clients_(0),
        remaining_deleters_(0),
        callback_(std::move(callback)) {}

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    remaining_clients_ = manager()->client_types_[type_].size();

    for (const auto& client_and_type : manager()->client_types_[type_]) {
      client_and_type.first->GetOriginsForHost(
          type_, host_,
          base::BindOnce(&HostDataDeleter::DidGetOriginsForHost,
                         weak_factory_.GetWeakPtr()));
    }
  }

  void Completed() override {
    if (error_count_ == 0) {
      std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback_).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    DeleteSoon();
  }

 private:
  void DidGetOriginsForHost(const std::vector<url::Origin>& origins) {
    DCHECK_GT(remaining_clients_, 0U);

    for (const auto& origin : origins)
      origins_.insert(origin);

    if (--remaining_clients_ == 0) {
      if (!origins_.empty())
        ScheduleOriginsDeletion();
      else
        CallCompleted();
    }
  }

  void ScheduleOriginsDeletion() {
    remaining_deleters_ = origins_.size();
    for (const auto& origin : origins_) {
      OriginDataDeleter* deleter = new OriginDataDeleter(
          manager(), origin, type_, std::move(quota_client_types_), false,
          base::BindOnce(&HostDataDeleter::DidDeleteOriginData,
                         weak_factory_.GetWeakPtr()));
      deleter->Start();
    }
  }

  void DidDeleteOriginData(blink::mojom::QuotaStatusCode status) {
    DCHECK_GT(remaining_deleters_, 0U);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_deleters_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  const std::string host_;
  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  std::set<url::Origin> origins_;
  int error_count_;
  size_t remaining_clients_;
  size_t remaining_deleters_;
  StatusCallback callback_;

  base::WeakPtrFactory<HostDataDeleter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(HostDataDeleter);
};

class QuotaManager::StorageCleanupHelper : public QuotaTask {
 public:
  StorageCleanupHelper(QuotaManager* manager,
                       StorageType type,
                       QuotaClientTypes quota_client_types,
                       base::OnceClosure callback)
      : QuotaTask(manager),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        callback_(std::move(callback)) {
    DCHECK(manager->client_types_.contains(type_));
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
      QuotaClient* client = client_and_type.first;
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
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<StorageCleanupHelper> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StorageCleanupHelper);
};

// Fetch origins that have been modified since the specified time. This is used
// to clear data for origins that have been modified within the user specified
// time frame.
//
// This class is granted ownership of itself when it is passed to
// DidGetModifiedBetween() via base::Owned(). When the closure for said
// function goes out of scope, the object is deleted. This is a thread-safe
// class.
class QuotaManager::GetModifiedSinceHelper {
 public:
  bool GetModifiedBetweenOnDBThread(StorageType type,
                                    base::Time begin,
                                    base::Time end,
                                    QuotaDatabase* database) {
    DCHECK(database);
    return database->GetOriginsModifiedBetween(type, &origins_, begin, end);
  }

  void DidGetModifiedBetween(const base::WeakPtr<QuotaManager>& manager,
                             GetOriginsCallback callback,
                             StorageType type,
                             bool success) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(std::set<url::Origin>(), type);
      return;
    }
    manager->DidDatabaseWork(success);
    std::move(callback).Run(origins_, type);
  }

 private:
  std::set<url::Origin> origins_;
};

// Gather origin info table for quota-internals page.
//
// This class is granted ownership of itself when it is passed to
// DidDumpQuotaTable() via base::Owned(). When the closure for said function
// goes out of scope, the object is deleted.
// This class is not thread-safe because there can be a race when entries_ is
// modified.
class QuotaManager::DumpQuotaTableHelper {
 public:
  bool DumpQuotaTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpQuotaTable(base::BindRepeating(
        &DumpQuotaTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpQuotaTable(const base::WeakPtr<QuotaManager>& manager,
                         DumpQuotaTableCallback callback,
                         bool success) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(QuotaTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    std::move(callback).Run(entries_);
  }

 private:
  bool AppendEntry(const QuotaTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  QuotaTableEntries entries_;
};

// Gather origin info table for quota-internals page.
//
// This class is granted ownership of itself when it is passed to
// DidDumpQuotaTable() via base::Owned(). When the closure for said function
// goes out of scope, the object is deleted.
// This class is not thread-safe because there can be races when entries_ is
// modified.
class QuotaManager::DumpOriginInfoTableHelper {
 public:
  bool DumpOriginInfoTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpOriginInfoTable(base::BindRepeating(
        &DumpOriginInfoTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpOriginInfoTable(const base::WeakPtr<QuotaManager>& manager,
                              DumpOriginInfoTableCallback callback,
                              bool success) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(OriginInfoTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    std::move(callback).Run(entries_);
  }

 private:
  bool AppendEntry(const OriginInfoTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  OriginInfoTableEntries entries_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const GetQuotaSettingsFunc& get_settings_function)
    : RefCountedDeleteOnSequence<QuotaManager>(io_thread),
      is_incognito_(is_incognito),
      profile_path_(profile_path),
      proxy_(new QuotaManagerProxy(this, io_thread)),
      db_disabled_(false),
      eviction_disabled_(false),
      io_thread_(io_thread),
      db_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      get_settings_function_(get_settings_function),
      is_getting_eviction_origin_(false),
      special_storage_policy_(std::move(special_storage_policy)),
      get_volume_info_fn_(&QuotaManager::GetVolumeInfo) {
  DCHECK_EQ(settings_.refresh_interval, base::TimeDelta::Max());
  if (!get_settings_function.is_null()) {
    // Reset the interval to ensure we use the get_settings_function
    // the first times settings_ is needed.
    settings_.refresh_interval = base::TimeDelta();
    get_settings_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void QuotaManager::SetQuotaSettings(const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  settings_ = settings;
  settings_timestamp_ = base::TimeTicks::Now();
}

void QuotaManager::GetUsageInfo(GetUsageInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  GetUsageInfoTask* get_usage_info =
      new GetUsageInfoTask(this, std::move(callback));
  get_usage_info->Start();
}

void QuotaManager::GetUsageAndQuotaForWebApps(const url::Origin& origin,
                                              StorageType type,
                                              UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUsageAndQuotaWithBreakdown(
      origin, type,
      base::BindOnce(&DidGetUsageAndQuotaStripBreakdown, std::move(callback)));
}

void QuotaManager::GetUsageAndQuotaWithBreakdown(
    const url::Origin& origin,
    StorageType type,
    UsageAndQuotaWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorNotSupported, /*status*/
        0,                                                 /*usage*/
        0,                                                 /*quota*/
        nullptr);                                          /*usage_breakdown*/
    return;
  }
  LazyInitialize();

  bool is_session_only =
      type == StorageType::kTemporary && special_storage_policy_ &&
      special_storage_policy_->IsStorageSessionOnly(origin.GetURL());
  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, origin, type, IsStorageUnlimited(origin, type), is_session_only,
      is_incognito_, std::move(callback));
  helper->Start();
}

void QuotaManager::GetUsageAndQuota(const url::Origin& origin,
                                    StorageType type,
                                    UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsStorageUnlimited(origin, type)) {
    // TODO(michaeln): This seems like a non-obvious odd behavior, probably for
    // apps/extensions, but it would be good to eliminate this special case.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0, kNoLimit);
    return;
  }

  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    std::move(callback).Run(
        /*status*/ blink::mojom::QuotaStatusCode::kErrorNotSupported,
        /*usage*/ 0,
        /*quota*/ 0);
    return;
  }
  LazyInitialize();

  bool is_session_only =
      type == StorageType::kTemporary && special_storage_policy_ &&
      special_storage_policy_->IsStorageSessionOnly(origin.GetURL());
  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, origin, type, IsStorageUnlimited(origin, type), is_session_only,
      is_incognito_,
      base::BindOnce(&DidGetUsageAndQuotaStripBreakdown, std::move(callback)));
  helper->Start();
}

void QuotaManager::NotifyStorageAccessed(const url::Origin& origin,
                                         StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyStorageAccessedInternal(origin, type, base::Time::Now());
}

void QuotaManager::NotifyStorageModified(QuotaClientType client_id,
                                         const url::Origin& origin,
                                         StorageType type,
                                         int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyStorageModifiedInternal(client_id, origin, type, delta,
                                base::Time::Now());
}

void QuotaManager::NotifyWriteFailed(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto age_of_disk_stats = base::TimeTicks::Now() -
                           std::get<0>(cached_disk_stats_for_storage_pressure_);

  // Avoid polling for free disk space if disk stats have been recently
  // queried.
  if (age_of_disk_stats < kStoragePressureCheckDiskStatsInterval) {
    int64_t total_space = std::get<1>(cached_disk_stats_for_storage_pressure_);
    int64_t available_space =
        std::get<2>(cached_disk_stats_for_storage_pressure_);
    MaybeRunStoragePressureCallback(origin, total_space, available_space);
  }

  GetStorageCapacity(
      base::BindOnce(&QuotaManager::MaybeRunStoragePressureCallback,
                     weak_factory_.GetWeakPtr(), origin));
}

void QuotaManager::NotifyOriginInUse(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  origins_in_use_[origin]++;
}

void QuotaManager::NotifyOriginNoLongerInUse(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(IsOriginInUse(origin));
  int& count = origins_in_use_[origin];
  if (--count == 0)
    origins_in_use_.erase(origin);
}

void QuotaManager::SetUsageCacheEnabled(QuotaClientType client_id,
                                        const url::Origin& origin,
                                        StorageType type,
                                        bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->SetUsageCacheEnabled(client_id, origin, enabled);
}

void QuotaManager::DeleteOriginData(const url::Origin& origin,
                                    StorageType type,
                                    QuotaClientTypes quota_client_types,
                                    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeleteOriginDataInternal(origin, type, std::move(quota_client_types), false,
                           std::move(callback));
}

void QuotaManager::PerformStorageCleanup(StorageType type,
                                         QuotaClientTypes quota_client_types,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StorageCleanupHelper* deleter = new StorageCleanupHelper(
      this, type, std::move(quota_client_types), std::move(callback));
  deleter->Start();
}

void QuotaManager::DeleteHostData(const std::string& host,
                                  StorageType type,
                                  QuotaClientTypes quota_client_types,
                                  StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();

  DCHECK(client_types_.contains(type));
  if (host.empty() || client_types_[type].empty()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  HostDataDeleter* deleter = new HostDataDeleter(
      this, host, type, std::move(quota_client_types), std::move(callback));
  deleter->Start();
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0);
    return;
  }

  if (!persistent_host_quota_callbacks_.Add(host, std::move(callback)))
    return;

  int64_t* quota_ptr = new int64_t(0);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&GetPersistentHostQuotaOnDBThread, host,
                     base::Unretained(quota_ptr)),
      base::BindOnce(&QuotaManager::DidGetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host, base::Owned(quota_ptr)));
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64_t new_quota,
                                          QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
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
      FROM_HERE,
      base::BindOnce(&SetPersistentHostQuotaOnDBThread, host,
                     base::Unretained(new_quota_ptr)),
      base::BindOnce(&QuotaManager::DidSetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host, std::move(callback),
                     base::Owned(new_quota_ptr)));
}

void QuotaManager::GetGlobalUsage(StorageType type,
                                  GlobalUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetGlobalUsage(std::move(callback));
}

void QuotaManager::GetHostUsageWithBreakdown(
    const std::string& host,
    StorageType type,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsageWithBreakdown(host, std::move(callback));
}

std::map<std::string, std::string> QuotaManager::GetStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, std::string> statistics;
  if (temporary_storage_evictor_) {
    std::map<std::string, int64_t> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (const auto& origin_usage_pair : stats) {
      statistics[origin_usage_pair.first] =
          base::NumberToString(origin_usage_pair.second);
    }
  }
  return statistics;
}

bool QuotaManager::IsStorageUnlimited(const url::Origin& origin,
                                      StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == StorageType::kSyncable)
    return false;
  if (type == StorageType::kQuotaNotManaged)
    return true;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin.GetURL());
}

void QuotaManager::GetOriginsModifiedBetween(StorageType type,
                                             base::Time begin,
                                             base::Time end,
                                             GetOriginsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  GetModifiedSinceHelper* helper = new GetModifiedSinceHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&GetModifiedSinceHelper::GetModifiedBetweenOnDBThread,
                     base::Unretained(helper), type, begin, end),
      base::BindOnce(&GetModifiedSinceHelper::DidGetModifiedBetween,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback), type));
}

bool QuotaManager::ResetUsageTracker(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetUsageTracker(type));
  if (GetUsageTracker(type)->IsWorking())
    return false;

  auto usage_tracker = std::make_unique<UsageTracker>(
      client_types_[type], type, special_storage_policy_.get());
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

QuotaManager::~QuotaManager() {
  proxy_->manager_ = nullptr;

  // Iterating over |clients_for_ownership_| is correct here because we want to
  // call OnQuotaManagerDestroyed() once per QuotaClient.
  for (const auto& client : clients_for_ownership_)
    client->OnQuotaManagerDestroyed();

  if (database_)
    db_runner_->DeleteSoon(FROM_HERE, database_.release());
}

QuotaManager::EvictionContext::EvictionContext()
    : evicted_type(StorageType::kUnknown) {}

QuotaManager::EvictionContext::~EvictionContext() = default;

void QuotaManager::LazyInitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Already initialized.
    return;
  }

  // Use an empty path to open an in-memory only database for incognito.
  database_ = std::make_unique<QuotaDatabase>(
      is_incognito_ ? base::FilePath()
                    : profile_path_.AppendASCII(kDatabaseName));

  temporary_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kTemporary], StorageType::kTemporary,
      special_storage_policy_.get());
  persistent_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kPersistent], StorageType::kPersistent,
      special_storage_policy_.get());
  syncable_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kSyncable], StorageType::kSyncable,
      special_storage_policy_.get());

  if (!is_incognito_) {
    histogram_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kReportHistogramInterval),
        this, &QuotaManager::ReportHistogram);
  }

  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaDatabase::IsOriginDatabaseBootstrapped,
                     base::Unretained(database_.get())),
      base::BindOnce(&QuotaManager::FinishLazyInitialize,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::FinishLazyInitialize(bool is_database_bootstrapped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_database_bootstrapped_ = is_database_bootstrapped;
  StartEviction();
}

void QuotaManager::BootstrapDatabaseForEviction(
    GetOriginCallback did_get_origin_callback,
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The usage cache should be fully populated now so we can
  // seed the database with origins we know about.
  std::set<url::Origin> origins = temporary_usage_tracker_->GetCachedOrigins();
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&BootstrapDatabaseOnDBThread, std::move(origins)),
      base::BindOnce(&QuotaManager::DidBootstrapDatabase,
                     weak_factory_.GetWeakPtr(),
                     std::move(did_get_origin_callback)));
}

void QuotaManager::DidBootstrapDatabase(
    GetOriginCallback did_get_origin_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_database_bootstrapped_ = success;
  DidDatabaseWork(success);
  GetLRUOrigin(StorageType::kTemporary, std::move(did_get_origin_callback));
}

void QuotaManager::RegisterClient(
    scoped_refptr<QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!database_.get())
      << "All clients must be registered before the database is initialized";
  DCHECK(client.get());

  for (blink::mojom::StorageType storage_type : storage_types)
    client_types_[storage_type].insert({client.get(), client_type});
  clients_for_ownership_.push_back(std::move(client));
}

UsageTracker* QuotaManager::GetUsageTracker(StorageType type) const {
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

std::set<url::Origin> QuotaManager::GetCachedOrigins(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  return GetUsageTracker(type)->GetCachedOrigins();
}

void QuotaManager::NotifyStorageAccessedInternal(const url::Origin& origin,
                                                 StorageType type,
                                                 base::Time accessed_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  if (type == StorageType::kTemporary && is_getting_eviction_origin_) {
    // Record the accessed origins while GetLRUOrigin task is runing
    // to filter out them from eviction.
    access_notified_origins_.insert(origin);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateAccessTimeOnDBThread, origin, type, accessed_time),
      base::BindOnce(&QuotaManager::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::NotifyStorageModifiedInternal(QuotaClientType client_id,
                                                 const url::Origin& origin,
                                                 StorageType type,
                                                 int64_t delta,
                                                 base::Time modified_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);

  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateModifiedTimeOnDBThread, origin, type,
                     modified_time),
      base::BindOnce(&QuotaManager::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DumpQuotaTable(DumpQuotaTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DumpQuotaTableHelper* helper = new DumpQuotaTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DumpQuotaTableHelper::DumpQuotaTableOnDBThread,
                     base::Unretained(helper)),
      base::BindOnce(&DumpQuotaTableHelper::DidDumpQuotaTable,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void QuotaManager::DumpOriginInfoTable(DumpOriginInfoTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DumpOriginInfoTableHelper* helper = new DumpOriginInfoTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DumpOriginInfoTableHelper::DumpOriginInfoTableOnDBThread,
                     base::Unretained(helper)),
      base::BindOnce(&DumpOriginInfoTableHelper::DidDumpOriginInfoTable,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void QuotaManager::StartEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!temporary_storage_evictor_.get());
  if (eviction_disabled_)
    return;
  temporary_storage_evictor_ = std::make_unique<QuotaTemporaryStorageEvictor>(
      this, kEvictionIntervalInMilliSeconds);
  temporary_storage_evictor_->Start();
}

void QuotaManager::DeleteOriginFromDatabase(const url::Origin& origin,
                                            StorageType type,
                                            bool is_eviction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DeleteOriginInfoOnDBThread, origin, type, is_eviction),
      base::BindOnce(&QuotaManager::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidOriginDataEvicted(blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try evict origins that are not in use, so basically
  // deletion attempt for eviction should not fail.  Let's record
  // the origin if we get error and exclude it from future eviction
  // if the error happens consistently (> kThresholdOfErrorsToBeDenylisted).
  if (status != blink::mojom::QuotaStatusCode::kOk)
    origins_in_error_[eviction_context_.evicted_origin]++;

  std::move(eviction_context_.evict_origin_data_callback).Run(status);
}

void QuotaManager::DeleteOriginDataInternal(const url::Origin& origin,
                                            StorageType type,
                                            QuotaClientTypes quota_client_types,
                                            bool is_eviction,
                                            StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();

  OriginDataDeleter* deleter =
      new OriginDataDeleter(this, origin, type, std::move(quota_client_types),
                            is_eviction, std::move(callback));
  deleter->Start();
}

void QuotaManager::MaybeRunStoragePressureCallback(const url::Origin& origin,
                                                   int64_t total_space,
                                                   int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/1059560): Figure out what 0 total_space means
  // and how to handle the storage pressure callback in these cases.
  if (total_space == 0)
    return;

  if (!storage_pressure_callback_) {
    // Quota will hold onto a storage pressure notification if no storage
    // pressure callback is set.
    origin_for_pending_storage_pressure_callback_ = std::move(origin);
    return;
  }

  if (available_space < kStoragePressureThresholdRatio * total_space) {
    storage_pressure_callback_.Run(std::move(origin));
  }
}

void QuotaManager::SimulateStoragePressure(const url::Origin origin) {
  storage_pressure_callback_.Run(origin);
}

void QuotaManager::DetermineStoragePressure(int64_t free_space,
                                            int64_t total_space) {
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

  if (free_space < threshold) {
    // TODO(https://crbug.com/1096549): Implement StoragePressureEvent
    // dispatching.
  }
}

void QuotaManager::SetStoragePressureCallback(
    base::RepeatingCallback<void(url::Origin)> storage_pressure_callback) {
  storage_pressure_callback_ = storage_pressure_callback;
  if (origin_for_pending_storage_pressure_callback_.has_value()) {
    storage_pressure_callback_.Run(
        std::move(origin_for_pending_storage_pressure_callback_.value()));
    origin_for_pending_storage_pressure_callback_ = base::nullopt;
  }
}

void QuotaManager::ReportHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_incognito_);
  GetGlobalUsage(
      StorageType::kTemporary,
      base::BindOnce(&QuotaManager::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetTemporaryGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  GetStorageCapacity(
      base::BindOnce(&QuotaManager::DidGetStorageCapacityForHistogram,
                     weak_factory_.GetWeakPtr(), usage));
}

void QuotaManager::DidGetStorageCapacityForHistogram(int64_t usage,
                                                     int64_t total_space,
                                                     int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      base::BindOnce(&QuotaManager::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetPersistentGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  // We DumpOriginInfoTable last to ensure the trackers caches are loaded.
  DumpOriginInfoTable(
      base::BindOnce(&QuotaManager::DidDumpOriginInfoTableForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidDumpOriginInfoTableForHistogram(
    const OriginInfoTableEntries& entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<url::Origin, int64_t> usage_map =
      GetUsageTracker(StorageType::kTemporary)->GetCachedOriginsUsage();
  base::Time now = base::Time::Now();
  for (const auto& info : entries) {
    if (info.type != StorageType::kTemporary)
      continue;

    // Ignore stale database entries. If there is no map entry, the origin's
    // data has been deleted.
    auto it = usage_map.find(info.origin);
    if (it == usage_map.end() || it->second == 0)
      continue;

    base::TimeDelta age = now - std::max(info.last_access_time,
                                         info.last_modified_time);
    UMA_HISTOGRAM_COUNTS_1000("Quota.AgeOfOriginInDays", age.InDays());

    int64_t kilobytes = std::max(it->second / INT64_C(1024), INT64_C(1));
    base::Histogram::FactoryGet(
        "Quota.AgeOfDataInDays", 1, 1000, 50,
        base::HistogramBase::kUmaTargetedHistogramFlag)->
            AddCount(age.InDays(),
                     base::saturated_cast<int>(kilobytes));
  }
}

std::set<url::Origin> QuotaManager::GetEvictionOriginExceptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<url::Origin> exceptions;
  for (const auto& p : origins_in_use_) {
    if (p.second > 0)
      exceptions.insert(p.first);
  }

  for (const auto& p : origins_in_error_) {
    if (p.second > QuotaManager::kThresholdOfErrorsToBeDenylisted)
      exceptions.insert(p.first);
  }

  return exceptions;
}

void QuotaManager::DidGetEvictionOrigin(
    GetOriginCallback callback,
    const base::Optional<url::Origin>& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  DCHECK(!origin.has_value() || !origin->GetURL().is_empty());
  if (origin.has_value() &&
      (base::Contains(origins_in_use_, *origin) ||
       base::Contains(access_notified_origins_, *origin))) {
    std::move(callback).Run(base::nullopt);
  } else {
    std::move(callback).Run(origin);
  }
  access_notified_origins_.clear();

  is_getting_eviction_origin_ = false;
}

void QuotaManager::GetEvictionOrigin(
    StorageType type,
    int64_t global_quota,
    GetOriginCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(!is_getting_eviction_origin_);
  is_getting_eviction_origin_ = true;

  auto did_get_origin_callback =
      base::BindOnce(&QuotaManager::DidGetEvictionOrigin,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  if (!is_database_bootstrapped_ && !eviction_disabled_) {
    // Once bootstrapped, GetLRUOrigin will be called.
    GetGlobalUsage(StorageType::kTemporary,
                   base::BindOnce(&QuotaManager::BootstrapDatabaseForEviction,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(did_get_origin_callback)));
    return;
  }

  GetLRUOrigin(type, std::move(did_get_origin_callback));
}

void QuotaManager::EvictOriginData(const url::Origin& origin,
                                   StorageType type,
                                   StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(type, StorageType::kTemporary);

  eviction_context_.evicted_origin = origin;
  eviction_context_.evicted_type = type;
  eviction_context_.evict_origin_data_callback = std::move(callback);

  DeleteOriginDataInternal(origin, type, AllQuotaClientTypes(), true,
                           base::BindOnce(&QuotaManager::DidOriginDataEvicted,
                                          weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetEvictionRoundInfo(EvictionRoundInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  LazyInitialize();
  EvictionRoundInfoHelper* helper =
      new EvictionRoundInfoHelper(this, std::move(callback));
  helper->Start();
}

void QuotaManager::GetLRUOrigin(StorageType type, GetOriginCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(lru_origin_callback_.is_null());
  lru_origin_callback_ = std::move(callback);
  if (db_disabled_) {
    std::move(lru_origin_callback_).Run(base::nullopt);
    return;
  }

  auto origin = std::make_unique<base::Optional<url::Origin>>();
  auto* origin_ptr = origin.get();
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&GetLRUOriginOnDBThread, type,
                     GetEvictionOriginExceptions(),
                     base::RetainedRef(special_storage_policy_),
                     base::Unretained(origin_ptr)),
      base::BindOnce(&QuotaManager::DidGetLRUOrigin, weak_factory_.GetWeakPtr(),
                     std::move(origin)));
}

void QuotaManager::DidGetPersistentHostQuota(const std::string& host,
                                             const int64_t* quota,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(success);
  persistent_host_quota_callbacks_.Run(
      host, blink::mojom::QuotaStatusCode::kOk,
      std::min(*quota, kPerHostPersistentQuotaLimit));
}

void QuotaManager::DidSetPersistentHostQuota(const std::string& host,
                                             QuotaCallback callback,
                                             const int64_t* new_quota,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(success);
  std::move(callback).Run(
      success ? blink::mojom::QuotaStatusCode::kOk
              : blink::mojom::QuotaStatusCode::kErrorInvalidAccess,
      *new_quota);
}

void QuotaManager::DidGetLRUOrigin(
    std::unique_ptr<base::Optional<url::Origin>> origin,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(success);

  std::move(lru_origin_callback_).Run(*origin);
}

namespace {
void DidGetSettingsThreadAdapter(base::TaskRunner* task_runner,
                                 OptionalQuotaSettingsCallback callback,
                                 base::Optional<QuotaSettings> settings) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(settings)));
}
}  // namespace

void QuotaManager::GetQuotaSettings(QuotaSettingsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
          base::BindOnce(&DidGetSettingsThreadAdapter,
                         base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                         base::BindOnce(&QuotaManager::DidGetSettings,
                                        weak_factory_.GetWeakPtr()))));
}

void QuotaManager::DidGetSettings(base::Optional<QuotaSettings> settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!settings) {
    settings = settings_;
    settings->refresh_interval = base::TimeDelta::FromMinutes(1);
  }
  SetQuotaSettings(*settings);
  settings_callbacks_.Run(*settings);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalTemporaryPoolSize", settings->pool_size);
  LOG_IF(WARNING, settings->pool_size == 0)
      << "No storage quota provided in QuotaSettings.";
}

void QuotaManager::GetStorageCapacity(StorageCapacityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_capacity_callbacks_.Add(std::move(callback)))
    return;
  if (is_incognito_) {
    GetQuotaSettings(
        base::BindOnce(&QuotaManager::ContinueIncognitoGetStorageCapacity,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaManager::CallGetVolumeInfo, get_volume_info_fn_,
                     profile_path_),
      base::BindOnce(&QuotaManager::DidGetStorageCapacity,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::ContinueIncognitoGetStorageCapacity(
    const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t current_usage =
      GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
  current_usage += GetUsageTracker(StorageType::kPersistent)->GetCachedUsage();
  int64_t available_space =
      std::max(INT64_C(0), settings.pool_size - current_usage);
  DidGetStorageCapacity(std::make_tuple(settings.pool_size, available_space));
}

void QuotaManager::DidGetStorageCapacity(
    const std::tuple<int64_t, int64_t>& total_and_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cached_disk_stats_for_storage_pressure_ =
      std::make_tuple(base::TimeTicks::Now(), std::get<0>(total_and_available),
                      std::get<1>(total_and_available));
  storage_capacity_callbacks_.Run(std::get<0>(total_and_available),
                                  std::get<1>(total_and_available));
}

void QuotaManager::DidDatabaseWork(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_disabled_ = !success;
}

void QuotaManager::PostTaskAndReplyWithResultForDBThread(
    const base::Location& from_here,
    base::OnceCallback<bool(QuotaDatabase*)> task,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deleting manager will post another task to DB sequence to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

// static
std::tuple<int64_t, int64_t> QuotaManager::CallGetVolumeInfo(
    GetVolumeInfoFn get_volume_info_fn,
    const base::FilePath& path) {
  if (!base::CreateDirectory(path)) {
    LOG(WARNING) << "Create directory failed for path" << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }
  int64_t total;
  int64_t available;
  std::tie(total, available) = get_volume_info_fn(path);
  if (total < 0 || available < 0) {
    LOG(WARNING) << "Unable to get volume info: " << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }
  UMA_HISTOGRAM_MBYTES("Quota.TotalDiskSpace", total);
  UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace", available);
  if (total > 0) {
    UMA_HISTOGRAM_PERCENTAGE("Quota.PercentDiskAvailable",
        std::min(100, static_cast<int>((available * 100) / total)));
  }
  return std::make_tuple(total, available);
}

// static
std::tuple<int64_t, int64_t> QuotaManager::GetVolumeInfo(
    const base::FilePath& path) {
  return std::make_tuple(base::SysInfo::AmountOfTotalDiskSpace(path),
                         base::SysInfo::AmountOfFreeDiskSpace(path));
}

}  // namespace storage

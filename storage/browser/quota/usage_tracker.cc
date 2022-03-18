// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/usage_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

struct UsageTracker::AccumulateInfo {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr usage_breakdown =
      blink::mojom::UsageBreakdown::New();
};

UsageTracker::UsageTracker(
    QuotaManagerImpl* quota_manager_impl,
    const base::flat_map<mojom::QuotaClient*, QuotaClientType>& client_types,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : quota_manager_impl_(quota_manager_impl), type_(type) {
  DCHECK(quota_manager_impl_);

  for (const auto& client_and_type : client_types) {
    mojom::QuotaClient* client = client_and_type.first;
    QuotaClientType client_type = client_and_type.second;
    client_tracker_map_[client_type].push_back(
        std::make_unique<ClientUsageTracker>(this, client, type,
                                             special_storage_policy));
  }
}

UsageTracker::~UsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsageTracker::GetGlobalUsage(UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_usage_callbacks_.emplace_back(std::move(callback));
  if (global_usage_callbacks_.size() > 1)
    return;

  quota_manager_impl_->GetBucketsForType(
      type_, base::BindOnce(&UsageTracker::DidGetBucketsForType,
                            weak_factory_.GetWeakPtr()));
}

void UsageTracker::GetHostUsageWithBreakdown(
    const std::string& host,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<UsageWithBreakdownCallback>& host_callbacks =
      host_usage_callbacks_[host];
  host_callbacks.emplace_back(std::move(callback));
  if (host_callbacks.size() > 1)
    return;

  quota_manager_impl_->GetBucketsForHost(
      host, type_,
      base::BindOnce(&UsageTracker::DidGetBucketsForHost,
                     weak_factory_.GetWeakPtr(), host));
}

void UsageTracker::UpdateBucketUsageCache(QuotaClientType client_type,
                                          const BucketLocator& bucket,
                                          int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));

  for (const auto& client_tracker : client_tracker_map_[client_type])
    client_tracker->UpdateBucketUsageCache(bucket, delta);
}

void UsageTracker::DeleteBucketCache(QuotaClientType client_type,
                                     const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  DCHECK_EQ(bucket.type, type_);

  for (const auto& client_tracker : client_tracker_map_[client_type])
    client_tracker->DeleteBucketCache(bucket);
}

int64_t UsageTracker::GetCachedUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t usage = 0;
  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second)
      usage += client_tracker->GetCachedUsage();
  }
  return usage;
}

std::map<std::string, int64_t> UsageTracker::GetCachedHostsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, int64_t> host_usage;
  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      std::map<std::string, int64_t> client_host_usage =
          client_tracker->GetCachedHostsUsage();
      for (const auto& host_and_usage : client_host_usage)
        host_usage[host_and_usage.first] += host_and_usage.second;
    }
  }
  return host_usage;
}

std::map<blink::StorageKey, int64_t> UsageTracker::GetCachedStorageKeysUsage()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<blink::StorageKey, int64_t> storage_key_usage;
  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      std::map<blink::StorageKey, int64_t> client_storage_key_usage =
          client_tracker->GetCachedStorageKeysUsage();
      for (const auto& storage_key_and_usage : client_storage_key_usage)
        storage_key_usage[storage_key_and_usage.first] +=
            storage_key_and_usage.second;
    }
  }
  return storage_key_usage;
}

void UsageTracker::SetUsageCacheEnabled(QuotaClientType client_type,
                                        const blink::StorageKey& storage_key,
                                        bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  for (const auto& client_tracker : client_tracker_map_[client_type])
    client_tracker->SetUsageCacheEnabled(storage_key, enabled);
}

void UsageTracker::DidGetBucketsForType(
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto info = std::make_unique<AccumulateInfo>();
  if (!result.ok()) {
    // Return with invalid values on error.
    info->usage = -1;
    info->unlimited_usage = -1;
    FinallySendGlobalUsage(std::move(info));
    return;
  }

  const std::set<BucketLocator>& buckets = result.value();
  if (buckets.empty()) {
    FinallySendGlobalUsage(std::move(info));
    return;
  }

  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendGlobalUsage,
                     weak_factory_.GetWeakPtr(), std::move(info)));

  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      client_tracker->GetBucketsUsage(
          buckets,
          // base::Unretained usage is safe here because BarrierClosure holds
          // the std::unque_ptr that keeps AccumulateInfo alive, and the
          // BarrierClosure will outlive all the AccumulateClientGlobalUsage
          // closures.
          base::BindOnce(&UsageTracker::AccumulateClientGlobalUsage,
                         weak_factory_.GetWeakPtr(), barrier,
                         base::Unretained(info_ptr)));
    }
  }
}

void UsageTracker::DidGetBucketsForHost(
    const std::string& host,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto info = std::make_unique<AccumulateInfo>();
  if (!result.ok()) {
    // Return with invalid values on error.
    info->usage = -1;
    info->unlimited_usage = -1;
    FinallySendHostUsageWithBreakdown(std::move(info), host);
    return;
  }

  const std::set<BucketLocator>& buckets = result.value();
  if (buckets.empty()) {
    FinallySendHostUsageWithBreakdown(std::move(info), host);
    return;
  }

  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendHostUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), std::move(info), host));

  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      client_tracker->GetBucketsUsage(
          buckets,
          // base::Unretained usage is safe here because BarrierClosure holds
          // the std::unque_ptr that keeps AccumulateInfo alive, and the
          // BarrierClosure will outlive all the AccumulateClientGlobalUsage
          // closures.
          base::BindOnce(&UsageTracker::AccumulateClientHostUsage,
                         weak_factory_.GetWeakPtr(), barrier,
                         base::Unretained(info_ptr), host,
                         client_type_and_trackers.first));
    }
  }
}

void UsageTracker::AccumulateClientGlobalUsage(
    base::OnceClosure barrier_callback,
    AccumulateInfo* info,
    int64_t total_usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(unlimited_usage, 0);
  DCHECK_GE(total_usage, unlimited_usage);

  info->usage += total_usage;
  info->unlimited_usage += unlimited_usage;

  std::move(barrier_callback).Run();
}

void UsageTracker::AccumulateClientHostUsage(base::OnceClosure barrier_callback,
                                             AccumulateInfo* info,
                                             const std::string& host,
                                             QuotaClientType client,
                                             int64_t total_usage,
                                             int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(unlimited_usage, 0);
  DCHECK_GE(total_usage, unlimited_usage);

  info->usage += total_usage;

  switch (client) {
    case QuotaClientType::kFileSystem:
      info->usage_breakdown->fileSystem += total_usage;
      break;
    case QuotaClientType::kDatabase:
      info->usage_breakdown->webSql += total_usage;
      break;
    case QuotaClientType::kIndexedDatabase:
      info->usage_breakdown->indexedDatabase += total_usage;
      break;
    case QuotaClientType::kServiceWorkerCache:
      info->usage_breakdown->serviceWorkerCache += total_usage;
      break;
    case QuotaClientType::kServiceWorker:
      info->usage_breakdown->serviceWorker += total_usage;
      break;
    case QuotaClientType::kBackgroundFetch:
      info->usage_breakdown->backgroundFetch += total_usage;
      break;
    case QuotaClientType::kNativeIO:
      info->usage_breakdown->fileSystem += total_usage;
      break;
    case QuotaClientType::kMediaLicense:
      // Media license data does not count against quota and should always
      // report 0 usage.
      // TODO(crbug.com/1305441): Consider counting media license data against
      // quota.
      DCHECK_EQ(total_usage, 0);
      break;
  }

  std::move(barrier_callback).Run();
}

void UsageTracker::FinallySendGlobalUsage(
    std::unique_ptr<AccumulateInfo> info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(info->unlimited_usage, -1);
  DCHECK_GE(info->usage, info->unlimited_usage);

  // Moving callbacks out of the original vector early handles the case where a
  // callback makes a new quota call.
  std::vector<UsageCallback> pending_callbacks;
  pending_callbacks.swap(global_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->unlimited_usage);
}

void UsageTracker::FinallySendHostUsageWithBreakdown(
    std::unique_ptr<AccumulateInfo> info,
    const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = host_usage_callbacks_.find(host);
  if (it == host_usage_callbacks_.end())
    return;

  std::vector<UsageWithBreakdownCallback> pending_callbacks;
  pending_callbacks.swap(it->second);
  DCHECK(pending_callbacks.size() > 0)
      << "host_usage_callbacks_ should only have non-empty callback lists";
  host_usage_callbacks_.erase(it);

  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->usage_breakdown->Clone());
}

}  // namespace storage

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/usage_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

struct UsageTracker::AccumulateInfo {
  AccumulateInfo() = default;
  ~AccumulateInfo() = default;

  size_t pending_clients = 0;
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr usage_breakdown =
      blink::mojom::UsageBreakdown::New();
};

UsageTracker::UsageTracker(
    const base::flat_map<mojom::QuotaClient*, QuotaClientType>& client_types,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : type_(type) {
  size_t client_count = 0;

  for (const auto& client_and_type : client_types) {
    mojom::QuotaClient* client = client_and_type.first;
    QuotaClientType client_type = client_and_type.second;
    client_tracker_map_[client_type].push_back(
        std::make_unique<ClientUsageTracker>(this, client, type,
                                             special_storage_policy));
    ++client_count;
  }
  client_count_ = client_count;
}

UsageTracker::~UsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsageTracker::GetGlobalUsage(GlobalUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_usage_callbacks_.emplace_back(std::move(callback));
  if (global_usage_callbacks_.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  auto accumulator =
      base::BindRepeating(&UsageTracker::AccumulateClientGlobalUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info));

  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second)
      client_tracker->GetGlobalUsage(accumulator);
  }

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0, 0);
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

  AccumulateInfo* info = new AccumulateInfo;
  // We use BarrierClosure here instead of manually counting pending_clients.
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendHostUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), base::Owned(info), host));

  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      client_tracker->GetHostUsage(
          host, base::BindOnce(&UsageTracker::AccumulateClientHostUsage,
                               weak_factory_.GetWeakPtr(), barrier, info, host,
                               client_type_and_trackers.first));
    }
  }
}

void UsageTracker::UpdateUsageCache(QuotaClientType client_type,
                                    const blink::StorageKey& storage_key,
                                    int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  for (const auto& client_tracker : client_tracker_map_[client_type])
    client_tracker->UpdateUsageCache(storage_key, delta);
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

std::set<blink::StorageKey> UsageTracker::GetCachedStorageKeys() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<blink::StorageKey> storage_keys;
  for (const auto& client_type_and_trackers : client_tracker_map_) {
    for (const auto& client_tracker : client_type_and_trackers.second) {
      std::set<blink::StorageKey> client_storage_keys =
          client_tracker->GetCachedStorageKeys();
      for (const auto& client_storage_key : client_storage_keys)
        storage_keys.insert(client_storage_key);
    }
  }
  return storage_keys;
}

void UsageTracker::SetUsageCacheEnabled(QuotaClientType client_type,
                                        const blink::StorageKey& storage_key,
                                        bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  for (const auto& client_tracker : client_tracker_map_[client_type])
    client_tracker->SetUsageCacheEnabled(storage_key, enabled);
}

void UsageTracker::AccumulateClientGlobalUsage(AccumulateInfo* info,
                                               int64_t usage,
                                               int64_t unlimited_usage) {
  DCHECK_GT(info->pending_clients, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->usage += usage;
  info->unlimited_usage += unlimited_usage;
  if (--info->pending_clients)
    return;

  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  // TODO(michaeln): The unlimited number is not trustworthy, it
  // can get out of whack when apps are installed or uninstalled.
  if (info->unlimited_usage > info->usage)
    info->unlimited_usage = info->usage;
  else if (info->unlimited_usage < 0)
    info->unlimited_usage = 0;

  // Moving callbacks out of the original vector early handles the case where a
  // callback makes a new quota call.
  std::vector<GlobalUsageCallback> pending_callbacks;
  pending_callbacks.swap(global_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->unlimited_usage);
}

void UsageTracker::AccumulateClientHostUsage(base::OnceClosure callback,
                                             AccumulateInfo* info,
                                             const std::string& host,
                                             QuotaClientType client,
                                             int64_t usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->usage += usage;
  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  switch (client) {
    case QuotaClientType::kFileSystem:
      info->usage_breakdown->fileSystem += usage;
      break;
    case QuotaClientType::kDatabase:
      info->usage_breakdown->webSql += usage;
      break;
    case QuotaClientType::kAppcache:
      info->usage_breakdown->appcache += usage;
      break;
    case QuotaClientType::kIndexedDatabase:
      info->usage_breakdown->indexedDatabase += usage;
      break;
    case QuotaClientType::kServiceWorkerCache:
      info->usage_breakdown->serviceWorkerCache += usage;
      break;
    case QuotaClientType::kServiceWorker:
      info->usage_breakdown->serviceWorker += usage;
      break;
    case QuotaClientType::kBackgroundFetch:
      info->usage_breakdown->backgroundFetch += usage;
      break;
    case QuotaClientType::kNativeIO:
      info->usage_breakdown->fileSystem += usage;
      break;
  }

  std::move(callback).Run();
}

void UsageTracker::FinallySendHostUsageWithBreakdown(AccumulateInfo* info,
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

  for (auto& callback : pending_callbacks) {
    std::move(callback).Run(info->usage, info->usage_breakdown->Clone());
  }
}

}  // namespace storage

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
#include "storage/browser/quota/storage_monitor.h"

namespace storage {

namespace {

void DidGetGlobalUsageForLimitedGlobalUsage(UsageCallback callback,
                                            int64_t total_global_usage,
                                            int64_t global_unlimited_usage) {
  std::move(callback).Run(total_global_usage - global_unlimited_usage);
}

void StripUsageWithBreakdownCallback(
    UsageCallback callback,
    int64_t usage,
    base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
  std::move(callback).Run(usage);
}

}  // namespace

UsageTracker::UsageTracker(const std::vector<QuotaClient*>& clients,
                           blink::mojom::StorageType type,
                           SpecialStoragePolicy* special_storage_policy,
                           StorageMonitor* storage_monitor)
    : type_(type), storage_monitor_(storage_monitor), weak_factory_(this) {
  for (auto* client : clients) {
    if (client->DoesSupport(type)) {
      client_tracker_map_[client->id()] = std::make_unique<ClientUsageTracker>(
          this, client, type, special_storage_policy, storage_monitor_);
    }
  }
}

UsageTracker::~UsageTracker() = default;

ClientUsageTracker* UsageTracker::GetClientTracker(QuotaClient::ID client_id) {
  auto found = client_tracker_map_.find(client_id);
  if (found != client_tracker_map_.end())
    return found->second.get();
  return nullptr;
}

void UsageTracker::GetGlobalLimitedUsage(UsageCallback callback) {
  if (!global_usage_callbacks_.empty()) {
    global_usage_callbacks_.emplace_back(base::BindOnce(
        &DidGetGlobalUsageForLimitedGlobalUsage, std::move(callback)));
    return;
  }

  global_limited_usage_callbacks_.emplace_back(std::move(callback));
  if (global_limited_usage_callbacks_.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalLimitedUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  auto accumulator =
      base::BindRepeating(&UsageTracker::AccumulateClientGlobalLimitedUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info));

  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetGlobalLimitedUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0);
}

void UsageTracker::GetGlobalUsage(GlobalUsageCallback callback) {
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

  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetGlobalUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0, 0);
}

void UsageTracker::GetHostUsage(const std::string& host,
                                UsageCallback callback) {
  UsageTracker::GetHostUsageWithBreakdown(
      host,
      base::BindOnce(&StripUsageWithBreakdownCallback, std::move(callback)));
}

void UsageTracker::GetHostUsageWithBreakdown(
    const std::string& host,
    UsageWithBreakdownCallback callback) {
  std::vector<UsageWithBreakdownCallback>& host_callbacks =
      host_usage_callbacks_[host];
  host_callbacks.emplace_back(std::move(callback));
  if (host_callbacks.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // We use BarrierClosure here instead of manually counting pending_clients.
  base::Closure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendHostUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), base::Owned(info), host));

  for (const auto& client_id_and_tracker : client_tracker_map_) {
    client_id_and_tracker.second->GetHostUsage(
        host, base::BindOnce(&UsageTracker::AccumulateClientHostUsage,
                             weak_factory_.GetWeakPtr(), barrier, info, host,
                             client_id_and_tracker.first));
  }
}

void UsageTracker::UpdateUsageCache(QuotaClient::ID client_id,
                                    const url::Origin& origin,
                                    int64_t delta) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);
  client_tracker->UpdateUsageCache(origin, delta);
}

int64_t UsageTracker::GetCachedUsage() const {
  int64_t usage = 0;
  for (const auto& client_id_and_tracker : client_tracker_map_)
    usage += client_id_and_tracker.second->GetCachedUsage();
  return usage;
}

void UsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64_t>* host_usage) const {
  DCHECK(host_usage);
  host_usage->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedHostsUsage(host_usage);
}

void UsageTracker::GetCachedOriginsUsage(
    std::map<url::Origin, int64_t>* origin_usage) const {
  DCHECK(origin_usage);
  origin_usage->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedOriginsUsage(origin_usage);
}

void UsageTracker::GetCachedOrigins(std::set<url::Origin>* origins) const {
  DCHECK(origins);
  origins->clear();
  for (const auto& client_id_and_tracker : client_tracker_map_)
    client_id_and_tracker.second->GetCachedOrigins(origins);
}

void UsageTracker::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const url::Origin& origin,
                                        bool enabled) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);

  client_tracker->SetUsageCacheEnabled(origin, enabled);
}

UsageTracker::AccumulateInfo::AccumulateInfo() = default;

UsageTracker::AccumulateInfo::~AccumulateInfo() = default;

void UsageTracker::AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                                      int64_t limited_usage) {
  info->usage += limited_usage;
  if (--info->pending_clients)
    return;

  // Moving callbacks out of the original vector handles the case where a
  // callback makes a new quota call.
  std::vector<UsageCallback> pending_callbacks;
  pending_callbacks.swap(global_limited_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage);
}

void UsageTracker::AccumulateClientGlobalUsage(AccumulateInfo* info,
                                               int64_t usage,
                                               int64_t unlimited_usage) {
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

void UsageTracker::AccumulateClientHostUsage(
    const base::RepeatingClosure& barrier,
    AccumulateInfo* info,
    const std::string& host,
    QuotaClient::ID client,
    int64_t usage) {
  info->usage += usage;
  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  info->usage_breakdown[client] += usage;
  barrier.Run();
}

void UsageTracker::FinallySendHostUsageWithBreakdown(AccumulateInfo* info,
                                                     const std::string& host) {
  auto host_it = host_usage_callbacks_.find(host);
  if (host_it == host_usage_callbacks_.end())
    return;

  std::vector<UsageWithBreakdownCallback> pending_callbacks;
  pending_callbacks.swap(host_it->second);
  DCHECK(pending_callbacks.size() > 0)
      << "host_usage_callbacks_ should only have non-empty callback lists";
  host_usage_callbacks_.erase(host_it);

  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->usage_breakdown);
}

}  // namespace storage

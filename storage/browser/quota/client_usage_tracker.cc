// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/client_usage_tracker.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"

namespace storage {

namespace {

using OriginSetByHost = ClientUsageTracker::OriginSetByHost;

void DidGetHostUsage(UsageCallback callback,
                     int64_t limited_usage,
                     int64_t unlimited_usage) {
  DCHECK_GE(limited_usage, 0);
  DCHECK_GE(unlimited_usage, 0);
  std::move(callback).Run(limited_usage + unlimited_usage);
}

bool EraseOriginFromOriginSet(OriginSetByHost* origins_by_host,
                              const std::string& host,
                              const url::Origin& origin) {
  auto it = origins_by_host->find(host);
  if (it == origins_by_host->end())
    return false;

  if (!it->second.erase(origin))
    return false;

  if (it->second.empty())
    origins_by_host->erase(host);
  return true;
}

bool OriginSetContainsOrigin(const OriginSetByHost& origins,
                             const std::string& host,
                             const url::Origin& origin) {
  auto itr = origins.find(host);
  return itr != origins.end() && base::Contains(itr->second, origin);
}

void RecordSkippedOriginHistogram(const InvalidOriginReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Quota.SkippedInvalidOriginUsage", reason);
}

}  // namespace

struct ClientUsageTracker::AccumulateInfo {
  AccumulateInfo() = default;
  ~AccumulateInfo() = default;

  AccumulateInfo(const AccumulateInfo&) = delete;
  AccumulateInfo& operator=(const AccumulateInfo&) = delete;

  size_t pending_jobs = 0;
  int64_t limited_usage = 0;
  int64_t unlimited_usage = 0;
};

ClientUsageTracker::ClientUsageTracker(
    UsageTracker* tracker,
    scoped_refptr<QuotaClient> client,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : client_(std::move(client)),
      type_(type),
      global_limited_usage_(0),
      global_unlimited_usage_(0),
      global_usage_retrieved_(false),
      special_storage_policy_(std::move(special_storage_policy)) {
  DCHECK(client_);
  if (special_storage_policy_.get())
    special_storage_policy_->AddObserver(this);
}

ClientUsageTracker::~ClientUsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (special_storage_policy_.get())
    special_storage_policy_->RemoveObserver(this);
}

void ClientUsageTracker::GetGlobalUsage(GlobalUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (global_usage_retrieved_ &&
      non_cached_limited_origins_by_host_.empty() &&
      non_cached_unlimited_origins_by_host_.empty()) {
    std::move(callback).Run(global_limited_usage_ + global_unlimited_usage_,
                            global_unlimited_usage_);
    return;
  }

  client_->GetOriginsForType(
      type_, base::BindOnce(&ClientUsageTracker::DidGetOriginsForGlobalUsage,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientUsageTracker::GetHostUsage(const std::string& host,
                                      UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::Contains(cached_hosts_, host) &&
      !base::Contains(non_cached_limited_origins_by_host_, host) &&
      !base::Contains(non_cached_unlimited_origins_by_host_, host)) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    std::move(callback).Run(GetCachedHostUsage(host));
    return;
  }

  if (!host_usage_accumulators_.Add(
          host, base::BindOnce(&DidGetHostUsage, std::move(callback))))
    return;
  client_->GetOriginsForHost(
      type_, host,
      base::BindOnce(&ClientUsageTracker::DidGetOriginsForHostUsage,
                     weak_factory_.GetWeakPtr(), host));
}

void ClientUsageTracker::UpdateUsageCache(const url::Origin& origin,
                                          int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = origin.host();
  if (base::Contains(cached_hosts_, host)) {
    if (!IsUsageCacheEnabledForOrigin(origin))
      return;

    // Constrain |delta| to avoid negative usage values.
    // TODO(michaeln): crbug/463729
    delta = std::max(delta, -cached_usage_by_host_[host][origin]);
    cached_usage_by_host_[host][origin] += delta;
    UpdateGlobalUsageValue(IsStorageUnlimited(origin) ? &global_unlimited_usage_
                                                      : &global_limited_usage_,
                           delta);

    return;
  }

  // We call GetHostUsage() so that the cache still updates, but we don't need
  // to do anything else with the usage so we do not pass a callback.
  GetHostUsage(host, base::DoNothing());
}

int64_t ClientUsageTracker::GetCachedUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t usage = 0;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& origin_and_usage : host_and_usage_map.second)
      usage += origin_and_usage.second;
  }
  return usage;
}

std::map<std::string, int64_t> ClientUsageTracker::GetCachedHostsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, int64_t> host_usage;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    const std::string& host = host_and_usage_map.first;
    host_usage[host] += GetCachedHostUsage(host);
  }
  return host_usage;
}

std::map<url::Origin, int64_t> ClientUsageTracker::GetCachedOriginsUsage()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<url::Origin, int64_t> origin_usage;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& origin_and_usage : host_and_usage_map.second)
      origin_usage[origin_and_usage.first] += origin_and_usage.second;
  }
  return origin_usage;
}

std::set<url::Origin> ClientUsageTracker::GetCachedOrigins() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<url::Origin> origins;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& origin_and_usage : host_and_usage_map.second)
      origins.insert(origin_and_usage.first);
  }
  return origins;
}

void ClientUsageTracker::SetUsageCacheEnabled(const url::Origin& origin,
                                              bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = origin.host();
  if (!enabled) {
    // Erase |origin| from cache and subtract its usage.
    auto host_it = cached_usage_by_host_.find(host);
    if (host_it != cached_usage_by_host_.end()) {
      UsageMap& cached_usage_for_host = host_it->second;

      auto origin_it = cached_usage_for_host.find(origin);
      if (origin_it != cached_usage_for_host.end()) {
        int64_t usage = origin_it->second;
        UpdateUsageCache(origin, -usage);
        cached_usage_for_host.erase(origin_it);
        if (cached_usage_for_host.empty()) {
          cached_usage_by_host_.erase(host_it);
          cached_hosts_.erase(host);
        }
      }
    }

    if (IsStorageUnlimited(origin))
      non_cached_unlimited_origins_by_host_[host].insert(origin);
    else
      non_cached_limited_origins_by_host_[host].insert(origin);
  } else {
    // Erase |origin| from |non_cached_origins_| and invalidate the usage cache
    // for the host.
    if (EraseOriginFromOriginSet(&non_cached_limited_origins_by_host_,
                                 host, origin) ||
        EraseOriginFromOriginSet(&non_cached_unlimited_origins_by_host_,
                                 host, origin)) {
      cached_hosts_.erase(host);
      global_usage_retrieved_ = false;
    }
  }
}

void ClientUsageTracker::DidGetOriginsForGlobalUsage(
    GlobalUsageCallback callback,
    const std::vector<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, std::vector<url::Origin>> origins_by_host;
  for (const auto& origin : origins)
    origins_by_host[origin.host()].push_back(origin);

  AccumulateInfo* info = new AccumulateInfo;
  // Getting host usage may synchronously return the result if the usage is
  // cached, which may in turn dispatch the completion callback before we finish
  // looping over all hosts (because info->pending_jobs may reach 0 during the
  // loop).  To avoid this, we add one more pending host as a sentinel and
  // fire the sentinel callback at the end.
  info->pending_jobs = origins_by_host.size() + 1;
  auto accumulator = base::BindRepeating(
      &ClientUsageTracker::AccumulateHostUsage, weak_factory_.GetWeakPtr(),
      base::Owned(info), base::AdaptCallbackForRepeating(std::move(callback)));

  for (const auto& host_and_origins : origins_by_host) {
    const std::string& host = host_and_origins.first;
    const std::vector<url::Origin>& origins = host_and_origins.second;
    if (host_usage_accumulators_.Add(host, accumulator))
      GetUsageForOrigins(host, origins);
  }

  // Fire the sentinel as we've now called GetUsageForOrigins for all clients.
  accumulator.Run(0, 0);
}

void ClientUsageTracker::AccumulateHostUsage(AccumulateInfo* info,
                                             GlobalUsageCallback callback,
                                             int64_t limited_usage,
                                             int64_t unlimited_usage) {
  DCHECK_GT(info->pending_jobs, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->limited_usage += limited_usage;
  info->unlimited_usage += unlimited_usage;
  if (--info->pending_jobs)
    return;

  DCHECK_GE(info->limited_usage, 0);
  DCHECK_GE(info->unlimited_usage, 0);

  global_usage_retrieved_ = true;
  std::move(callback).Run(info->limited_usage + info->unlimited_usage,
                          info->unlimited_usage);
}

void ClientUsageTracker::DidGetOriginsForHostUsage(
    const std::string& host,
    const std::vector<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUsageForOrigins(host, origins);
}

void ClientUsageTracker::GetUsageForOrigins(
    const std::string& host,
    const std::vector<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AccumulateInfo* info = new AccumulateInfo;
  // Getting origin usage may synchronously return the result if the usage is
  // cached, which may in turn dispatch the completion callback before we finish
  // looping over all origins (because info->pending_jobs may reach 0 during the
  // loop).  To avoid this, we add one more pending origin as a sentinel and
  // fire the sentinel callback at the end.
  info->pending_jobs = origins.size() + 1;
  auto accumulator =
      base::BindRepeating(&ClientUsageTracker::AccumulateOriginUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info), host);

  for (const auto& origin : origins) {
    DCHECK_EQ(host, origin.host());

    int64_t origin_usage = 0;
    if (GetCachedOriginUsage(origin, &origin_usage)) {
      accumulator.Run(origin, origin_usage);
    } else {
      client_->GetOriginUsage(origin, type_,
                              base::BindOnce(accumulator, origin));
    }
  }

  // Fire the sentinel as we've now called GetOriginUsage for all clients.
  accumulator.Run(base::nullopt, 0);
}

void ClientUsageTracker::AccumulateOriginUsage(
    AccumulateInfo* info,
    const std::string& host,
    const base::Optional<url::Origin>& origin,
    int64_t usage) {
  DCHECK_GT(info->pending_jobs, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin.has_value()) {
    // TODO(https://crbug.com/941480): |origin| should not be opaque or have an
    // empty url, but sometimes it is.
    if (origin->opaque()) {
      DVLOG(1) << "AccumulateOriginUsage for opaque origin!";
      RecordSkippedOriginHistogram(InvalidOriginReason::kIsOpaque);
    } else if (origin->GetURL().is_empty()) {
      DVLOG(1) << "AccumulateOriginUsage for origin with empty url!";
      RecordSkippedOriginHistogram(InvalidOriginReason::kIsEmpty);
    } else {
      if (usage < 0)
        usage = 0;

      if (IsStorageUnlimited(*origin))
        info->unlimited_usage += usage;
      else
        info->limited_usage += usage;
      if (IsUsageCacheEnabledForOrigin(*origin))
        AddCachedOrigin(*origin, usage);
    }
  }
  if (--info->pending_jobs)
    return;

  AddCachedHost(host);
  host_usage_accumulators_.Run(
      host, info->limited_usage, info->unlimited_usage);
}

void ClientUsageTracker::AddCachedOrigin(const url::Origin& origin,
                                         int64_t new_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsUsageCacheEnabledForOrigin(origin));

  const std::string& host = origin.host();
  int64_t* usage = &cached_usage_by_host_[host][origin];
  int64_t delta = new_usage - *usage;
  *usage = new_usage;
  if (delta) {
    UpdateGlobalUsageValue(IsStorageUnlimited(origin) ? &global_unlimited_usage_
                                                      : &global_limited_usage_,
                           delta);
  }
}

void ClientUsageTracker::AddCachedHost(const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cached_hosts_.insert(host);
}

int64_t ClientUsageTracker::GetCachedHostUsage(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = cached_usage_by_host_.find(host);
  if (it == cached_usage_by_host_.end())
    return 0;

  int64_t usage = 0;
  const UsageMap& usage_map = it->second;
  for (const auto& origin_and_usage : usage_map)
    usage += origin_and_usage.second;
  return usage;
}

bool ClientUsageTracker::GetCachedOriginUsage(const url::Origin& origin,
                                              int64_t* usage) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = origin.host();
  auto host_it = cached_usage_by_host_.find(host);
  if (host_it == cached_usage_by_host_.end())
    return false;

  auto origin_it = host_it->second.find(origin);
  if (origin_it == host_it->second.end())
    return false;

  DCHECK(IsUsageCacheEnabledForOrigin(origin));
  *usage = origin_it->second;
  return true;
}

bool ClientUsageTracker::IsUsageCacheEnabledForOrigin(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = origin.host();
  return !OriginSetContainsOrigin(non_cached_limited_origins_by_host_,
                                  host, origin) &&
      !OriginSetContainsOrigin(non_cached_unlimited_origins_by_host_,
                               host, origin);
}

void ClientUsageTracker::OnGranted(const url::Origin& origin,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64_t usage = 0;
    if (GetCachedOriginUsage(origin, &usage)) {
      global_unlimited_usage_ += usage;
      global_limited_usage_ -= usage;
    }

    const std::string& host = origin.host();
    if (EraseOriginFromOriginSet(&non_cached_limited_origins_by_host_,
                                 host, origin))
      non_cached_unlimited_origins_by_host_[host].insert(origin);
  }
}

void ClientUsageTracker::OnRevoked(const url::Origin& origin,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64_t usage = 0;
    if (GetCachedOriginUsage(origin, &usage)) {
      global_unlimited_usage_ -= usage;
      global_limited_usage_ += usage;
    }

    const std::string& host = origin.host();
    if (EraseOriginFromOriginSet(&non_cached_unlimited_origins_by_host_,
                                 host, origin))
      non_cached_limited_origins_by_host_[host].insert(origin);
  }
}

void ClientUsageTracker::OnCleared() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_limited_usage_ += global_unlimited_usage_;
  global_unlimited_usage_ = 0;

  for (const auto& host_and_origins : non_cached_unlimited_origins_by_host_) {
    const auto& host = host_and_origins.first;
    for (const auto& origin : host_and_origins.second)
      non_cached_limited_origins_by_host_[host].insert(origin);
  }
  non_cached_unlimited_origins_by_host_.clear();
}

void ClientUsageTracker::UpdateGlobalUsageValue(int64_t* usage_value,
                                                int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *usage_value += delta;
  if (*usage_value >= 0)
    return;

  // If we have a negative global usage value, recalculate them.
  // TODO(michaeln): There are book keeping bugs, crbug/463729
  global_limited_usage_ = 0;
  global_unlimited_usage_ = 0;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& origin_and_usage : host_and_usage_map.second) {
      if (IsStorageUnlimited(origin_and_usage.first))
        global_unlimited_usage_ += origin_and_usage.second;
      else
        global_limited_usage_ += origin_and_usage.second;
    }
  }
}

bool ClientUsageTracker::IsStorageUnlimited(const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type_ == blink::mojom::StorageType::kSyncable)
    return false;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin.GetURL());
}

}  // namespace storage

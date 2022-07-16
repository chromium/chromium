// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/client_usage_tracker.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {

using StorageKeySetByHost = ClientUsageTracker::StorageKeySetByHost;

void DidGetHostUsage(UsageCallback callback,
                     int64_t limited_usage,
                     int64_t unlimited_usage) {
  DCHECK_GE(limited_usage, 0);
  DCHECK_GE(unlimited_usage, 0);
  std::move(callback).Run(limited_usage + unlimited_usage);
}

bool EraseStorageKeyFromStorageKeySet(StorageKeySetByHost* storage_keys_by_host,
                                      const std::string& host,
                                      const blink::StorageKey& storage_key) {
  auto it = storage_keys_by_host->find(host);
  if (it == storage_keys_by_host->end())
    return false;

  if (!it->second.erase(storage_key))
    return false;

  if (it->second.empty())
    storage_keys_by_host->erase(host);
  return true;
}

bool StorageKeySetContainsStorageKey(const StorageKeySetByHost& storage_keys,
                                     const std::string& host,
                                     const blink::StorageKey& storage_key) {
  auto itr = storage_keys.find(host);
  return itr != storage_keys.end() && base::Contains(itr->second, storage_key);
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
    mojom::QuotaClient* client,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : client_(client),
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
      non_cached_limited_storage_keys_by_host_.empty() &&
      non_cached_unlimited_storage_keys_by_host_.empty()) {
    std::move(callback).Run(global_limited_usage_ + global_unlimited_usage_,
                            global_unlimited_usage_);
    return;
  }

  client_->GetStorageKeysForType(
      type_,
      base::BindOnce(&ClientUsageTracker::DidGetStorageKeysForGlobalUsage,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientUsageTracker::GetHostUsage(const std::string& host,
                                      UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::Contains(cached_hosts_, host) &&
      !base::Contains(non_cached_limited_storage_keys_by_host_, host) &&
      !base::Contains(non_cached_unlimited_storage_keys_by_host_, host)) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    std::move(callback).Run(GetCachedHostUsage(host));
    return;
  }

  if (!host_usage_accumulators_.Add(
          host, base::BindOnce(&DidGetHostUsage, std::move(callback))))
    return;
  client_->GetStorageKeysForHost(
      type_, host,
      base::BindOnce(&ClientUsageTracker::DidGetStorageKeysForHostUsage,
                     weak_factory_.GetWeakPtr(), host));
}

void ClientUsageTracker::UpdateUsageCache(const blink::StorageKey& storage_key,
                                          int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = storage_key.origin().host();
  if (base::Contains(cached_hosts_, host)) {
    if (!IsUsageCacheEnabledForStorageKey(storage_key))
      return;

    // Constrain |delta| to avoid negative usage values.
    // TODO(michaeln): crbug/463729
    delta = std::max(delta, -cached_usage_by_host_[host][storage_key]);
    cached_usage_by_host_[host][storage_key] += delta;
    UpdateGlobalUsageValue(IsStorageUnlimited(storage_key)
                               ? &global_unlimited_usage_
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
    for (const auto& storage_key_and_usage : host_and_usage_map.second)
      usage += storage_key_and_usage.second;
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

std::map<blink::StorageKey, int64_t>
ClientUsageTracker::GetCachedStorageKeysUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<blink::StorageKey, int64_t> storage_key_usage;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& storage_key_and_usage : host_and_usage_map.second)
      storage_key_usage[storage_key_and_usage.first] +=
          storage_key_and_usage.second;
  }
  return storage_key_usage;
}

std::set<blink::StorageKey> ClientUsageTracker::GetCachedStorageKeys() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<blink::StorageKey> storage_keys;
  for (const auto& host_and_usage_map : cached_usage_by_host_) {
    for (const auto& storage_key_and_usage : host_and_usage_map.second)
      storage_keys.insert(storage_key_and_usage.first);
  }
  return storage_keys;
}

void ClientUsageTracker::SetUsageCacheEnabled(
    const blink::StorageKey& storage_key,
    bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = storage_key.origin().host();
  if (!enabled) {
    // Erase `storage_key` from cache and subtract its usage.
    auto host_it = cached_usage_by_host_.find(host);
    if (host_it != cached_usage_by_host_.end()) {
      UsageMap& cached_usage_for_host = host_it->second;

      auto storage_key_it = cached_usage_for_host.find(storage_key);
      if (storage_key_it != cached_usage_for_host.end()) {
        int64_t usage = storage_key_it->second;
        UpdateUsageCache(storage_key, -usage);
        cached_usage_for_host.erase(storage_key_it);
        if (cached_usage_for_host.empty()) {
          cached_usage_by_host_.erase(host_it);
          cached_hosts_.erase(host);
        }
      }
    }

    if (IsStorageUnlimited(storage_key))
      non_cached_unlimited_storage_keys_by_host_[host].insert(storage_key);
    else
      non_cached_limited_storage_keys_by_host_[host].insert(storage_key);
  } else {
    // Erase `storage_key` from `non_cached_storage_keys_` and invalidate the
    // usage cache for the host.
    if (EraseStorageKeyFromStorageKeySet(
            &non_cached_limited_storage_keys_by_host_, host, storage_key) ||
        EraseStorageKeyFromStorageKeySet(
            &non_cached_unlimited_storage_keys_by_host_, host, storage_key)) {
      cached_hosts_.erase(host);
      global_usage_retrieved_ = false;
    }
  }
}

void ClientUsageTracker::DidGetStorageKeysForGlobalUsage(
    GlobalUsageCallback callback,
    const std::vector<blink::StorageKey>& storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, std::vector<blink::StorageKey>> storage_keys_by_host;
  for (const auto& storage_key : storage_keys)
    storage_keys_by_host[storage_key.origin().host()].push_back(storage_key);

  AccumulateInfo* info = new AccumulateInfo;
  // Getting host usage may synchronously return the result if the usage is
  // cached, which may in turn dispatch the completion callback before we finish
  // looping over all hosts (because info->pending_jobs may reach 0 during the
  // loop).  To avoid this, we add one more pending host as a sentinel and
  // fire the sentinel callback at the end.
  info->pending_jobs = storage_keys_by_host.size() + 1;
  auto accumulator = base::BindRepeating(
      &ClientUsageTracker::AccumulateHostUsage, weak_factory_.GetWeakPtr(),
      base::Owned(info),
      // The `accumulator` is called multiple times, but the `callback` inside
      // of it will only be called a single time, so we give ownership to the
      // `accumulator` itself.
      base::OwnedRef(std::move(callback)));

  for (const auto& host_and_storage_keys : storage_keys_by_host) {
    const std::string& host = host_and_storage_keys.first;
    const std::vector<blink::StorageKey>& storage_keys_for_host =
        host_and_storage_keys.second;
    if (host_usage_accumulators_.Add(host, accumulator))
      GetUsageForStorageKeys(host, storage_keys_for_host);
  }

  // Fire the sentinel as we've now called GetUsageForStorageKeys for all
  // clients.
  std::move(accumulator).Run(0, 0);
}

void ClientUsageTracker::AccumulateHostUsage(AccumulateInfo* info,
                                             GlobalUsageCallback& callback,
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

void ClientUsageTracker::DidGetStorageKeysForHostUsage(
    const std::string& host,
    const std::vector<blink::StorageKey>& storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUsageForStorageKeys(host, storage_keys);
}

void ClientUsageTracker::GetUsageForStorageKeys(
    const std::string& host,
    const std::vector<blink::StorageKey>& storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AccumulateInfo* info = new AccumulateInfo;
  // Getting storage_key usage may synchronously return the result if the usage
  // is cached, which may in turn dispatch the completion callback before we
  // finish looping over all storage_keys (because info->pending_jobs may reach
  // 0 during the loop).  To avoid this, we add one more pending storage_key as
  // a sentinel and fire the sentinel callback at the end.
  info->pending_jobs = storage_keys.size() + 1;
  auto accumulator =
      base::BindRepeating(&ClientUsageTracker::AccumulateStorageKeyUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info), host);

  for (const auto& storage_key : storage_keys) {
    DCHECK_EQ(host, storage_key.origin().host());

    int64_t storage_key_usage = 0;
    if (GetCachedStorageKeyUsage(storage_key, &storage_key_usage)) {
      accumulator.Run(storage_key, storage_key_usage);
    } else {
      client_->GetStorageKeyUsage(storage_key, type_,
                                  base::BindOnce(accumulator, storage_key));
    }
  }

  // Fire the sentinel as we've now called GetStorageKeyUsage for all clients.
  accumulator.Run(absl::nullopt, 0);
}

void ClientUsageTracker::AccumulateStorageKeyUsage(
    AccumulateInfo* info,
    const std::string& host,
    const absl::optional<blink::StorageKey>& storage_key,
    int64_t usage) {
  DCHECK_GT(info->pending_jobs, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_key.has_value()) {
    // TODO(https://crbug.com/941480): `storage_key` should not be opaque or
    // have an empty url, but sometimes it is.
    if (storage_key->origin().opaque()) {
      DVLOG(1) << "AccumulateStorageKeyUsage for opaque storage_key!";
      RecordSkippedOriginHistogram(InvalidOriginReason::kIsOpaque);
    } else if (storage_key->origin().GetURL().is_empty()) {
      DVLOG(1) << "AccumulateStorageKeyUsage for storage_key with empty url!";
      RecordSkippedOriginHistogram(InvalidOriginReason::kIsEmpty);
    } else {
      if (usage < 0)
        usage = 0;

      if (IsStorageUnlimited(*storage_key))
        info->unlimited_usage += usage;
      else
        info->limited_usage += usage;
      if (IsUsageCacheEnabledForStorageKey(*storage_key))
        AddCachedStorageKey(*storage_key, usage);
    }
  }
  if (--info->pending_jobs)
    return;

  AddCachedHost(host);
  host_usage_accumulators_.Run(
      host, info->limited_usage, info->unlimited_usage);
}

void ClientUsageTracker::AddCachedStorageKey(
    const blink::StorageKey& storage_key,
    int64_t new_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsUsageCacheEnabledForStorageKey(storage_key));

  const std::string& host = storage_key.origin().host();
  int64_t* usage = &cached_usage_by_host_[host][storage_key];
  int64_t delta = new_usage - *usage;
  *usage = new_usage;
  if (delta) {
    UpdateGlobalUsageValue(IsStorageUnlimited(storage_key)
                               ? &global_unlimited_usage_
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
  for (const auto& storage_key_and_usage : usage_map)
    usage += storage_key_and_usage.second;
  return usage;
}

bool ClientUsageTracker::GetCachedStorageKeyUsage(
    const blink::StorageKey& storage_key,
    int64_t* usage) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = storage_key.origin().host();
  auto host_it = cached_usage_by_host_.find(host);
  if (host_it == cached_usage_by_host_.end())
    return false;

  auto storage_key_it = host_it->second.find(storage_key);
  if (storage_key_it == host_it->second.end())
    return false;

  DCHECK(IsUsageCacheEnabledForStorageKey(storage_key));
  *usage = storage_key_it->second;
  return true;
}

bool ClientUsageTracker::IsUsageCacheEnabledForStorageKey(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& host = storage_key.origin().host();
  return !StorageKeySetContainsStorageKey(
             non_cached_limited_storage_keys_by_host_, host, storage_key) &&
         !StorageKeySetContainsStorageKey(
             non_cached_unlimited_storage_keys_by_host_, host, storage_key);
}

void ClientUsageTracker::OnGranted(const url::Origin& origin_url,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1215208): Remove this conversion once the storage policy
  // APIs are converted to use StorageKey instead of Origin.
  const blink::StorageKey storage_key(origin_url);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64_t usage = 0;
    if (GetCachedStorageKeyUsage(storage_key, &usage)) {
      global_unlimited_usage_ += usage;
      global_limited_usage_ -= usage;
    }

    const std::string& host = storage_key.origin().host();
    if (EraseStorageKeyFromStorageKeySet(
            &non_cached_limited_storage_keys_by_host_, host, storage_key))
      non_cached_unlimited_storage_keys_by_host_[host].insert(storage_key);
  }
}

void ClientUsageTracker::OnRevoked(const url::Origin& origin_url,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1215208): Remove this conversion once the storage policy
  // APIs are converted to use StorageKey instead of Origin.
  const blink::StorageKey storage_key(origin_url);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64_t usage = 0;
    if (GetCachedStorageKeyUsage(storage_key, &usage)) {
      global_unlimited_usage_ -= usage;
      global_limited_usage_ += usage;
    }

    const std::string& host = storage_key.origin().host();
    if (EraseStorageKeyFromStorageKeySet(
            &non_cached_unlimited_storage_keys_by_host_, host, storage_key))
      non_cached_limited_storage_keys_by_host_[host].insert(storage_key);
  }
}

void ClientUsageTracker::OnCleared() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_limited_usage_ += global_unlimited_usage_;
  global_unlimited_usage_ = 0;

  for (const auto& host_and_storage_keys :
       non_cached_unlimited_storage_keys_by_host_) {
    const auto& host = host_and_storage_keys.first;
    for (const auto& storage_key : host_and_storage_keys.second)
      non_cached_limited_storage_keys_by_host_[host].insert(storage_key);
  }
  non_cached_unlimited_storage_keys_by_host_.clear();
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
    for (const auto& storage_key_and_usage : host_and_usage_map.second) {
      if (IsStorageUnlimited(storage_key_and_usage.first))
        global_unlimited_usage_ += storage_key_and_usage.second;
      else
        global_limited_usage_ += storage_key_and_usage.second;
    }
  }
}

bool ClientUsageTracker::IsStorageUnlimited(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type_ == blink::mojom::StorageType::kSyncable)
    return false;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(
             storage_key.origin().GetURL());
}

}  // namespace storage

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

MockQuotaManager::StorageKeyInfo::StorageKeyInfo(
    const StorageKey& storage_key,
    StorageType type,
    QuotaClientTypes quota_client_types,
    base::Time modified)
    : storage_key(storage_key),
      type(type),
      quota_client_types(std::move(quota_client_types)),
      modified(modified) {}

MockQuotaManager::StorageKeyInfo::~StorageKeyInfo() = default;

MockQuotaManager::StorageKeyInfo::StorageKeyInfo(
    MockQuotaManager::StorageKeyInfo&&) = default;
MockQuotaManager::StorageKeyInfo& MockQuotaManager::StorageKeyInfo::operator=(
    MockQuotaManager::StorageKeyInfo&&) = default;

MockQuotaManager::StorageInfo::StorageInfo()
    : usage(0), quota(std::numeric_limits<int64_t>::max()) {}
MockQuotaManager::StorageInfo::~StorageInfo() = default;

MockQuotaManager::MockQuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : QuotaManager(is_incognito,
                   profile_path,
                   std::move(io_thread),
                   /*quota_change_callback=*/base::DoNothing(),
                   std::move(special_storage_policy),
                   GetQuotaSettingsFunc()) {}

void MockQuotaManager::GetUsageAndQuota(const StorageKey& storage_key,
                                        StorageType type,
                                        UsageAndQuotaCallback callback) {
  StorageInfo& info = usage_and_quota_map_[std::make_pair(storage_key, type)];
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, info.usage,
                          info.quota);
}

void MockQuotaManager::SetQuota(const StorageKey& storage_key,
                                StorageType type,
                                int64_t quota) {
  usage_and_quota_map_[std::make_pair(storage_key, type)].quota = quota;
}

bool MockQuotaManager::AddStorageKey(const StorageKey& storage_key,
                                     StorageType type,
                                     QuotaClientTypes quota_client_types,
                                     base::Time modified) {
  storage_keys_.emplace_back(StorageKeyInfo(
      storage_key, type, std::move(quota_client_types), modified));
  return true;
}

bool MockQuotaManager::StorageKeyHasData(const StorageKey& storage_key,
                                         StorageType type,
                                         QuotaClientType quota_client) const {
  for (const auto& info : storage_keys_) {
    if (info.storage_key == storage_key && info.type == type &&
        info.quota_client_types.contains(quota_client))
      return true;
  }
  return false;
}

void MockQuotaManager::GetStorageKeysModifiedBetween(
    StorageType type,
    base::Time begin,
    base::Time end,
    GetStorageKeysCallback callback) {
  auto storage_keys_to_return = std::make_unique<std::set<StorageKey>>();
  for (const auto& info : storage_keys_) {
    if (info.type == type && info.modified >= begin && info.modified < end)
      storage_keys_to_return->insert(info.storage_key);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidGetModifiedInTimeRange,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                std::move(storage_keys_to_return), type));
}

void MockQuotaManager::DeleteStorageKeyData(const StorageKey& storage_key,
                                            StorageType type,
                                            QuotaClientTypes quota_client_types,
                                            StatusCallback callback) {
  for (auto current = storage_keys_.begin(); current != storage_keys_.end();
       ++current) {
    if (current->storage_key == storage_key && current->type == type) {
      // Modify the mask: if it's 0 after "deletion", remove the storage key.
      for (QuotaClientType type : quota_client_types)
        current->quota_client_types.erase(type);
      if (current->quota_client_types.empty())
        storage_keys_.erase(current);
      break;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidDeleteStorageKeyData,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                blink::mojom::QuotaStatusCode::kOk));
}

void MockQuotaManager::NotifyWriteFailed(const StorageKey& storage_key) {
  auto storage_key_error_log =
      write_error_tracker_.insert(std::pair<StorageKey, int>(storage_key, 0))
          .first;
  ++storage_key_error_log->second;
}

MockQuotaManager::~MockQuotaManager() = default;

void MockQuotaManager::UpdateUsage(const StorageKey& storage_key,
                                   StorageType type,
                                   int64_t delta) {
  usage_and_quota_map_[std::make_pair(storage_key, type)].usage += delta;
}

void MockQuotaManager::DidGetModifiedInTimeRange(
    GetStorageKeysCallback callback,
    std::unique_ptr<std::set<StorageKey>> storage_keys,
    StorageType storage_type) {
  std::move(callback).Run(*storage_keys, storage_type);
}

void MockQuotaManager::DidDeleteStorageKeyData(
    StatusCallback callback,
    blink::mojom::QuotaStatusCode status) {
  std::move(callback).Run(status);
}

}  // namespace storage

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
#include "url/gurl.h"

namespace storage {

MockQuotaManager::OriginInfo::OriginInfo(const url::Origin& origin,
                                         blink::mojom::StorageType type,
                                         QuotaClientTypes quota_client_types,
                                         base::Time modified)
    : origin(origin),
      type(type),
      quota_client_types(std::move(quota_client_types)),
      modified(modified) {}

MockQuotaManager::OriginInfo::~OriginInfo() = default;

MockQuotaManager::OriginInfo::OriginInfo(MockQuotaManager::OriginInfo&&) =
    default;
MockQuotaManager::OriginInfo& MockQuotaManager::OriginInfo::operator=(
    MockQuotaManager::OriginInfo&&) = default;

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

void MockQuotaManager::GetUsageAndQuota(const url::Origin& origin,
                                        blink::mojom::StorageType type,
                                        UsageAndQuotaCallback callback) {
  StorageInfo& info = usage_and_quota_map_[std::make_pair(origin, type)];
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, info.usage,
                          info.quota);
}

void MockQuotaManager::SetQuota(const url::Origin& origin,
                                StorageType type,
                                int64_t quota) {
  usage_and_quota_map_[std::make_pair(origin, type)].quota = quota;
}

bool MockQuotaManager::AddOrigin(const url::Origin& origin,
                                 blink::mojom::StorageType type,
                                 QuotaClientTypes quota_client_types,
                                 base::Time modified) {
  origins_.push_back(
      OriginInfo(origin, type, std::move(quota_client_types), modified));
  return true;
}

bool MockQuotaManager::OriginHasData(const url::Origin& origin,
                                     blink::mojom::StorageType type,
                                     QuotaClientType quota_client) const {
  for (const auto& info : origins_) {
    if (info.origin == origin && info.type == type &&
        info.quota_client_types.contains(quota_client))
      return true;
  }
  return false;
}

void MockQuotaManager::GetOriginsModifiedBetween(blink::mojom::StorageType type,
                                                 base::Time begin,
                                                 base::Time end,
                                                 GetOriginsCallback callback) {
  auto origins_to_return = std::make_unique<std::set<url::Origin>>();
  for (const auto& info : origins_) {
    if (info.type == type && info.modified >= begin && info.modified < end)
      origins_to_return->insert(info.origin);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidGetModifiedInTimeRange,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                std::move(origins_to_return), type));
}

void MockQuotaManager::DeleteOriginData(const url::Origin& origin,
                                        blink::mojom::StorageType type,
                                        QuotaClientTypes quota_client_types,
                                        StatusCallback callback) {
  for (auto current = origins_.begin(); current != origins_.end(); ++current) {
    if (current->origin == origin && current->type == type) {
      // Modify the mask: if it's 0 after "deletion", remove the origin.
      for (QuotaClientType type : quota_client_types)
        current->quota_client_types.erase(type);
      if (current->quota_client_types.empty())
        origins_.erase(current);
      break;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidDeleteOriginData,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                blink::mojom::QuotaStatusCode::kOk));
}

void MockQuotaManager::NotifyWriteFailed(const url::Origin& origin) {
  auto origin_error_log =
      write_error_tracker_.insert(std::pair<url::Origin, int>(origin, 0)).first;
  ++origin_error_log->second;
}

MockQuotaManager::~MockQuotaManager() = default;

void MockQuotaManager::UpdateUsage(const url::Origin& origin,
                                   blink::mojom::StorageType type,
                                   int64_t delta) {
  usage_and_quota_map_[std::make_pair(origin, type)].usage += delta;
}

void MockQuotaManager::DidGetModifiedInTimeRange(
    GetOriginsCallback callback,
    std::unique_ptr<std::set<url::Origin>> origins,
    blink::mojom::StorageType storage_type) {
  std::move(callback).Run(*origins, storage_type);
}

void MockQuotaManager::DidDeleteOriginData(
    StatusCallback callback,
    blink::mojom::QuotaStatusCode status) {
  std::move(callback).Run(status);
}

}  // namespace storage

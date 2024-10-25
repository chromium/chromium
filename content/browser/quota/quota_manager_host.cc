// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_manager_host.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

QuotaManagerHost::QuotaManagerHost(const blink::StorageKey& storage_key,
                                   storage::QuotaManager* quota_manager)
    : storage_key_(storage_key), quota_manager_(quota_manager) {
  DCHECK(quota_manager);
}

void QuotaManagerHost::QueryStorageUsageAndQuota(
    QueryStorageUsageAndQuotaCallback callback) {
  quota_manager_->GetUsageAndReportedQuotaWithBreakdown(
      storage_key_, blink::mojom::StorageType::kTemporary,
      base::BindOnce(&QuotaManagerHost::DidQueryStorageUsageAndQuota,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerHost::DidQueryStorageUsageAndQuota(
    QueryStorageUsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(status, usage, quota, std::move(usage_breakdown));
}

QuotaManagerHost::~QuotaManagerHost() = default;

}  // namespace content

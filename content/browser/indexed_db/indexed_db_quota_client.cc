// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/database/database_util.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using storage::DatabaseUtil;
using storage::mojom::QuotaClient;

namespace content {

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl& indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBQuotaClient::GetOriginUsage(const url::Origin& origin,
                                          StorageType type,
                                          GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);

  std::move(callback).Run(indexed_db_context_.GetOriginDiskUsage(origin));
}

void IndexedDBQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);

  std::move(callback).Run(indexed_db_context_.GetAllOrigins());
}

void IndexedDBQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);

  std::vector<url::Origin> host_origins;
  // In the vast majority of cases, this vector will end up with exactly one
  // origin. The origin will be https://host or http://host.
  host_origins.reserve(1);

  for (auto& origin : indexed_db_context_.GetAllOrigins()) {
    if (host == origin.host())
      host_origins.push_back(std::move(origin));
  }
  std::move(callback).Run(std::move(host_origins));
}

void IndexedDBQuotaClient::DeleteOriginData(const url::Origin& origin,
                                            StorageType type,
                                            DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_.DeleteForOrigin(
      origin, base::BindOnce(
                  [](DeleteOriginDataCallback callback, bool success) {
                    blink::mojom::QuotaStatusCode status =
                        success ? blink::mojom::QuotaStatusCode::kOk
                                : blink::mojom::QuotaStatusCode::kUnknown;
                    std::move(callback).Run(status);
                  },
                  std::move(callback)));
}

void IndexedDBQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::move(callback).Run();
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_quota_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using storage::QuotaClient;

namespace content {
namespace {
void ReportToQuotaStatus(QuotaClient::DeleteOriginDataCallback callback,
                         bool status) {
  std::move(callback).Run(status ? blink::mojom::QuotaStatusCode::kOk
                                 : blink::mojom::QuotaStatusCode::kUnknown);
}

void FindUsageForOrigin(QuotaClient::GetOriginUsageCallback callback,
                        blink::ServiceWorkerStatusCode status,
                        int64_t usage) {
  std::move(callback).Run(usage);
}
}  // namespace

ServiceWorkerQuotaClient::ServiceWorkerQuotaClient(
    ServiceWorkerContextWrapper* context)
    : context_(context) {
  DCHECK(context);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ServiceWorkerQuotaClient::~ServiceWorkerQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceWorkerQuotaClient::GetOriginUsage(const url::Origin& origin,
                                              StorageType type,
                                              GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->GetStorageUsageForOrigin(
      origin, base::BindOnce(&FindUsageForOrigin, std::move(callback)));
}

void ServiceWorkerQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->GetInstalledRegistrationOrigins(base::nullopt, std::move(callback));
}

void ServiceWorkerQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->GetInstalledRegistrationOrigins(host, std::move(callback));
}

void ServiceWorkerQuotaClient::DeleteOriginData(
    const url::Origin& origin,
    StorageType type,
    DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->DeleteForOrigin(
      origin, base::BindOnce(&ReportToQuotaStatus, std::move(callback)));
}

void ServiceWorkerQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->PerformStorageCleanup(std::move(callback));
}

}  // namespace content

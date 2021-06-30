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
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;
using ::storage::mojom::QuotaClient;

namespace content {
namespace {
void ReportToQuotaStatus(QuotaClient::DeleteStorageKeyDataCallback callback,
                         blink::ServiceWorkerStatusCode status) {
  std::move(callback).Run((status == blink::ServiceWorkerStatusCode::kOk)
                              ? blink::mojom::QuotaStatusCode::kOk
                              : blink::mojom::QuotaStatusCode::kUnknown);
}

void FindUsageForStorageKey(QuotaClient::GetStorageKeyUsageCallback callback,
                            blink::ServiceWorkerStatusCode status,
                            int64_t usage) {
  std::move(callback).Run(usage);
}
}  // namespace

ServiceWorkerQuotaClient::ServiceWorkerQuotaClient(
    ServiceWorkerContextCore& context)
    : context_(&context) {}

ServiceWorkerQuotaClient::~ServiceWorkerQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceWorkerQuotaClient::GetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    StorageType type,
    GetStorageKeyUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->GetStorageUsageForStorageKey(
      storage_key,
      base::BindOnce(&FindUsageForStorageKey, std::move(callback)));
}

void ServiceWorkerQuotaClient::GetStorageKeysForType(
    StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->GetRegisteredStorageKeys(std::move(callback));
}

void ServiceWorkerQuotaClient::GetStorageKeysForHost(
    StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->GetRegisteredStorageKeys(base::BindOnce(
      [](const std::string& host, GetStorageKeysForTypeCallback callback,
         const std::vector<blink::StorageKey>& all_storage_keys) {
        std::vector<blink::StorageKey> host_storage_keys;
        for (auto& storage_key : all_storage_keys) {
          if (host != storage_key.origin().host())
            continue;
          host_storage_keys.push_back(storage_key);
        }
        std::move(callback).Run(host_storage_keys);
      },
      host, std::move(callback)));
}

void ServiceWorkerQuotaClient::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    StorageType type,
    DeleteStorageKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->DeleteForStorageKey(
      storage_key, base::BindOnce(&ReportToQuotaStatus, std::move(callback)));
}

void ServiceWorkerQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->PerformStorageCleanup(std::move(callback));
}

}  // namespace content

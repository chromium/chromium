// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_quota_client.h"

#include "base/sequence_checker.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace content {

NativeIOQuotaClient::NativeIOQuotaClient(NativeIOManager* manager)
    : manager_(manager) {}

NativeIOQuotaClient::~NativeIOQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         blink::mojom::StorageType type,
                                         GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetOriginUsage(origin, type, std::move(callback));
  return;
}

void NativeIOQuotaClient::GetOriginsForType(
    blink::mojom::StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetOriginsForType(type, std::move(callback));
}

void NativeIOQuotaClient::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetOriginsForHost(type, std::move(host), std::move(callback));
}

void NativeIOQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           blink::mojom::StorageType type,
                                           DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->DeleteOriginData(origin, std::move(callback));
}

void NativeIOQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
}

}  // namespace content

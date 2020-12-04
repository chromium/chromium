// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_quota_client.h"

#include "base/sequence_checker.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace content {

NativeIOQuotaClient::NativeIOQuotaClient() {
  // Constructed on the UI thread and used on the IO thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NativeIOQuotaClient::~NativeIOQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOQuotaClient::OnQuotaManagerDestroyed() {}

void NativeIOQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         blink::mojom::StorageType type,
                                         GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  // TODO(crbug.com/1137788): Implement quota accounting.
  std::move(callback).Run(0);
  return;
}

void NativeIOQuotaClient::GetOriginsForType(
    blink::mojom::StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  std::vector<url::Origin> origins;
  // TODO(crbug.com/1137788): Implement quota accounting.
  std::move(callback).Run(std::move(origins));
}

void NativeIOQuotaClient::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  std::vector<url::Origin> origins;
  // TODO(crbug.com/1137788): Implement quota accounting.
  std::move(callback).Run(std::move(origins));
}

void NativeIOQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           blink::mojom::StorageType type,
                                           DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  // TODO(crbug.com/1137788): Implement quota accounting.
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

void NativeIOQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1137788): Implement quota accounting.
  std::move(callback).Run();
}

}  // namespace content

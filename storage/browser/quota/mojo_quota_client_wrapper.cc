// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/mojo_quota_client_wrapper.h"

#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "url/origin.h"

namespace storage {

MojoQuotaClientWrapper::MojoQuotaClientWrapper(
    mojom::QuotaClient* wrapped_client)
    : wrapped_client_(wrapped_client) {
  DCHECK(wrapped_client);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MojoQuotaClientWrapper::~MojoQuotaClientWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoQuotaClientWrapper::GetOriginUsage(const url::Origin& origin,
                                            blink::mojom::StorageType type,
                                            GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginUsage(origin, type, std::move(callback));
}

void MojoQuotaClientWrapper::GetOriginsForType(
    blink::mojom::StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginsForType(type, std::move(callback));
}

void MojoQuotaClientWrapper::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginsForHost(type, host, std::move(callback));
}

void MojoQuotaClientWrapper::DeleteOriginData(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->DeleteOriginData(origin, type, std::move(callback));
}

void MojoQuotaClientWrapper::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->PerformStorageCleanup(type, std::move(callback));
}

void MojoQuotaClientWrapper::OnQuotaManagerDestroyed() {}

}  // namespace storage

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_quota_client.h"

#include "base/sequence_checker.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

NativeIOQuotaClient::NativeIOQuotaClient(NativeIOManager* manager)
    : manager_(manager) {}

NativeIOQuotaClient::~NativeIOQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOQuotaClient::GetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    GetStorageKeyUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetStorageKeyUsage(storage_key, type, std::move(callback));
  return;
}

void NativeIOQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetStorageKeysForType(type, std::move(callback));
}

void NativeIOQuotaClient::GetStorageKeysForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetStorageKeysForHost(type, std::move(host), std::move(callback));
}

void NativeIOQuotaClient::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    DeleteStorageKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->DeleteStorageKeyData(storage_key, std::move(callback));
}

void NativeIOQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
}

}  // namespace content

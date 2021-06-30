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
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;
using ::storage::DatabaseUtil;
using ::storage::mojom::QuotaClient;

namespace content {

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl& indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBQuotaClient::GetStorageKeyUsage(
    const StorageKey& storage_key,
    StorageType type,
    GetStorageKeyUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);

  std::move(callback).Run(
      indexed_db_context_.GetStorageKeyDiskUsage(storage_key));
}

void IndexedDBQuotaClient::GetStorageKeysForType(
    StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::vector<StorageKey> storage_keys =
      indexed_db_context_.GetAllStorageKeys();
  std::move(callback).Run(std::move(storage_keys));
}

void IndexedDBQuotaClient::GetStorageKeysForHost(
    StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);

  std::vector<StorageKey> host_storage_keys;
  // In the vast majority of cases, this vector will end up with exactly one
  // storage key. The storage key will be https://host or http://host.
  host_storage_keys.reserve(1);

  for (auto& storage_key : indexed_db_context_.GetAllStorageKeys()) {
    if (host == storage_key.origin().host())
      host_storage_keys.push_back(std::move(storage_key));
  }
  std::move(callback).Run(std::move(host_storage_keys));
}

void IndexedDBQuotaClient::DeleteStorageKeyData(
    const StorageKey& storage_key,
    StorageType type,
    DeleteStorageKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_.DeleteForStorageKey(
      storage_key,
      base::BindOnce(
          [](DeleteStorageKeyDataCallback callback, bool success) {
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

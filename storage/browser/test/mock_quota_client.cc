// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage {

MockQuotaClient::MockQuotaClient(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    base::span<const MockStorageKeyData> mock_data,
    QuotaClientType client_type)
    : quota_manager_proxy_(std::move(quota_manager_proxy)),
      client_type_(client_type) {
  for (const MockStorageKeyData& mock_storage_key_data : mock_data) {
    storage_key_data_[{blink::StorageKey::CreateFromStringForTesting(
                           mock_storage_key_data.origin),
                       mock_storage_key_data.type}] =
        mock_storage_key_data.usage;
  }
}

MockQuotaClient::~MockQuotaClient() = default;

void MockQuotaClient::AddStorageKeyAndNotify(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType storage_type,
    int64_t size) {
  DCHECK(storage_key_data_.find({storage_key, storage_type}) ==
         storage_key_data_.end());
  DCHECK_GE(size, 0);
  storage_key_data_[{storage_key, storage_type}] = size;
  quota_manager_proxy_->NotifyStorageModified(
      client_type_, storage_key, storage_type, size, IncrementMockTime());
}

void MockQuotaClient::ModifyStorageKeyAndNotify(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType storage_type,
    int64_t delta) {
  auto it = storage_key_data_.find({storage_key, storage_type});
  DCHECK(it != storage_key_data_.end());
  it->second += delta;
  DCHECK_GE(it->second, 0);

  // TODO(tzik): Check quota to prevent usage exceed
  quota_manager_proxy_->NotifyStorageModified(
      client_type_, storage_key, storage_type, delta, IncrementMockTime());
}

void MockQuotaClient::TouchAllStorageKeysAndNotify() {
  for (const auto& storage_key_type : storage_key_data_) {
    quota_manager_proxy_->NotifyStorageModified(
        client_type_, storage_key_type.first.first,
        storage_key_type.first.second, 0, IncrementMockTime());
  }
}

void MockQuotaClient::AddStorageKeyToErrorSet(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type) {
  error_storage_keys_.insert(std::make_pair(storage_key, type));
}

base::Time MockQuotaClient::IncrementMockTime() {
  ++mock_time_counter_;
  return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
}

void MockQuotaClient::GetStorageKeyUsage(const blink::StorageKey& storage_key,
                                         blink::mojom::StorageType type,
                                         GetStorageKeyUsageCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunGetStorageKeyUsage,
                                weak_factory_.GetWeakPtr(), storage_key, type,
                                std::move(callback)));
}

void MockQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQuotaClient::RunGetStorageKeysForType,
                     weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void MockQuotaClient::GetStorageKeysForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunGetStorageKeysForHost,
                                weak_factory_.GetWeakPtr(), type, host,
                                std::move(callback)));
}

void MockQuotaClient::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    DeleteStorageKeyDataCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunDeleteStorageKeyData,
                                weak_factory_.GetWeakPtr(), storage_key, type,
                                std::move(callback)));
}

void MockQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  std::move(callback).Run();
}

void MockQuotaClient::RunGetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    GetStorageKeyUsageCallback callback) {
  auto it = storage_key_data_.find(std::make_pair(storage_key, type));
  if (it == storage_key_data_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(it->second);
  }
}

void MockQuotaClient::RunGetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  std::vector<blink::StorageKey> storage_keys;
  for (const auto& storage_key_type_usage : storage_key_data_) {
    if (type == storage_key_type_usage.first.second)
      storage_keys.push_back(storage_key_type_usage.first.first);
  }
  std::move(callback).Run(std::move(storage_keys));
}

void MockQuotaClient::RunGetStorageKeysForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  std::vector<blink::StorageKey> storage_keys;
  for (const auto& storage_key_type_usage : storage_key_data_) {
    if (type == storage_key_type_usage.first.second &&
        host == storage_key_type_usage.first.first.origin().host()) {
      storage_keys.push_back(storage_key_type_usage.first.first);
    }
  }
  std::move(callback).Run(std::move(storage_keys));
}

void MockQuotaClient::RunDeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType storage_type,
    DeleteStorageKeyDataCallback callback) {
  auto error_it =
      error_storage_keys_.find(std::make_pair(storage_key, storage_type));
  if (error_it != error_storage_keys_.end()) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    return;
  }

  auto it = storage_key_data_.find(std::make_pair(storage_key, storage_type));
  if (it != storage_key_data_.end()) {
    int64_t delta = it->second;
    quota_manager_proxy_->NotifyStorageModified(
        client_type_, blink::StorageKey(storage_key), storage_type, -delta,
        base::Time::Now());
    storage_key_data_.erase(it);
  }

  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace storage

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
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

MockQuotaClient::MockQuotaClient(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    base::span<const MockOriginData> mock_data,
    QuotaClientType client_type)
    : quota_manager_proxy_(std::move(quota_manager_proxy)),
      client_type_(client_type),
      mock_time_counter_(0) {
  for (const MockOriginData& mock_origin_data : mock_data) {
    // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
    origin_data_[{url::Origin::Create(GURL(mock_origin_data.origin)),
                  mock_origin_data.type}] = mock_origin_data.usage;
  }
}

MockQuotaClient::~MockQuotaClient() = default;

void MockQuotaClient::AddOriginAndNotify(const url::Origin& origin,
                                         blink::mojom::StorageType storage_type,
                                         int64_t size) {
  DCHECK(origin_data_.find({origin, storage_type}) == origin_data_.end());
  DCHECK_GE(size, 0);
  origin_data_[{origin, storage_type}] = size;
  quota_manager_proxy_->NotifyStorageModified(
      client_type_, origin, storage_type, size, IncrementMockTime());
}

void MockQuotaClient::ModifyOriginAndNotify(
    const url::Origin& origin,
    blink::mojom::StorageType storage_type,
    int64_t delta) {
  auto it = origin_data_.find({origin, storage_type});
  DCHECK(it != origin_data_.end());
  it->second += delta;
  DCHECK_GE(it->second, 0);

  // TODO(tzik): Check quota to prevent usage exceed
  quota_manager_proxy_->NotifyStorageModified(
      client_type_, origin, storage_type, delta, IncrementMockTime());
}

void MockQuotaClient::TouchAllOriginsAndNotify() {
  for (const auto& origin_type : origin_data_) {
    quota_manager_proxy_->NotifyStorageModified(
        client_type_, origin_type.first.first, origin_type.first.second, 0,
        IncrementMockTime());
  }
}

void MockQuotaClient::AddOriginToErrorSet(const url::Origin& origin,
                                          blink::mojom::StorageType type) {
  error_origins_.insert(std::make_pair(origin, type));
}

base::Time MockQuotaClient::IncrementMockTime() {
  ++mock_time_counter_;
  return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
}

void MockQuotaClient::GetOriginUsage(const url::Origin& origin,
                                     blink::mojom::StorageType type,
                                     GetOriginUsageCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunGetOriginUsage,
                                weak_factory_.GetWeakPtr(), origin, type,
                                std::move(callback)));
}

void MockQuotaClient::GetOriginsForType(blink::mojom::StorageType type,
                                        GetOriginsForTypeCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQuotaClient::RunGetOriginsForType,
                     weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void MockQuotaClient::GetOriginsForHost(blink::mojom::StorageType type,
                                        const std::string& host,
                                        GetOriginsForHostCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunGetOriginsForHost,
                                weak_factory_.GetWeakPtr(), type, host,
                                std::move(callback)));
}

void MockQuotaClient::DeleteOriginData(const url::Origin& origin,
                                       blink::mojom::StorageType type,
                                       DeleteOriginDataCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaClient::RunDeleteOriginData,
                                weak_factory_.GetWeakPtr(), origin, type,
                                std::move(callback)));
}

void MockQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  std::move(callback).Run();
}

void MockQuotaClient::RunGetOriginUsage(const url::Origin& origin,
                                        blink::mojom::StorageType type,
                                        GetOriginUsageCallback callback) {
  auto it = origin_data_.find(std::make_pair(origin, type));
  if (it == origin_data_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(it->second);
  }
}

void MockQuotaClient::RunGetOriginsForType(blink::mojom::StorageType type,
                                           GetOriginsForTypeCallback callback) {
  std::vector<url::Origin> origins;
  for (const auto& origin_type_usage : origin_data_) {
    if (type == origin_type_usage.first.second)
      origins.push_back(origin_type_usage.first.first);
  }
  std::move(callback).Run(std::move(origins));
}

void MockQuotaClient::RunGetOriginsForHost(blink::mojom::StorageType type,
                                           const std::string& host,
                                           GetOriginsForHostCallback callback) {
  std::vector<url::Origin> origins;
  for (const auto& origin_type_usage : origin_data_) {
    if (type == origin_type_usage.first.second &&
        host == origin_type_usage.first.first.host()) {
      origins.push_back(origin_type_usage.first.first);
    }
  }
  std::move(callback).Run(std::move(origins));
}

void MockQuotaClient::RunDeleteOriginData(
    const url::Origin& origin,
    blink::mojom::StorageType storage_type,
    DeleteOriginDataCallback callback) {
  auto error_it = error_origins_.find(std::make_pair(origin, storage_type));
  if (error_it != error_origins_.end()) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    return;
  }

  auto it = origin_data_.find(std::make_pair(origin, storage_type));
  if (it != origin_data_.end()) {
    int64_t delta = it->second;
    quota_manager_proxy_->NotifyStorageModified(
        client_type_, origin, storage_type, -delta, base::Time::Now());
    origin_data_.erase(it);
  }

  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace storage

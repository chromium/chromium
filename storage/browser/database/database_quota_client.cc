// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_quota_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;

namespace storage {

DatabaseQuotaClient::DatabaseQuotaClient(DatabaseTracker& db_tracker)
    : db_tracker_(db_tracker) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DatabaseQuotaClient::~DatabaseQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DatabaseQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         StorageType type,
                                         GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  OriginInfo info;
  if (db_tracker_.GetOriginInfo(GetIdentifierFromOrigin(origin), &info)) {
    std::move(callback).Run(info.TotalSize());
  } else {
    std::move(callback).Run(0);
  }
}

void DatabaseQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  std::vector<url::Origin> all_origins;
  std::vector<std::string> origin_identifiers;
  if (db_tracker_.GetAllOriginIdentifiers(&origin_identifiers)) {
    all_origins.reserve(origin_identifiers.size());
    for (const auto& identifier : origin_identifiers)
      all_origins.push_back(GetOriginFromIdentifier(identifier));
  }
  std::move(callback).Run(all_origins);
}

void DatabaseQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  std::vector<url::Origin> host_origins;
  // In the vast majority of cases, this vector will end up with exactly one
  // origin. The origin will be https://host or http://host.
  host_origins.reserve(1);

  std::vector<std::string> origin_identifiers;
  if (db_tracker_.GetAllOriginIdentifiers(&origin_identifiers)) {
    for (const auto& identifier : origin_identifiers) {
      url::Origin origin = GetOriginFromIdentifier(identifier);
      if (host == origin.host())
        host_origins.push_back(std::move(origin));
    }
  }
  std::move(callback).Run(host_origins);
}

void DatabaseQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           StorageType type,
                                           DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  db_tracker_.DeleteDataForOrigin(
      origin, base::BindOnce(
                  [](DeleteOriginDataCallback callback, int result) {
                    std::move(callback).Run(
                        (result == net::OK)
                            ? blink::mojom::QuotaStatusCode::kOk
                            : blink::mojom::QuotaStatusCode::kUnknown);
                  },
                  std::move(callback)));
}

void DatabaseQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  std::move(callback).Run();
}

}  // namespace storage

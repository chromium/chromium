// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_quota_client.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
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

namespace {

int64_t GetOriginUsageOnDBThread(DatabaseTracker* db_tracker,
                                 const url::Origin& origin) {
  OriginInfo info;
  if (db_tracker->GetOriginInfo(GetIdentifierFromOrigin(origin), &info))
    return info.TotalSize();
  return 0;
}

void GetOriginsOnDBThread(DatabaseTracker* db_tracker,
                          std::vector<url::Origin>* origins_ptr) {
  std::vector<std::string> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (const auto& identifier : origin_identifiers) {
      origins_ptr->push_back(GetOriginFromIdentifier(identifier));
    }
  }
}

void GetOriginsForHostOnDBThread(DatabaseTracker* db_tracker,
                                 std::vector<url::Origin>* origins_ptr,
                                 const std::string& host) {
  std::vector<std::string> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (const auto& identifier : origin_identifiers) {
      url::Origin origin = GetOriginFromIdentifier(identifier);
      if (host == origin.host())
        origins_ptr->push_back(std::move(origin));
    }
  }
}

void DidGetQuotaClientOrigins(QuotaClient::GetOriginsForTypeCallback callback,
                              std::vector<url::Origin>* origins_ptr) {
  std::move(callback).Run(*origins_ptr);
}

void DidDeleteOriginData(base::SequencedTaskRunner* original_task_runner,
                         QuotaClient::DeleteOriginDataCallback callback,
                         int result) {
  if (result == net::ERR_IO_PENDING) {
    // The callback will be invoked via
    // DatabaseTracker::ScheduleDatabasesForDeletion.
    return;
  }

  blink::mojom::QuotaStatusCode status;
  if (result == net::OK)
    status = blink::mojom::QuotaStatusCode::kOk;
  else
    status = blink::mojom::QuotaStatusCode::kUnknown;

  original_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
}

}  // namespace

DatabaseQuotaClient::DatabaseQuotaClient(
    scoped_refptr<DatabaseTracker> db_tracker)
    : db_tracker_(std::move(db_tracker)) {}

DatabaseQuotaClient::~DatabaseQuotaClient() {
  if (!db_tracker_->task_runner()->RunsTasksInCurrentSequence()) {
    db_tracker_->task_runner()->ReleaseSoon(FROM_HERE, std::move(db_tracker_));
  }
}

void DatabaseQuotaClient::OnQuotaManagerDestroyed() {}

void DatabaseQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         StorageType type,
                                         GetOriginUsageCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());
  DCHECK_EQ(type, StorageType::kTemporary);

  base::PostTaskAndReplyWithResult(
      db_tracker_->task_runner(), FROM_HERE,
      base::BindOnce(&GetOriginUsageOnDBThread, base::RetainedRef(db_tracker_),
                     origin),
      std::move(callback));
}

void DatabaseQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());
  DCHECK_EQ(type, StorageType::kTemporary);

  auto* origins_ptr = new std::vector<url::Origin>();
  db_tracker_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsOnDBThread, base::RetainedRef(db_tracker_),
                     base::Unretained(origins_ptr)),
      base::BindOnce(&DidGetQuotaClientOrigins, std::move(callback),
                     base::Owned(origins_ptr)));
}

void DatabaseQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());
  DCHECK_EQ(type, StorageType::kTemporary);

  auto* origins_ptr = new std::vector<url::Origin>();
  db_tracker_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnDBThread,
                     base::RetainedRef(db_tracker_),
                     base::Unretained(origins_ptr), host),
      base::BindOnce(&DidGetQuotaClientOrigins, std::move(callback),
                     base::Owned(origins_ptr)));
}

void DatabaseQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           StorageType type,
                                           DeleteOriginDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());
  DCHECK_EQ(type, StorageType::kTemporary);

  // DidDeleteOriginData() translates the net::Error response to a
  // blink::mojom::QuotaStatusCode if necessary, and no-ops as appropriate if
  // DatabaseTracker::ScheduleDatabasesForDeletion will also invoke the
  // callback.
  auto delete_callback = base::BindRepeating(
      &DidDeleteOriginData,
      base::RetainedRef(base::SequencedTaskRunnerHandle::Get()),
      base::AdaptCallbackForRepeating(std::move(callback)));

  base::PostTaskAndReplyWithResult(
      db_tracker_->task_runner(), FROM_HERE,
      base::BindOnce(&DatabaseTracker::DeleteDataForOrigin, db_tracker_, origin,
                     delete_callback),
      net::CompletionOnceCallback(delete_callback));
}

void DatabaseQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);
  std::move(callback).Run();
}

}  // namespace storage

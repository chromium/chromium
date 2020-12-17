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
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using storage::DatabaseUtil;
using storage::QuotaClient;

namespace content {
namespace {

void DidDeleteIDBData(scoped_refptr<base::SequencedTaskRunner> task_runner,
                      IndexedDBQuotaClient::DeleteOriginDataCallback callback,
                      bool success) {
  // Runs on the IDB task runner. Called asynchronously by
  // IndexedDBContextImpl::DeleteForOrigin().
  blink::mojom::QuotaStatusCode status =
      success ? blink::mojom::QuotaStatusCode::kOk
              : blink::mojom::QuotaStatusCode::kUnknown;
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), status));
}

int64_t GetOriginUsageOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const url::Origin& origin) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());
  return context->GetOriginDiskUsage(origin);
}

std::vector<url::Origin> GetAllOriginsOnIndexedDBThread(
    IndexedDBContextImpl* context) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());
  return context->GetAllOrigins();
}

void DidGetOrigins(IndexedDBQuotaClient::GetOriginsForTypeCallback callback,
                   std::vector<url::Origin> origins) {
  // Run on the same sequence that GetOriginsForType was called on,
  // which is likely the IO thread.
  std::move(callback).Run(std::move(origins));
}

std::vector<url::Origin> GetOriginsForHostOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const std::string& host) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());

  std::vector<url::Origin> origins_to_return;
  // In the vast majority of cases, this vector will end up with exactly one
  // origin. The origin will be https://host or http://host.
  origins_to_return.reserve(1);

  std::vector<url::Origin> all_origins = context->GetAllOrigins();
  for (auto& origin : all_origins) {
    if (host == origin.host())
      origins_to_return.push_back(std::move(origin));
  }
  return origins_to_return;
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context)
    : indexed_db_context_(std::move(indexed_db_context)) {
  DCHECK(indexed_db_context_.get());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  IndexedDBContextImpl::ReleaseOnIDBSequence(std::move(indexed_db_context_));
}

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBQuotaClient::GetOriginUsage(const url::Origin& origin,
                                          StorageType type,
                                          GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_->IDBTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetOriginUsageOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), origin),
      std::move(callback));
}

void IndexedDBQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_->IDBTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetAllOriginsOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_)),
      base::BindOnce(&DidGetOrigins, std::move(callback)));
}

void IndexedDBQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_->IDBTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), host),
      base::BindOnce(&DidGetOrigins, std::move(callback)));
}

void IndexedDBQuotaClient::DeleteOriginData(const url::Origin& origin,
                                            StorageType type,
                                            DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBContextImpl::DeleteForOrigin,
                     base::RetainedRef(indexed_db_context_), origin,
                     base::BindOnce(DidDeleteIDBData,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback))));
}

void IndexedDBQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::move(callback).Run();
}

}  // namespace content

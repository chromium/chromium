// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
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
                      bool) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), blink::mojom::QuotaStatusCode::kOk));
}

int64_t GetOriginUsageOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const url::Origin& origin) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());
  return context->GetOriginDiskUsage(origin);
}

void GetAllOriginsOnIndexedDBThread(
    IndexedDBContextImpl* context,
    std::vector<url::Origin>* origins_to_return) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());
  *origins_to_return = context->GetAllOrigins();
}

void DidGetOrigins(IndexedDBQuotaClient::GetOriginsForTypeCallback callback,
                   const std::vector<url::Origin>* origins) {
  // Run on the same sequence that GetOriginsForType was called on,
  // which is likely the IO thread.
  std::move(callback).Run(*origins);
}

void GetOriginsForHostOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const std::string& host,
    std::vector<url::Origin>* origins_to_return) {
  DCHECK(context->IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<url::Origin> all_origins = context->GetAllOrigins();
  for (auto& origin : all_origins) {
    if (host == origin.host())
      origins_to_return->push_back(std::move(origin));
  }
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context)
    : indexed_db_context_(std::move(indexed_db_context)) {
  DCHECK(indexed_db_context_.get());
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  IndexedDBContextImpl::ReleaseOnIDBSequence(std::move(indexed_db_context_));
}

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() {}

void IndexedDBQuotaClient::GetOriginUsage(const url::Origin& origin,
                                          StorageType type,
                                          GetOriginUsageCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->IDBTaskRunner(), FROM_HERE,
      base::BindOnce(&GetOriginUsageOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), origin),
      std::move(callback));
}

void IndexedDBQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  auto* origins_to_return = new std::vector<url::Origin>();
  indexed_db_context_->IDBTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetAllOriginsOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_),
                     base::Unretained(origins_to_return)),
      base::BindOnce(&DidGetOrigins, std::move(callback),
                     base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

  auto* origins_to_return = new std::vector<url::Origin>();
  indexed_db_context_->IDBTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), host,
                     base::Unretained(origins_to_return)),
      base::BindOnce(&DidGetOrigins, std::move(callback),
                     base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::DeleteOriginData(const url::Origin& origin,
                                            StorageType type,
                                            DeleteOriginDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(type, StorageType::kTemporary);

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
  std::move(callback).Run();
}

}  // namespace content

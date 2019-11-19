// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "storage/browser/database/database_util.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::StorageType;
using storage::QuotaClient;
using storage::DatabaseUtil;

namespace content {
namespace {

blink::mojom::QuotaStatusCode DeleteOriginDataOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const url::Origin& origin) {
  context->DeleteForOrigin(origin);
  return blink::mojom::QuotaStatusCode::kOk;
}

int64_t GetOriginUsageOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const url::Origin& origin) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());
  return context->GetOriginDiskUsage(origin);
}

void GetAllOriginsOnIndexedDBThread(IndexedDBContextImpl* context,
                                    std::set<url::Origin>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());
  for (const auto& origin : context->GetAllOrigins())
    origins_to_return->insert(origin);
}

void DidGetOrigins(IndexedDBQuotaClient::GetOriginsCallback callback,
                   const std::set<url::Origin>* origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(*origins);
}

void GetOriginsForHostOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const std::string& host,
    std::set<url::Origin>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksInCurrentSequence());
  for (const auto& origin : context->GetAllOrigins()) {
    GURL origin_url(origin.Serialize());
    if (host == net::GetHostOrSpecFromURL(origin_url))
      origins_to_return->insert(origin);
  }
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {}

QuotaClient::ID IndexedDBQuotaClient::id() const { return kIndexedDatabase; }

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() { delete this; }

void IndexedDBQuotaClient::GetOriginUsage(const url::Origin& origin,
                                          StorageType type,
                                          GetUsageCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // IndexedDB is in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    std::move(callback).Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(), FROM_HERE,
      base::BindOnce(&GetOriginUsageOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), origin),
      std::move(callback));
}

void IndexedDBQuotaClient::GetOriginsForType(StorageType type,
                                             GetOriginsCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  std::set<url::Origin>* origins_to_return = new std::set<url::Origin>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetAllOriginsOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_),
                     base::Unretained(origins_to_return)),
      base::BindOnce(&DidGetOrigins, std::move(callback),
                     base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::GetOriginsForHost(StorageType type,
                                             const std::string& host,
                                             GetOriginsCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  std::set<url::Origin>* origins_to_return = new std::set<url::Origin>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), host,
                     base::Unretained(origins_to_return)),
      base::BindOnce(&DidGetOrigins, std::move(callback),
                     base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::DeleteOriginData(const url::Origin& origin,
                                            StorageType type,
                                            DeletionCallback callback) {
  if (type != StorageType::kTemporary) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(), FROM_HERE,
      base::BindOnce(&DeleteOriginDataOnIndexedDBThread,
                     base::RetainedRef(indexed_db_context_), origin),
      std::move(callback));
}

bool IndexedDBQuotaClient::DoesSupport(StorageType type) const {
  return type == StorageType::kTemporary;
}

}  // namespace content

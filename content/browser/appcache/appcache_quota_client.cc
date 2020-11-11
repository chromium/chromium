// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_quota_client.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::StorageType;

namespace content {
namespace {
blink::mojom::QuotaStatusCode NetErrorCodeToQuotaStatus(int code) {
  if (code == net::OK)
    return blink::mojom::QuotaStatusCode::kOk;
  else if (code == net::ERR_ABORTED)
    return blink::mojom::QuotaStatusCode::kErrorAbort;
  else
    return blink::mojom::QuotaStatusCode::kUnknown;
}

void RunFront(content::AppCacheQuotaClient::RequestQueue* queue) {
  base::OnceClosure request = std::move(queue->front());
  queue->pop_front();
  std::move(request).Run();
}

void RunDeleteOnIO(const base::Location& from_here,
                   net::CompletionRepeatingCallback callback,
                   int result) {
  base::PostTask(from_here, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}
}  // namespace

AppCacheQuotaClient::AppCacheQuotaClient(
    base::WeakPtr<AppCacheServiceImpl> service)
    : service_(std::move(service)) {}

AppCacheQuotaClient::~AppCacheQuotaClient() {
  DCHECK(pending_batch_requests_.empty());
  DCHECK(pending_serial_requests_.empty());
  DCHECK(current_delete_request_callback_.is_null());
}

void AppCacheQuotaClient::OnQuotaManagerDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeletePendingRequests();
  if (!current_delete_request_callback_.is_null()) {
    current_delete_request_callback_.Reset();
    GetServiceDeleteCallback()->Cancel();
  }
}

void AppCacheQuotaClient::GetOriginUsage(const url::Origin& origin,
                                         StorageType type,
                                         GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());

  if (service_is_destroyed_) {
    std::move(callback).Run(0);
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(base::BindOnce(
        &AppCacheQuotaClient::GetOriginUsage, base::RetainedRef(this), origin,
        type, std::move(callback)));
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AppCacheServiceImpl> service,
             const url::Origin& origin) -> int64_t {
            if (!service)
              return 0;

            const std::map<url::Origin, int64_t>& map =
                service->storage()->usage_map();
            auto it = map.find(origin);
            if (it == map.end())
              return 0;

            return it->second;
          },
          service_, origin),
      std::move(callback));
}

void AppCacheQuotaClient::GetOriginsForType(
    StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());

  GetOriginsHelper(std::string(), std::move(callback));
}

void AppCacheQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());

  if (host.empty()) {
    std::move(callback).Run(std::vector<url::Origin>());
    return;
  }
  GetOriginsHelper(host, std::move(callback));
}

void AppCacheQuotaClient::DeleteOriginData(const url::Origin& origin,
                                           StorageType type,
                                           DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());

  if (service_is_destroyed_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    return;
  }

  if (!appcache_is_ready_ || !current_delete_request_callback_.is_null()) {
    pending_serial_requests_.push_back(base::BindOnce(
        &AppCacheQuotaClient::DeleteOriginData, base::RetainedRef(this), origin,
        type, std::move(callback)));
    return;
  }

  current_delete_request_callback_ = std::move(callback);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AppCacheServiceImpl::DeleteAppCachesForOrigin, service_,
                     origin,
                     base::BindOnce(&RunDeleteOnIO, FROM_HERE,
                                    GetServiceDeleteCallback()->callback())));
}

void AppCacheQuotaClient::PerformStorageCleanup(blink::mojom::StorageType type,
                                                base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());

  std::move(callback).Run();
}

void AppCacheQuotaClient::DidDeleteAppCachesForOrigin(int rv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Finish the request by calling our callers callback.
  std::move(current_delete_request_callback_)
      .Run(NetErrorCodeToQuotaStatus(rv));
  if (pending_serial_requests_.empty())
    return;

  // Start the next in the queue.
  RunFront(&pending_serial_requests_);
}

void AppCacheQuotaClient::GetOriginsHelper(const std::string& opt_host,
                                           GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (service_is_destroyed_) {
    std::move(callback).Run(std::vector<url::Origin>());
    return;
  }

  if (!appcache_is_ready_) {
    pending_batch_requests_.push_back(
        base::BindOnce(&AppCacheQuotaClient::GetOriginsHelper,
                       base::RetainedRef(this), opt_host, std::move(callback)));
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AppCacheServiceImpl> service,
             const std::string& opt_host) {
            std::vector<url::Origin> origins;
            if (!service)
              return origins;

            for (const auto& pair : service->storage()->usage_map()) {
              if (opt_host.empty() || pair.first.host() == opt_host)
                origins.push_back(pair.first);
            }
            return origins;
          },
          service_, opt_host),
      std::move(callback));
}

void AppCacheQuotaClient::ProcessPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(appcache_is_ready_);

  while (!pending_batch_requests_.empty())
    RunFront(&pending_batch_requests_);

  if (!pending_serial_requests_.empty())
    RunFront(&pending_serial_requests_);
}

void AppCacheQuotaClient::DeletePendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_batch_requests_.clear();
  pending_serial_requests_.clear();
}

net::CancelableCompletionRepeatingCallback*
AppCacheQuotaClient::GetServiceDeleteCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Lazily created due to base::CancelableCallback's threading restrictions,
  // there is no way to detach from the thread created on.
  if (!service_delete_callback_) {
    service_delete_callback_ =
        std::make_unique<net::CancelableCompletionRepeatingCallback>(
            base::BindRepeating(
                &AppCacheQuotaClient::DidDeleteAppCachesForOrigin,
                base::RetainedRef(this)));
  }
  return service_delete_callback_.get();
}

void AppCacheQuotaClient::NotifyStorageReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Can reoccur during reinitialization.
  if (!appcache_is_ready_) {
    appcache_is_ready_ = true;
    ProcessPendingRequests();
  }
}

void AppCacheQuotaClient::NotifyServiceDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_ = nullptr;
  service_is_destroyed_ = true;
  while (!pending_batch_requests_.empty())
    RunFront(&pending_batch_requests_);

  while (!pending_serial_requests_.empty())
    RunFront(&pending_serial_requests_);

  if (!current_delete_request_callback_.is_null()) {
    std::move(current_delete_request_callback_)
        .Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    GetServiceDeleteCallback()->Cancel();
  }

  if (service_delete_callback_)
    service_delete_callback_.reset();
}

}  // namespace content

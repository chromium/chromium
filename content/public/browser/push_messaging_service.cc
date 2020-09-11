// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/push_messaging_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace content {

namespace {

void CallStringCallbackFromIO(
    PushMessagingService::RegistrationUserDataCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool success = service_worker_status == blink::ServiceWorkerStatusCode::kOk;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                success ? data : std::vector<std::string>()));
}

void CallClosureFromIO(base::OnceClosure callback,
                       blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

void GetUserDataOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper,
    int64_t service_worker_registration_id,
    const std::vector<std::string>& keys,
    PushMessagingService::RegistrationUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_wrapper->GetRegistrationUserData(
      service_worker_registration_id, keys,
      base::BindOnce(&CallStringCallbackFromIO, std::move(callback)));
}

void ClearPushSubscriptionIdOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t service_worker_registration_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->ClearRegistrationUserData(
      service_worker_registration_id, {kPushRegistrationIdServiceWorkerKey},
      base::BindOnce(&CallClosureFromIO, std::move(callback)));
}

void UpdatePushSubscriptionIdOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& subscription_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context->StoreRegistrationUserData(
      service_worker_registration_id, url::Origin::Create(origin),
      {{kPushRegistrationIdServiceWorkerKey, subscription_id}},
      base::BindOnce(&CallClosureFromIO, std::move(callback)));
}

void StorePushSubscriptionOnIOForTesting(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& subscription_id,
    const std::string& sender_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->StoreRegistrationUserData(
      service_worker_registration_id, url::Origin::Create(origin),
      {{kPushRegistrationIdServiceWorkerKey, subscription_id},
       {kPushSenderIdServiceWorkerKey, sender_id}},
      base::BindOnce(&CallClosureFromIO, std::move(callback)));
}

scoped_refptr<ServiceWorkerContextWrapper> GetServiceWorkerContext(
    BrowserContext* browser_context, const GURL& origin) {
  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);
  return base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
      partition->GetServiceWorkerContext()));
}

void GetSWDataCallback(PushMessagingService::SWDataCallback callback,
                       const std::vector<std::string>& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string sender_id;
  std::string subscription_id;
  if (!result.empty()) {
    DCHECK_EQ(2u, result.size());
    sender_id = result[0];
    subscription_id = result[1];
  }
  std::move(callback).Run(sender_id, subscription_id);
}

void GetSenderIdCallback(PushMessagingService::SenderIdCallback callback,
                         const std::vector<std::string>& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string sender_id;
  if (!result.empty()) {
    DCHECK_EQ(1u, result.size());
    sender_id = result[0];
  }
  std::move(callback).Run(sender_id);
}

}  // anonymous namespace

// static
void PushMessagingService::GetSenderId(BrowserContext* browser_context,
                                       const GURL& origin,
                                       int64_t service_worker_registration_id,
                                       SenderIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetUserDataOnIO, GetServiceWorkerContext(browser_context, origin),
          service_worker_registration_id,
          std::vector<std::string>{kPushSenderIdServiceWorkerKey},
          base::BindOnce(&GetSenderIdCallback, std::move(callback))));
}

// static
void PushMessagingService::GetSWData(BrowserContext* browser_context,
                                     const GURL& origin,
                                     int64_t service_worker_registration_id,
                                     SWDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetUserDataOnIO, GetServiceWorkerContext(browser_context, origin),
          service_worker_registration_id,
          std::vector<std::string>{kPushSenderIdServiceWorkerKey,
                                   kPushRegistrationIdServiceWorkerKey},
          base::BindOnce(&GetSWDataCallback, std::move(callback))));
}

// static
void PushMessagingService::ClearPushSubscriptionId(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearPushSubscriptionIdOnIO,
                     GetServiceWorkerContext(browser_context, origin),
                     service_worker_registration_id, std::move(callback)));
}

// static
void PushMessagingService::UpdatePushSubscriptionId(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& subscription_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdatePushSubscriptionIdOnIO,
                     GetServiceWorkerContext(browser_context, origin),
                     service_worker_registration_id, origin, subscription_id,
                     std::move(callback)));
}

// static
void PushMessagingService::StorePushSubscriptionForTesting(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& subscription_id,
    const std::string& sender_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StorePushSubscriptionOnIOForTesting,
                     GetServiceWorkerContext(browser_context, origin),
                     service_worker_registration_id, origin, subscription_id,
                     sender_id, std::move(callback)));
}

}  // namespace content

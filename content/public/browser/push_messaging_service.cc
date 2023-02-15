// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/push_messaging_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

void CallStringCallback(
    PushMessagingService::RegistrationUserDataCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool success = service_worker_status == blink::ServiceWorkerStatusCode::kOk;
  std::move(callback).Run(success ? data : std::vector<std::string>());
}

void CallClosure(base::OnceClosure callback,
                 blink::ServiceWorkerStatusCode status) {
  std::move(callback).Run();
}

scoped_refptr<ServiceWorkerContextWrapper> GetServiceWorkerContext(
    BrowserContext* browser_context, const GURL& origin) {
  StoragePartition* partition =
      browser_context->GetStoragePartitionForUrl(origin);
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
  PushMessagingService::RegistrationUserDataCallback service_callback =
      base::BindOnce(&GetSenderIdCallback, std::move(callback));
  ServiceWorkerContextWrapper::GetUserDataCallback wrapper_callback =
      base::BindOnce(&CallStringCallback, std::move(service_callback));

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetServiceWorkerContext(browser_context, origin)
      ->GetRegistrationUserData(
          service_worker_registration_id,
          std::vector<std::string>{kPushSenderIdServiceWorkerKey},
          std::move(wrapper_callback));
}

// static
void PushMessagingService::GetSWData(BrowserContext* browser_context,
                                     const GURL& origin,
                                     int64_t service_worker_registration_id,
                                     SWDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService::RegistrationUserDataCallback service_callback =
      base::BindOnce(&GetSWDataCallback, std::move(callback));
  ServiceWorkerContextWrapper::GetUserDataCallback wrapper_callback =
      base::BindOnce(&CallStringCallback, std::move(service_callback));

  GetServiceWorkerContext(browser_context, origin)
      ->GetRegistrationUserData(
          service_worker_registration_id,
          std::vector<std::string>{kPushSenderIdServiceWorkerKey,
                                   kPushRegistrationIdServiceWorkerKey},
          std::move(wrapper_callback));
}

// static
void PushMessagingService::ClearPushSubscriptionId(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetServiceWorkerContext(browser_context, origin)
      ->ClearRegistrationUserData(
          service_worker_registration_id, {kPushRegistrationIdServiceWorkerKey},
          base::BindOnce(&CallClosure, std::move(callback)));
}

// static
void PushMessagingService::UpdatePushSubscriptionId(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& subscription_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetServiceWorkerContext(browser_context, origin)
      ->StoreRegistrationUserData(
          service_worker_registration_id,
          blink::StorageKey::CreateFirstParty(url::Origin::Create(origin)),
          {{kPushRegistrationIdServiceWorkerKey, subscription_id}},
          base::BindOnce(&CallClosure, std::move(callback)));
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
  GetServiceWorkerContext(browser_context, origin)
      ->StoreRegistrationUserData(
          service_worker_registration_id,
          blink::StorageKey::CreateFirstParty(url::Origin::Create(origin)),
          {{kPushRegistrationIdServiceWorkerKey, subscription_id},
           {kPushSenderIdServiceWorkerKey, sender_id}},
          base::BindOnce(&CallClosure, std::move(callback)));
}

}  // namespace content

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/service_worker_host_interceptor.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

ServiceWorkerHostInterceptor::ServiceWorkerHostInterceptor() = default;

ServiceWorkerHostInterceptor::~ServiceWorkerHostInterceptor() = default;

blink::ServiceWorkerStatusCode
ServiceWorkerHostInterceptor::InterceptServiceWorkerHostWithScope(
    BrowserContext* browser_context,
    const GURL& scope,
    int* service_worker_process_id_out) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerContextWrapper> context =
      static_cast<ServiceWorkerContextWrapper*>(
          browser_context->GetDefaultStoragePartition()
              ->GetServiceWorkerContext());
  base::RunLoop run_loop;
  FindRegistration(context, scope, run_loop.QuitClosure());
  run_loop.Run();
  *service_worker_process_id_out = service_worker_process_id_;
  return status_;
}

bool ServiceWorkerHostInterceptor::WillOpenPaymentHandlerWindow(GURL* url) {
  return true;
}

blink::mojom::ServiceWorkerHost*
ServiceWorkerHostInterceptor::GetForwardingInterface() {
  return service_worker_version_;
}

void ServiceWorkerHostInterceptor::OpenPaymentHandlerWindow(
    const GURL& url,
    OpenPaymentHandlerWindowCallback callback) {
  GURL url_copy = url;
  if (WillOpenPaymentHandlerWindow(&url_copy)) {
    GetForwardingInterface()->OpenPaymentHandlerWindow(url_copy,
                                                       std::move(callback));
  }
}

void ServiceWorkerHostInterceptor::FindRegistration(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GURL& scope,
    base::OnceClosure done) {
  context->FindRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ServiceWorkerHostInterceptor::OnFoundRegistration,
                     base::Unretained(this), std::move(done)));
}

void ServiceWorkerHostInterceptor::OnFoundRegistration(
    base::OnceClosure done,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  status_ = status;
  service_worker_version_ = registration->active_version();
  service_worker_process_id_ =
      service_worker_version_->embedded_worker()->process_id();
  forwarding_interface_ =
      service_worker_version_->service_worker_host_receiver_for_testing()
          .SwapImplForTesting(this);
  std::move(done).Run();
}

}  // namespace content

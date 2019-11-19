// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/browser/payments/payment_manager.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppContextImpl::Init(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if DCHECK_IS_ON()
  DCHECK(!did_shutdown_on_core_.IsSet());
#endif

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &PaymentAppContextImpl::CreatePaymentAppDatabaseOnCoreThread, this,
          service_worker_context));
}

void PaymentAppContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Schedule a ShutdownOnCoreThread() callback that holds a reference to |this|
  // on the core thread. When the last reference to |this| is released, |this|
  // is automatically scheduled for deletion on the UI thread (see
  // content::BrowserThread::DeleteOnUIThread in the header file).
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&PaymentAppContextImpl::ShutdownOnCoreThread, this));
}

void PaymentAppContextImpl::CreatePaymentManagerForOrigin(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &PaymentAppContextImpl::CreatePaymentManagerForOriginOnCoreThread,
          this, origin, std::move(receiver)));
}

void PaymentAppContextImpl::PaymentManagerHadConnectionError(
    PaymentManager* payment_manager) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(base::Contains(payment_managers_, payment_manager));

  payment_managers_.erase(payment_manager);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return payment_app_database_.get();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if DCHECK_IS_ON()
  DCHECK(did_shutdown_on_core_.IsSet());
#endif
}

void PaymentAppContextImpl::CreatePaymentAppDatabaseOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  payment_app_database_ =
      std::make_unique<PaymentAppDatabase>(service_worker_context);
}

void PaymentAppContextImpl::CreatePaymentManagerForOriginOnCoreThread(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto payment_manager =
      std::make_unique<PaymentManager>(this, origin, std::move(receiver));
  payment_managers_[payment_manager.get()] = std::move(payment_manager);
}

void PaymentAppContextImpl::ShutdownOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  payment_managers_.clear();
  payment_app_database_.reset();

#if DCHECK_IS_ON()
  did_shutdown_on_core_.Set();
#endif
}

}  // namespace content

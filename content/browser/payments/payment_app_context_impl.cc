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
  DCHECK(!did_shutdown_on_io_.IsSet());
#endif

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO, this,
                     service_worker_context));
}

void PaymentAppContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Schedule a ShutdownOnIO() callback that holds a reference to |this| on the
  // IO thread. When the last reference to |this| is released, |this| is
  // automatically scheduled for deletion on the UI thread (see
  // content::BrowserThread::DeleteOnUIThread in the header file).
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PaymentAppContextImpl::ShutdownOnIO, this));
}

void PaymentAppContextImpl::CreatePaymentManager(
    payments::mojom::PaymentManagerRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PaymentAppContextImpl::CreatePaymentManagerOnIO, this,
                     std::move(request)));
}

void PaymentAppContextImpl::PaymentManagerHadConnectionError(
    PaymentManager* payment_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(payment_managers_, payment_manager));

  payment_managers_.erase(payment_manager);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return payment_app_database_.get();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if DCHECK_IS_ON()
  DCHECK(did_shutdown_on_io_.IsSet());
#endif
}

void PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  payment_app_database_ =
      std::make_unique<PaymentAppDatabase>(service_worker_context);
}

void PaymentAppContextImpl::CreatePaymentManagerOnIO(
    mojo::InterfaceRequest<payments::mojom::PaymentManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto payment_manager =
      std::make_unique<PaymentManager>(this, std::move(request));
  payment_managers_[payment_manager.get()] = std::move(payment_manager);
}

void PaymentAppContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_managers_.clear();
  payment_app_database_.reset();

#if DCHECK_IS_ON()
  did_shutdown_on_io_.Set();
#endif
}

}  // namespace content

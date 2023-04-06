// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/browser/payments/payment_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppContextImpl::Init(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  payment_app_database_ =
      std::make_unique<PaymentAppDatabase>(std::move(service_worker_context));
}

void PaymentAppContextImpl::CreatePaymentManagerForOrigin(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto payment_manager =
      std::make_unique<PaymentManager>(this, origin, std::move(receiver));
  payment_managers_[payment_manager.get()] = std::move(payment_manager);
}

void PaymentAppContextImpl::PaymentManagerHadConnectionError(
    PaymentManager* payment_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::Contains(payment_managers_, payment_manager));

  payment_managers_.erase(payment_manager);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return payment_app_database_.get();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

}  // namespace content

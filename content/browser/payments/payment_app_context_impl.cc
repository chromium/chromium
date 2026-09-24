// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
}

void PaymentAppContextImpl::Init(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  payment_app_database_ =
      std::make_unique<PaymentAppDatabase>(std::move(service_worker_context));
}

void PaymentAppContextImpl::CreatePaymentManagerForOrigin(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  auto payment_manager =
      std::make_unique<PaymentManager>(this, origin, std::move(receiver));
  payment_managers_[payment_manager.get()] = std::move(payment_manager);
}

void PaymentAppContextImpl::PaymentManagerHadConnectionError(
    PaymentManager* payment_manager) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  CHECK(payment_managers_.contains(payment_manager), base::NotFatalUntil::M160);

  payment_managers_.erase(payment_manager);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  return payment_app_database_.get();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
}

}  // namespace content

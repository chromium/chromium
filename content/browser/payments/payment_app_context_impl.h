// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_IMPL_H_

#include <map>
#include <memory>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "content/browser/payments/payment_app_database.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class PaymentAppDatabase;
class PaymentManager;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. Most logic is delegated to the owned PaymentAppDatabase
// instance.
//
// This class is created by StoragePartitionImpl. It lives on the UI thread.
class CONTENT_EXPORT PaymentAppContextImpl
    : public base::RefCounted<PaymentAppContextImpl> {
 public:
  PaymentAppContextImpl();

  PaymentAppContextImpl(const PaymentAppContextImpl&) = delete;
  PaymentAppContextImpl& operator=(const PaymentAppContextImpl&) = delete;

  // Init() must be called before using this. It is separate from the
  // constructor for tests that want to inject a `service_worker_context`.
  void Init(scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  // Creates a PaymentManager that is owned by this.
  void CreatePaymentManagerForOrigin(
      const url::Origin& origin,
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver);

  // Called by PaymentManager objects so that they can be deleted.
  void PaymentManagerHadConnectionError(PaymentManager* service);

  PaymentAppDatabase* payment_app_database() const;

 private:
  friend class PaymentAppContentUnitTestBase;
  friend class base::RefCounted<PaymentAppContextImpl>;
  ~PaymentAppContextImpl();

  std::unique_ptr<PaymentAppDatabase> payment_app_database_;

  // The PaymentManagers are owned by this. They're either deleted in the
  // destructor or when the channel is closed via
  // PaymentManagerHadConnectionError.
  std::map<PaymentManager*, std::unique_ptr<PaymentManager>> payment_managers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_IMPL_H_

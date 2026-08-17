// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class ServiceWorkerRegistration;

class CONTENT_EXPORT PaymentAppDatabase {
 public:
  using PaymentApps = std::map<int64_t, std::unique_ptr<StoredPaymentApp>>;
  using ReadAllPaymentAppsCallback = base::OnceCallback<void(PaymentApps)>;

  using SetPaymentAppInfoCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using EnableDelegationsCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;

  explicit PaymentAppDatabase(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  PaymentAppDatabase(const PaymentAppDatabase&) = delete;
  PaymentAppDatabase& operator=(const PaymentAppDatabase&) = delete;

  ~PaymentAppDatabase();

  void ReadAllPaymentApps(ReadAllPaymentAppsCallback callback);

  void SetPaymentAppUserHint(const GURL& scope, const std::string& user_hint);
  void EnablePaymentAppDelegations(
      const GURL& scope,
      const std::vector<payments::mojom::PaymentDelegation>& delegations,
      EnableDelegationsCallback callback);
  void SetPaymentAppInfoForRegisteredServiceWorker(
      int64_t registration_id,
      const std::string& instrument_key,
      const std::string& name,
      const std::string& icon,
      const std::string& method,
      const SupportedDelegations& supported_delegations,
      SetPaymentAppInfoCallback callback);

 private:
  // ReadAllPaymentApps callbacks
  void DidReadAllPaymentApps(
      ReadAllPaymentAppsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& raw_data,
      blink::ServiceWorkerStatusCode status);
  void DidReadAllPaymentInstruments(
      PaymentApps apps,
      ReadAllPaymentAppsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& raw_data,
      blink::ServiceWorkerStatusCode status);


  // SetPaymentAppUserHint callbacks.
  void DidFindRegistrationToSetPaymentAppUserHint(
      const std::string& user_hint,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetPaymentAppInfoToSetUserHint(
      const std::string& user_hint,
      scoped_refptr<ServiceWorkerRegistration> registration,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);
  void DidSetPaymentAppUserHint(blink::ServiceWorkerStatusCode status);

  // EnablePaymentAppDelegations callbacks.
  void DidFindRegistrationToEnablePaymentAppDelegations(
      const std::vector<payments::mojom::PaymentDelegation>& delegations,
      EnableDelegationsCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetPaymentAppInfoToEnableDelegations(
      const std::vector<payments::mojom::PaymentDelegation>& delegations,
      EnableDelegationsCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);
  void DidEnablePaymentAppDelegations(EnableDelegationsCallback callback,
                                      blink::ServiceWorkerStatusCode status);

  // SetPaymentAppInfoForRegisteredServiceWorker callbacks.
  void DidFindRegistrationToSetPaymentApp(
      const std::string& instrument_key,
      const std::string& name,
      const std::string& icon,
      const std::string& method,
      const SupportedDelegations& supported_delegations,
      SetPaymentAppInfoCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidWritePaymentAppForSetPaymentApp(
      const std::string& instrument_key,
      const std::string& method,
      SetPaymentAppInfoCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration,
      blink::ServiceWorkerStatusCode status);
  void DidWritePaymentInstrumentForSetPaymentApp(
      SetPaymentAppInfoCallback callback,
      blink::ServiceWorkerStatusCode status);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<PaymentAppDatabase> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_

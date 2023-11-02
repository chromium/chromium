// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/payments/payment_app_info_fetcher.h"
#include "content/browser/payments/payment_instrument_icon_fetcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class ServiceWorkerRegistration;

class PaymentAppDatabase {
 public:
  using PaymentApps = std::map<int64_t, std::unique_ptr<StoredPaymentApp>>;
  using ReadAllPaymentAppsCallback = base::OnceCallback<void(PaymentApps)>;

  using DeletePaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using ReadPaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentInstrumentPtr,
                              payments::mojom::PaymentHandlerStatus)>;
  using KeysOfPaymentInstrumentsCallback =
      base::OnceCallback<void(const std::vector<std::string>&,
                              payments::mojom::PaymentHandlerStatus)>;
  using HasPaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using WritePaymentInstrumentCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using FetchAndUpdatePaymentAppInfoCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
  using ClearPaymentInstrumentsCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerStatus)>;
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

  void DeletePaymentInstrument(const GURL& scope,
                               const std::string& instrument_key,
                               DeletePaymentInstrumentCallback callback);
  void ReadPaymentInstrument(const GURL& scope,
                             const std::string& instrument_key,
                             ReadPaymentInstrumentCallback callback);
  void KeysOfPaymentInstruments(const GURL& scope,
                                KeysOfPaymentInstrumentsCallback callback);
  void HasPaymentInstrument(const GURL& scope,
                            const std::string& instrument_key,
                            HasPaymentInstrumentCallback callback);
  void WritePaymentInstrument(const GURL& scope,
                              const std::string& instrument_key,
                              payments::mojom::PaymentInstrumentPtr instrument,
                              WritePaymentInstrumentCallback callback);
  void FetchAndUpdatePaymentAppInfo(
      const GURL& context,
      const GURL& scope,
      FetchAndUpdatePaymentAppInfoCallback callback);
  void ClearPaymentInstruments(const GURL& scope,
                               ClearPaymentInstrumentsCallback callback);
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

  // DeletePaymentInstrument callbacks
  void DidFindRegistrationToDeletePaymentInstrument(
      const std::string& instrument_key,
      DeletePaymentInstrumentCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindPaymentInstrument(int64_t registration_id,
                                const std::string& instrument_key,
                                DeletePaymentInstrumentCallback callback,
                                const std::vector<std::string>& data,
                                blink::ServiceWorkerStatusCode status);
  void DidDeletePaymentInstrument(DeletePaymentInstrumentCallback callback,
                                  blink::ServiceWorkerStatusCode status);

  // ReadPaymentInstrument callbacks
  void DidFindRegistrationToReadPaymentInstrument(
      const std::string& instrument_key,
      ReadPaymentInstrumentCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidReadPaymentInstrument(ReadPaymentInstrumentCallback callback,
                                const std::vector<std::string>& data,
                                blink::ServiceWorkerStatusCode status);

  // KeysOfPaymentInstruments callbacks
  void DidFindRegistrationToGetKeys(
      KeysOfPaymentInstrumentsCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetKeysOfPaymentInstruments(KeysOfPaymentInstrumentsCallback callback,
                                      const std::vector<std::string>& data,
                                      blink::ServiceWorkerStatusCode status);

  // HasPaymentInstrument callbacks
  void DidFindRegistrationToHasPaymentInstrument(
      const std::string& instrument_key,
      HasPaymentInstrumentCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidHasPaymentInstrument(DeletePaymentInstrumentCallback callback,
                               const std::vector<std::string>& data,
                               blink::ServiceWorkerStatusCode status);

  // WritePaymentInstrument callbacks
  void DidFindRegistrationToWritePaymentInstrument(
      const std::string& instrument_key,
      payments::mojom::PaymentInstrumentPtr instrument,
      const std::string& decoded_instrument_icon,
      WritePaymentInstrumentCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidWritePaymentInstrument(WritePaymentInstrumentCallback callback,
                                 blink::ServiceWorkerStatusCode status);

  // FetchAndUpdatePaymentAppInfo callbacks.
  void FetchPaymentAppInfoCallback(
      const GURL& scope,
      FetchAndUpdatePaymentAppInfoCallback callback,
      std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info);
  void DidFindRegistrationToUpdatePaymentAppInfo(
      FetchAndUpdatePaymentAppInfoCallback callback,
      std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetPaymentAppInfoToUpdatePaymentAppInfo(
      FetchAndUpdatePaymentAppInfoCallback callback,
      std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info,
      scoped_refptr<ServiceWorkerRegistration> registration,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);
  void DidUpdatePaymentApp(FetchAndUpdatePaymentAppInfoCallback callback,
                           bool fetch_app_info_failed,
                           blink::ServiceWorkerStatusCode status);

  // PaymentInstrumentIconFetcherCallback.
  void DidFetchedPaymentInstrumentIcon(
      const GURL& scope,
      const std::string& instrument_key,
      payments::mojom::PaymentInstrumentPtr instrument,
      WritePaymentInstrumentCallback callback,
      const std::string& icon);

  // ClearPaymentInstruments callbacks
  void DidFindRegistrationToClearPaymentInstruments(
      const GURL& scope,
      ClearPaymentInstrumentsCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetKeysToClearPaymentInstruments(
      scoped_refptr<ServiceWorkerRegistration> registration,
      ClearPaymentInstrumentsCallback callback,
      const std::vector<std::string>& keys,
      payments::mojom::PaymentHandlerStatus status);
  void DidClearPaymentInstruments(ClearPaymentInstrumentsCallback callback,
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

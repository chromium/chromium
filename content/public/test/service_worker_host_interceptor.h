// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SERVICE_WORKER_HOST_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_SERVICE_WORKER_HOST_INTERCEPTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-test-utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

class GURL;

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Allows intercepting calls to mojom::ServiceWorkerHost (e.g.
// OpenPaymentHandlerWindow) just before they are dispatched to the
// implementation. This enables browser tests to alter the parameters.
class ServiceWorkerHostInterceptor
    : public blink::mojom::ServiceWorkerHostInterceptorForTesting {
 public:
  ServiceWorkerHostInterceptor();

  ServiceWorkerHostInterceptor(const ServiceWorkerHostInterceptor&) = delete;
  ServiceWorkerHostInterceptor& operator=(const ServiceWorkerHostInterceptor&) =
      delete;

  ~ServiceWorkerHostInterceptor() override;

  // Looks for the service worker with the |scope| and starts intercepting calls
  // to its mojom::ServiceWorkerHost. Blocks while looking up the service worker
  // information on the "service worker core" thread.
  //
  // On success, sets |service_worker_process_id_out| to the identifier of the
  // process of the service worker, which can be used to observe process
  // shutdown, and returns blink::ServiceWorkerStatusCode::kOk.
  blink::ServiceWorkerStatusCode InterceptServiceWorkerHostWithScope(
      BrowserContext* browser_context,
      const GURL& scope,
      int* service_worker_process_id_out);

  // This method can be overridden to change the |url| of the payment handler
  // window or to prevent the OpenPaymentHandlerWindow call from going through
  // by returning false. By default, this method does not modify the |url| and
  // returns true.
  virtual bool WillOpenPaymentHandlerWindow(GURL* url);

 private:
  // mojom::ServiceWorkerHostInterceptorForTesting:
  blink::mojom::ServiceWorkerHost* GetForwardingInterface() override;
  void OpenPaymentHandlerWindow(
      const GURL& url,
      OpenPaymentHandlerWindowCallback callback) override;

  void FindRegistration(scoped_refptr<ServiceWorkerContextWrapper> context,
                        const GURL& scope,
                        base::OnceClosure done);

  void OnFoundRegistration(
      base::OnceClosure done,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  blink::ServiceWorkerStatusCode status_ =
      blink::ServiceWorkerStatusCode::kErrorFailed;
  int service_worker_process_id_ = -1;
  raw_ptr<ServiceWorkerVersion, AcrossTasksDanglingUntriaged>
      service_worker_version_ = nullptr;
  raw_ptr<blink::mojom::ServiceWorkerHost, AcrossTasksDanglingUntriaged>
      forwarding_interface_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SERVICE_WORKER_HOST_INTERCEPTOR_H_

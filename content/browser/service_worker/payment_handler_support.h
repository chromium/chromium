// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_PAYMENT_HANDLER_SUPPORT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_PAYMENT_HANDLER_SUPPORT_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;

// Support for Payment Handler <https://w3c.github.io/payment-handler/>.
class PaymentHandlerSupport {
 public:
  using OpenWindowFallback = base::OnceCallback<void(
      blink::mojom::ServiceWorkerHost::
          OpenPaymentHandlerWindowCallback /* response_callback */)>;
  using ShowPaymentHandlerWindowCallback = base::OnceCallback<void(
      blink::mojom::ServiceWorkerHost::
          OpenPaymentHandlerWindowCallback /* response_callback */,
      bool /* success */,
      int /* render_process_id */,
      int /* render_frame_id */)>;

  // Tries to open a Payment Handler window. The //content embedder may or may
  // not support this operation. If the embedder supports it, |callback| is
  // called with |response_callback| passed to it. Otherwise, |fallback| is
  // called with |response_callback| passed to it. A typical fallback is to open
  // a normal window.
  static void ShowPaymentHandlerWindow(
      const GURL& url,
      ServiceWorkerContextCore* context,
      ShowPaymentHandlerWindowCallback callback,
      OpenWindowFallback fallback,
      blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback
          response_callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_PAYMENT_HANDLER_SUPPORT_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_UTIL_H_

#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class CONTENT_EXPORT PaymentAppProviderUtil {
 public:
  // Gets the ukm source id for a payment app with |sw_scope|.
  // This must ONLY be called when payment app window has been opened.
  static ukm::SourceId GetSourceIdForPaymentAppFromScope(const GURL& sw_scope);

  // Check whether given |sw_js_url| from |manifest_url| is allowed to register
  // with |sw_scope|.
  static bool IsValidInstallablePaymentApp(const GURL& manifest_url,
                                           const GURL& sw_js_url,
                                           const GURL& sw_scope,
                                           std::string* error_message);

  // Create blank struct for response to "can make payment".
  static payments::mojom::CanMakePaymentResponsePtr
  CreateBlankCanMakePaymentResponse(
      payments::mojom::CanMakePaymentEventResponseType response_type);

  // Create blank struct for receipt payment app response from render side.
  static payments::mojom::PaymentHandlerResponsePtr
  CreateBlankPaymentHandlerResponse(
      payments::mojom::PaymentEventResponseType response_type);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_UTIL_H_

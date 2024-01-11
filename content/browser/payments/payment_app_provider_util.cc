// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/payment_app_provider_util.h"

#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
ukm::SourceId PaymentAppProviderUtil::GetSourceIdForPaymentAppFromScope(
    const GURL& sw_scope) {
  return ukm::UkmRecorder::GetSourceIdForPaymentAppFromScope(
      base::PassKey<PaymentAppProviderUtil>(),
      sw_scope.DeprecatedGetOriginAsURL());
}

// static
bool PaymentAppProviderUtil::IsValidInstallablePaymentApp(
    const GURL& manifest_url,
    const GURL& sw_js_url,
    const GURL& sw_scope,
    std::string* error_message) {
  DCHECK(manifest_url.is_valid() && sw_js_url.is_valid() &&
         sw_scope.is_valid());

  // Scope will be checked against service worker js url when registering, but
  // we check it here earlier to avoid presenting unusable payment handlers.
  if (!service_worker_loader_helpers::IsPathRestrictionSatisfiedWithoutHeader(
          sw_scope, sw_js_url, error_message)) {
    return false;
  }

  std::vector<GURL> urls = {manifest_url, sw_js_url, sw_scope};
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    *error_message =
        "Origins are not matching, or some origins cannot access service "
        "worker (manifest:" +
        manifest_url.spec() + " scope:" + sw_scope.spec() +
        " sw:" + sw_js_url.spec() + ")";
    return false;
  }

  return true;
}

// static
payments::mojom::CanMakePaymentResponsePtr
PaymentAppProviderUtil::CreateBlankCanMakePaymentResponse(
    payments::mojom::CanMakePaymentEventResponseType response_type) {
  return payments::mojom::CanMakePaymentResponse::New(
      response_type, /*can_make_payment=*/false);
}

// static
payments::mojom::PaymentHandlerResponsePtr
PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
    payments::mojom::PaymentEventResponseType response_type) {
  return payments::mojom::PaymentHandlerResponse::New(
      /*method_name=*/"", /*stringified_details=*/"", response_type,
      /*payer_name=*/std::nullopt, /*payer_email=*/std::nullopt,
      /*payer_phone=*/std::nullopt, /*shipping_address=*/nullptr,
      /*shipping_option=*/std::nullopt);
}

}  // namespace content

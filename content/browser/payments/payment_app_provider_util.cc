// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/payment_app_provider_util.h"

#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
ukm::SourceId PaymentAppProviderUtil::GetSourceIdForPaymentAppFromScope(
    const GURL& sw_scope) {
  return ukm::UkmRecorder::GetSourceIdForPaymentAppFromScope(
      sw_scope.GetOrigin());
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
  if (!ServiceWorkerUtils::IsPathRestrictionSatisfiedWithoutHeader(
          sw_scope, sw_js_url, error_message)) {
    return false;
  }

  // TODO(crbug.com/855312): Unify duplicated code between here and
  // ServiceWorkerProviderHost::IsValidRegisterMessage.
  std::vector<GURL> urls = {manifest_url, sw_js_url, sw_scope};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
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
      response_type, /*can_make_payment=*/false,
      /*ready_for_minimal_ui=*/false,
      /*account_balance=*/base::nullopt);
}

// static
payments::mojom::PaymentHandlerResponsePtr
PaymentAppProviderUtil::CreateBlankPaymentHandlerResponse(
    payments::mojom::PaymentEventResponseType response_type) {
  return payments::mojom::PaymentHandlerResponse::New(
      /*method_name=*/"", /*stringified_details=*/"", response_type,
      /*payer_name=*/base::nullopt, /*payer_email=*/base::nullopt,
      /*payer_phone=*/base::nullopt, /*shipping_address=*/nullptr,
      /*shipping_option=*/base::nullopt);
}

}  // namespace content

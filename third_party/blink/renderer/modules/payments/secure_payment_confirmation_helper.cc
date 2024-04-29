// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_helper.h"

#include <stdint.h>

#include "base/logging.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_network_or_issuer_information.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_type_converter.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {
bool IsEmpty(const V8UnionArrayBufferOrArrayBufferView* buffer) {
  DCHECK(buffer);
  switch (buffer->GetContentType()) {
    case V8BufferSource::ContentType::kArrayBuffer:
      return buffer->GetAsArrayBuffer()->ByteLength() == 0;
    case V8BufferSource::ContentType::kArrayBufferView:
      return buffer->GetAsArrayBufferView()->byteLength() == 0;
  }
}

// Determine whether an RP ID is a 'valid domain' as per the URL spec:
// https://url.spec.whatwg.org/#valid-domain
//
// TODO(crbug.com/1354209): This is a workaround to a lack of support for 'valid
// domain's in the //url code.
bool IsValidDomain(const String& rp_id) {
  // A valid domain, such as 'site.example', should be a URL host (and nothing
  // more of the URL!) that is not an IP address.
  KURL url("https://" + rp_id);
  return url.IsValid() && url.Host() == rp_id &&
         !url::HostIsIPAddress(url.Host().Utf8());
}
}  // namespace

// static
::payments::mojom::blink::SecurePaymentConfirmationRequestPtr
SecurePaymentConfirmationHelper::ParseSecurePaymentConfirmationData(
    const ScriptValue& input,
    ExecutionContext& execution_context,
    ExceptionState& exception_state) {
  DCHECK(!input.IsEmpty());
  SecurePaymentConfirmationRequest* request =
      NativeValueTraits<SecurePaymentConfirmationRequest>::NativeValue(
          input.GetIsolate(), input.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (request->credentialIds().empty()) {
    exception_state.ThrowRangeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"credentialIds\" field.");
    return nullptr;
  }
  for (const V8UnionArrayBufferOrArrayBufferView* id :
       request->credentialIds()) {
    if (IsEmpty(id)) {
      exception_state.ThrowRangeError(
          "The \"secure-payment-confirmation\" method requires that elements "
          "in the \"credentialIds\" field are non-empty.");
      return nullptr;
    }
  }
  if (IsEmpty(request->challenge())) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"challenge\" field.");
    return nullptr;
  }

  if (request->instrument()->displayName().empty()) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"instrument.displayName\" field.");
    return nullptr;
  }
  if (request->instrument()->icon().empty()) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"instrument.icon\" field.");
    return nullptr;
  }
  if (!KURL(request->instrument()->icon()).IsValid()) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a valid URL in "
        "the \"instrument.icon\" field.");
    return nullptr;
  }
  if (!IsValidDomain(request->rpId())) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a valid domain "
        "in the \"rpId\" field.");
    return nullptr;
  }
  if ((!request->hasPayeeOrigin() && !request->hasPayeeName()) ||
      (request->hasPayeeOrigin() && request->payeeOrigin().empty()) ||
      (request->hasPayeeName() && request->payeeName().empty())) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires a non-empty "
        "\"payeeOrigin\" or \"payeeName\" field.");
    return nullptr;
  }
  if (request->hasPayeeOrigin()) {
    KURL payee_url(request->payeeOrigin());
    if (!payee_url.IsValid() || !payee_url.ProtocolIs("https")) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a valid HTTPS "
          "URL in the \"payeeOrigin\" field.");
      return nullptr;
    }
  }

  // Opt Out should only be carried through if the flag is enabled.
  if (request->hasShowOptOut() &&
      !blink::RuntimeEnabledFeatures::SecurePaymentConfirmationOptOutEnabled(
          &execution_context)) {
    request->setShowOptOut(false);
  }

  if (request->hasNetworkInfo()) {
    if (request->networkInfo()->name().empty()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a non-empty "
          "\"networkInfo.name\" field.");
      return nullptr;
    }

    if (request->networkInfo()->icon().empty()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a non-empty "
          "\"networkInfo.icon\" field.");
      return nullptr;
    }

    if (!KURL(request->networkInfo()->icon()).IsValid()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a valid URL in "
          "the \"networkInfo.icon\" field.");
      return nullptr;
    }
  }

  if (request->hasIssuerInfo()) {
    if (request->issuerInfo()->name().empty()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a non-empty "
          "\"issuerInfo.name\" field.");
      return nullptr;
    }

    if (request->issuerInfo()->icon().empty()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a non-empty "
          "\"issuerInfo.icon\" field.");
      return nullptr;
    }

    if (!KURL(request->issuerInfo()->icon()).IsValid()) {
      exception_state.ThrowTypeError(
          "The \"secure-payment-confirmation\" method requires a valid URL in "
          "the \"issuerInfo.icon\" field.");
      return nullptr;
    }
  }

  return mojo::ConvertTo<
      payments::mojom::blink::SecurePaymentConfirmationRequestPtr>(request);
}

}  // namespace blink

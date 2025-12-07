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
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_entity_logo.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_secure_payment_confirmation_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_type_converter.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

// The maximum size of the payment instrument details string. Arbitrarily chosen
// while being much larger than any reasonable input.
constexpr size_t kMaxInstrumentDetailsLength = 4096;

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
  KURL url(StrCat({"https://", rp_id}));
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
  if (request->instrument()->hasDetails() &&
      request->instrument()->details().empty()) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires the "
        "\"instrument.details\" field, if present, to be non-empty.");
    return nullptr;
  }
  if (request->instrument()->hasDetails() &&
      request->instrument()->details().length() > kMaxInstrumentDetailsLength) {
    exception_state.ThrowTypeError(
        "The \"secure-payment-confirmation\" method requires the string "
        "length in the \"instrument.details\" field to be at most 4096.");
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

  if (request->hasPaymentEntitiesLogos()) {
    for (const PaymentEntityLogo* logo : request->paymentEntitiesLogos()) {
      // The IDL bindings code does not allow the sequence to contain null
      // entries.
      CHECK(logo);

      if (logo->url().empty()) {
        exception_state.ThrowTypeError(
            "The \"secure-payment-confirmation\" method requires that each "
            "entry in \"paymentEntitiesLogos\" has a non-empty \"url\" field.");
        return nullptr;
      }
      KURL logo_url(logo->url());
      if (!logo_url.IsValid()) {
        exception_state.ThrowTypeError(
            "The \"secure-payment-confirmation\" method requires that each "
            "entry in \"paymentEntitiesLogos\" has a valid URL in the \"url\" "
            "field.");
        return nullptr;
      }
      if (!logo_url.ProtocolIsInHTTPFamily() && !logo_url.ProtocolIsData()) {
        exception_state.ThrowTypeError(
            "The \"secure-payment-confirmation\" method requires that each "
            "entry in \"paymentEntitiesLogos\" has a URL whose scheme is one "
            "of \"https\", \"http\", or \"data\" in the \"url\" field.");
        return nullptr;
      }
      if (logo->label().empty()) {
        exception_state.ThrowTypeError(
            "The \"secure-payment-confirmation\" method requires that each "
            "entry in \"paymentEntitiesLogos\" has a non-empty \"label\" "
            "field.");
        return nullptr;
      }
    }
  }

  return mojo::ConvertTo<
      payments::mojom::blink::SecurePaymentConfirmationRequestPtr>(request);
}

}  // namespace blink

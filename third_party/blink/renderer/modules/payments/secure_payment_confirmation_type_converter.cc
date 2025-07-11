// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/secure_payment_confirmation_type_converter.h"

#include <cstdint>

#include "base/time/time.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_credential_instrument.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_entity_logo.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

payments::mojom::blink::SecurePaymentConfirmationRequestPtr
TypeConverter<payments::mojom::blink::SecurePaymentConfirmationRequestPtr,
              blink::SecurePaymentConfirmationRequest*>::
    Convert(const blink::SecurePaymentConfirmationRequest* input) {
  auto output = payments::mojom::blink::SecurePaymentConfirmationRequest::New();
  output->credential_ids =
      mojo::ConvertTo<blink::Vector<blink::Vector<uint8_t>>>(
          input->credentialIds());
  output->challenge =
      mojo::ConvertTo<blink::Vector<uint8_t>>(input->challenge());

  // If a timeout was not specified in JavaScript, then pass a null `timeout`
  // through mojo IPC, so the browser can set a default (e.g., 3 minutes).
  if (input->hasTimeout())
    output->timeout = base::Milliseconds(input->timeout());

  output->instrument = blink::mojom::blink::PaymentCredentialInstrument::New(
      input->instrument()->displayName(),
      blink::KURL(input->instrument()->icon()),
      input->instrument()->iconMustBeShown(),
      // blink::String()'s empty constructor constructs a 'null' string.
      input->instrument()->hasDetails() ? input->instrument()->details()
                                        : blink::String());

  if (input->hasPayeeOrigin()) {
    output->payee_origin =
        blink::SecurityOrigin::CreateFromString(input->payeeOrigin());
  }

  output->rp_id = input->rpId();
  if (input->hasPayeeName())
    output->payee_name = input->payeeName();

  if (input->hasExtensions()) {
    output->extensions =
        ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
            *input->extensions());
  }

  if (input->hasPaymentEntitiesLogos()) {
    output->payment_entities_logos =
        ConvertTo<blink::Vector<payments::mojom::blink::PaymentEntityLogoPtr>>(
            input->paymentEntitiesLogos());
  }

  if (input->hasBrowserBoundPubKeyCredParams()) {
    output->browser_bound_pub_key_cred_params = ConvertTo<
        blink::Vector<blink::mojom::blink::PublicKeyCredentialParametersPtr>>(
        input->browserBoundPubKeyCredParams());
  }

  output->show_opt_out = input->getShowOptOutOr(false);

  return output;
}

payments::mojom::blink::PaymentEntityLogoPtr TypeConverter<
    payments::mojom::blink::PaymentEntityLogoPtr,
    blink::PaymentEntityLogo*>::Convert(const blink::PaymentEntityLogo* input) {
  return payments::mojom::blink::PaymentEntityLogo::New(
      blink::KURL(input->url()), input->label());
}

}  // namespace mojo

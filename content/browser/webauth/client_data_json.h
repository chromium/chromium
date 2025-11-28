// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_CLIENT_DATA_JSON_H_
#define CONTENT_BROWSER_WEBAUTH_CLIENT_DATA_JSON_H_

#include "components/webauthn/core/browser/client_data_json.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {

// Builds the CollectedClientData[1] dictionary with the given values,
// serializes it to JSON, and returns the resulting string.
// This CHECKs if `challenge` has not been provided with a value.
// [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
CONTENT_EXPORT std::string BuildClientDataJson(
    webauthn::ClientDataJsonParams params);

// Same as BuildClientDataJson above, but with payment data.
// The 'payment_options' and 'payment_rp' arguments are used if the
// `params.type` is `kPaymentGet`.
// The browser bound key in the 'payment_options' argument is used if the
// `params.type` is `kWebAuthnCreate`.
CONTENT_EXPORT std::string BuildClientDataJsonWithPayment(
    webauthn::ClientDataJsonParams params,
    blink::mojom::PaymentOptionsPtr payment_options,
    std::string_view payment_rp);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_CLIENT_DATA_JSON_H_

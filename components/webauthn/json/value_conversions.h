// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_JSON_VALUE_CONVERSIONS_H_
#define COMPONENTS_WEBAUTHN_JSON_VALUE_CONVERSIONS_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

namespace webauthn {

// Converts a `PublicKeyCredentialCreationOptions` into a `base::Value`.
//
// The output conforms to the WebAuthn `PublicKeyCredentialCreationOptionsJSON`
// dictionary IDL (see
// https://w3c.github.io/webauthn/#dictdef-publickeycredentialcreationoptionsjson).
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options);

// Converts a `PublicKeyCredentialRequestOptions` into a `base::Value`.
//
// The output conforms to the WebAuthn `PublicKeyCredentialRequestOptionsJSON`
// dictionary IDL (see
// https://w3c.github.io/webauthn/#dictdef-publickeycredentialrequestoptionsjson).
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `MakeCredentialAuthenticatorResponse`.
// Returns a pair of the converted message and an error string. The message will
// be nullptr on error, and the error string empty on success.
//
// The input is expected to conform to the WebAuthn RegistrationResponseJSON
// dictionary IDL (see
// https://w3c.github.io/webauthn/#dictdef-registrationresponsejson).
std::pair<blink::mojom::MakeCredentialAuthenticatorResponsePtr, std::string>
MakeCredentialResponseFromValue(const base::Value& value);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `GetAssertionAuthenticatorResponse`. Returns
// a pair of the converted message and an error string. The message will be
// nullptr on error, and the error string empty on success.
//
// The input is expected to conform to the WebAuthn AuthenticationResponseJSON
// dictionary IDL (see
// https://w3c.github.io/webauthn/#dictdef-authenticationresponsejson).
std::pair<blink::mojom::GetAssertionAuthenticatorResponsePtr, std::string>
GetAssertionResponseFromValue(const base::Value& value);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_JSON_VALUE_CONVERSIONS_H_

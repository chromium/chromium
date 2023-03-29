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
// The output conforms to the WebAuthn `PublicKeyCredentialCreationOptions`
// dictionary IDL, but with all ArrayBuffer and BufferSource attributes
// represented as base64url-encoded strings instead. The `timeout` field is
// omitted.
// (https://w3c.github.io/webauthn/#dictdef-publickeycredentialcreationoptions)
//
// TODO(crbug.com/1231802): Reference spec and update code to match once
// https://github.com/w3c/webauthn/pull/1703 lands.
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options);

// Converts a `PublicKeyCredentialRequestOptions` into a `base::Value`.
//
// The output conforms to the WebAuthn `PublicKeyCredentialRequestOptions`
// dictionary IDL, but with all ArrayBuffer and BufferSource attributes
// represented as base64url-encoded strings instead. The `timeout` field is
// omitted.
// (https://w3c.github.io/webauthn/#dictdef-publickeycredentialrequestoptions)
//
// TODO(crbug.com/1231802): Reference spec and update code to match once
// https://github.com/w3c/webauthn/pull/1703 lands.
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `MakeCredentialAuthenticatorResponse`.
// Returns a pair of the converted message and an error string. The message will
// be nullptr on error, and the error string empty on success.
//
// The input is expected to be a JSON-serialized `PublicKeyCredential` in which
// ArrayBuffer-valued attributes are replaced by base64url-encoded strings. The
// `response` value must be a an `AuthenticatorAttestationResponse`.
//
// TODO(crbug.com/1231802): Reference spec and update code to match once
// https://github.com/w3c/webauthn/pull/1703 lands.
std::pair<blink::mojom::MakeCredentialAuthenticatorResponsePtr, std::string>
MakeCredentialResponseFromValue(const base::Value& value);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `GetAssertionAuthenticatorResponse`. Returns
// a pair of the converted message and an error string. The message will be
// nullptr on error, and the error string empty on success.
//
// The input is expected to be a JSON-serialized `PublicKeyCredential` in which
// ArrayBuffer-valued attributes are replaced by base64url-encoded strings. The
// `response` value must be a an `AuthenticatorAssertionResponse`.
//
// TODO(crbug.com/1231802): Reference spec and update code to match once
// https://github.com/w3c/webauthn/pull/1703 lands.
std::pair<blink::mojom::GetAssertionAuthenticatorResponsePtr, std::string>
GetAssertionResponseFromValue(const base::Value& value);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_JSON_VALUE_CONVERSIONS_H_

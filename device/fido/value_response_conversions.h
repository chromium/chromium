// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_
#define DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_

#include "base/component_export.h"
#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// Methods for creating an AuthenticatorGetAssertionResponse or
// AuthenticatorMakeCredentialResponse, respectively, from a base::Value based
// on the specified JSON format. These only parse a subset of the fields, since
// they are creating CTAP-level objects and the JSON is defined for WebAuthn
// responses.
// Returns absl::nullopt on error.
// https://w3c.github.io/webauthn/#dictdef-authenticatorassertionresponsejson
// https://w3c.github.io/webauthn/#ref-for-dictdef-authenticatorattestationresponsejson
COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponseFromValue(const base::Value& value);
COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<AuthenticatorMakeCredentialResponse>
AuthenticatorMakeCredentialResponseFromValue(const base::Value& value);

}  // namespace device

#endif  // DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_

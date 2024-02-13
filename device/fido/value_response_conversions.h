// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_
#define DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"

namespace device {

// Method for creating an AuthenticatorGetAssertionResponse from a base::Value
// based on the specified JSON format. It only parses a subset of the fields,
// since they are creating CTAP-level objects and the JSON is defined for
// WebAuthn responses.
// Returns std::nullopt on error.
// https://w3c.github.io/webauthn/#dictdef-authenticatorassertionresponsejson
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<AuthenticatorGetAssertionResponse>
AuthenticatorGetAssertionResponseFromValue(const base::Value& value);

}  // namespace device

#endif  // DEVICE_FIDO_VALUE_RESPONSE_CONVERSIONS_H_

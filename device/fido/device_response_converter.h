// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DEVICE_RESPONSE_CONVERTER_H_
#define DEVICE_FIDO_DEVICE_RESPONSE_CONVERTER_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

// Converts response from authenticators to CTAPResponse objects. If the
// response of the authenticator does not conform to format specified by the
// CTAP protocol, null optional is returned.
namespace device {

// Parses response code from response received from the authenticator. If
// unknown response code value is received, then CTAP2_ERR_OTHER is returned.
COMPONENT_EXPORT(DEVICE_FIDO)
CtapDeviceResponseCode GetResponseCode(base::span<const uint8_t> buffer);

// Converts |cbor| to an |AuthenticatorMakeCredentialResponse| using map keys
// that conform to format of attestation object defined by the Webauthn spec:
// https://w3c.github.io/webauthn/#fig-attStructs
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorMakeCredentialResponse>
ReadCTAPMakeCredentialResponse(FidoTransportProtocol transport_used,
                               const base::Optional<cbor::Value>& cbor);

// Converts |cbor|, the response to an |AuthenticatorGetAssertion| /
// |AuthenticatorGetNextAssertion| request, to an
// |AuthenticatorGetAssertionResponse|.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorGetAssertionResponse> ReadCTAPGetAssertionResponse(
    const base::Optional<cbor::Value>& cbor);

// De-serializes CBOR encoded response to AuthenticatorGetInfo request to
// AuthenticatorGetInfoResponse object.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorGetInfoResponse> ReadCTAPGetInfoResponse(
    base::span<const uint8_t> buffer);

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<cbor::Value> FixInvalidUTF8(
    cbor::Value in,
    bool (*predicate)(const std::vector<const cbor::Value*>&));

}  // namespace device

#endif  // DEVICE_FIDO_DEVICE_RESPONSE_CONVERTER_H_

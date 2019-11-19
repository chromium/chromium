// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_COMMAND_CONSTRUCTOR_H_
#define DEVICE_FIDO_U2F_COMMAND_CONSTRUCTOR_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"

namespace device {

// Checks whether the request can be translated to valid U2F request
// parameter. Namely, U2F request does not support resident key and
// user verification, and ES256 algorithm must be used for public key
// credential.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the-ctap2-authenticatormakecredential-command-with-ctap1-u2f-authenticators
COMPONENT_EXPORT(DEVICE_FIDO)
bool IsConvertibleToU2fRegisterCommand(
    const CtapMakeCredentialRequest& request);

// Checks whether user verification is not required and that allow list is
// not empty.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the-ctap2-authenticatorgetassertion-command-with-ctap1-u2f-authenticators
COMPONENT_EXPORT(DEVICE_FIDO)
bool IsConvertibleToU2fSignCommand(const CtapGetAssertionRequest& request);

// Extracts APDU encoded U2F register command from CtapMakeCredentialRequest.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> ConvertToU2fRegisterCommand(
    const CtapMakeCredentialRequest& request);

// Turns a CtapMakeCredentialRequest into an APDU encoded U2F sign command
// for the same RP and key handle, but a bogus challenge.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> ConvertToU2fSignCommandWithBogusChallenge(
    const CtapMakeCredentialRequest& request,
    base::span<const uint8_t> key_handle);

// Extracts APDU encoded U2F sign command from CtapGetAssertionRequest.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> ConvertToU2fSignCommand(
    const CtapGetAssertionRequest& request,
    ApplicationParameterType application_parameter_type,
    base::span<const uint8_t> key_handle);

// TODO(hongjunchoi): Move this logic inside ConvertToU2fRegisterCommand()
// once U2fRegister is removed.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> ConstructU2fRegisterCommand(
    base::span<const uint8_t, kU2fApplicationParamLength> application_parameter,
    base::span<const uint8_t, kU2fChallengeParamLength> challenge_parameter,
    bool is_individual_attestation = false);

// TODO(hongjunchoi): Move this logic inside ConvertToU2fSignCommand() once
// U2fSign is deleted.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> ConstructU2fSignCommand(
    base::span<const uint8_t, kU2fApplicationParamLength> application_parameter,
    base::span<const uint8_t, kU2fChallengeParamLength> challenge_parameter,
    base::span<const uint8_t> key_handle);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> ConstructBogusU2fRegistrationCommand();

}  // namespace device

#endif  // DEVICE_FIDO_U2F_COMMAND_CONSTRUCTOR_H_

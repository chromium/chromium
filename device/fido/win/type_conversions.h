// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_
#define DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_

#include <windows.h>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_constants.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

enum class GetAssertionStatus;
enum class MakeCredentialStatus;

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation);

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(
    const WEBAUTHN_ASSERTION& credential_attestation);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinUserVerificationRequirement(
    UserVerificationRequirement user_verification_requirement);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinAuthenticatorAttachment(
    AuthenticatorAttachment authenticator_attachment);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<WEBAUTHN_CREDENTIAL> ToWinCredentialVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<WEBAUTHN_CREDENTIAL_EX> ToWinCredentialExVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials);

// WinErrorNameToCtapDeviceResponseCode maps a string returned by
// WebAuthNGetErrorName() to a CtapDeviceResponseCode.
//
// The Windows WebAuthn API returns errors as defined by the WebAuthn spec,
// whereas FidoAuthenticator callbacks generally resolve with a
// CtapDeviceResponseCode. This method hence yields a "synthetic"
// CtapDeviceResponseCode that can then be mapped to the corresponding
// {MakeCredential,GetAssertion}Status by calling
// WinCtapDeviceResponseCodeTo{MakeCredential,GetAssertion}Status().
COMPONENT_EXPORT(DEVICE_FIDO)
CtapDeviceResponseCode WinErrorNameToCtapDeviceResponseCode(
    const base::string16& error_name);

// WinCtapDeviceResponseCodeToMakeCredentialStatus returns the
// MakeCredentialStatus that corresponds to a synthetic CtapDeviceResponseCode
// obtained from WinErrorNameToCtapDeviceResponseCode(). Return values are one
// of {kSuccess, kWinInvalidStateError, kWinNotAllowedError}.
COMPONENT_EXPORT(DEVICE_FIDO)
MakeCredentialStatus WinCtapDeviceResponseCodeToMakeCredentialStatus(
    CtapDeviceResponseCode status);

// WinCtapDeviceResponseCodeToGetAssertionStatus returns the GetAssertionStatus
// that corresponds to a synthetic CtapDeviceResponseCode obtained from
// WinErrorNameToCtapDeviceResponseCode(). Return values are one of {kSuccess,
// kWinNotAllowedError}.
COMPONENT_EXPORT(DEVICE_FIDO)
GetAssertionStatus WinCtapDeviceResponseCodeToGetAssertionStatus(
    CtapDeviceResponseCode status);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinAttestationConveyancePreference(
    const AttestationConveyancePreference&);

}  // namespace device

#endif  // DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_
#define DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_

#include <windows.h>

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

class DiscoverableCredentialMetadata;
enum class GetAssertionStatus;
enum class MakeCredentialStatus;

COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation);

COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(
    const WEBAUTHN_ASSERTION& credential_attestation,
    const CtapGetAssertionOptions& request_options);

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

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinLargeBlobSupport(LargeBlobSupport large_blob_support);

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
    const std::u16string& error_name);

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
    const AttestationConveyancePreference&,
    int api_version);

COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<DiscoverableCredentialMetadata>
WinCredentialDetailsListToCredentialMetadata(
    const WEBAUTHN_CREDENTIAL_DETAILS_LIST& credentials);

COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<FidoTransportProtocol> FromWinTransportsMask(
    const DWORD transport);

COMPONENT_EXPORT(DEVICE_FIDO)
uint32_t ToWinTransportsMask(
    const base::flat_set<FidoTransportProtocol>& transports);

}  // namespace device

#endif  // DEVICE_FIDO_WIN_TYPE_CONVERSIONS_H_

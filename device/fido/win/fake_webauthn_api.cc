// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/fake_webauthn_api.h"

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"

namespace device {

FakeWinWebAuthnApi::FakeWinWebAuthnApi() = default;
FakeWinWebAuthnApi::~FakeWinWebAuthnApi() = default;

bool FakeWinWebAuthnApi::IsAvailable() const {
  return is_available_;
}
HRESULT FakeWinWebAuthnApi::IsUserVerifyingPlatformAuthenticatorAvailable(
    BOOL* result) {
  DCHECK(is_available_);
  *result = is_uvpaa_;
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorMakeCredential(
    HWND h_wnd,
    PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
    PCWEBAUTHN_USER_ENTITY_INFORMATION user,
    PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
    PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) {
  DCHECK(is_available_);
  *credential_attestation_ptr = &attestation_;
  return result_;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    LPCWSTR rp_id,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    PWEBAUTHN_ASSERTION* assertion_ptr) {
  DCHECK(is_available_);
  *assertion_ptr = &assertion_;
  return result_;
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  NOTREACHED() << "not implemented";
  return E_NOTIMPL;
}

PCWSTR FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  // See the comment for WebAuthNGetErrorName() in <webauthn.h>.
  switch (hr) {
    case S_OK:
      return STRING16_LITERAL("Success");
    case NTE_EXISTS:
      return STRING16_LITERAL("InvalidStateError");
    case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):
    case NTE_NOT_SUPPORTED:
    case NTE_TOKEN_KEYSET_STORAGE_FULL:
      return STRING16_LITERAL("ConstraintError");
    case NTE_INVALID_PARAMETER:
      return STRING16_LITERAL("NotSupportedError");
    case NTE_DEVICE_NOT_FOUND:
    case NTE_NOT_FOUND:
    case HRESULT_FROM_WIN32(ERROR_CANCELLED):
    case NTE_USER_CANCELLED:
    case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
      return STRING16_LITERAL("NotAllowedError");
    default:
      return STRING16_LITERAL("UnknownError");
  }
}

void FakeWinWebAuthnApi::FreeCredentialAttestation(
    PWEBAUTHN_CREDENTIAL_ATTESTATION) {}

void FakeWinWebAuthnApi::FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) {
}

int FakeWinWebAuthnApi::Version() {
  return version_;
}

// static
WEBAUTHN_CREDENTIAL_ATTESTATION FakeWinWebAuthnApi::FakeAttestation() {
  WEBAUTHN_CREDENTIAL_ATTESTATION attestation = {};
  attestation.dwVersion = WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_1;
  attestation.cbAuthenticatorData =
      sizeof(test_data::kCtap2MakeCredentialAuthData);
  attestation.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kCtap2MakeCredentialAuthData));
  attestation.cbAttestation =
      sizeof(test_data::kPackedAttestationStatementCBOR);
  attestation.pbAttestation = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kPackedAttestationStatementCBOR));
  attestation.cbAttestationObject = 0;
  attestation.cbCredentialId = 0;
  attestation.pwszFormatType = L"packed";
  attestation.dwAttestationDecodeType = 0;
  return attestation;
}

// static
WEBAUTHN_ASSERTION FakeWinWebAuthnApi::FakeAssertion() {
  WEBAUTHN_CREDENTIAL credential = {};
  // No constant macro available because 1 is the current version
  credential.dwVersion = 1;
  credential.cbId = sizeof(test_data::kCredentialId);
  credential.pbId =
      reinterpret_cast<PBYTE>(const_cast<uint8_t*>(test_data::kCredentialId));
  credential.pwszCredentialType = L"public-key";

  WEBAUTHN_ASSERTION assertion = {};
  // No constant macro available because 1 is the current version
  assertion.dwVersion = 1;
  assertion.cbAuthenticatorData = sizeof(test_data::kTestSignAuthenticatorData);
  assertion.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(test_data::kTestSignAuthenticatorData));
  assertion.cbSignature = sizeof(test_data::kCtap2GetAssertionSignature);
  assertion.pbSignature = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(test_data::kCtap2GetAssertionSignature));
  assertion.Credential = credential;
  assertion.pbUserId = nullptr;
  assertion.cbUserId = 0;
  return assertion;
}

}  // namespace device

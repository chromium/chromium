// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_

#include "base/component_export.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) FakeWinWebAuthnApi : public WinWebAuthnApi {
 public:
  FakeWinWebAuthnApi();
  ~FakeWinWebAuthnApi() override;

  // Inject the return value for WinWebAuthnApi::IsAvailable().
  void set_available(bool available) { is_available_ = available; }

  void set_hresult(HRESULT result) { result_ = result; }

  // Inject the return value for
  // WinWebAuthnApi::IsUserverifyingPlatformAuthenticatorAvailable().
  void set_is_uvpaa(bool is_uvpaa) { is_uvpaa_ = is_uvpaa; }

  void set_version(int version) { version_ = version; }

  // WinWebAuthnApi:
  bool IsAvailable() const override;
  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override;
  HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) override;
  HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) override;
  HRESULT CancelCurrentOperation(GUID* cancellation_id) override;
  PCWSTR GetErrorName(HRESULT hr) override;
  void FreeCredentialAttestation(PWEBAUTHN_CREDENTIAL_ATTESTATION) override;
  void FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) override;
  int Version() override;

 private:
  static WEBAUTHN_CREDENTIAL_ATTESTATION FakeAttestation();
  static WEBAUTHN_ASSERTION FakeAssertion();

  bool is_available_ = true;
  bool is_uvpaa_ = false;
  int version_ = WEBAUTHN_API_VERSION_2;
  WEBAUTHN_CREDENTIAL_ATTESTATION attestation_ = FakeAttestation();
  WEBAUTHN_ASSERTION assertion_ = FakeAssertion();
  HRESULT result_ = S_OK;
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
